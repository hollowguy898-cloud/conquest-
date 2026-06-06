#pragma once
// ============================================================================
// CONQUEST - Core Types, Hex Math, Constants, Zobrist Hashing
// Zero randomness. Perfect information. Deterministic. All of it.
// ============================================================================

#include <cstdint>
#include <cstring>
#include <cmath>
#include <array>
#include <vector>
#include <string>
#include <algorithm>
#include <cassert>

// ============================================================================
// CONSTANTS
// ============================================================================
constexpr int HEX_RADIUS      = 6;     // Hex board radius (0..6 = 7 rings)
constexpr int BOARD_SIZE       = 2 * HEX_RADIUS + 1; // 13
constexpr int MAX_TILES        = BOARD_SIZE * BOARD_SIZE; // 169 (some invalid)
constexpr int VALID_TILE_COUNT = 127;  // 3*R^2 + 3*R + 1 for R=6
constexpr int MAX_UNITS        = 64;   // per player
constexpr int VICTORY_NODES    = 9;    // on the map
constexpr int VICTORY_NEEDED   = 7;    // control to win
constexpr int BASE_ENERGY      = 3;    // energy per turn base
constexpr int NODE_ENERGY      = 1;    // energy per controlled node
constexpr int AP_PER_UNIT      = 2;    // action points per unit per turn
constexpr int DRAFT_COUNT      = 10;   // techs drafted per player
constexpr int TOTAL_TECHS      = 20;   // total available tech upgrades
constexpr int MAX_MOVE_COST    = 4;    // max AP for a single move
constexpr int COMMANDER_BUFF_RADIUS = 2;
constexpr int ISOLATED_PENALTY = 2;    // stat reduction when isolated
constexpr int ISOLATED_MOVE_PEN = 1;   // move reduction when isolated
constexpr int FORMATION_BONUS  = 1;    // bonus from formations
constexpr int ELEVATION_RANGE_BONUS = 1;
constexpr int ELEVATION_DEFENSE_BONUS = 1;
constexpr int CANNON_MIN_RANGE = 2;
constexpr int HQ_INFLUENCE     = 3;

// ============================================================================
// ENUMERATIONS
// ============================================================================
enum class Player : uint8_t {
    NONE = 0,
    P1   = 1,
    P2   = 2
};

inline Player other_player(Player p) {
    return (p == Player::P1) ? Player::P2 : Player::P1;
}

enum class UnitType : uint8_t {
    NONE     = 0,
    SCOUT    = 1,
    SOLDIER  = 2,
    FORTRESS = 3,
    CANNON   = 4,
    COMMANDER= 5,
    HQ       = 6  // special: each player has one, immovable
};

constexpr int UNIT_TYPE_COUNT = 7;

enum class Terrain : uint8_t {
    PLAINS      = 0,
    MOUNTAIN    = 1,  // impassable, blocks cannon LOS
    WATER       = 2,  // impassable, doesn't block LOS
    HIGH_GROUND = 3,  // +1 range, +1 defense
    VICTORY     = 4   // victory node
};

enum class ActionType : uint8_t {
    NONE      = 0,
    MOVE      = 1,
    ATTACK    = 2,
    CAPTURE   = 3,  // claim victory node
    FORTIFY   = 4   // +1 defense until next turn
};

enum class TechID : uint8_t {
    NONE              = 0,
    SIEGE             = 1,  // Cannon range +2
    RAPID_LOGISTICS   = 2,  // Scout move +1
    ARMOR             = 3,  // Soldier HP +2
    FORTIFICATION     = 4,  // Fortress defense +1
    COMMAND_AUTHORITY = 5,  // Commander buff radius +1
    ARTILLERY         = 6,  // Cannon attack +1
    MOBILIZATION      = 7,  // Soldier move +1
    SCOUT_ARMOR       = 8,  // Scout HP +1
    HEAVY_FORTRESSES  = 9,  // Fortress HP +2
    WAR_COLLEGE       = 10, // All units attack +1
    EXTENDED_RANGE    = 11, // Cannon range +1
    SCOUT_TACTICS     = 12, // Scout attack +1
    SHIELD_WALL       = 13, // Formation defense bonus +1
    SPEAR_FORMATION   = 14, // Formation attack bonus +1
    LOGISTICS_NETWORK = 15, // Isolated penalty reduced to 1
    ENERGY_GRID       = 16, // +1 energy per controlled node
    RAPID_DEPLOY      = 17, // Units +1 AP on first turn
    FORTIFIED_POS     = 18, // High ground +2 defense instead of +1
    OVERWATCH         = 19, // Fortress gains attack 1
    DEEP_STRIKE       = 20  // Commander influence +1
};

constexpr int TECH_COUNT = 21; // NONE + 20

enum class FormationType : uint8_t {
    NONE          = 0,
    LINE          = 1,  // 3 soldiers in a line: +1 defense
    TRIANGLE      = 2   // 3 soldiers in triangle: +1 attack
};

enum class GamePhase : uint8_t {
    DRAFT_P1      = 0,  // Player 1 drafting techs
    DRAFT_P2      = 1,  // Player 2 drafting techs
    RESEARCH_P1   = 2,  // Player 1 research phase
    RESEARCH_P2   = 3,  // Player 2 research phase
    ACTION_P1     = 4,  // Player 1 action phase
    ACTION_P2     = 5,  // Player 2 action phase
    GAME_OVER     = 6
};

// ============================================================================
// HEX COORDINATE MATH (Axial coordinates, pointy-top)
// ============================================================================
struct Hex {
    int q; // column
    int r; // row

    bool operator==(Hex o) const { return q == o.q && r == o.r; }
    bool operator!=(Hex o) const { return !(*this == o); }

    // Cube coordinate s (derived: s = -q - r)
    int s() const { return -q - r; }

    // Valid hex on our board?
    bool valid() const {
        return q >= -HEX_RADIUS && q <= HEX_RADIUS &&
               r >= -HEX_RADIUS && r <= HEX_RADIUS &&
               s() >= -HEX_RADIUS && s() <= HEX_RADIUS;
    }

    // Distance to another hex
    int distance(Hex o) const {
        return (std::abs(q - o.q) + std::abs(r - o.r) + std::abs(s() - o.s())) / 2;
    }

    // Convert to linear array index
    int index() const {
        return (q + HEX_RADIUS) * BOARD_SIZE + (r + HEX_RADIUS);
    }

    // Hex from index
    static Hex from_index(int idx) {
        return { idx / BOARD_SIZE - HEX_RADIUS, idx % BOARD_SIZE - HEX_RADIUS };
    }
};

// 6 hex neighbor directions (axial, pointy-top)
constexpr Hex HEX_DIRS[6] = {
    {1, 0},   // E
    {1, -1},  // NE
    {0, -1},  // NW
    {-1, 0},  // W
    {-1, 1},  // SW
    {0, 1}    // SE
};

inline Hex hex_add(Hex a, Hex b) { return {a.q + b.q, a.r + b.r}; }
inline Hex hex_sub(Hex a, Hex b) { return {a.q - b.q, a.r - b.r}; }

// Multiply hex direction by scalar
inline Hex hex_multiply(Hex h, int k) { return {h.q * k, h.r * k}; }

inline Hex hex_neighbor(Hex h, int dir) {
    return hex_add(h, HEX_DIRS[dir]);
}

// All 6 neighbors of a hex
inline void hex_neighbors(Hex h, Hex out[6]) {
    for (int i = 0; i < 6; i++) out[i] = hex_add(h, HEX_DIRS[i]);
}

// Round fractional hex to nearest integer hex
inline Hex hex_round(float qf, float rf) {
    float sf = -qf - rf;
    int q = (int)std::round(qf);
    int r = (int)std::round(rf);
    int s = (int)std::round(sf);
    float dq = std::abs(q - qf);
    float dr = std::abs(r - rf);
    float ds = std::abs(s - sf);
    if (dq > dr && dq > ds) q = -r - s;
    else if (dr > ds) r = -q - s;
    return {q, r};
}

// Hex ring at distance k from center
inline std::vector<Hex> hex_ring(Hex center, int k) {
    std::vector<Hex> result;
    if (k == 0) { result.push_back(center); return result; }
    Hex cur = hex_add(center, hex_multiply(HEX_DIRS[4], k)); // start SW
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < k; j++) {
            if (cur.valid()) result.push_back(cur);
            cur = hex_neighbor(cur, i);
        }
    }
    return result;
}

// Hex spiral from center outward
inline std::vector<Hex> hex_spiral(Hex center, int max_radius) {
    std::vector<Hex> result;
    for (int k = 0; k <= max_radius; k++) {
        auto ring = hex_ring(center, k);
        result.insert(result.end(), ring.begin(), ring.end());
    }
    return result;
}

// Hex to pixel (pointy-top)
inline void hex_to_pixel(Hex h, float size, float& px, float& py) {
    px = size * (1.7320508075688772f * h.q + 0.8660254037844386f * h.r);
    py = size * 1.5f * h.r;
}

// Pixel to hex (pointy-top, inverse)
inline Hex pixel_to_hex(float px, float py, float size) {
    float q = (0.5773502691896258f * px - 0.3333333333333333f * py) / size;
    float r = (0.6666666666666666f * py) / size;
    return hex_round(q, r);
}

// Line drawing between two hexes (for cannon LOS)
inline std::vector<Hex> hex_line(Hex a, Hex b) {
    int N = a.distance(b);
    if (N == 0) return {a};
    std::vector<Hex> result;
    float aq = (float)a.q, ar = (float)a.r, as_ = (float)a.s();
    float bq = (float)b.q, br = (float)b.r, bs_ = (float)b.s();
    for (int i = 0; i <= N; i++) {
        float t = (float)i / (float)N;
        float q = aq + (bq - aq) * t;
        float r = ar + (br - ar) * t;
        // s is implicit: s = -q - r
        (void)as_; (void)bs_;
        result.push_back(hex_round(q, r));
    }
    return result;
}

// ============================================================================
// UNIT BASE STATS (before any tech modifications)
// ============================================================================
struct UnitStats {
    int cost;       // energy cost to produce
    int move;       // movement range (hexes per AP)
    int attack;     // attack power
    int defense;    // defense power
    int hp;         // hit points
    int max_hp;     // max hit points
    int influence;  // influence radius
    int range;      // attack range (1 = melee, 0 = can't attack)
};

constexpr UnitStats UNIT_BASE_STATS[UNIT_TYPE_COUNT] = {
    {0, 0, 0, 0, 0, 0, 0, 0},         // NONE
    {2, 4, 1, 0, 2, 2, 1, 1},          // SCOUT
    {3, 2, 2, 2, 4, 4, 1, 1},          // SOLDIER
    {4, 0, 0, 4, 8, 8, 3, 0},          // FORTRESS
    {5, 1, 4, 1, 3, 3, 0, 3},          // CANNON
    {6, 2, 2, 2, 5, 5, 2, 1},          // COMMANDER
    {0, 0, 0, 0, 20, 20, 3, 0}         // HQ
};

inline UnitStats get_base_stats(UnitType t) {
    return UNIT_BASE_STATS[static_cast<int>(t)];
}

// ============================================================================
// TILE DATA (Structure of Arrays style)
// ============================================================================
struct Tile {
    Player  owner    = Player::NONE;
    Terrain  terrain  = Terrain::PLAINS;
    int8_t  p1_influence = 0;
    int8_t  p2_influence = 0;
    int8_t  elevation = 0;  // 0=normal, 1=high ground
};

// ============================================================================
// UNIT DATA
// ============================================================================
struct Unit {
    UnitType type     = UnitType::NONE;
    Player   owner    = Player::NONE;
    Hex      pos      = {0, 0};
    int      hp       = 0;
    int      max_hp   = 0;
    int      ap       = 0;       // remaining action points this turn
    bool     fortified= false;   // +1 defense from fortify action
    int      id       = -1;      // unique unit ID
    bool     first_turn = true;  // for RAPID_DEPLOY tech
};

// ============================================================================
// MOVE / ACTION
// ============================================================================
struct Move {
    ActionType action = ActionType::NONE;
    int        unit_id = -1;
    Hex        from    = {0, 0};
    Hex        to      = {0, 0};     // target hex (move dest / attack target)
    int        cost_ap = 0;          // AP cost of this action
};

// ============================================================================
// TECH INSTANCE
// ============================================================================
struct TechUpgrade {
    TechID id      = TechID::NONE;
    bool   active  = false;  // drafted?
    bool   researched = false; // paid energy to unlock?
    int    research_cost = 3; // energy cost to research
};

// ============================================================================
// GAME STATE
// ============================================================================
struct GameState {
    // Board
    Tile tiles[BOARD_SIZE * BOARD_SIZE];
    bool tile_valid[BOARD_SIZE * BOARD_SIZE]; // which indices are valid hexes

    // Units
    Unit units[MAX_UNITS * 2]; // P1: 0..MAX_UNITS-1, P2: MAX_UNITS..2*MAX_UNITS-1
    int  unit_count_p1 = 0;
    int  unit_count_p2 = 0;
    int  next_unit_id  = 0;

    // Resources
    int  energy[3] = {0, 0, 0}; // indexed by Player (1=P1, 2=P2)

    // Tech
    TechUpgrade techs_p1[TECH_COUNT];
    TechUpgrade techs_p2[TECH_COUNT];

    // Game flow
    GamePhase phase       = GamePhase::DRAFT_P1;
    Player    current     = Player::P1;
    int       turn        = 0;
    Player    winner      = Player::NONE;
    int       nodes_controlled[3] = {0, 0, 0}; // victory nodes per player

    // Map
    uint32_t  map_seed    = 0;
    Hex       hq_p1       = {0, 0};
    Hex       hq_p2       = {0, 0};
    Hex       victory_hexes[VICTORY_NODES];

    // Zobrist hash
    uint64_t  hash        = 0;

    // Tile ownership count for scoring
    int       tiles_controlled[3] = {0, 0, 0};
};

// ============================================================================
// ZOBRIST HASHING
// ============================================================================
struct ZobristTable {
    uint64_t piece_pos[UNIT_TYPE_COUNT][BOARD_SIZE * BOARD_SIZE]; // piece type at position
    uint64_t player_turn;    // whose turn it is
    uint64_t tile_owner[3][BOARD_SIZE * BOARD_SIZE]; // tile ownership
    uint64_t tech_p1[TECH_COUNT];
    uint64_t tech_p2[TECH_COUNT];

    void init(uint32_t seed) {
        // Simple PRNG for initialization (xoshiro256**)
        uint64_t s[4] = { seed * 0x1234567890ABCDEFull, seed * 0xFEDCBA0987654321ull,
                          seed * 0x13579BDF02468ACEull, seed * 0xECA8642013579BDFull };
        auto next = [&]() {
            uint64_t result = (s[1] * 5) << 7 | ((s[1] * 5) >> 57);
            result ^= result >> 7;
            uint64_t t = s[1] << 17;
            s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
            s[2] ^= t;
            s[3] = (s[3] << 45) | (s[3] >> 19);
            return result;
        };
        for (int t = 0; t < UNIT_TYPE_COUNT; t++)
            for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++)
                piece_pos[t][i] = next();
        player_turn = next();
        for (int p = 0; p < 3; p++)
            for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++)
                tile_owner[p][i] = next();
        for (int i = 0; i < TECH_COUNT; i++) {
            tech_p1[i] = next();
            tech_p2[i] = next();
        }
    }
};

// ============================================================================
// TECH MODIFICATION HELPERS
// ============================================================================
struct ModifiedStats {
    int attack;
    int defense;
    int hp;
    int max_hp;
    int move;
    int influence;
    int range;
};

inline ModifiedStats get_modified_stats(UnitType type, Player player, const GameState& state) {
    UnitStats base = get_base_stats(type);
    ModifiedStats m = { base.attack, base.defense, base.hp, base.max_hp,
                        base.move, base.influence, base.range };

    const TechUpgrade* techs = (player == Player::P1) ? state.techs_p1 : state.techs_p2;

    auto has = [&](TechID id) -> bool {
        int idx = static_cast<int>(id);
        return techs[idx].active && techs[idx].researched;
    };

    // Global
    if (has(TechID::WAR_COLLEGE)) m.attack += 1;

    switch (type) {
    case UnitType::SCOUT:
        if (has(TechID::RAPID_LOGISTICS)) m.move += 1;
        if (has(TechID::SCOUT_ARMOR))     m.hp += 1, m.max_hp += 1;
        if (has(TechID::SCOUT_TACTICS))   m.attack += 1;
        break;
    case UnitType::SOLDIER:
        if (has(TechID::ARMOR))        m.hp += 2, m.max_hp += 2;
        if (has(TechID::MOBILIZATION)) m.move += 1;
        break;
    case UnitType::FORTRESS:
        if (has(TechID::FORTIFICATION))    m.defense += 1;
        if (has(TechID::HEAVY_FORTRESSES)) m.hp += 2, m.max_hp += 2;
        if (has(TechID::OVERWATCH))        m.attack = 1, m.range = 1;
        break;
    case UnitType::CANNON:
        if (has(TechID::SIEGE))          m.range += 2;
        if (has(TechID::ARTILLERY))      m.attack += 1;
        if (has(TechID::EXTENDED_RANGE)) m.range += 1;
        break;
    case UnitType::COMMANDER:
        if (has(TechID::DEEP_STRIKE))    m.influence += 1;
        break;
    default: break;
    }

    return m;
}

inline int get_commander_buff_radius(Player player, const GameState& state) {
    int r = COMMANDER_BUFF_RADIUS;
    const TechUpgrade* techs = (player == Player::P1) ? state.techs_p1 : state.techs_p2;
    if (techs[static_cast<int>(TechID::COMMAND_AUTHORITY)].active &&
        techs[static_cast<int>(TechID::COMMAND_AUTHORITY)].researched)
        r += 1;
    return r;
}

inline int get_energy_per_node(Player player, const GameState& state) {
    int e = NODE_ENERGY;
    const TechUpgrade* techs = (player == Player::P1) ? state.techs_p1 : state.techs_p2;
    if (techs[static_cast<int>(TechID::ENERGY_GRID)].active &&
        techs[static_cast<int>(TechID::ENERGY_GRID)].researched)
        e += 1;
    return e;
}

inline int get_isolated_penalty(Player player, const GameState& state) {
    const TechUpgrade* techs = (player == Player::P1) ? state.techs_p1 : state.techs_p2;
    if (techs[static_cast<int>(TechID::LOGISTICS_NETWORK)].active &&
        techs[static_cast<int>(TechID::LOGISTICS_NETWORK)].researched)
        return 1;
    return ISOLATED_PENALTY;
}

inline int get_formation_defense_bonus(Player player, const GameState& state) {
    int b = FORMATION_BONUS;
    const TechUpgrade* techs = (player == Player::P1) ? state.techs_p1 : state.techs_p2;
    if (techs[static_cast<int>(TechID::SHIELD_WALL)].active &&
        techs[static_cast<int>(TechID::SHIELD_WALL)].researched)
        b += 1;
    return b;
}

inline int get_formation_attack_bonus(Player player, const GameState& state) {
    int b = FORMATION_BONUS;
    const TechUpgrade* techs = (player == Player::P1) ? state.techs_p1 : state.techs_p2;
    if (techs[static_cast<int>(TechID::SPEAR_FORMATION)].active &&
        techs[static_cast<int>(TechID::SPEAR_FORMATION)].researched)
        b += 1;
    return b;
}

inline int get_high_ground_defense_bonus(Player player, const GameState& state) {
    int b = ELEVATION_DEFENSE_BONUS;
    const TechUpgrade* techs = (player == Player::P1) ? state.techs_p1 : state.techs_p2;
    if (techs[static_cast<int>(TechID::FORTIFIED_POS)].active &&
        techs[static_cast<int>(TechID::FORTIFIED_POS)].researched)
        b += 1;
    return b;
}

// ============================================================================
// UTILITY
// ============================================================================
inline const char* unit_type_name(UnitType t) {
    switch (t) {
    case UnitType::SCOUT:     return "Scout";
    case UnitType::SOLDIER:   return "Soldier";
    case UnitType::FORTRESS:  return "Fortress";
    case UnitType::CANNON:    return "Cannon";
    case UnitType::COMMANDER: return "Commander";
    case UnitType::HQ:        return "HQ";
    default:                  return "None";
    }
}

inline const char* terrain_name(Terrain t) {
    switch (t) {
    case Terrain::PLAINS:      return "Plains";
    case Terrain::MOUNTAIN:    return "Mountain";
    case Terrain::WATER:       return "Water";
    case Terrain::HIGH_GROUND: return "High Ground";
    case Terrain::VICTORY:     return "Victory Node";
    default:                   return "Unknown";
    }
}

inline const char* tech_name(TechID t) {
    switch (t) {
    case TechID::SIEGE:             return "Siege";
    case TechID::RAPID_LOGISTICS:   return "Rapid Logistics";
    case TechID::ARMOR:             return "Armor";
    case TechID::FORTIFICATION:     return "Fortification";
    case TechID::COMMAND_AUTHORITY: return "Command Authority";
    case TechID::ARTILLERY:         return "Artillery";
    case TechID::MOBILIZATION:      return "Mobilization";
    case TechID::SCOUT_ARMOR:       return "Scout Armor";
    case TechID::HEAVY_FORTRESSES:  return "Heavy Fortresses";
    case TechID::WAR_COLLEGE:       return "War College";
    case TechID::EXTENDED_RANGE:    return "Extended Range";
    case TechID::SCOUT_TACTICS:     return "Scout Tactics";
    case TechID::SHIELD_WALL:       return "Shield Wall";
    case TechID::SPEAR_FORMATION:   return "Spear Formation";
    case TechID::LOGISTICS_NETWORK: return "Logistics Network";
    case TechID::ENERGY_GRID:       return "Energy Grid";
    case TechID::RAPID_DEPLOY:      return "Rapid Deploy";
    case TechID::FORTIFIED_POS:     return "Fortified Positions";
    case TechID::OVERWATCH:         return "Overwatch";
    case TechID::DEEP_STRIKE:       return "Deep Strike";
    default:                        return "None";
    }
}

inline const char* action_name(ActionType a) {
    switch (a) {
    case ActionType::MOVE:    return "Move";
    case ActionType::ATTACK:  return "Attack";
    case ActionType::CAPTURE: return "Capture";
    case ActionType::FORTIFY: return "Fortify";
    default:                  return "None";
    }
}
