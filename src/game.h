#pragma once
// ============================================================================
// CONQUEST - Game Logic Header
// All game rules: influence, combat, formations, moves, turns, tech, victory.
// ============================================================================

#include <cmath>    // required by core.h for std::round
#include "core.h"
#include "board.h"

// Extended action type for spawning units (not in core.h ActionType enum)
constexpr ActionType ACTION_SPAWN = static_cast<ActionType>(5);

// Full game initialization (calls board_init, map_generate, map_place_hqs)
void game_init(GameState& state, uint32_t map_seed);

// Full board influence computation from scratch
void game_compute_influence(GameState& state);

// Remove a unit's influence from the board (incremental)
void game_incremental_influence_remove(GameState& state, int unit_idx);

// Add a unit's influence to the board (incremental)
void game_incremental_influence_add(GameState& state, int unit_idx);

// Update tile ownership based on influence; count tiles/nodes controlled
void game_update_ownership(GameState& state);

// Returns 0=none, 1=P1 wins, 2=P2 wins
int game_check_victory(const GameState& state);

// Check if a soldier is part of a formation (LINE or TRIANGLE)
FormationType game_check_formation(const GameState& state, int unit_idx);

// Check if unit is connected to HQ via adjacent allies (BFS)
bool game_is_isolated(const GameState& state, int unit_idx);

// Get unit's attack with all modifiers (tech, formation, commander buff, isolation)
int game_get_effective_attack(const GameState& state, int unit_idx);

// Get unit's defense with all modifiers (tech, formation, commander buff, isolation, fortify, elevation)
int game_get_effective_defense(const GameState& state, int unit_idx);

// Get unit's attack range with modifiers (tech, elevation)
int game_get_effective_range(const GameState& state, int unit_idx);

// Get unit's move range with modifiers (tech, isolation)
int game_get_effective_move(const GameState& state, int unit_idx);

// Apply a move to the game state. Returns true if valid.
bool game_apply_move(GameState& state, const Move& move);

// Begin a new turn (energy gain, AP reset, clear fortify)
void game_start_turn(GameState& state);

// End current player's turn, switch to other player
void game_end_turn(GameState& state);

// Generate ALL legal moves for current player. Returns count.
int game_generate_moves(const GameState& state, Move* moves, int max_moves);

// Check if a move is legal
bool game_validate_move(const GameState& state, const Move& move);

// Compute full Zobrist hash
void game_compute_hash(GameState& state, const ZobristTable& zt);

// Spawn a unit if player has enough energy. Returns unit index or -1.
int game_spawn_unit(GameState& state, Player player, UnitType type, Hex pos);

// Draft a tech (mark as active)
void game_draft_tech(GameState& state, Player player, TechID tech);

// Spend energy to research a drafted tech
bool game_research_tech(GameState& state, Player player, TechID tech);

// Apply damage, remove if HP <= 0
void game_damage_unit(GameState& state, int unit_idx, int damage);

// Remove unit from game, update influence
void game_remove_unit(GameState& state, int unit_idx);

// Transition from draft to first research phase
void game_end_draft(GameState& state);
