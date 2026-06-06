#pragma once
// ============================================================================
// CONQUEST - AI Player (Alpha-Beta + Iterative Deepening + Transposition Table)
// ============================================================================

#include <cmath>    // required by core.h for std::round (hex_round)
#include "core.h"
#include <chrono>

struct AIResult {
    Move best_move;
    int  score;      // evaluation score (from P1's perspective)
    int  depth;      // search depth reached
    int  nodes;      // total nodes searched
};

class AI {
public:
    AI();

    // Set search parameters
    void set_depth(int depth);
    void set_time_limit(int ms);

    // Find best move for current player
    AIResult search(const GameState& state, const ZobristTable& zt);

private:
    int  max_depth_;
    int  time_limit_ms_;
    int  nodes_searched_;
    bool abort_;
    std::chrono::steady_clock::time_point start_time_;

    // Alpha-beta with fail-soft
    int alpha_beta(GameState& state, const ZobristTable& zt,
                   int depth, int alpha, int beta, bool maximizing);

    // Quiescence search (search captures even at depth 0)
    int quiescence(GameState& state, const ZobristTable& zt,
                   int alpha, int beta, bool maximizing, int qdepth);

    // Static evaluation (from P1's perspective: positive = good for P1)
    int evaluate(const GameState& state);

    // Move ordering heuristic (higher = search first)
    int move_score(const GameState& state, const Move& move);

    // Transposition table ---------------------------------------------------
    struct TTEntry {
        uint64_t hash;
        int      score;
        int      depth;
        Move     best_move;
        enum Flag : uint8_t { EXACT, LOWER, UPPER } flag;
    };

    static constexpr int TT_SIZE   = 1 << 20;   // ~1M entries
    static constexpr int TT_INVALID = 999999;     // sentinel: no usable score

    TTEntry tt_[TT_SIZE];

    int  tt_probe(uint64_t hash, int depth, int alpha, int beta, Move& best_move);
    void tt_store(uint64_t hash, int depth, int score, int flag, const Move& best_move);
};
