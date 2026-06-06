// ============================================================================
// CONQUEST - AI Player Implementation
// Alpha-Beta + Iterative Deepening + Quiescence + Transposition Table
// ============================================================================

#include "ai.h"
#include "game.h"
#include "board.h"

#include <algorithm>
#include <cstring>
#include <chrono>

// ============================================================================
// Helpers (file-local)
// ============================================================================

// Material value of a unit type (evaluation scale x10).
// Spec: Scout=2, Soldier=3, Fortress=4, Cannon=5, Commander=6, HQ=100
static int unit_material_value(UnitType t) {
    switch (t) {
        case UnitType::SCOUT:     return 20;   // 2 * 10
        case UnitType::SOLDIER:   return 30;   // 3 * 10
        case UnitType::FORTRESS:  return 40;   // 4 * 10
        case UnitType::CANNON:    return 50;   // 5 * 10
        case UnitType::COMMANDER: return 60;   // 6 * 10
        case UnitType::HQ:        return 1000; // 100 * 10
        default:                  return 0;
    }
}

// Find the index of a unit by its unique id. Returns -1 if not found.
static int find_unit_by_id(const GameState& state, int unit_id) {
    for (int i = 0; i < MAX_UNITS * 2; i++) {
        if (state.units[i].id == unit_id) return i;
    }
    return -1;
}

// Sort an array of Moves by move_score (descending) using selection sort.
// Pre-computes scores to avoid redundant calls.
static void sort_moves_by_score(const GameState& state, Move* moves, int count,
                                int (*scorer)(const GameState&, const Move&)) {
    if (count <= 1) return;
    int* scores = static_cast<int*>(alloca(count * sizeof(int)));
    for (int i = 0; i < count; i++)
        scores[i] = scorer(state, moves[i]);

    for (int i = 0; i < count - 1; i++) {
        int best = i;
        for (int j = i + 1; j < count; j++) {
            if (scores[j] > scores[best]) best = j;
        }
        if (best != i) {
            std::swap(moves[i], moves[best]);
            std::swap(scores[i], scores[best]);
        }
    }
}

// Promote a specific move to the front of the list (if found).
static void promote_move_to_front(Move* moves, int count, const Move& target) {
    if (target.unit_id < 0) return;
    for (int i = 0; i < count; i++) {
        if (moves[i].unit_id == target.unit_id &&
            moves[i].from   == target.from   &&
            moves[i].to     == target.to     &&
            moves[i].action == target.action) {
            if (i != 0) std::swap(moves[0], moves[i]);
            return;
        }
    }
}

// ============================================================================
// AI -- Constructor
// ============================================================================

AI::AI() {
    max_depth_      = 4;
    time_limit_ms_  = 5000;
    nodes_searched_ = 0;
    abort_          = false;
    for (int i = 0; i < TT_SIZE; i++) {
        tt_[i] = TTEntry{};
    }
}

// ============================================================================
// AI -- Parameter setters
// ============================================================================

void AI::set_depth(int depth) {
    max_depth_ = depth;
}

void AI::set_time_limit(int ms) {
    time_limit_ms_ = ms;
}

// ============================================================================
// AI -- Top-level search (iterative deepening)
// ============================================================================

AIResult AI::search(const GameState& state, const ZobristTable& zt) {
    nodes_searched_ = 0;
    abort_          = false;
    start_time_     = std::chrono::steady_clock::now();

    AIResult result;
    result.best_move = {};
    result.score     = 0;
    result.depth     = 0;
    result.nodes     = 0;

    // Quick terminal check
    int victory = game_check_victory(state);
    if (victory == 1) { result.score =  100000; return result; }
    if (victory == 2) { result.score = -100000; return result; }

    // Generate all legal moves for the current player
    Move moves[512];
    int move_count = game_generate_moves(state, moves, 512);

    if (move_count == 0) {
        result.score = evaluate(state);
        return result;
    }

    // Heuristic sort at the root (best-first for early cutoffs)
    sort_moves_by_score(state, moves, move_count, /* scorer */ nullptr);
    // We pass nullptr as a signal -- we'll use our own move_score through a
    // wrapper.  Actually let's just inline the sort here:
    {
        int scores[512];
        for (int i = 0; i < move_count; i++)
            scores[i] = move_score(state, moves[i]);
        for (int i = 0; i < move_count - 1; i++) {
            int best = i;
            for (int j = i + 1; j < move_count; j++) {
                if (scores[j] > scores[best]) best = j;
            }
            if (best != i) {
                std::swap(moves[i], moves[best]);
                std::swap(scores[i], scores[best]);
            }
        }
    }

    Move best_move      = moves[0];
    int  best_score      = 0;
    int  depth_reached   = 0;
    bool root_maximizing = (state.current == Player::P1);

    // Iterative deepening from depth 1 up to max_depth_
    for (int depth = 1; depth <= max_depth_; depth++) {
        int  alpha           = -100000;
        int  beta            =  100000;
        Move current_best    = moves[0];
        int  current_best_score = root_maximizing ? -100000 : 100000;

        for (int i = 0; i < move_count; i++) {
            if (abort_) break;

            GameState copy = state;
            game_apply_move(copy, moves[i]);

            bool next_max = (copy.current == Player::P1);
            int score = alpha_beta(copy, zt, depth - 1, alpha, beta, next_max);

            if (abort_) break;

            if (root_maximizing) {
                if (score > current_best_score) {
                    current_best_score = score;
                    current_best       = moves[i];
                }
                alpha = std::max(alpha, score);
            } else {
                if (score < current_best_score) {
                    current_best_score = score;
                    current_best       = moves[i];
                }
                beta = std::min(beta, score);
            }
        }

        if (!abort_) {
            best_move     = current_best;
            best_score    = current_best_score;
            depth_reached = depth;

            // Promote the best move to front for the next iteration (PV move)
            for (int i = 0; i < move_count; i++) {
                if (moves[i].unit_id == best_move.unit_id &&
                    moves[i].from   == best_move.from   &&
                    moves[i].to     == best_move.to     &&
                    moves[i].action == best_move.action) {
                    if (i != 0) std::swap(moves[0], moves[i]);
                    break;
                }
            }
        } else {
            // Time ran out -- keep the best result from the last fully
            // completed depth.
            break;
        }
    }

    result.best_move = best_move;
    result.score     = best_score;
    result.depth     = depth_reached;
    result.nodes     = nodes_searched_;
    return result;
}

// ============================================================================
// AI -- Alpha-Beta (fail-soft)
// ============================================================================

int AI::alpha_beta(GameState& state, const ZobristTable& zt,
                   int depth, int alpha, int beta, bool maximizing)
{
    nodes_searched_++;

    // Check time every 4096 nodes
    if ((nodes_searched_ & 4095) == 0) {
        auto now     = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - start_time_).count();
        if (elapsed >= time_limit_ms_) {
            abort_ = true;
            return 0;
        }
    }
    if (abort_) return 0;

    // Terminal position?
    int victory = game_check_victory(state);
    if (victory == 1) return  100000 + depth;   // prefer faster P1 wins
    if (victory == 2) return -100000 - depth;   // prefer faster P2 wins

    // Leaf node: drop into quiescence
    if (depth <= 0) {
        return quiescence(state, zt, alpha, beta, maximizing, 0);
    }

    // Transposition table probe
    Move tt_best_move = {};
    int  tt_score = tt_probe(state.hash, depth, alpha, beta, tt_best_move);
    if (tt_score != TT_INVALID) {
        return tt_score;
    }

    // Generate moves
    Move moves[512];
    int move_count = game_generate_moves(state, moves, 512);

    if (move_count == 0) {
        // No moves: static evaluation
        return evaluate(state);
    }

    // Sort by heuristic (best first for pruning)
    {
        int scores[512];
        for (int i = 0; i < move_count; i++)
            scores[i] = move_score(state, moves[i]);
        for (int i = 0; i < move_count - 1; i++) {
            int best = i;
            for (int j = i + 1; j < move_count; j++) {
                if (scores[j] > scores[best]) best = j;
            }
            if (best != i) {
                std::swap(moves[i], moves[best]);
                std::swap(scores[i], scores[best]);
            }
        }
    }

    // Put TT best move first (if any)
    promote_move_to_front(moves, move_count, tt_best_move);

    int orig_alpha = alpha;
    int best_score = maximizing ? -100000 : 100000;
    Move best_move = moves[0];

    for (int i = 0; i < move_count; i++) {
        if (abort_) break;

        // Copy state, apply move, recurse
        GameState copy = state;
        game_apply_move(copy, moves[i]);

        bool next_max = (copy.current == Player::P1);
        int score = alpha_beta(copy, zt, depth - 1, alpha, beta, next_max);

        if (abort_) break;

        if (maximizing) {
            if (score > best_score) {
                best_score = score;
                best_move  = moves[i];
            }
            alpha = std::max(alpha, score);
        } else {
            if (score < best_score) {
                best_score = score;
                best_move  = moves[i];
            }
            beta = std::min(beta, score);
        }

        // Beta cutoff
        if (alpha >= beta) break;
    }

    // Store in transposition table (only if we weren't aborted)
    if (!abort_) {
        int flag;
        if (best_score <= orig_alpha) flag = TTEntry::UPPER;   // failed low
        else if (best_score >= beta)  flag = TTEntry::LOWER;   // beta cutoff
        else                          flag = TTEntry::EXACT;
        tt_store(state.hash, depth, best_score, flag, best_move);
    }

    return best_score;
}

// ============================================================================
// AI -- Quiescence Search
// Only explores ATTACK moves at leaf nodes to avoid the horizon effect.
// ============================================================================

int AI::quiescence(GameState& state, const ZobristTable& zt,
                   int alpha, int beta, bool maximizing, int qdepth)
{
    nodes_searched_++;

    // Periodic time check
    if ((nodes_searched_ & 4095) == 0) {
        auto now     = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - start_time_).count();
        if (elapsed >= time_limit_ms_) {
            abort_ = true;
            return 0;
        }
    }
    if (abort_) return 0;

    // Terminal position
    int victory = game_check_victory(state);
    if (victory == 1) return  100000;
    if (victory == 2) return -100000;

    // Stand-pat: the static evaluation of the quiet position
    int stand_pat = evaluate(state);

    // Prevent quiescence explosion
    if (qdepth >= 4) return stand_pat;

    if (maximizing) {
        if (stand_pat >= beta) return stand_pat;   // fail-soft
        int best = stand_pat;
        if (stand_pat > alpha) alpha = stand_pat;

        // Generate and filter ATTACK moves only
        Move moves[512];
        int move_count = game_generate_moves(state, moves, 512);

        int attack_count = 0;
        for (int i = 0; i < move_count; i++) {
            if (moves[i].action == ActionType::ATTACK) {
                moves[attack_count++] = moves[i];
            }
        }

        if (attack_count == 0) return stand_pat;

        // Sort attacks by heuristic (MVV-LVA style)
        {
            int scores[512];
            for (int i = 0; i < attack_count; i++)
                scores[i] = move_score(state, moves[i]);
            for (int i = 0; i < attack_count - 1; i++) {
                int best_idx = i;
                for (int j = i + 1; j < attack_count; j++) {
                    if (scores[j] > scores[best_idx]) best_idx = j;
                }
                if (best_idx != i) {
                    std::swap(moves[i], moves[best_idx]);
                    std::swap(scores[i], scores[best_idx]);
                }
            }
        }

        for (int i = 0; i < attack_count; i++) {
            if (abort_) break;

            GameState copy = state;
            game_apply_move(copy, moves[i]);

            bool next_max = (copy.current == Player::P1);
            int score = quiescence(copy, zt, alpha, beta, next_max, qdepth + 1);

            if (abort_) break;

            best = std::max(best, score);
            if (best >= beta) return best;    // fail-soft cutoff
            alpha = std::max(alpha, best);
        }
        return best;

    } else {
        // Minimizing
        if (stand_pat <= alpha) return stand_pat;  // fail-soft
        int best = stand_pat;
        if (stand_pat < beta) beta = stand_pat;

        Move moves[512];
        int move_count = game_generate_moves(state, moves, 512);

        int attack_count = 0;
        for (int i = 0; i < move_count; i++) {
            if (moves[i].action == ActionType::ATTACK) {
                moves[attack_count++] = moves[i];
            }
        }

        if (attack_count == 0) return stand_pat;

        {
            int scores[512];
            for (int i = 0; i < attack_count; i++)
                scores[i] = move_score(state, moves[i]);
            for (int i = 0; i < attack_count - 1; i++) {
                int best_idx = i;
                for (int j = i + 1; j < attack_count; j++) {
                    if (scores[j] > scores[best_idx]) best_idx = j;
                }
                if (best_idx != i) {
                    std::swap(moves[i], moves[best_idx]);
                    std::swap(scores[i], scores[best_idx]);
                }
            }
        }

        for (int i = 0; i < attack_count; i++) {
            if (abort_) break;

            GameState copy = state;
            game_apply_move(copy, moves[i]);

            bool next_max = (copy.current == Player::P1);
            int score = quiescence(copy, zt, alpha, beta, next_max, qdepth + 1);

            if (abort_) break;

            best = std::min(best, score);
            if (best <= alpha) return best;    // fail-soft cutoff
            beta = std::min(beta, best);
        }
        return best;
    }
}

// ============================================================================
// AI -- Static Evaluation
// All values from P1's perspective (positive = good for P1).
// Internal scale: x10 to keep fractional contributions in integer math.
// ============================================================================

int AI::evaluate(const GameState& state) {
    // ------------------------------------------------------------------
    // Terminal: quick victory check
    // ------------------------------------------------------------------
    int victory = game_check_victory(state);
    if (victory == 1) return  100000;
    if (victory == 2) return -100000;

    int score = 0;

    // ------------------------------------------------------------------
    // Locate commanders (for buff radius check later)
    // ------------------------------------------------------------------
    int p1_cmd = -1, p2_cmd = -1;
    for (int i = 0; i < MAX_UNITS * 2; i++) {
        const Unit& u = state.units[i];
        if (u.type == UnitType::COMMANDER) {
            if (u.owner == Player::P1) p1_cmd = i;
            else                       p2_cmd = i;
        }
    }

    // ------------------------------------------------------------------
    // 1) Material + HP
    //    Material value scaled by HP ratio so damaged units are worth less.
    // ------------------------------------------------------------------
    for (int i = 0; i < MAX_UNITS * 2; i++) {
        const Unit& u = state.units[i];
        if (u.type == UnitType::NONE || u.owner == Player::NONE) continue;

        int base_val = unit_material_value(u.type);
        int hp_val   = (u.max_hp > 0) ? base_val * u.hp / u.max_hp : 0;

        if (u.owner == Player::P1) score += hp_val;
        else                       score -= hp_val;
    }

    // ------------------------------------------------------------------
    // 2) Position: tiles controlled (+1 each, x10 = +10)
    // ------------------------------------------------------------------
    score += (state.tiles_controlled[1] - state.tiles_controlled[2]) * 10;

    // ------------------------------------------------------------------
    // 3) Position: victory nodes controlled (+3 each, x10 = +30)
    // ------------------------------------------------------------------
    score += (state.nodes_controlled[1] - state.nodes_controlled[2]) * 30;

    // ------------------------------------------------------------------
    // 4) Energy (+0.5 each, x10 = +5)
    // ------------------------------------------------------------------
    score += (state.energy[1] - state.energy[2]) * 5;

    // ------------------------------------------------------------------
    // 5) Tech (+2 per researched tech, x10 = +20)
    // ------------------------------------------------------------------
    int p1_techs = 0, p2_techs = 0;
    for (int i = 1; i < TECH_COUNT; i++) {
        if (state.techs_p1[i].active && state.techs_p1[i].researched) p1_techs++;
        if (state.techs_p2[i].active && state.techs_p2[i].researched) p2_techs++;
    }
    score += (p1_techs - p2_techs) * 20;

    // ------------------------------------------------------------------
    // 6-8) Per-unit modifiers: isolation, formation, commander buff
    // ------------------------------------------------------------------
    for (int i = 0; i < MAX_UNITS * 2; i++) {
        const Unit& u = state.units[i];
        if (u.type == UnitType::NONE || u.owner == Player::NONE) continue;
        if (u.type == UnitType::HQ) continue;   // HQ can't be isolated/buffed

        int sign = (u.owner == Player::P1) ? 1 : -1;

        // 6) Isolation: -1 per isolated unit (x10 = -10)
        if (game_is_isolated(state, i)) {
            score -= sign * 10;
        }

        // 7) Formation: +1 per unit in a formation (x10 = +10)
        FormationType fmt = game_check_formation(state, i);
        if (fmt != FormationType::NONE) {
            score += sign * 10;
        }

        // 8) Commander buff: +0.5 per buffed unit (x10 = +5)
        if (u.type != UnitType::COMMANDER) {
            int cmd_idx = (u.owner == Player::P1) ? p1_cmd : p2_cmd;
            if (cmd_idx >= 0) {
                int buff_radius = get_commander_buff_radius(u.owner, state);
                if (u.pos.distance(state.units[cmd_idx].pos) <= buff_radius) {
                    score += sign * 5;
                }
            }
        }
    }

    // ------------------------------------------------------------------
    // 9) Victory progress: big bonus for controlling more victory nodes
    //    Quadratic scaling rewards concentration.
    // ------------------------------------------------------------------
    int node_diff = state.nodes_controlled[1] - state.nodes_controlled[2];
    if (node_diff > 0) {
        score += node_diff * node_diff * 50;     // accelerating returns
    } else if (node_diff < 0) {
        score -= (-node_diff) * (-node_diff) * 50;
    }

    return score;
}

// ============================================================================
// AI -- Move Ordering Heuristic
// Higher score = search first (more promising or more forcing).
// ============================================================================

int AI::move_score(const GameState& state, const Move& move) {
    int score = 0;

    switch (move.action) {
    case ActionType::ATTACK: {
        // +10 base, plus (target_value - attacker_value) so that
        // attacking high-value targets with cheap units is prioritized.
        score += 10;
        int attacker_idx = find_unit_by_id(state, move.unit_id);
        int target_idx   = board_unit_at(state, move.to);
        if (attacker_idx >= 0 && target_idx >= 0) {
            int attacker_val = unit_material_value(state.units[attacker_idx].type);
            int target_val   = unit_material_value(state.units[target_idx].type);
            score += (target_val - attacker_val);
        }
        break;
    }

    case ActionType::CAPTURE:
        // Claiming a victory node is extremely valuable
        score += 50;
        break;

    case ActionType::FORTIFY:
        score += 3;
        break;

    case ActionType::MOVE: {
        // +5 if moving toward any victory node, +1 otherwise
        int old_min = 1000, new_min = 1000;
        for (int v = 0; v < VICTORY_NODES; v++) {
            int od = move.from.distance(state.victory_hexes[v]);
            int nd = move.to.distance(state.victory_hexes[v]);
            if (od < old_min) old_min = od;
            if (nd < new_min) new_min = nd;
        }
        score += (new_min < old_min) ? 5 : 1;
        break;
    }

    default:
        // SPAWN (ACTION_SPAWN = static_cast<ActionType>(5)) and others
        if (move.action == ACTION_SPAWN) {
            score += 4;
        } else {
            score += 1;
        }
        break;
    }

    return score;
}

// ============================================================================
// AI -- Transposition Table Probe
// Returns TT_INVALID if no usable score is found.
// Sets best_move from the TT entry (even on depth mismatch, for ordering).
// ============================================================================

int AI::tt_probe(uint64_t hash, int depth, int alpha, int beta, Move& best_move) {
    int idx = static_cast<int>(hash & (TT_SIZE - 1));
    TTEntry& entry = tt_[idx];

    if (entry.hash != hash) return TT_INVALID;

    // Always harvest the best move (useful for ordering even at lower depth)
    best_move = entry.best_move;

    if (entry.depth >= depth) {
        switch (entry.flag) {
        case TTEntry::EXACT:
            return entry.score;
        case TTEntry::LOWER:
            if (entry.score >= beta) return entry.score;
            break;
        case TTEntry::UPPER:
            if (entry.score <= alpha) return entry.score;
            break;
        }
    }

    return TT_INVALID;
}

// ============================================================================
// AI -- Transposition Table Store
// Overwrites if the entry is empty or the new depth >= existing depth.
// ============================================================================

void AI::tt_store(uint64_t hash, int depth, int score, int flag,
                  const Move& best_move)
{
    int idx = static_cast<int>(hash & (TT_SIZE - 1));
    TTEntry& entry = tt_[idx];

    // Replacement policy: always overwrite if empty or equal/greater depth
    if (entry.hash == 0 || entry.depth <= depth) {
        entry.hash      = hash;
        entry.score     = score;
        entry.depth     = depth;
        entry.best_move = best_move;
        entry.flag      = static_cast<TTEntry::Flag>(flag);
    }
}
