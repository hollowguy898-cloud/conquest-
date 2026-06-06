// ============================================================================
// CONQUEST - Board Initialization, Map Generation, Hex Queries
// Full implementation — no stubs, no placeholders.
// ============================================================================

#include "board.h"
#include <cstring>
#include <algorithm>

// ============================================================================
// Internal helper: check if a hex is connected to the player's HQ via passable
// terrain (ignoring units — this is for supply/isolation checks).
// Uses BFS through non-MOUNTAIN, non-WATER valid hexes.
// ============================================================================
static bool is_connected_to_hq(const GameState& state, Hex pos, Hex hq) {
    if (pos == hq) return true;

    bool visited[MAX_TILES];
    std::memset(visited, 0, sizeof(visited));

    Hex queue[MAX_TILES];
    int qhead = 0, qtail = 0;

    visited[pos.index()] = true;
    queue[qtail++] = pos;

    while (qhead < qtail) {
        Hex cur = queue[qhead++];

        Hex nbrs[6];
        hex_neighbors(cur, nbrs);

        for (int d = 0; d < 6; d++) {
            Hex nb = nbrs[d];
            if (!nb.valid()) continue;
            int nb_idx = nb.index();
            if (!state.tile_valid[nb_idx]) continue;
            if (visited[nb_idx]) continue;

            Terrain t = state.tiles[nb_idx].terrain;
            if (t == Terrain::MOUNTAIN || t == Terrain::WATER) continue;

            if (nb == hq) return true;

            visited[nb_idx] = true;
            queue[qtail++] = nb;
        }
    }

    return false;
}

// ============================================================================
// board_init — mark which linear indices correspond to valid hexes on the grid
// ============================================================================
void board_init(GameState& state) {
    for (int i = 0; i < MAX_TILES; i++) {
        Hex h = Hex::from_index(i);
        state.tile_valid[i] = h.valid();
    }
}

// ============================================================================
// map_generate — deterministically generate a symmetric map from a seed
//
// Strategy:
//   1. All valid tiles start as PLAINS
//   2. xorshift32 PRNG from the seed
//   3. Terrain placed on the "generating half" (q > 0, or q==0 && r >= 0),
//      then each placed hex is mirrored via (q,r) -> (-q,-r) for symmetry
//   4. HQ and victory node positions are forced to appropriate terrain
//   5. Initial influence and tile ownership are computed from HQ positions
// ============================================================================
void map_generate(GameState& state, uint32_t seed) {
    state.map_seed = seed;

    // ---- PRNG (xorshift32) ----
    uint32_t rng = seed;
    if (rng == 0) rng = 1;  // avoid degenerate zero-state
    auto rng_next = [&rng]() -> uint32_t {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return rng;
    };
    auto rng_int = [&](int n) -> int {
        return (int)(rng_next() % (uint32_t)n);
    };

    // ---- 1. Initialize tile_valid and all valid tiles to PLAINS ----
    board_init(state);
    for (int i = 0; i < MAX_TILES; i++) {
        state.tiles[i] = Tile();
    }

    // ---- 2. Set HQ positions ----
    state.hq_p1 = {-5, 2};
    state.hq_p2 = {5, -2};

    // ---- 3. Collect candidate hexes in the generating half ----
    //    Generating half: q > 0, OR (q == 0 AND r >= 0)
    //    Each hex in this half either mirrors to itself (origin only) or to a
    //    hex NOT in this half, so we get exact 2-fold symmetry.
    std::vector<Hex> candidates;
    candidates.reserve(64);
    for (int i = 0; i < MAX_TILES; i++) {
        if (!state.tile_valid[i]) continue;
        Hex h = Hex::from_index(i);
        if (h.q > 0 || (h.q == 0 && h.r >= 0)) {
            // Exclude HQ positions from terrain candidates
            if (h == state.hq_p1 || h == state.hq_p2) continue;
            candidates.push_back(h);
        }
    }

    // Fisher-Yates shuffle using our PRNG
    for (int i = (int)candidates.size() - 1; i > 0; i--) {
        int j = rng_int(i + 1);
        std::swap(candidates[i], candidates[j]);
    }

    // ---- 4. Place terrain on generating half, then mirror ----
    int num_mountains   = 8 + rng_int(3);   // 8..10
    int num_water       = 6 + rng_int(3);   // 6..8
    int num_high_ground = 4 + rng_int(3);   // 4..6

    int ci = 0;  // candidate index

    // Mountains
    for (int i = 0; i < num_mountains && ci < (int)candidates.size(); i++, ci++) {
        Hex h = candidates[ci];
        state.tiles[h.index()].terrain = Terrain::MOUNTAIN;
        Hex mirror = {-h.q, -h.r};
        if (mirror != h && mirror.valid()) {
            state.tiles[mirror.index()].terrain = Terrain::MOUNTAIN;
        }
    }

    // Water
    for (int i = 0; i < num_water && ci < (int)candidates.size(); i++, ci++) {
        Hex h = candidates[ci];
        state.tiles[h.index()].terrain = Terrain::WATER;
        Hex mirror = {-h.q, -h.r};
        if (mirror != h && mirror.valid()) {
            state.tiles[mirror.index()].terrain = Terrain::WATER;
        }
    }

    // High ground
    for (int i = 0; i < num_high_ground && ci < (int)candidates.size(); i++, ci++) {
        Hex h = candidates[ci];
        state.tiles[h.index()].terrain = Terrain::HIGH_GROUND;
        state.tiles[h.index()].elevation = 1;
        Hex mirror = {-h.q, -h.r};
        if (mirror != h && mirror.valid()) {
            state.tiles[mirror.index()].terrain = Terrain::HIGH_GROUND;
            state.tiles[mirror.index()].elevation = 1;
        }
    }

    // ---- 5. Force HQ positions to PLAINS ----
    state.tiles[state.hq_p1.index()].terrain = Terrain::PLAINS;
    state.tiles[state.hq_p1.index()].elevation = 0;
    state.tiles[state.hq_p2.index()].terrain = Terrain::PLAINS;
    state.tiles[state.hq_p2.index()].elevation = 0;

    // ---- 6. Place 9 victory nodes symmetrically ----
    //    5 on center column (self-mirror pairs: (0,r)<->(0,-r), plus origin)
    //    2 mirrored pairs from the outer rings
    //    Spread: center, mid-ring, outer-ring
    state.victory_hexes[0] = {0, 0};      // center
    state.victory_hexes[1] = {0, -3};     // center column, mid
    state.victory_hexes[2] = {0, 3};      // center column, mid (mirror of [1])
    state.victory_hexes[3] = {0, 5};      // center column, outer
    state.victory_hexes[4] = {0, -5};     // center column, outer (mirror of [3])
    state.victory_hexes[5] = {3, -1};     // mid ring
    state.victory_hexes[6] = {-3, 1};     // mid ring (mirror of [5])
    state.victory_hexes[7] = {4, 1};      // outer-mid ring
    state.victory_hexes[8] = {-4, -1};    // outer-mid ring (mirror of [7])

    for (int i = 0; i < VICTORY_NODES; i++) {
        int idx = state.victory_hexes[i].index();
        state.tiles[idx].terrain = Terrain::VICTORY;
        state.tiles[idx].elevation = 0;
    }

    // ---- 7. Calculate initial influence and tile ownership ----
    //    Influence radiates from each HQ with radius HQ_INFLUENCE (3).
    //    Influence at distance d = max(0, HQ_INFLUENCE - d).
    //    Tile owner = player with higher influence, or NONE if tied.
    for (int i = 0; i < MAX_TILES; i++) {
        if (!state.tile_valid[i]) continue;
        Hex h = Hex::from_index(i);

        int d1 = h.distance(state.hq_p1);
        int d2 = h.distance(state.hq_p2);

        state.tiles[i].p1_influence = (d1 <= HQ_INFLUENCE)
            ? (int8_t)(HQ_INFLUENCE - d1) : 0;
        state.tiles[i].p2_influence = (d2 <= HQ_INFLUENCE)
            ? (int8_t)(HQ_INFLUENCE - d2) : 0;

        if (state.tiles[i].p1_influence > state.tiles[i].p2_influence) {
            state.tiles[i].owner = Player::P1;
        } else if (state.tiles[i].p2_influence > state.tiles[i].p1_influence) {
            state.tiles[i].owner = Player::P2;
        } else {
            state.tiles[i].owner = Player::NONE;
        }

        // Ensure high-ground elevation is set (may have been cleared by
        // victory node override; only HIGH_GROUND terrain keeps elevation 1)
        if (state.tiles[i].terrain == Terrain::HIGH_GROUND) {
            state.tiles[i].elevation = 1;
        } else {
            state.tiles[i].elevation = 0;
        }
    }

    // ---- 8. Count tiles controlled per player ----
    state.tiles_controlled[0] = 0;
    state.tiles_controlled[1] = 0;
    state.tiles_controlled[2] = 0;
    for (int i = 0; i < MAX_TILES; i++) {
        if (!state.tile_valid[i]) continue;
        int p = static_cast<int>(state.tiles[i].owner);
        state.tiles_controlled[p]++;
    }

    // ---- 9. Count victory nodes controlled per player ----
    state.nodes_controlled[0] = 0;
    state.nodes_controlled[1] = 0;
    state.nodes_controlled[2] = 0;
    for (int i = 0; i < VICTORY_NODES; i++) {
        int idx = state.victory_hexes[i].index();
        int p = static_cast<int>(state.tiles[idx].owner);
        state.nodes_controlled[p]++;
    }
}

// ============================================================================
// map_place_hqs — create HQ units for both players at their designated hexes
// ============================================================================
void map_place_hqs(GameState& state) {
    // Player 1 HQ — first slot in P1's unit range [0, MAX_UNITS)
    int p1_idx = 0;
    state.units[p1_idx].type       = UnitType::HQ;
    state.units[p1_idx].owner      = Player::P1;
    state.units[p1_idx].pos        = state.hq_p1;
    state.units[p1_idx].hp         = 20;
    state.units[p1_idx].max_hp     = 20;
    state.units[p1_idx].ap         = 0;
    state.units[p1_idx].fortified  = false;
    state.units[p1_idx].id         = state.next_unit_id++;
    state.units[p1_idx].first_turn = true;
    state.unit_count_p1 = 1;

    // Player 2 HQ — first slot in P2's unit range [MAX_UNITS, 2*MAX_UNITS)
    int p2_idx = MAX_UNITS;
    state.units[p2_idx].type       = UnitType::HQ;
    state.units[p2_idx].owner      = Player::P2;
    state.units[p2_idx].pos        = state.hq_p2;
    state.units[p2_idx].hp         = 20;
    state.units[p2_idx].max_hp     = 20;
    state.units[p2_idx].ap         = 0;
    state.units[p2_idx].fortified  = false;
    state.units[p2_idx].id         = state.next_unit_id++;
    state.units[p2_idx].first_turn = true;
    state.unit_count_p2 = 1;
}

// ============================================================================
// board_unit_at — find the index into state.units[] of the unit at hex h
// Returns -1 if no unit occupies that hex.
// ============================================================================
int board_unit_at(const GameState& state, Hex h) {
    // Check P1 units
    for (int i = 0; i < state.unit_count_p1; i++) {
        if (state.units[i].pos == h) return i;
    }
    // Check P2 units
    for (int i = MAX_UNITS; i < MAX_UNITS + state.unit_count_p2; i++) {
        if (state.units[i].pos == h) return i;
    }
    return -1;
}

// ============================================================================
// hex_passable — can a unit move to / through this hex?
//   - Must be a valid hex on the board
//   - Must not be MOUNTAIN or WATER (impassable terrain)
//   - Must not have any unit already there
// ============================================================================
bool hex_passable(const GameState& state, Hex h) {
    if (!h.valid()) return false;
    int idx = h.index();
    if (!state.tile_valid[idx]) return false;

    Terrain t = state.tiles[idx].terrain;
    if (t == Terrain::MOUNTAIN || t == Terrain::WATER) return false;

    if (board_unit_at(state, h) != -1) return false;

    return true;
}

// ============================================================================
// hex_los_blocked — is line of sight blocked from `from` to `to`?
//   - Draws a hex line between the two points
//   - If any INTERMEDIATE hex (excluding endpoints) is MOUNTAIN, LOS is blocked
//   - WATER does NOT block LOS
// ============================================================================
bool hex_los_blocked(const GameState& state, Hex from, Hex to) {
    auto line = hex_line(from, to);

    // Check intermediate hexes only (skip from and to)
    for (int i = 1; i < (int)line.size() - 1; i++) {
        Hex h = line[i];
        if (!h.valid()) continue;
        int idx = h.index();
        if (!state.tile_valid[idx]) continue;

        if (state.tiles[idx].terrain == Terrain::MOUNTAIN) {
            return true;  // LOS blocked by mountain
        }
        // WATER does NOT block LOS — explicitly skipped
    }

    return false;
}

// ============================================================================
// board_get_reachable — BFS to find all hexes a unit can reach within its
// effective move range, respecting terrain and other units.
//
// Movement rules:
//   - Can't enter MOUNTAIN or WATER hexes
//   - Can't move through hexes occupied by enemy units
//   - CAN move through hexes with friendly units, but CAN'T stop there
//   - Can only stop on empty hexes
//   - If isolated (no passable path to friendly HQ), move range reduced by
//     ISOLATED_MOVE_PEN (minimum 0)
//   - HQ units cannot move (move stat = 0)
// ============================================================================
void board_get_reachable(const GameState& state, int unit_idx, Hex* out,
                         int* count, int max_out) {
    *count = 0;

    // Bounds check
    if (unit_idx < 0 || unit_idx >= MAX_UNITS * 2) return;

    const Unit& unit = state.units[unit_idx];
    if (unit.type == UnitType::NONE || unit.owner == Player::NONE) return;

    // HQ cannot move
    if (unit.type == UnitType::HQ) return;

    // Effective move range from tech-modified stats
    ModifiedStats stats = get_modified_stats(unit.type, unit.owner, state);
    int move_range = stats.move;

    // Isolation check: is there a passable path from this unit to its HQ?
    Hex hq_pos = (unit.owner == Player::P1) ? state.hq_p1 : state.hq_p2;
    bool isolated = !is_connected_to_hq(state, unit.pos, hq_pos);
    if (isolated) {
        move_range = std::max(0, move_range - ISOLATED_MOVE_PEN);
    }

    if (move_range <= 0) return;

    // BFS
    int dist[MAX_TILES];
    std::fill(dist, dist + MAX_TILES, -1);

    Hex queue[MAX_TILES];
    int qhead = 0, qtail = 0;

    int start_idx = unit.pos.index();
    dist[start_idx] = 0;
    queue[qtail++] = unit.pos;

    while (qhead < qtail) {
        Hex cur = queue[qhead++];
        int cur_dist = dist[cur.index()];
        if (cur_dist >= move_range) continue;

        Hex nbrs[6];
        hex_neighbors(cur, nbrs);

        for (int d = 0; d < 6; d++) {
            Hex nb = nbrs[d];
            if (!nb.valid()) continue;
            int nb_idx = nb.index();
            if (!state.tile_valid[nb_idx]) continue;
            if (dist[nb_idx] >= 0) continue;  // already visited

            // Terrain check — can't enter MOUNTAIN or WATER
            Terrain terrain = state.tiles[nb_idx].terrain;
            if (terrain == Terrain::MOUNTAIN || terrain == Terrain::WATER) continue;

            int occupant = board_unit_at(state, nb);

            // Can't move through enemy units
            if (occupant >= 0 && state.units[occupant].owner != unit.owner) {
                continue;
            }

            // This hex is reachable (at distance cur_dist + 1)
            dist[nb_idx] = cur_dist + 1;
            queue[qtail++] = nb;

            // Can we STOP here? Only if no unit is present.
            // Friendly units can be passed through but not stopped on.
            if (occupant < 0) {
                if (*count < max_out) {
                    out[*count] = nb;
                    (*count)++;
                }
            }
        }
    }
}
