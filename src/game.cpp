// ============================================================================
// CONQUEST - Game Logic Implementation
// All game rules: influence, combat, formations, moves, turns, tech, victory.
// ============================================================================

#include "game.h"
#include <queue>

// ============================================================================
// Static helpers (internal linkage)
// ============================================================================

// Find unit array index by its unique id. Returns -1 if not found.
static int find_unit_by_id(const GameState& state, int unit_id) {
    for (int i = 0; i < MAX_UNITS * 2; i++) {
        if (state.units[i].type != UnitType::NONE && state.units[i].id == unit_id)
            return i;
    }
    return -1;
}

// Starting array index for a player's units (P1: 0, P2: MAX_UNITS)
static int player_unit_start(Player p) {
    return (p == Player::P1) ? 0 : MAX_UNITS;
}

// One-past-end array index for a player's units
static int player_unit_end(Player p) {
    return (p == Player::P1) ? MAX_UNITS : MAX_UNITS * 2;
}

// Check if a hex has high-ground elevation
static bool is_high_ground(const GameState& state, Hex h) {
    if (!h.valid()) return false;
    return state.tiles[h.index()].elevation > 0;
}

// Check if a unit is within commander buff radius of an allied commander
static bool has_commander_buff(const GameState& state, int unit_idx) {
    const Unit& unit = state.units[unit_idx];
    Player player = unit.owner;
    int buff_radius = get_commander_buff_radius(player, state);

    int start = player_unit_start(player);
    int end   = player_unit_end(player);

    for (int i = start; i < end; i++) {
        if (state.units[i].type == UnitType::COMMANDER) {
            int dist = unit.pos.distance(state.units[i].pos);
            if (dist > 0 && dist <= buff_radius) {
                return true;
            }
        }
    }
    return false;
}

// Apply or remove a single unit's influence on the board (without updating ownership).
// delta = +1 to add influence, -1 to subtract.
static void apply_unit_influence(GameState& state, int unit_idx, int delta) {
    const Unit& unit = state.units[unit_idx];
    if (unit.type == UnitType::NONE) return;

    int inf_radius;
    if (unit.type == UnitType::HQ) {
        inf_radius = HQ_INFLUENCE;
    } else {
        ModifiedStats ms = get_modified_stats(unit.type, unit.owner, state);
        inf_radius = ms.influence;
    }

    for (int dist = 0; dist <= inf_radius; dist++) {
        std::vector<Hex> ring = hex_ring(unit.pos, dist);
        for (Hex& h : ring) {
            if (!h.valid()) continue;
            int idx = h.index();
            if (!state.tile_valid[idx]) continue;
            if (unit.owner == Player::P1) {
                state.tiles[idx].p1_influence =
                    static_cast<int8_t>(state.tiles[idx].p1_influence + delta);
            } else {
                state.tiles[idx].p2_influence =
                    static_cast<int8_t>(state.tiles[idx].p2_influence + delta);
            }
        }
    }
}

// Decode unit type from a SPAWN move's unit_id field.
// Encoding: unit_id = -(static_cast<int>(UnitType) + 1)
static UnitType decode_spawn_type(int unit_id) {
    return static_cast<UnitType>(-unit_id - 1);
}

// Encode unit type into a SPAWN move's unit_id field.
static int encode_spawn_type(UnitType type) {
    return -(static_cast<int>(type) + 1);
}

// ============================================================================
// Public API implementation
// ============================================================================

void game_init(GameState& state, uint32_t map_seed) {
    state = GameState{};  // Value-initialize all fields to zero/defaults

    state.map_seed = map_seed;

    board_init(state);
    map_generate(state, map_seed);
    map_place_hqs(state);

    // Starting energy
    state.energy[static_cast<int>(Player::P1)] = 10;
    state.energy[static_cast<int>(Player::P2)] = 10;

    // Initialize tech arrays
    for (int i = 0; i < TECH_COUNT; i++) {
        state.techs_p1[i].id = static_cast<TechID>(i);
        state.techs_p1[i].active = false;
        state.techs_p1[i].researched = false;
        state.techs_p1[i].research_cost = 3;

        state.techs_p2[i].id = static_cast<TechID>(i);
        state.techs_p2[i].active = false;
        state.techs_p2[i].researched = false;
        state.techs_p2[i].research_cost = 3;
    }

    state.phase   = GamePhase::DRAFT_P1;
    state.current = Player::P1;
    state.turn    = 0;
    state.winner  = Player::NONE;

    // Compute initial Zobrist hash
    ZobristTable zt;
    zt.init(map_seed);
    game_compute_hash(state, zt);

    // Compute initial influence and ownership
    game_compute_influence(state);
}

// ---------------------------------------------------------------------------
// Influence
// ---------------------------------------------------------------------------

void game_compute_influence(GameState& state) {
    // Clear all influence
    for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        state.tiles[i].p1_influence = 0;
        state.tiles[i].p2_influence = 0;
    }

    // Add each unit's influence
    for (int i = 0; i < MAX_UNITS * 2; i++) {
        if (state.units[i].type != UnitType::NONE) {
            apply_unit_influence(state, i, +1);
        }
    }

    game_update_ownership(state);
}

void game_incremental_influence_remove(GameState& state, int unit_idx) {
    apply_unit_influence(state, unit_idx, -1);
    game_update_ownership(state);
}

void game_incremental_influence_add(GameState& state, int unit_idx) {
    apply_unit_influence(state, unit_idx, +1);
    game_update_ownership(state);
}

// ---------------------------------------------------------------------------
// Ownership
// ---------------------------------------------------------------------------

void game_update_ownership(GameState& state) {
    // Reset counters
    state.tiles_controlled[0] = 0;
    state.tiles_controlled[1] = 0;
    state.tiles_controlled[2] = 0;
    state.nodes_controlled[0] = 0;
    state.nodes_controlled[1] = 0;
    state.nodes_controlled[2] = 0;

    for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        if (!state.tile_valid[i]) continue;

        Tile& tile = state.tiles[i];
        if (tile.p1_influence > tile.p2_influence) {
            tile.owner = Player::P1;
            state.tiles_controlled[1]++;
        } else if (tile.p2_influence > tile.p1_influence) {
            tile.owner = Player::P2;
            state.tiles_controlled[2]++;
        } else {
            tile.owner = Player::NONE;
            state.tiles_controlled[0]++;
        }

        // Track victory node control
        if (tile.terrain == Terrain::VICTORY) {
            int owner_idx = static_cast<int>(tile.owner);
            state.nodes_controlled[owner_idx]++;
        }
    }
}

// ---------------------------------------------------------------------------
// Victory
// ---------------------------------------------------------------------------

int game_check_victory(const GameState& state) {
    // Victory node control check
    if (state.nodes_controlled[1] >= VICTORY_NEEDED) return 1;
    if (state.nodes_controlled[2] >= VICTORY_NEEDED) return 2;

    // HQ destruction check
    bool p1_hq_alive = false;
    bool p2_hq_alive = false;
    for (int i = 0; i < MAX_UNITS; i++) {
        if (state.units[i].type == UnitType::HQ) p1_hq_alive = true;
    }
    for (int i = MAX_UNITS; i < MAX_UNITS * 2; i++) {
        if (state.units[i].type == UnitType::HQ) p2_hq_alive = true;
    }

    if (!p1_hq_alive) return 2;  // P2 wins
    if (!p2_hq_alive) return 1;  // P1 wins

    return 0;  // No winner yet
}

// ---------------------------------------------------------------------------
// Formation
// ---------------------------------------------------------------------------

FormationType game_check_formation(const GameState& state, int unit_idx) {
    const Unit& unit = state.units[unit_idx];
    if (unit.type != UnitType::SOLDIER) return FormationType::NONE;

    // Determine which neighbor directions have allied soldiers
    bool ally_in_dir[6] = {};
    for (int d = 0; d < 6; d++) {
        Hex nb = hex_neighbor(unit.pos, d);
        int nb_idx = board_unit_at(state, nb);
        if (nb_idx >= 0 &&
            state.units[nb_idx].type == UnitType::SOLDIER &&
            state.units[nb_idx].owner == unit.owner) {
            ally_in_dir[d] = true;
        }
    }

    // TRIANGLE: two allied soldiers in adjacent directions (they are mutual
    // neighbors of each other, forming a triangle with the central unit).
    for (int d = 0; d < 6; d++) {
        if (ally_in_dir[d] && ally_in_dir[(d + 1) % 6]) {
            return FormationType::TRIANGLE;
        }
    }

    // LINE: two allied soldiers in opposite directions (3 in a straight line
    // through the central unit, sharing a common direction).
    for (int d = 0; d < 3; d++) {
        if (ally_in_dir[d] && ally_in_dir[d + 3]) {
            return FormationType::LINE;
        }
    }

    return FormationType::NONE;
}

// ---------------------------------------------------------------------------
// Isolation (BFS through allied units to HQ)
// ---------------------------------------------------------------------------

bool game_is_isolated(const GameState& state, int unit_idx) {
    const Unit& unit = state.units[unit_idx];

    // HQ is never isolated
    if (unit.type == UnitType::HQ) return false;

    // BFS from unit position through adjacent allied units only
    bool visited[BOARD_SIZE * BOARD_SIZE] = {};
    std::queue<Hex> q;

    q.push(unit.pos);
    visited[unit.pos.index()] = true;

    while (!q.empty()) {
        Hex cur = q.front();
        q.pop();

        for (int d = 0; d < 6; d++) {
            Hex nb = hex_neighbor(cur, d);
            if (!nb.valid() || !state.tile_valid[nb.index()]) continue;
            if (visited[nb.index()]) continue;

            int nb_idx = board_unit_at(state, nb);
            if (nb_idx < 0) continue;              // No unit there
            if (state.units[nb_idx].owner != unit.owner) continue;  // Enemy

            // Found our HQ — not isolated
            if (state.units[nb_idx].type == UnitType::HQ) {
                return false;
            }

            visited[nb.index()] = true;
            q.push(nb);
        }
    }

    // BFS exhausted without reaching HQ
    return true;
}

// ---------------------------------------------------------------------------
// Effective stats
// ---------------------------------------------------------------------------

int game_get_effective_attack(const GameState& state, int unit_idx) {
    const Unit& unit = state.units[unit_idx];
    ModifiedStats ms = get_modified_stats(unit.type, unit.owner, state);
    int attack = ms.attack;

    // Formation bonus — TRIANGLE gives attack bonus
    FormationType form = game_check_formation(state, unit_idx);
    if (form == FormationType::TRIANGLE) {
        attack += get_formation_attack_bonus(unit.owner, state);
    }

    // Commander buff: +1 if within buff radius of an allied commander
    if (has_commander_buff(state, unit_idx)) {
        attack += 1;
    }

    // Isolation penalty
    if (game_is_isolated(state, unit_idx)) {
        attack -= get_isolated_penalty(unit.owner, state);
    }

    if (attack < 0) attack = 0;
    return attack;
}

int game_get_effective_defense(const GameState& state, int unit_idx) {
    const Unit& unit = state.units[unit_idx];
    ModifiedStats ms = get_modified_stats(unit.type, unit.owner, state);
    int defense = ms.defense;

    // Formation bonus — LINE gives defense bonus
    FormationType form = game_check_formation(state, unit_idx);
    if (form == FormationType::LINE) {
        defense += get_formation_defense_bonus(unit.owner, state);
    }

    // Commander buff: +1 if within buff radius of an allied commander
    if (has_commander_buff(state, unit_idx)) {
        defense += 1;
    }

    // Isolation penalty
    if (game_is_isolated(state, unit_idx)) {
        defense -= get_isolated_penalty(unit.owner, state);
    }

    // Fortify bonus
    if (unit.fortified) {
        defense += 1;
    }

    // Elevation defense bonus (high ground)
    if (is_high_ground(state, unit.pos)) {
        defense += get_high_ground_defense_bonus(unit.owner, state);
    }

    if (defense < 0) defense = 0;
    return defense;
}

int game_get_effective_range(const GameState& state, int unit_idx) {
    const Unit& unit = state.units[unit_idx];
    ModifiedStats ms = get_modified_stats(unit.type, unit.owner, state);
    int range = ms.range;

    // Elevation range bonus (high ground)
    if (is_high_ground(state, unit.pos)) {
        range += ELEVATION_RANGE_BONUS;
    }

    return range;
}

int game_get_effective_move(const GameState& state, int unit_idx) {
    const Unit& unit = state.units[unit_idx];
    ModifiedStats ms = get_modified_stats(unit.type, unit.owner, state);
    int move = ms.move;

    // Isolation move penalty
    if (game_is_isolated(state, unit_idx)) {
        move -= ISOLATED_MOVE_PEN;
    }

    if (move < 0) move = 0;
    return move;
}

// ---------------------------------------------------------------------------
// Move application
// ---------------------------------------------------------------------------

bool game_apply_move(GameState& state, const Move& move) {
    if (!game_validate_move(state, move)) return false;

    switch (move.action) {

    case ActionType::MOVE: {
        int unit_idx = find_unit_by_id(state, move.unit_id);
        // Validation already confirmed unit_idx >= 0, but be safe
        if (unit_idx < 0) return false;
        Unit& unit = state.units[unit_idx];

        int dist = unit.pos.distance(move.to);

        // Remove influence at old position, move, add influence at new position
        game_incremental_influence_remove(state, unit_idx);
        unit.pos = move.to;
        game_incremental_influence_add(state, unit_idx);

        // Consume AP equal to movement distance
        unit.ap -= dist;
        return true;
    }

    case ActionType::ATTACK: {
        int unit_idx = find_unit_by_id(state, move.unit_id);
        if (unit_idx < 0) return false;
        Unit& attacker = state.units[unit_idx];

        int target_idx = board_unit_at(state, move.to);
        if (target_idx < 0) return false;

        int attack_val  = game_get_effective_attack(state, unit_idx);
        int defense_val = game_get_effective_defense(state, target_idx);

        // Unit must have positive attack to deal damage
        if (attack_val <= 0) return false;

        int damage = attack_val - defense_val;
        if (damage < 1) damage = 1;  // Minimum 1 damage when attack > 0

        game_damage_unit(state, target_idx, damage);

        // Consume 1 AP
        attacker.ap -= 1;
        return true;
    }

    case ActionType::CAPTURE: {
        int unit_idx = find_unit_by_id(state, move.unit_id);
        if (unit_idx < 0) return false;
        Unit& unit = state.units[unit_idx];

        // Capturing is formalized via influence — the unit's presence on the
        // victory node already exerts influence, which determines ownership.
        // The CAPTURE action spends 1 AP to formally claim the node.
        unit.ap -= 1;
        return true;
    }

    case ActionType::FORTIFY: {
        int unit_idx = find_unit_by_id(state, move.unit_id);
        if (unit_idx < 0) return false;
        Unit& unit = state.units[unit_idx];

        unit.fortified = true;
        unit.ap -= 1;
        return true;
    }

    case ACTION_SPAWN: {
        UnitType type = decode_spawn_type(move.unit_id);
        int result = game_spawn_unit(state, state.current, type, move.to);
        return result >= 0;
    }

    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// Turn management
// ---------------------------------------------------------------------------

void game_start_turn(GameState& state) {
    Player cp = state.current;
    int cp_idx = static_cast<int>(cp);

    // Energy gain: BASE_ENERGY + (nodes_controlled * energy_per_node)
    int epn = get_energy_per_node(cp, state);
    state.energy[cp_idx] += BASE_ENERGY + state.nodes_controlled[cp_idx] * epn;

    // Check RAPID_DEPLOY tech
    const TechUpgrade* techs = (cp == Player::P1) ? state.techs_p1 : state.techs_p2;
    bool has_rapid_deploy =
        techs[static_cast<int>(TechID::RAPID_DEPLOY)].active &&
        techs[static_cast<int>(TechID::RAPID_DEPLOY)].researched;

    // Reset AP, apply RAPID_DEPLOY bonus, clear fortified status
    int start = player_unit_start(cp);
    int end   = player_unit_end(cp);
    for (int i = start; i < end; i++) {
        if (state.units[i].type != UnitType::NONE) {
            state.units[i].ap = AP_PER_UNIT;
            if (has_rapid_deploy && state.units[i].first_turn) {
                state.units[i].ap += 1;
                state.units[i].first_turn = false;
            }
            state.units[i].fortified = false;
        }
    }

    // Set phase
    state.phase = (cp == Player::P1) ? GamePhase::ACTION_P1 : GamePhase::ACTION_P2;
}

void game_end_turn(GameState& state) {
    // Switch current player
    state.current = other_player(state.current);

    // If it's now P1's turn, increment the global turn counter
    if (state.current == Player::P1) {
        state.turn++;
    }

    // Start the new player's turn
    game_start_turn(state);
}

// ---------------------------------------------------------------------------
// Move generation
// ---------------------------------------------------------------------------

int game_generate_moves(const GameState& state, Move* moves, int max_moves) {
    int count = 0;
    Player cp = state.current;

    auto add_move = [&](const Move& m) {
        if (count < max_moves) {
            moves[count++] = m;
        }
    };

    // --- Per-unit actions ---
    int start = player_unit_start(cp);
    int end   = player_unit_end(cp);

    for (int i = start; i < end; i++) {
        const Unit& unit = state.units[i];
        if (unit.type == UnitType::NONE) continue;
        if (unit.type == UnitType::HQ) continue;   // HQ cannot take actions
        if (unit.ap <= 0) continue;

        // ---- MOVE ----
        {
            int move_range = game_get_effective_move(state, i);
            if (move_range > 0 && unit.ap > 0) {
                Hex reachable[VALID_TILE_COUNT];
                int reachable_count = 0;
                board_get_reachable(state, i, reachable, &reachable_count, VALID_TILE_COUNT);

                for (int j = 0; j < reachable_count; j++) {
                    int dist = unit.pos.distance(reachable[j]);
                    if (dist > 0 && dist <= unit.ap) {
                        Move m;
                        m.action  = ActionType::MOVE;
                        m.unit_id = unit.id;
                        m.from    = unit.pos;
                        m.to      = reachable[j];
                        m.cost_ap = dist;
                        add_move(m);
                    }
                }
            }
        }

        // ---- ATTACK ----
        {
            int eff_range  = game_get_effective_range(state, i);
            int eff_attack = game_get_effective_attack(state, i);
            if (eff_range > 0 && eff_attack > 0 && unit.ap >= 1) {
                for (int dist = 1; dist <= eff_range; dist++) {
                    // Cannon minimum range
                    if (unit.type == UnitType::CANNON && dist < CANNON_MIN_RANGE) continue;

                    std::vector<Hex> ring = hex_ring(unit.pos, dist);
                    for (Hex& h : ring) {
                        if (!h.valid() || !state.tile_valid[h.index()]) continue;

                        int target_idx = board_unit_at(state, h);
                        if (target_idx < 0) continue;
                        if (state.units[target_idx].owner == cp) continue;
                        if (state.units[target_idx].owner == Player::NONE) continue;

                        // LOS check for cannon
                        if (unit.type == UnitType::CANNON) {
                            if (hex_los_blocked(state, unit.pos, h)) continue;
                        }

                        Move m;
                        m.action  = ActionType::ATTACK;
                        m.unit_id = unit.id;
                        m.from    = unit.pos;
                        m.to      = h;
                        m.cost_ap = 1;
                        add_move(m);
                    }
                }
            }
        }

        // ---- CAPTURE ----
        if (unit.ap >= 1) {
            for (int v = 0; v < VICTORY_NODES; v++) {
                if (unit.pos == state.victory_hexes[v]) {
                    Move m;
                    m.action  = ActionType::CAPTURE;
                    m.unit_id = unit.id;
                    m.from    = unit.pos;
                    m.to      = unit.pos;
                    m.cost_ap = 1;
                    add_move(m);
                    break;  // Only one capture move per unit per turn
                }
            }
        }

        // ---- FORTIFY ----
        if (!unit.fortified && unit.ap >= 1) {
            Move m;
            m.action  = ActionType::FORTIFY;
            m.unit_id = unit.id;
            m.from    = unit.pos;
            m.to      = unit.pos;
            m.cost_ap = 1;
            add_move(m);
        }
    }

    // --- SPAWN moves ---
    {
        Hex hq_pos = (cp == Player::P1) ? state.hq_p1 : state.hq_p2;
        int cp_idx = static_cast<int>(cp);

        // Collect empty, passable hexes adjacent to HQ
        Hex spawn_hexes[6];
        int spawn_count = 0;
        for (int d = 0; d < 6; d++) {
            Hex nb = hex_neighbor(hq_pos, d);
            if (!nb.valid() || !state.tile_valid[nb.index()]) continue;
            if (!hex_passable(state, nb)) continue;
            // hex_passable already checks for no unit, but double-check
            if (board_unit_at(state, nb) >= 0) continue;
            spawn_hexes[spawn_count++] = nb;
        }

        // Check if player has an empty unit slot
        bool has_slot = false;
        for (int si = start; si < end; si++) {
            if (state.units[si].type == UnitType::NONE) {
                has_slot = true;
                break;
            }
        }

        if (has_slot) {
            static const UnitType spawnable_types[] = {
                UnitType::SCOUT, UnitType::SOLDIER, UnitType::FORTRESS,
                UnitType::CANNON, UnitType::COMMANDER
            };
            static const int spawnable_count = 5;

            for (int t = 0; t < spawnable_count; t++) {
                UnitType type = spawnable_types[t];
                int cost = get_base_stats(type).cost;
                if (state.energy[cp_idx] >= cost) {
                    for (int s = 0; s < spawn_count; s++) {
                        Move m;
                        m.action  = ACTION_SPAWN;
                        m.unit_id = encode_spawn_type(type);
                        m.from    = hq_pos;
                        m.to      = spawn_hexes[s];
                        m.cost_ap = cost;
                        add_move(m);
                    }
                }
            }
        }
    }

    return count;
}

// ---------------------------------------------------------------------------
// Move validation
// ---------------------------------------------------------------------------

bool game_validate_move(const GameState& state, const Move& move) {
    Player cp = state.current;

    switch (move.action) {

    case ActionType::MOVE: {
        int unit_idx = find_unit_by_id(state, move.unit_id);
        if (unit_idx < 0) return false;
        const Unit& unit = state.units[unit_idx];
        if (unit.owner != cp) return false;
        if (unit.type == UnitType::HQ) return false;

        int dist = unit.pos.distance(move.to);
        if (dist <= 0) return false;
        if (dist > unit.ap) return false;

        // Verify destination is within effective move range
        int move_range = game_get_effective_move(state, unit_idx);
        if (dist > move_range) return false;

        // Verify destination is actually reachable (respects terrain, units)
        Hex reachable[VALID_TILE_COUNT];
        int reachable_count = 0;
        board_get_reachable(state, unit_idx, reachable, &reachable_count, VALID_TILE_COUNT);

        bool found = false;
        for (int j = 0; j < reachable_count; j++) {
            if (reachable[j] == move.to) {
                found = true;
                break;
            }
        }
        if (!found) return false;

        return true;
    }

    case ActionType::ATTACK: {
        int unit_idx = find_unit_by_id(state, move.unit_id);
        if (unit_idx < 0) return false;
        const Unit& unit = state.units[unit_idx];
        if (unit.owner != cp) return false;
        if (unit.ap < 1) return false;

        int eff_range  = game_get_effective_range(state, unit_idx);
        int eff_attack = game_get_effective_attack(state, unit_idx);
        if (eff_range <= 0 || eff_attack <= 0) return false;

        int dist = unit.pos.distance(move.to);
        if (dist < 1 || dist > eff_range) return false;

        // Cannon minimum range
        if (unit.type == UnitType::CANNON && dist < CANNON_MIN_RANGE) return false;

        // Must target an enemy unit
        int target_idx = board_unit_at(state, move.to);
        if (target_idx < 0) return false;
        if (state.units[target_idx].owner == cp) return false;
        if (state.units[target_idx].owner == Player::NONE) return false;

        // LOS check for cannon
        if (unit.type == UnitType::CANNON) {
            if (hex_los_blocked(state, unit.pos, move.to)) return false;
        }

        return true;
    }

    case ActionType::CAPTURE: {
        int unit_idx = find_unit_by_id(state, move.unit_id);
        if (unit_idx < 0) return false;
        const Unit& unit = state.units[unit_idx];
        if (unit.owner != cp) return false;
        if (unit.ap < 1) return false;

        // Unit must be standing on a victory node
        bool on_victory = false;
        for (int v = 0; v < VICTORY_NODES; v++) {
            if (unit.pos == state.victory_hexes[v]) {
                on_victory = true;
                break;
            }
        }
        if (!on_victory) return false;

        return true;
    }

    case ActionType::FORTIFY: {
        int unit_idx = find_unit_by_id(state, move.unit_id);
        if (unit_idx < 0) return false;
        const Unit& unit = state.units[unit_idx];
        if (unit.owner != cp) return false;
        if (unit.ap < 1) return false;
        if (unit.fortified) return false;

        return true;
    }

    case ACTION_SPAWN: {
        UnitType type = decode_spawn_type(move.unit_id);
        int type_int = static_cast<int>(type);
        // Must be a valid spawnable type (SCOUT..COMMANDER)
        if (type_int < static_cast<int>(UnitType::SCOUT) ||
            type_int > static_cast<int>(UnitType::COMMANDER))
            return false;

        int cost = get_base_stats(type).cost;
        if (state.energy[static_cast<int>(cp)] < cost) return false;

        // Hex must be valid, passable, and unoccupied
        if (!move.to.valid()) return false;
        if (!state.tile_valid[move.to.index()]) return false;
        if (!hex_passable(state, move.to)) return false;
        if (board_unit_at(state, move.to) >= 0) return false;

        // Must be adjacent to player's HQ
        Hex hq_pos = (cp == Player::P1) ? state.hq_p1 : state.hq_p2;
        if (hq_pos.distance(move.to) != 1) return false;

        // Must have an empty unit slot
        int s_start = player_unit_start(cp);
        int s_end   = player_unit_end(cp);
        bool has_slot = false;
        for (int si = s_start; si < s_end; si++) {
            if (state.units[si].type == UnitType::NONE) {
                has_slot = true;
                break;
            }
        }
        if (!has_slot) return false;

        return true;
    }

    default:
        return false;
    }
}

// ---------------------------------------------------------------------------
// Zobrist hashing
// ---------------------------------------------------------------------------

void game_compute_hash(GameState& state, const ZobristTable& zt) {
    uint64_t h = 0;

    // Pieces at positions
    for (int i = 0; i < MAX_UNITS * 2; i++) {
        if (state.units[i].type != UnitType::NONE) {
            int type_idx = static_cast<int>(state.units[i].type);
            int pos_idx  = state.units[i].pos.index();
            h ^= zt.piece_pos[type_idx][pos_idx];
        }
    }

    // Tile ownership
    for (int idx = 0; idx < BOARD_SIZE * BOARD_SIZE; idx++) {
        if (state.tile_valid[idx]) {
            int owner_idx = static_cast<int>(state.tiles[idx].owner);
            h ^= zt.tile_owner[owner_idx][idx];
        }
    }

    // Player turn (XOR in the turn bit when it's P2's turn)
    if (state.current == Player::P2) {
        h ^= zt.player_turn;
    }

    // Researched techs
    for (int i = 0; i < TECH_COUNT; i++) {
        if (state.techs_p1[i].active && state.techs_p1[i].researched) {
            h ^= zt.tech_p1[i];
        }
        if (state.techs_p2[i].active && state.techs_p2[i].researched) {
            h ^= zt.tech_p2[i];
        }
    }

    state.hash = h;
}

// ---------------------------------------------------------------------------
// Unit spawning
// ---------------------------------------------------------------------------

int game_spawn_unit(GameState& state, Player player, UnitType type, Hex pos) {
    // Validate type
    int type_int = static_cast<int>(type);
    if (type_int < static_cast<int>(UnitType::SCOUT) ||
        type_int > static_cast<int>(UnitType::COMMANDER))
        return -1;

    // Check energy
    int cost = get_base_stats(type).cost;
    int player_idx = static_cast<int>(player);
    if (state.energy[player_idx] < cost) return -1;

    // Check hex validity
    if (!pos.valid() || !state.tile_valid[pos.index()]) return -1;
    if (!hex_passable(state, pos)) return -1;
    if (board_unit_at(state, pos) >= 0) return -1;

    // Find empty slot in player's section
    int slot_start = player_unit_start(player);
    int slot_end   = player_unit_end(player);
    int slot = -1;
    for (int i = slot_start; i < slot_end; i++) {
        if (state.units[i].type == UnitType::NONE) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return -1;  // No room

    // Create the unit with modified stats
    ModifiedStats ms = get_modified_stats(type, player, state);
    Unit& unit = state.units[slot];
    unit.type       = type;
    unit.owner      = player;
    unit.pos        = pos;
    unit.hp         = ms.max_hp;
    unit.max_hp     = ms.max_hp;
    unit.ap         = 0;            // AP set at start of owner's turn
    unit.fortified  = false;
    unit.id         = state.next_unit_id++;
    unit.first_turn = true;

    // Deduct energy
    state.energy[player_idx] -= cost;

    // Update unit count
    if (player == Player::P1) state.unit_count_p1++;
    else                      state.unit_count_p2++;

    // Incrementally add influence
    game_incremental_influence_add(state, slot);

    return slot;
}

// ---------------------------------------------------------------------------
// Tech
// ---------------------------------------------------------------------------

void game_draft_tech(GameState& state, Player player, TechID tech) {
    TechUpgrade* techs = (player == Player::P1) ? state.techs_p1 : state.techs_p2;
    int idx = static_cast<int>(tech);
    if (idx < 1 || idx >= TECH_COUNT) return;  // TechID::NONE (0) is invalid

    techs[idx].id            = tech;
    techs[idx].active        = true;
    techs[idx].research_cost = 3;  // Default research cost
}

bool game_research_tech(GameState& state, Player player, TechID tech) {
    TechUpgrade* techs = (player == Player::P1) ? state.techs_p1 : state.techs_p2;
    int idx = static_cast<int>(tech);
    if (idx < 1 || idx >= TECH_COUNT) return false;

    if (!techs[idx].active)     return false;   // Must be drafted first
    if (techs[idx].researched)  return false;   // Already researched

    int cost = techs[idx].research_cost;
    int player_idx = static_cast<int>(player);
    if (state.energy[player_idx] < cost) return false;

    // Pay energy and mark researched
    state.energy[player_idx] -= cost;
    techs[idx].researched = true;
    return true;
}

// ---------------------------------------------------------------------------
// Damage / removal
// ---------------------------------------------------------------------------

void game_damage_unit(GameState& state, int unit_idx, int damage) {
    Unit& unit = state.units[unit_idx];
    unit.hp -= damage;
    if (unit.hp <= 0) {
        game_remove_unit(state, unit_idx);
    }
}

void game_remove_unit(GameState& state, int unit_idx) {
    Unit& unit = state.units[unit_idx];

    // Remove influence before clearing type (need type/pos/owner to compute)
    game_incremental_influence_remove(state, unit_idx);

    // Update unit counts
    if (unit.owner == Player::P1)      state.unit_count_p1--;
    else if (unit.owner == Player::P2) state.unit_count_p2--;

    // Clear the unit slot
    unit.type      = UnitType::NONE;
    unit.owner     = Player::NONE;
    unit.hp        = 0;
    unit.max_hp    = 0;
    unit.ap        = 0;
    unit.fortified = false;
    unit.id        = -1;
    unit.first_turn = false;
}

// ---------------------------------------------------------------------------
// Draft phase transition
// ---------------------------------------------------------------------------

void game_end_draft(GameState& state) {
    // After both players have drafted, transition to first research phase
    state.phase = GamePhase::RESEARCH_P1;
}
