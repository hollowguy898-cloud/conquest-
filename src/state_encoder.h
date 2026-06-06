#pragma once
// ============================================================================
// CONQUEST - State Encoder for ML Training
// Converts GameState to flat feature tensors and policy vectors.
//
// Feature encoding (AlphaZero-style channel layout per hex):
//   Per-hex channels (127 hexes × 28 channels = 3556 floats):
//     Ch 0-4:   Terrain one-hot (plains, mountain, water, high_ground, victory)
//     Ch 5:     Elevation (0 or 1)
//     Ch 6-7:   Influence (P1, P2, normalized 0-1)
//     Ch 8-9:   Ownership (P1, P2, binary)
//     Ch 10-16: P1 unit type one-hot (NONE,SCOUT,SOLDIER,FORTRESS,CANNON,COMMANDER,HQ)
//     Ch 17-23: P2 unit type one-hot
//     Ch 24:    P1 unit HP ratio (0-1)
//     Ch 25:    P2 unit HP ratio
//     Ch 26:    P1 unit AP (normalized 0-1, max=3)
//     Ch 27:    P2 unit AP
//
//   Global features (12 floats):
//     G0:  Current player (0=P1, 1=P2)
//     G1:  Turn number (normalized by 100)
//     G2:  P1 energy (normalized by 50)
//     G3:  P2 energy
//     G4:  P1 victory nodes / VICTORY_NEEDED
//     G5:  P2 victory nodes / VICTORY_NEEDED
//     G6:  P1 tiles / VALID_TILE_COUNT
//     G7:  P2 tiles / VALID_TILE_COUNT
//     G8:  P1 unit count / MAX_UNITS
//     G9:  P2 unit count / MAX_UNITS
//     G10: P1 researched techs / 20
//     G11: P2 researched techs / 20
//
//   Total feature size: 3556 + 12 = 3568 floats
//
// Policy encoding (1018 floats):
//   Slots 0-126:     Move to hex (which hex the current player moves a unit to)
//   Slots 127-253:   Attack hex
//   Slots 254-387:   Spawn type at hex (5 types × 127 hexes)
//   Slots 388-514:   Capture at hex
//   Slot 515:        Fortify
//   Slot 516:        End turn / pass
//
//   Illegal moves get probability 0; the policy is normalized over legal moves.
// ============================================================================

#include "core.h"
#include "game.h"
#include "board.h"

// ============================================================================
// Constants
// ============================================================================
constexpr int HEX_COUNT       = VALID_TILE_COUNT;  // 127
constexpr int TERRAIN_CHANNELS = 5;
constexpr int PER_HEX_CHANNELS = 28;
constexpr int GLOBAL_FEATURES  = 12;
constexpr int FEATURE_SIZE     = HEX_COUNT * PER_HEX_CHANNELS + GLOBAL_FEATURES;  // 3568
constexpr int POLICY_SIZE      = 1018;

// Policy slot offsets
constexpr int POLICY_MOVE_START    = 0;
constexpr int POLICY_MOVE_END      = 127;    // exclusive
constexpr int POLICY_ATTACK_START  = 127;
constexpr int POLICY_ATTACK_END    = 254;
constexpr int POLICY_SPAWN_START   = 254;
constexpr int POLICY_SPAWN_END    = 254 + 5 * 127;  // 889
constexpr int POLICY_CAPTURE_START = 889;
constexpr int POLICY_CAPTURE_END   = 889 + 127;     // 1016
constexpr int POLICY_FORTIFY       = 1016;
constexpr int POLICY_END_TURN      = 1017;

// Maximum turns before declaring a draw
constexpr int MAX_TRAINING_TURNS = 200;

// ============================================================================
// TrainingExample — one (state, policy, value) tuple
// ============================================================================
struct TrainingExample {
    float features[FEATURE_SIZE];   // state encoding
    float policy[POLICY_SIZE];      // action probabilities (normalized)
    float value;                    // game outcome from P1's perspective (+1/-1/0)
    int   current_player;           // 1=P1, 2=P2
    uint64_t hash;                  // Zobrist hash for dedup
    int   turn;                     // turn number when this example was generated
};

// ============================================================================
// StateEncoder — static methods for encoding game state
// ============================================================================
class StateEncoder {
public:
    // Encode the full game state into a feature vector
    static void encode_features(const GameState& state, float* out);

    // Encode legal moves as a policy vector (1.0/legal_count for each legal move, 0 otherwise)
    static void encode_policy(const Move* moves, int move_count, float* out);

    // Encode legal moves with AI-computed priors (softmax of move scores)
    static void encode_policy_with_priors(const GameState& state,
                                           const Move* moves, int move_count,
                                           const int* move_scores, float temperature,
                                           float* out);

    // Map a move to its policy slot index (-1 if unmappable)
    static int move_to_policy_slot(const Move& move);

    // Map a policy slot back to a move template (action type + target hex)
    struct PolicyAction {
        ActionType action;
        int        target_hex_index;  // index into valid hex list
        UnitType   spawn_type;        // only for ACTION_SPAWN
    };
    static PolicyAction policy_slot_to_action(int slot);

    // Get the valid hex at a given linear index (0..126)
    static Hex valid_hex_at(int index);

    // Get the linear index of a valid hex (0..126), or -1
    static int valid_hex_index(Hex h);

    // Build the valid hex lookup table (call once at startup)
    static void init_hex_lut();

private:
    static Hex  valid_hexes_[VALID_TILE_COUNT];
    static int  hex_to_lut_[BOARD_SIZE * BOARD_SIZE];  // hex index -> LUT index
    static bool lut_initialized_;
};

// ============================================================================
// EpisodeBuffer — stores all examples from one self-play game
// ============================================================================
struct EpisodeBuffer {
    TrainingExample* examples;
    int               count;
    int               capacity;
    int               winner;  // 0=draw, 1=P1, 2=P2
    int               turns;
    uint32_t          seed;

    EpisodeBuffer();
    ~EpisodeBuffer();

    void init(int max_examples);
    void add(const TrainingExample& ex);
    void clear();
};

// ============================================================================
// DataWriter — writes training examples to binary files
// ============================================================================
// Binary format:
//   Header (64 bytes):
//     bytes 0-3:   magic "CNQT"
//     bytes 4-7:   version (uint32, currently 1)
//     bytes 8-11:  feature_size (uint32)
//     bytes 12-15: policy_size (uint32)
//     bytes 16-19: example_count (uint32)
//     bytes 20-23: winner (int32, 0=draw, 1=P1, 2=P2)
//     bytes 24-27: turns (int32)
//     bytes 28-31: seed (uint32)
//     bytes 32-63: reserved (zeros)
//   Examples (repeated):
//     features: feature_size × float32
//     policy:   policy_size × float32
//     value:    1 × float32
//     current_player: 1 × int32
//     hash:     1 × uint64
//     turn:     1 × int32
// ============================================================================
class DataWriter {
public:
    DataWriter();
    ~DataWriter();

    // Open a file for writing (creates header)
    bool open(const char* path);

    // Write a complete episode buffer
    bool write_episode(const EpisodeBuffer& episode);

    // Close the file and update header with example count
    void close();

    // Write a batch of episodes to separate numbered files
    static int write_batch(const char* directory, int start_index,
                           EpisodeBuffer* episodes, int count);

private:
    FILE* file_;
    int   example_count_;
    int   header_pos_;  // file position of header for updating count
};
