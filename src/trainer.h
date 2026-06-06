#pragma once
// ============================================================================
// CONQUEST - Training Mode (Headless Self-Play)
// Runs AI vs AI games, collects (state, policy, value) training data.
// No SDL2 or OpenGL dependency — pure computation.
// ============================================================================

#include "core.h"
#include "game.h"
#include "ai.h"
#include "state_encoder.h"

// ============================================================================
// TrainingConfig — configurable parameters for a training run
// ============================================================================
struct TrainingConfig {
    int     num_episodes;       // total games to play (default: 100)
    int     ai_depth;           // search depth for both players (default: 3)
    int     ai_time_limit_ms;   // time limit per AI move in ms (default: 1000)
    float   temperature;        // policy temperature (default: 1.0)
    bool    use_priors;         // use AI eval as policy prior (default: true)
    int     max_turns;          // max turns before draw (default: 200)
    bool    random_draft;       // randomize draft instead of sequential (default: true)
    uint32_t seed_start;        // first map seed (default: 1)
    bool    seed_increment;     // increment seed each episode (default: true)
    bool    verbose;            // print progress (default: false)
    bool    deduplicate;        // skip duplicate positions (default: true)
    int     max_examples_per_episode;  // cap examples per game (default: 500)

    // Output
    char    output_dir[256];    // directory for episode files
    bool    single_file;        // write all to one file instead of per-episode (default: false)
    char    output_file[256];   // single output file path

    TrainingConfig();
};

// ============================================================================
// TrainingStats — aggregate statistics from a training run
// ============================================================================
struct TrainingStats {
    int     episodes_played;
    int     p1_wins;
    int     p2_wins;
    int     draws;
    int     total_examples;
    int     total_turns;
    double  total_time_seconds;
    int     avg_turns_per_episode;
    int     avg_examples_per_episode;
    double  avg_time_per_episode;

    TrainingStats();
    void print() const;
};

// ============================================================================
// Trainer — orchestrates headless self-play training
// ============================================================================
class Trainer {
public:
    Trainer();
    ~Trainer();

    // Run a full training session with the given config
    TrainingStats run(const TrainingConfig& config);

private:
    // Play a single self-play game, filling the episode buffer
    // Returns: 0=draw, 1=P1 win, 2=P2 win
    int play_episode(const TrainingConfig& config, uint32_t seed,
                     EpisodeBuffer& buffer, AI& ai_p1, AI& ai_p2);

    // Run the draft phase (random or sequential)
    void do_draft(GameState& state, bool random, uint32_t rng_state);

    // Run the research phase (AI-guided or random)
    void do_research(GameState& state, uint32_t rng_state);

    // Execute one AI turn and record the example
    void do_ai_turn(GameState& state, ZobristTable& zobrist, AI& ai,
                    const TrainingConfig& config, EpisodeBuffer& buffer,
                    int current_value_sign);

    // Simple PRNG for training decisions
    static uint32_t xorshift32(uint32_t& state);
};
