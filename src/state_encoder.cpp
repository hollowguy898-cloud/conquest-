// ============================================================================
// CONQUEST - State Encoder Implementation
// Converts GameState to flat feature tensors and policy vectors for ML.
// ============================================================================

#include "state_encoder.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cstdio>

// ============================================================================
// Static storage for hex LUT
// ============================================================================
Hex  StateEncoder::valid_hexes_[VALID_TILE_COUNT];
int  StateEncoder::hex_to_lut_[BOARD_SIZE * BOARD_SIZE];
bool StateEncoder::lut_initialized_ = false;

// ============================================================================
// Hex LUT initialization
// ============================================================================
void StateEncoder::init_hex_lut() {
    if (lut_initialized_) return;

    // Fill hex_to_lut_ with -1 (invalid)
    std::fill(hex_to_lut_, hex_to_lut_ + BOARD_SIZE * BOARD_SIZE, -1);

    // Iterate all valid hexes in a spiral from center
    int idx = 0;
    Hex center = {0, 0};
    auto spiral = hex_spiral(center, HEX_RADIUS);
    for (Hex h : spiral) {
        if (h.valid() && idx < VALID_TILE_COUNT) {
            valid_hexes_[idx] = h;
            hex_to_lut_[h.index()] = idx;
            idx++;
        }
    }

    lut_initialized_ = true;
}

Hex StateEncoder::valid_hex_at(int index) {
    if (!lut_initialized_) init_hex_lut();
    if (index < 0 || index >= VALID_TILE_COUNT) return {0, 0};
    return valid_hexes_[index];
}

int StateEncoder::valid_hex_index(Hex h) {
    if (!lut_initialized_) init_hex_lut();
    if (!h.valid()) return -1;
    int board_idx = h.index();
    if (board_idx < 0 || board_idx >= BOARD_SIZE * BOARD_SIZE) return -1;
    return hex_to_lut_[board_idx];
}

// ============================================================================
// Feature encoding
// ============================================================================
void StateEncoder::encode_features(const GameState& state, float* out) {
    if (!lut_initialized_) init_hex_lut();

    float* ptr = out;

    // --- Per-hex channels ---
    for (int i = 0; i < VALID_TILE_COUNT; i++) {
        Hex h = valid_hexes_[i];
        int board_idx = h.index();
        const Tile& tile = state.tiles[board_idx];

        // Ch 0-4: Terrain one-hot
        float terrain[TERRAIN_CHANNELS] = {0, 0, 0, 0, 0};
        switch (tile.terrain) {
        case Terrain::PLAINS:      terrain[0] = 1.0f; break;
        case Terrain::MOUNTAIN:    terrain[1] = 1.0f; break;
        case Terrain::WATER:       terrain[2] = 1.0f; break;
        case Terrain::HIGH_GROUND: terrain[3] = 1.0f; break;
        case Terrain::VICTORY:     terrain[4] = 1.0f; break;
        default:                   terrain[0] = 1.0f; break;
        }
        for (int c = 0; c < TERRAIN_CHANNELS; c++) *ptr++ = terrain[c];

        // Ch 5: Elevation
        *ptr++ = (float)tile.elevation;

        // Ch 6-7: Influence (normalized by HQ_INFLUENCE)
        *ptr++ = std::min(1.0f, (float)tile.p1_influence / (float)HQ_INFLUENCE);
        *ptr++ = std::min(1.0f, (float)tile.p2_influence / (float)HQ_INFLUENCE);

        // Ch 8-9: Ownership
        *ptr++ = (tile.owner == Player::P1) ? 1.0f : 0.0f;
        *ptr++ = (tile.owner == Player::P2) ? 1.0f : 0.0f;

        // Find units at this hex
        int p1_unit = -1, p2_unit = -1;
        for (int u = 0; u < MAX_UNITS; u++) {
            if (state.units[u].type != UnitType::NONE && state.units[u].pos == h) {
                p1_unit = u;
                break;
            }
        }
        for (int u = MAX_UNITS; u < MAX_UNITS * 2; u++) {
            if (state.units[u].type != UnitType::NONE && state.units[u].pos == h) {
                p2_unit = u;
                break;
            }
        }

        // Ch 10-16: P1 unit type one-hot
        for (int t = 0; t < UNIT_TYPE_COUNT; t++) *ptr++ = 0.0f;
        if (p1_unit >= 0) {
            ptr[-(UNIT_TYPE_COUNT - static_cast<int>(state.units[p1_unit].type))] = 1.0f;
        }

        // Ch 17-23: P2 unit type one-hot
        for (int t = 0; t < UNIT_TYPE_COUNT; t++) *ptr++ = 0.0f;
        if (p2_unit >= 0) {
            ptr[-(UNIT_TYPE_COUNT - static_cast<int>(state.units[p2_unit].type))] = 1.0f;
        }

        // Ch 24: P1 unit HP ratio
        if (p1_unit >= 0 && state.units[p1_unit].max_hp > 0) {
            *ptr++ = (float)state.units[p1_unit].hp / (float)state.units[p1_unit].max_hp;
        } else {
            *ptr++ = 0.0f;
        }

        // Ch 25: P2 unit HP ratio
        if (p2_unit >= 0 && state.units[p2_unit].max_hp > 0) {
            *ptr++ = (float)state.units[p2_unit].hp / (float)state.units[p2_unit].max_hp;
        } else {
            *ptr++ = 0.0f;
        }

        // Ch 26: P1 unit AP (normalized, max 3)
        if (p1_unit >= 0) {
            *ptr++ = std::min(1.0f, (float)state.units[p1_unit].ap / 3.0f);
        } else {
            *ptr++ = 0.0f;
        }

        // Ch 27: P2 unit AP
        if (p2_unit >= 0) {
            *ptr++ = std::min(1.0f, (float)state.units[p2_unit].ap / 3.0f);
        } else {
            *ptr++ = 0.0f;
        }
    }

    // --- Global features ---
    *ptr++ = (state.current == Player::P1) ? 0.0f : 1.0f;    // G0: current player
    *ptr++ = (float)state.turn / 100.0f;                       // G1: turn number
    *ptr++ = (float)state.energy[1] / 50.0f;                   // G2: P1 energy
    *ptr++ = (float)state.energy[2] / 50.0f;                   // G3: P2 energy
    *ptr++ = (float)state.nodes_controlled[1] / (float)VICTORY_NEEDED;  // G4
    *ptr++ = (float)state.nodes_controlled[2] / (float)VICTORY_NEEDED;  // G5
    *ptr++ = (float)state.tiles_controlled[1] / (float)VALID_TILE_COUNT; // G6
    *ptr++ = (float)state.tiles_controlled[2] / (float)VALID_TILE_COUNT; // G7

    // Unit counts
    int p1_count = 0, p2_count = 0;
    for (int i = 0; i < MAX_UNITS; i++)
        if (state.units[i].type != UnitType::NONE) p1_count++;
    for (int i = MAX_UNITS; i < MAX_UNITS * 2; i++)
        if (state.units[i].type != UnitType::NONE) p2_count++;
    *ptr++ = (float)p1_count / (float)MAX_UNITS;  // G8
    *ptr++ = (float)p2_count / (float)MAX_UNITS;  // G9

    // Researched techs
    int p1_techs = 0, p2_techs = 0;
    for (int i = 1; i < TECH_COUNT; i++) {
        if (state.techs_p1[i].active && state.techs_p1[i].researched) p1_techs++;
        if (state.techs_p2[i].active && state.techs_p2[i].researched) p2_techs++;
    }
    *ptr++ = (float)p1_techs / 20.0f;  // G10
    *ptr++ = (float)p2_techs / 20.0f;  // G11
}

// ============================================================================
// Policy encoding
// ============================================================================
int StateEncoder::move_to_policy_slot(const Move& move) {
    if (!lut_initialized_) init_hex_lut();

    int hex_idx = valid_hex_index(move.to);
    if (hex_idx < 0) return -1;

    switch (move.action) {
    case ActionType::MOVE:
        return POLICY_MOVE_START + hex_idx;

    case ActionType::ATTACK:
        return POLICY_ATTACK_START + hex_idx;

    case ACTION_SPAWN: {
        // Spawn encoding: type_index * 127 + hex_index
        UnitType spawn_type = static_cast<UnitType>(-(move.unit_id) - 1);
        int type_idx = static_cast<int>(spawn_type) - 1;  // SCOUT=0, SOLDIER=1, etc.
        if (type_idx < 0 || type_idx >= 5) return -1;
        return POLICY_SPAWN_START + type_idx * HEX_COUNT + hex_idx;
    }

    case ActionType::CAPTURE:
        return POLICY_CAPTURE_START + hex_idx;

    case ActionType::FORTIFY:
        return POLICY_FORTIFY;

    default:
        return -1;
    }
}

StateEncoder::PolicyAction StateEncoder::policy_slot_to_action(int slot) {
    PolicyAction pa;
    pa.action = ActionType::NONE;
    pa.target_hex_index = -1;
    pa.spawn_type = UnitType::NONE;

    if (slot >= POLICY_MOVE_START && slot < POLICY_MOVE_END) {
        pa.action = ActionType::MOVE;
        pa.target_hex_index = slot - POLICY_MOVE_START;
    } else if (slot >= POLICY_ATTACK_START && slot < POLICY_ATTACK_END) {
        pa.action = ActionType::ATTACK;
        pa.target_hex_index = slot - POLICY_ATTACK_START;
    } else if (slot >= POLICY_SPAWN_START && slot < POLICY_SPAWN_END) {
        pa.action = ACTION_SPAWN;
        int offset = slot - POLICY_SPAWN_START;
        int type_idx = offset / HEX_COUNT;
        pa.target_hex_index = offset % HEX_COUNT;
        pa.spawn_type = static_cast<UnitType>(type_idx + 1);  // SCOUT=1, etc.
    } else if (slot >= POLICY_CAPTURE_START && slot < POLICY_CAPTURE_END) {
        pa.action = ActionType::CAPTURE;
        pa.target_hex_index = slot - POLICY_CAPTURE_START;
    } else if (slot == POLICY_FORTIFY) {
        pa.action = ActionType::FORTIFY;
    } else if (slot == POLICY_END_TURN) {
        pa.action = ActionType::NONE;  // "end turn" is a meta-action
    }

    return pa;
}

void StateEncoder::encode_policy(const Move* moves, int move_count, float* out) {
    // Initialize all to 0
    std::fill(out, out + POLICY_SIZE, 0.0f);

    if (move_count == 0) {
        // No moves -> end turn
        out[POLICY_END_TURN] = 1.0f;
        return;
    }

    // Uniform distribution over legal moves
    float prob = 1.0f / (float)move_count;

    for (int i = 0; i < move_count; i++) {
        int slot = move_to_policy_slot(moves[i]);
        if (slot >= 0 && slot < POLICY_SIZE) {
            out[slot] += prob;
        }
    }

    // Any unmapped moves contribute to end_turn
    float mapped = 0.0f;
    for (int s = 0; s < POLICY_SIZE; s++) mapped += out[s];
    if (mapped < 1.0f) {
        out[POLICY_END_TURN] += (1.0f - mapped);
    }
}

void StateEncoder::encode_policy_with_priors(const GameState& state,
                                              const Move* moves, int move_count,
                                              const int* move_scores,
                                              float temperature,
                                              float* out) {
    std::fill(out, out + POLICY_SIZE, 0.0f);

    if (move_count == 0) {
        out[POLICY_END_TURN] = 1.0f;
        return;
    }

    // Compute softmax with temperature
    float max_score = -1e9f;
    for (int i = 0; i < move_count; i++) {
        float s = (float)move_scores[i] / temperature;
        if (s > max_score) max_score = s;
    }

    float sum_exp = 0.0f;
    float* exps = new float[move_count > 0 ? move_count : 1];
    for (int i = 0; i < move_count; i++) {
        exps[i] = std::exp(((float)move_scores[i] / temperature - max_score));
        sum_exp += exps[i];
    }

    if (sum_exp < 1e-8f) sum_exp = 1e-8f;

    for (int i = 0; i < move_count; i++) {
        float prob = exps[i] / sum_exp;
        int slot = move_to_policy_slot(moves[i]);
        if (slot >= 0 && slot < POLICY_SIZE) {
            out[slot] += prob;
        }
    }

    delete[] exps;

    // Unmapped probability goes to end_turn
    float mapped = 0.0f;
    for (int s = 0; s < POLICY_SIZE; s++) mapped += out[s];
    if (mapped < 1.0f) {
        out[POLICY_END_TURN] += (1.0f - mapped);
    }
}

// ============================================================================
// EpisodeBuffer
// ============================================================================
EpisodeBuffer::EpisodeBuffer()
    : examples(nullptr), count(0), capacity(0), winner(0), turns(0), seed(0) {}

EpisodeBuffer::~EpisodeBuffer() {
    delete[] examples;
}

void EpisodeBuffer::init(int max_examples) {
    capacity = max_examples;
    count = 0;
    examples = new TrainingExample[max_examples];
}

void EpisodeBuffer::add(const TrainingExample& ex) {
    if (count < capacity) {
        examples[count++] = ex;
    }
}

void EpisodeBuffer::clear() {
    count = 0;
    winner = 0;
    turns = 0;
}

// ============================================================================
// DataWriter
// ============================================================================
static const char DATA_MAGIC[4] = {'C', 'N', 'Q', 'T'};
static const uint32_t DATA_VERSION = 1;

DataWriter::DataWriter() : file_(nullptr), example_count_(0), header_pos_(0) {}

DataWriter::~DataWriter() {
    close();
}

bool DataWriter::open(const char* path) {
    file_ = std::fopen(path, "wb");
    if (!file_) return false;

    example_count_ = 0;

    // Write placeholder header (will be updated on close)
    char header[64];
    std::memset(header, 0, sizeof(header));
    std::memcpy(header, DATA_MAGIC, 4);
    uint32_t version = DATA_VERSION;
    uint32_t feat_size = FEATURE_SIZE;
    uint32_t pol_size = POLICY_SIZE;
    std::memcpy(header + 4, &version, 4);
    std::memcpy(header + 8, &feat_size, 4);
    std::memcpy(header + 12, &pol_size, 4);
    // example_count at offset 16 — filled on close
    // winner, turns, seed at offsets 20, 24, 28 — filled on close for single-episode files

    std::fwrite(header, 1, 64, file_);
    header_pos_ = 0;  // we'll seek back to update count

    return true;
}

bool DataWriter::write_episode(const EpisodeBuffer& episode) {
    if (!file_) return false;

    for (int i = 0; i < episode.count; i++) {
        const TrainingExample& ex = episode.examples[i];

        // features
        std::fwrite(ex.features, sizeof(float), FEATURE_SIZE, file_);
        // policy
        std::fwrite(ex.policy, sizeof(float), POLICY_SIZE, file_);
        // value
        std::fwrite(&ex.value, sizeof(float), 1, file_);
        // current_player
        std::fwrite(&ex.current_player, sizeof(int), 1, file_);
        // hash
        std::fwrite(&ex.hash, sizeof(uint64_t), 1, file_);
        // turn
        std::fwrite(&ex.turn, sizeof(int), 1, file_);

        example_count_++;
    }

    return true;
}

void DataWriter::close() {
    if (!file_) return;

    // Update the example count in the header
    std::fseek(file_, 16, SEEK_SET);
    std::fwrite(&example_count_, sizeof(uint32_t), 1, file_);

    std::fclose(file_);
    file_ = nullptr;
}

int DataWriter::write_batch(const char* directory, int start_index,
                             EpisodeBuffer* episodes, int count) {
    int written = 0;
    for (int i = 0; i < count; i++) {
        char path[512];
        std::snprintf(path, sizeof(path), "%s/episode_%06d.bin", directory, start_index + i);

        DataWriter writer;
        if (!writer.open(path)) continue;

        // Write header with episode metadata
        // Update winner, turns, seed in header
        std::fseek(writer.file_, 20, SEEK_SET);
        std::fwrite(&episodes[i].winner, sizeof(int), 1, writer.file_);
        std::fwrite(&episodes[i].turns, sizeof(int), 1, writer.file_);
        std::fwrite(&episodes[i].seed, sizeof(uint32_t), 1, writer.file_);
        std::fseek(writer.file_, 0, SEEK_END);

        writer.write_episode(episodes[i]);
        writer.close();
        written++;
    }
    return written;
}
