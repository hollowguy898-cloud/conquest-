// ============================================================================
// CONQUEST - Training Mode Implementation
// Headless self-play for ML model training.
// ============================================================================

#include "trainer.h"
#include "board.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <set>

// ============================================================================
// Local helper: material value for move scoring
// ============================================================================
static int train_unit_material_value(UnitType t) {
    switch (t) {
        case UnitType::SCOUT:     return 20;
        case UnitType::SOLDIER:   return 30;
        case UnitType::FORTRESS:  return 40;
        case UnitType::CANNON:    return 50;
        case UnitType::COMMANDER: return 60;
        case UnitType::HQ:        return 1000;
        default:                  return 0;
    }
}

// ============================================================================
// TrainingConfig defaults
// ============================================================================
TrainingConfig::TrainingConfig() {
    num_episodes       = 100;
    ai_depth           = 3;
    ai_time_limit_ms   = 1000;
    temperature        = 1.0f;
    use_priors         = true;
    max_turns          = MAX_TRAINING_TURNS;
    random_draft       = true;
    seed_start         = 1;
    seed_increment     = true;
    verbose            = false;
    deduplicate        = true;
    max_examples_per_episode = 500;

    std::memset(output_dir, 0, sizeof(output_dir));
    std::strncpy(output_dir, "./training_data", sizeof(output_dir) - 1);
    single_file = false;
    std::memset(output_file, 0, sizeof(output_file));
    std::strncpy(output_file, "training_data.bin", sizeof(output_file) - 1);
}

// ============================================================================
// TrainingStats
// ============================================================================
TrainingStats::TrainingStats()
    : episodes_played(0), p1_wins(0), p2_wins(0), draws(0),
      total_examples(0), total_turns(0), total_time_seconds(0.0),
      avg_turns_per_episode(0), avg_examples_per_episode(0),
      avg_time_per_episode(0.0) {}

void TrainingStats::print() const {
    std::printf("\n=== Training Stats ===\n");
    std::printf("Episodes played:   %d\n", episodes_played);
    std::printf("P1 wins:           %d (%.1f%%)\n", p1_wins,
                episodes_played > 0 ? 100.0 * p1_wins / episodes_played : 0.0);
    std::printf("P2 wins:           %d (%.1f%%)\n", p2_wins,
                episodes_played > 0 ? 100.0 * p2_wins / episodes_played : 0.0);
    std::printf("Draws:             %d (%.1f%%)\n", draws,
                episodes_played > 0 ? 100.0 * draws / episodes_played : 0.0);
    std::printf("Total examples:    %d\n", total_examples);
    std::printf("Total turns:       %d\n", total_turns);
    std::printf("Avg turns/game:    %d\n", avg_turns_per_episode);
    std::printf("Avg examples/game: %d\n", avg_examples_per_episode);
    std::printf("Total time:        %.1fs\n", total_time_seconds);
    std::printf("Avg time/game:     %.2fs\n", avg_time_per_episode);
    std::printf("======================\n\n");
}

// ============================================================================
// Trainer constructor/destructor
// ============================================================================
Trainer::Trainer() {}
Trainer::~Trainer() {}

// ============================================================================
// Simple PRNG
// ============================================================================
uint32_t Trainer::xorshift32(uint32_t& state) {
    if (state == 0) state = 1;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

// ============================================================================
// Run full training session
// ============================================================================
TrainingStats Trainer::run(const TrainingConfig& config) {
    TrainingStats stats;
    StateEncoder::init_hex_lut();

    auto run_start = std::chrono::steady_clock::now();

    // Create AI players (heap-allocated: each AI has a ~40MB transposition table)
    AI* ai_p1 = new AI();
    AI* ai_p2 = new AI();
    ai_p1->set_depth(config.ai_depth);
    ai_p1->set_time_limit(config.ai_time_limit_ms);
    ai_p2->set_depth(config.ai_depth);
    ai_p2->set_time_limit(config.ai_time_limit_ms);

    // Single-file mode
    DataWriter single_writer;
    if (config.single_file) {
        char path[512];
        std::snprintf(path, sizeof(path), "%s/%s", config.output_dir, config.output_file);
        if (!single_writer.open(path)) {
            std::fprintf(stderr, "Failed to open output file: %s\n", path);
            return stats;
        }
    }

    for (int ep = 0; ep < config.num_episodes; ep++) {
        uint32_t seed = config.seed_start + (config.seed_increment ? ep : 0);

        // Heap-allocate to avoid stack overflow (TrainingExample is ~18KB each)
        EpisodeBuffer* episode = new EpisodeBuffer();
        episode->init(config.max_examples_per_episode);
        episode->seed = seed;

        // Play one game
        int winner = play_episode(config, seed, *episode, *ai_p1, *ai_p2);

        // Set winner
        episode->winner = winner;

        // Assign value to all examples based on game outcome
        for (int i = 0; i < episode->count; i++) {
            if (winner == 1) {
                // P1 won: value is +1 for P1 perspectives, -1 for P2
                episode->examples[i].value =
                    (episode->examples[i].current_player == 1) ? 1.0f : -1.0f;
            } else if (winner == 2) {
                // P2 won: value is -1 for P1 perspectives, +1 for P2
                episode->examples[i].value =
                    (episode->examples[i].current_player == 1) ? -1.0f : 1.0f;
            } else {
                // Draw
                episode->examples[i].value = 0.0f;
            }
        }

        // Write data
        if (config.single_file) {
            single_writer.write_episode(*episode);
        } else {
            DataWriter::write_batch(config.output_dir, ep, episode, 1);
        }

        // Update stats
        stats.episodes_played++;
        if (winner == 1) stats.p1_wins++;
        else if (winner == 2) stats.p2_wins++;
        else stats.draws++;
        stats.total_examples += episode->count;
        stats.total_turns += episode->turns;

        if (config.verbose) {
            std::printf("Episode %d/%d: seed=%u winner=%s turns=%d examples=%d\n",
                        ep + 1, config.num_episodes, seed,
                        winner == 1 ? "P1" : (winner == 2 ? "P2" : "DRAW"),
                        episode->turns, episode->count);
        } else if ((ep + 1) % 10 == 0 || ep == 0) {
            std::printf("Episode %d/%d...\n", ep + 1, config.num_episodes);
        }

        delete episode;
    }

    if (config.single_file) {
        single_writer.close();
    }

    delete ai_p1;
    delete ai_p2;

    auto run_end = std::chrono::steady_clock::now();
    stats.total_time_seconds = std::chrono::duration<double>(run_end - run_start).count();

    if (stats.episodes_played > 0) {
        stats.avg_turns_per_episode = stats.total_turns / stats.episodes_played;
        stats.avg_examples_per_episode = stats.total_examples / stats.episodes_played;
        stats.avg_time_per_episode = stats.total_time_seconds / stats.episodes_played;
    }

    return stats;
}

// ============================================================================
// Play one self-play episode
// ============================================================================
int Trainer::play_episode(const TrainingConfig& config, uint32_t seed,
                           EpisodeBuffer& buffer, AI& ai_p1, AI& ai_p2) {
    // Initialize game
    GameState state;
    ZobristTable zobrist;
    zobrist.init(seed);
    game_init(state, seed);
    game_compute_hash(state, zobrist);

    buffer.turns = 0;

    // --- Draft phase ---
    uint32_t rng_state = seed;
    do_draft(state, config.random_draft, rng_state);

    // --- Research phase (initial) ---
    do_research(state, rng_state);

    // --- Action phase loop ---
    std::set<uint64_t> seen_hashes;
    int consecutive_end_turns = 0;

    while (state.phase != GamePhase::GAME_OVER && state.turn < config.max_turns) {
        // Sync turn count to buffer
        buffer.turns = state.turn;
        // Check for repetition
        if (config.deduplicate) {
            if (seen_hashes.count(state.hash) > 0) {
                // Repeated position — force end turn
                game_end_turn(state);
                game_compute_hash(state, zobrist);
                consecutive_end_turns++;
                if (consecutive_end_turns >= 10) break;  // stuck, call draw
                continue;
            }
            seen_hashes.insert(state.hash);
        }
        consecutive_end_turns = 0;

        // Record state for training (only during action phases)
        if (state.phase == GamePhase::ACTION_P1 || state.phase == GamePhase::ACTION_P2) {
            if (buffer.count < config.max_examples_per_episode) {
                // Heap-allocate to avoid stack overflow
                TrainingExample* ex = new TrainingExample();
                StateEncoder::encode_features(state, ex->features);
                ex->current_player = static_cast<int>(state.current);
                ex->hash = state.hash;
                ex->turn = state.turn;
                ex->value = 0.0f;  // will be filled after game ends

                // Generate moves and encode policy
                Move* moves = new Move[1024];
                int move_count = game_generate_moves(state, moves, 1024);

                if (config.use_priors && move_count > 0) {
                    // Use AI move scoring as prior
                    int* scores = new int[move_count];
                    for (int i = 0; i < move_count; i++) {
                        // Simple heuristic scoring
                        scores[i] = 0;
                        switch (moves[i].action) {
                        case ActionType::ATTACK:
                            scores[i] = 100;
                            {
                                int target_idx = board_unit_at(state, moves[i].to);
                                if (target_idx >= 0) {
                                    scores[i] += train_unit_material_value(state.units[target_idx].type);
                                }
                            }
                            break;
                        case ActionType::CAPTURE:
                            scores[i] = 80;
                            break;
                        case ActionType::MOVE:
                            scores[i] = 10;
                            break;
                        case ActionType::FORTIFY:
                            scores[i] = 5;
                            break;
                        default:
                            if (moves[i].action == ACTION_SPAWN) scores[i] = 20;
                            break;
                        }
                    }

                    StateEncoder::encode_policy_with_priors(
                        state, moves, move_count, scores,
                        config.temperature, ex->policy);
                    delete[] scores;
                } else {
                    StateEncoder::encode_policy(moves, move_count, ex->policy);
                }

                buffer.add(*ex);
                delete ex;
                delete[] moves;
            }
        }

        // Execute AI turn
        if (state.phase == GamePhase::ACTION_P1) {
            do_ai_turn(state, zobrist, ai_p1, config, buffer, 1);
        } else if (state.phase == GamePhase::ACTION_P2) {
            do_ai_turn(state, zobrist, ai_p2, config, buffer, -1);
        } else if (state.phase == GamePhase::RESEARCH_P1 ||
                   state.phase == GamePhase::RESEARCH_P2) {
            // Auto-advance research: spend energy on random affordable tech
            do_research(state, rng_state);
            // Transition to action phase
            if (state.phase == GamePhase::RESEARCH_P1) {
                state.phase = GamePhase::ACTION_P1;
                state.current = Player::P1;
                game_start_turn(state);
                game_compute_hash(state, zobrist);
            } else if (state.phase == GamePhase::RESEARCH_P2) {
                state.phase = GamePhase::ACTION_P2;
                state.current = Player::P2;
                game_start_turn(state);
                game_compute_hash(state, zobrist);
            }
        }

        // Check victory
        int victor = game_check_victory(state);
        if (victor > 0) {
            state.winner = (victor == 1) ? Player::P1 : Player::P2;
            state.phase = GamePhase::GAME_OVER;
            return victor;
        }
    }

    // Max turns reached or stuck — draw
    return 0;
}

// ============================================================================
// Draft phase (random or sequential)
// ============================================================================
void Trainer::do_draft(GameState& state, bool random, uint32_t rng_state) {
    // Each player drafts DRAFT_COUNT techs
    for (int player = 1; player <= 2; player++) {
        Player p = static_cast<Player>(player);
        TechUpgrade* techs = (p == Player::P1) ? state.techs_p1 : state.techs_p2;

        if (random) {
            // Collect unselected tech indices (1..20)
            int candidates[20];
            int cand_count = 0;
            for (int i = 1; i < TECH_COUNT; i++) {
                if (!techs[i].active) {
                    candidates[cand_count++] = i;
                }
            }

            // Shuffle and pick first DRAFT_COUNT
            for (int i = cand_count - 1; i > 0; i--) {
                uint32_t r = xorshift32(rng_state);
                int j = (int)(r % (uint32_t)(i + 1));
                std::swap(candidates[i], candidates[j]);
            }

            for (int i = 0; i < DRAFT_COUNT && i < cand_count; i++) {
                game_draft_tech(state, p, static_cast<TechID>(candidates[i]));
            }
        } else {
            // Sequential: just pick 1..10
            for (int i = 1; i <= DRAFT_COUNT; i++) {
                game_draft_tech(state, p, static_cast<TechID>(i));
            }
        }
    }

    game_end_draft(state);
}

// ============================================================================
// Research phase — spend energy on affordable drafted techs
// ============================================================================
void Trainer::do_research(GameState& state, uint32_t rng_state) {
    for (int player = 1; player <= 2; player++) {
        Player p = static_cast<Player>(player);
        TechUpgrade* techs = (p == Player::P1) ? state.techs_p1 : state.techs_p2;
        int player_idx = static_cast<int>(p);

        // Collect researchable techs
        int researchable[20];
        int res_count = 0;
        for (int i = 1; i < TECH_COUNT; i++) {
            if (techs[i].active && !techs[i].researched &&
                state.energy[player_idx] >= techs[i].research_cost) {
                researchable[res_count++] = i;
            }
        }

        // Research in random order while we can afford it
        for (int i = res_count - 1; i > 0; i--) {
            uint32_t r = xorshift32(rng_state);
            int j = (int)(r % (uint32_t)(i + 1));
            std::swap(researchable[i], researchable[j]);
        }

        for (int i = 0; i < res_count; i++) {
            int tech_idx = researchable[i];
            if (state.energy[player_idx] >= techs[tech_idx].research_cost) {
                game_research_tech(state, p, static_cast<TechID>(tech_idx));
            }
        }
    }
}

// ============================================================================
// Execute one AI turn
// ============================================================================
void Trainer::do_ai_turn(GameState& state, ZobristTable& zobrist, AI& ai,
                          const TrainingConfig& config, EpisodeBuffer& buffer,
                          int current_value_sign) {
    // AI makes moves until no AP left or it decides to end turn
    int moves_this_turn = 0;
    const int max_moves_per_turn = 20;  // safety cap

    while (state.phase == GamePhase::ACTION_P1 ||
           state.phase == GamePhase::ACTION_P2) {
        // Check if current player has AP
        Player cp = state.current;
        int start = (cp == Player::P1) ? 0 : MAX_UNITS;
        int end = (cp == Player::P1) ? MAX_UNITS : MAX_UNITS * 2;
        bool has_ap = false;
        for (int i = start; i < end; i++) {
            if (state.units[i].type != UnitType::NONE &&
                state.units[i].owner == cp && state.units[i].ap > 0 &&
                state.units[i].type != UnitType::HQ) {
                has_ap = true;
                break;
            }
        }
        if (!has_ap || moves_this_turn >= max_moves_per_turn) {
            game_end_turn(state);
            game_compute_hash(state, zobrist);
            break;
        }

        // AI search
        AIResult result = ai.search(state, zobrist);

        if (result.best_move.action == ActionType::NONE) {
            // AI passes
            game_end_turn(state);
            game_compute_hash(state, zobrist);
            break;
        }

        // Apply the move
        game_apply_move(state, result.best_move);
        game_compute_hash(state, zobrist);
        moves_this_turn++;

        // Check victory
        int victor = game_check_victory(state);
        if (victor > 0) {
            state.winner = (victor == 1) ? Player::P1 : Player::P2;
            state.phase = GamePhase::GAME_OVER;
            return;
        }
    }
}
