#pragma once
// ============================================================================
// CONQUEST - Board Initialization, Map Generation, Hex Queries
// ============================================================================

#include <cmath>   // needed by core.h for std::round (hex_round)
#include "core.h"

// Initialize the tile_valid array (mark which hex indices are valid on the grid)
void board_init(GameState& state);

// Generate a symmetric map with terrain, victory nodes, HQ positions
void map_generate(GameState& state, uint32_t seed);

// Place HQ units for both players at their designated positions
void map_place_hqs(GameState& state);

// Find unit index at hex position (-1 if none)
int board_unit_at(const GameState& state, Hex h);

// Check if a hex can be moved through (valid, not impassable terrain, no unit)
bool hex_passable(const GameState& state, Hex h);

// Check if line of sight is blocked between two hexes (mountain blocks, water does not)
bool hex_los_blocked(const GameState& state, Hex from, Hex to);

// BFS to find all hexes a unit can reach within its move range
// respecting terrain, other units (can pass through friendly, can't stop on them)
void board_get_reachable(const GameState& state, int unit_idx, Hex* out, int* count, int max_out);
