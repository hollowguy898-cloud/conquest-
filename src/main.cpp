// ============================================================================
// CONQUEST - Main Entry Point
// Zero randomness. Perfect information. Deterministic combat. All of it.
// ============================================================================

#include "core.h"
#include "board.h"
#include "game.h"
#include "ai.h"
#include "renderer.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>

// ============================================================================
// GAME APPLICATION STATE
// ============================================================================
struct App {
    GameState state;
    ZobristTable zobrist;
    Renderer renderer;
    AI ai;

    // Interaction state
    int selected_unit;       // index into state.units[], -1 = none
    Move valid_moves[1024];  // generated legal moves for selected unit
    int  valid_move_count;
    int  hovered_tech;       // tech ID under cursor during draft, 0 = none

    // Camera
    float hex_size;
    bool dragging;
    int   drag_start_x, drag_start_y;
    float drag_cam_x, drag_cam_y;

    // Time
    float elapsed_time;
    std::chrono::steady_clock::time_point start_time;

    // AI
    bool ai_enabled;     // P2 is AI?
    bool ai_thinking;
    AIResult ai_result;

    // Status text
    char status[256];

    // Draft state
    int draft_count[3]; // how many techs each player has drafted [1]=P1, [2]=P2

    // Spawn mode
    bool spawn_mode;
    UnitType spawn_type;

    // Research mode
    bool research_mode;

    // Replay
    Move move_history[8192];
    int  move_history_count;
};

// ============================================================================
// HELPER: Find unit by ID
// ============================================================================
static int find_unit_by_id(const GameState& state, int id) {
    for (int i = 0; i < MAX_UNITS * 2; i++) {
        if (state.units[i].type != UnitType::NONE && state.units[i].id == id)
            return i;
    }
    return -1;
}

// ============================================================================
// HELPER: Count player's drafted techs
// ============================================================================
static int count_drafted(const GameState& state, Player p) {
    const TechUpgrade* techs = (p == Player::P1) ? state.techs_p1 : state.techs_p2;
    int count = 0;
    for (int i = 1; i < TECH_COUNT; i++) {
        if (techs[i].active) count++;
    }
    return count;
}

// ============================================================================
// HELPER: Get tech at screen position during draft
// ============================================================================
static int get_tech_at_pos(int mx, int my, int screen_w, int screen_h) {
    // Must match the layout in render_draft_ui
    float grid_w = 500.0f;
    float cell_w = grid_w / 5.0f;
    float cell_h = 70.0f;
    float start_x = (screen_w - grid_w) * 0.5f;
    float start_y = 80.0f;

    if (mx < start_x || mx > start_x + grid_w) return 0;
    if (my < start_y || my > start_y + 4 * cell_h) return 0;

    int col = (int)((mx - start_x) / cell_w);
    int row = (int)((my - start_y) / cell_h);
    if (col < 0 || col >= 5 || row < 0 || row >= 4) return 0;

    int tech_idx = row * 5 + col + 1; // TechID starts at 1
    if (tech_idx > 20) return 0;
    return tech_idx;
}

// ============================================================================
// HELPER: Get spawn type from keyboard number
// ============================================================================
static UnitType key_to_spawn_type(int key) {
    switch (key) {
    case 1: return UnitType::SCOUT;
    case 2: return UnitType::SOLDIER;
    case 3: return UnitType::FORTRESS;
    case 4: return UnitType::CANNON;
    case 5: return UnitType::COMMANDER;
    default: return UnitType::NONE;
    }
}

// ============================================================================
// INITIALIZE APPLICATION
// ============================================================================
static void app_init(App& app, uint32_t map_seed) {
    memset(&app, 0, sizeof(app));

    app.selected_unit = -1;
    app.valid_move_count = 0;
    app.hovered_tech = 0;
    app.hex_size = 32.0f;
    app.dragging = false;
    app.drag_start_x = 0;
    app.drag_start_y = 0;
    app.drag_cam_x = 0;
    app.drag_cam_y = 0;
    app.ai_enabled = true;
    app.ai_thinking = false;
    app.spawn_mode = false;
    app.spawn_type = UnitType::NONE;
    app.research_mode = false;
    app.move_history_count = 0;
    app.draft_count[1] = 0;
    app.draft_count[2] = 0;

    snprintf(app.status, sizeof(app.status), "WELCOME TO CONQUEST");

    // Initialize Zobrist table
    app.zobrist.init(map_seed);

    // Initialize game
    game_init(app.state, map_seed);
    game_compute_hash(app.state, app.zobrist);

    // Set AI depth
    app.ai.set_depth(4);
    app.ai.set_time_limit(3000);

    // Start time
    app.start_time = std::chrono::steady_clock::now();
    app.elapsed_time = 0.0f;
}

// ============================================================================
// GENERATE MOVES FOR SELECTED UNIT
// ============================================================================
static void generate_unit_moves(App& app) {
    app.valid_move_count = 0;
    if (app.selected_unit < 0) return;

    const Unit& unit = app.state.units[app.selected_unit];
    if (unit.type == UnitType::NONE || unit.owner != app.state.current) return;

    // Generate ALL moves and filter to only this unit's moves
    Move all_moves[1024];
    int all_count = game_generate_moves(app.state, all_moves, 1024);

    for (int i = 0; i < all_count; i++) {
        if (all_moves[i].unit_id == unit.id) {
            app.valid_moves[app.valid_move_count++] = all_moves[i];
        }
    }
}

// ============================================================================
// HANDLE CLICK ON A HEX DURING ACTION PHASE
// ============================================================================
static void handle_action_click(App& app, Hex clicked_hex) {
    if (app.state.phase == GamePhase::GAME_OVER) return;

    Player current = app.state.current;

    // Check if clicking on a valid move target
    if (app.selected_unit >= 0 && app.valid_move_count > 0) {
        for (int i = 0; i < app.valid_move_count; i++) {
            if (app.valid_moves[i].to == clicked_hex) {
                // Execute this move
                Move m = app.valid_moves[i];

                // Record in history
                if (app.move_history_count < 8192) {
                    app.move_history[app.move_history_count++] = m;
                }

                game_apply_move(app.state, m);
                game_compute_hash(app.state, app.zobrist);

                // Check victory
                int victor = game_check_victory(app.state);
                if (victor > 0) {
                    app.state.winner = (victor == 1) ? Player::P1 : Player::P2;
                    app.state.phase = GamePhase::GAME_OVER;
                    snprintf(app.status, sizeof(app.status),
                             "%s WINS!", victor == 1 ? "PLAYER 1" : "PLAYER 2");
                }

                // Deselect
                app.selected_unit = -1;
                app.valid_move_count = 0;
                app.spawn_mode = false;
                snprintf(app.status, sizeof(app.status), "MOVE EXECUTED");
                return;
            }
        }
    }

    // Clicking on a unit - select it
    int unit_idx = board_unit_at(app.state, clicked_hex);
    if (unit_idx >= 0) {
        const Unit& unit = app.state.units[unit_idx];
        if (unit.owner == current && unit.ap > 0 && unit.type != UnitType::HQ) {
            app.selected_unit = unit_idx;
            app.spawn_mode = false;
            generate_unit_moves(app);
            snprintf(app.status, sizeof(app.status), "SELECTED %s (AP:%d)",
                     unit_type_name(unit.type), unit.ap);
            return;
        } else if (unit.owner == current && unit.type == UnitType::HQ) {
            // Clicking on own HQ - enter spawn mode
            app.selected_unit = unit_idx;
            app.spawn_mode = true;
            app.spawn_type = UnitType::NONE;
            // Generate spawn moves
            app.valid_move_count = 0;
            Move all_moves[1024];
            int all_count = game_generate_moves(app.state, all_moves, 1024);
            for (int i = 0; i < all_count; i++) {
                if (all_moves[i].action == ACTION_SPAWN) {
                    app.valid_moves[app.valid_move_count++] = all_moves[i];
                }
            }
            snprintf(app.status, sizeof(app.status),
                     "HQ SELECTED - PRESS 1-5 TO SPAWN (ENERGY: %d)",
                     app.state.energy[static_cast<int>(current)]);
            return;
        }
    }

    // Clicking on empty hex or enemy - deselect
    app.selected_unit = -1;
    app.valid_move_count = 0;
    app.spawn_mode = false;
    snprintf(app.status, sizeof(app.status), "DESELECTED");
}

// ============================================================================
// HANDLE DRAFT CLICK
// ============================================================================
static void handle_draft_click(App& app, int mx, int my) {
    int tech_idx = get_tech_at_pos(mx, my, app.renderer.window_width(),
                                    app.renderer.window_height());
    if (tech_idx < 1 || tech_idx > 20) return;

    Player drafter = app.state.current;
    TechID tid = static_cast<TechID>(tech_idx);
    TechUpgrade* techs = (drafter == Player::P1) ? app.state.techs_p1 : app.state.techs_p2;

    if (techs[tech_idx].active) {
        // Already drafted, un-draft it
        techs[tech_idx].active = false;
        app.draft_count[static_cast<int>(drafter)]--;
        snprintf(app.status, sizeof(app.status), "UN-DRAFTED %s",
                 tech_name(tid));
    } else if (app.draft_count[static_cast<int>(drafter)] < DRAFT_COUNT) {
        // Draft this tech
        game_draft_tech(app.state, drafter, tid);
        app.draft_count[static_cast<int>(drafter)]++;
        snprintf(app.status, sizeof(app.status), "DRAFTED %s (%d/%d)",
                 tech_name(tid),
                 app.draft_count[static_cast<int>(drafter)], DRAFT_COUNT);
    }

    // Auto-advance when both players have drafted enough
    if (app.draft_count[1] >= DRAFT_COUNT && app.draft_count[2] >= DRAFT_COUNT) {
        game_end_draft(app.state);
        game_start_turn(app.state);
        snprintf(app.status, sizeof(app.status), "DRAFT COMPLETE - RESEARCH PHASE P1");
    } else if (app.draft_count[static_cast<int>(drafter)] >= DRAFT_COUNT) {
        // Move to next player's draft
        if (app.state.phase == GamePhase::DRAFT_P1) {
            app.state.phase = GamePhase::DRAFT_P2;
            app.state.current = Player::P2;
            snprintf(app.status, sizeof(app.status), "PLAYER 2 DRAFT PHASE");
        } else if (app.state.phase == GamePhase::DRAFT_P2) {
            app.state.phase = GamePhase::DRAFT_P1;
            app.state.current = Player::P1;
            snprintf(app.status, sizeof(app.status), "PLAYER 1 DRAFT PHASE");
        }
    }
}

// ============================================================================
// HANDLE RESEARCH CLICK
// ============================================================================
static void handle_research_click(App& app, int mx, int my) {
    // Use same grid layout as draft for tech selection
    int tech_idx = get_tech_at_pos(mx, my, app.renderer.window_width(),
                                    app.renderer.window_height());
    if (tech_idx < 1 || tech_idx > 20) return;

    Player researcher = app.state.current;
    TechID tid = static_cast<TechID>(tech_idx);
    TechUpgrade* techs = (researcher == Player::P1) ? app.state.techs_p1 : app.state.techs_p2;

    if (!techs[tech_idx].active) {
        snprintf(app.status, sizeof(app.status), "NOT DRAFTED YET");
        return;
    }
    if (techs[tech_idx].researched) {
        snprintf(app.status, sizeof(app.status), "ALREADY RESEARCHED");
        return;
    }

    if (game_research_tech(app.state, researcher, tid)) {
        snprintf(app.status, sizeof(app.status), "RESEARCHED %s",
                 tech_name(tid));
    } else {
        snprintf(app.status, sizeof(app.status), "NOT ENOUGH ENERGY (NEED %d)",
                 techs[tech_idx].research_cost);
    }
}

// ============================================================================
// END TURN
// ============================================================================
static void end_turn(App& app) {
    app.selected_unit = -1;
    app.valid_move_count = 0;
    app.spawn_mode = false;
    app.research_mode = false;

    game_end_turn(app.state);
    game_compute_hash(app.state, app.zobrist);

    int victor = game_check_victory(app.state);
    if (victor > 0) {
        app.state.winner = (victor == 1) ? Player::P1 : Player::P2;
        app.state.phase = GamePhase::GAME_OVER;
        snprintf(app.status, sizeof(app.status),
                 "%s WINS!", victor == 1 ? "PLAYER 1" : "PLAYER 2");
    } else {
        snprintf(app.status, sizeof(app.status), "%s TURN %d",
                 app.state.current == Player::P1 ? "P1" : "P2", app.state.turn);
    }
}

// ============================================================================
// AI TURN
// ============================================================================
static void do_ai_turn(App& app) {
    if (!app.ai_enabled) return;
    if (app.state.phase != GamePhase::ACTION_P2) return;

    app.ai_thinking = true;
    app.ai_result = app.ai.search(app.state, app.zobrist);
    app.ai_thinking = false;

    if (app.ai_result.best_move.action != ActionType::NONE) {
        Move m = app.ai_result.best_move;

        // Record in history
        if (app.move_history_count < 8192) {
            app.move_history[app.move_history_count++] = m;
        }

        game_apply_move(app.state, m);
        game_compute_hash(app.state, app.zobrist);

        int victor = game_check_victory(app.state);
        if (victor > 0) {
            app.state.winner = (victor == 1) ? Player::P1 : Player::P2;
            app.state.phase = GamePhase::GAME_OVER;
            snprintf(app.status, sizeof(app.status),
                     "%s WINS!", victor == 1 ? "PLAYER 1" : "PLAYER 2");
        } else {
            snprintf(app.status, sizeof(app.status), "AI MOVED (depth %d, %d nodes)",
                     app.ai_result.depth, app.ai_result.nodes);
        }
    } else {
        // AI can't find a move or passes
        snprintf(app.status, sizeof(app.status), "AI PASSES");
        end_turn(app);
    }
}

// ============================================================================
// MAIN
// ============================================================================
int main(int argc, char* argv[]) {
    uint32_t map_seed = 18462;  // default seed
    if (argc > 1) {
        map_seed = (uint32_t)atoi(argv[1]);
    }

    App app;
    app_init(app, map_seed);

    // Initialize renderer
    if (!app.renderer.init(1280, 800, "CONQUEST")) {
        fprintf(stderr, "Failed to initialize renderer\n");
        return 1;
    }

    // Center camera on the board center
    app.renderer.set_camera(0.0f, 0.0f, 1.0f);

    snprintf(app.status, sizeof(app.status), "CONQUEST - SEED %u - DRAFT YOUR TECHS", map_seed);

    // Main loop
    bool running = true;
    bool ai_timer_active = false;
    float ai_timer = 0.0f;

    while (running) {
        // Update time
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - app.start_time).count();
        app.elapsed_time = dt;

        // Handle SDL events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                break;
            }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE) {
                running = false;
                break;
            }

            // --- MOUSE EVENTS ---
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                int mx = event.button.x;
                int my = event.button.y;

                if (event.button.button == SDL_BUTTON_LEFT) {
                    // Draft phase clicks
                    if (app.state.phase == GamePhase::DRAFT_P1 ||
                        app.state.phase == GamePhase::DRAFT_P2) {
                        handle_draft_click(app, mx, my);
                    }
                    // Research phase clicks
                    else if (app.state.phase == GamePhase::RESEARCH_P1 ||
                             app.state.phase == GamePhase::RESEARCH_P2) {
                        handle_research_click(app, mx, my);
                    }
                    // Action phase clicks
                    else if (app.state.phase == GamePhase::ACTION_P1 ||
                             app.state.phase == GamePhase::ACTION_P2) {
                        // Only allow clicks for the human player's turn
                        // (P1 is always human, P2 can be AI)
                        bool is_human_turn =
                            (app.state.phase == GamePhase::ACTION_P1) ||
                            (app.state.phase == GamePhase::ACTION_P2 && !app.ai_enabled);

                        if (is_human_turn) {
                            Hex clicked = app.renderer.screen_to_hex(
                                (float)mx, (float)my, app.hex_size);
                            if (clicked.valid()) {
                                handle_action_click(app, clicked);
                            }
                        }
                    }
                }
                // Right click - deselect / cancel
                else if (event.button.button == SDL_BUTTON_RIGHT) {
                    app.selected_unit = -1;
                    app.valid_move_count = 0;
                    app.spawn_mode = false;
                    app.spawn_type = UnitType::NONE;
                    snprintf(app.status, sizeof(app.status), "CANCELLED");
                }
                // Middle click - start camera drag
                else if (event.button.button == SDL_BUTTON_MIDDLE) {
                    app.dragging = true;
                    app.drag_start_x = mx;
                    app.drag_start_y = my;
                    float cx, cy, zoom;
                    app.renderer.get_camera(cx, cy, zoom);
                    app.drag_cam_x = cx;
                    app.drag_cam_y = cy;
                }
            }

            if (event.type == SDL_MOUSEBUTTONUP) {
                if (event.button.button == SDL_BUTTON_MIDDLE) {
                    app.dragging = false;
                }
            }

            if (event.type == SDL_MOUSEMOTION) {
                int mx = event.motion.x;
                int my = event.motion.y;

                // Camera dragging
                if (app.dragging) {
                    float dx = (float)(mx - app.drag_start_x) / app.hex_size;
                    float dy = (float)(my - app.drag_start_y) / app.hex_size;
                    app.renderer.set_camera(app.drag_cam_x - dx,
                                            app.drag_cam_y - dy,
                                            1.0f);
                }

                // Tech hover during draft/research
                if (app.state.phase == GamePhase::DRAFT_P1 ||
                    app.state.phase == GamePhase::DRAFT_P2 ||
                    app.state.phase == GamePhase::RESEARCH_P1 ||
                    app.state.phase == GamePhase::RESEARCH_P2) {
                    app.hovered_tech = get_tech_at_pos(mx, my,
                        app.renderer.window_width(), app.renderer.window_height());
                }
            }

            // --- MOUSE WHEEL - ZOOM ---
            if (event.type == SDL_MOUSEWHEEL) {
                float cx, cy, zoom;
                app.renderer.get_camera(cx, cy, zoom);
                if (event.wheel.y > 0) {
                    zoom *= 1.1f;
                    if (zoom > 5.0f) zoom = 5.0f;
                } else if (event.wheel.y < 0) {
                    zoom /= 1.1f;
                    if (zoom < 0.3f) zoom = 0.3f;
                }
                app.renderer.set_camera(cx, cy, zoom);
            }

            // --- KEYBOARD EVENTS ---
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    running = false;
                    break;

                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    // End turn / confirm draft / advance phase
                    if (app.state.phase == GamePhase::ACTION_P1 ||
                        app.state.phase == GamePhase::ACTION_P2) {
                        end_turn(app);
                    } else if (app.state.phase == GamePhase::RESEARCH_P1) {
                        // End research, go to action
                        app.state.phase = GamePhase::ACTION_P1;
                        app.state.current = Player::P1;
                        snprintf(app.status, sizeof(app.status), "P1 ACTION PHASE");
                    } else if (app.state.phase == GamePhase::RESEARCH_P2) {
                        app.state.phase = GamePhase::ACTION_P2;
                        app.state.current = Player::P2;
                        snprintf(app.status, sizeof(app.status), "P2 ACTION PHASE");
                    }
                    break;

                case SDLK_r:
                    // Toggle research mode during action phase
                    if (app.state.phase == GamePhase::ACTION_P1 ||
                        app.state.phase == GamePhase::ACTION_P2) {
                        app.research_mode = !app.research_mode;
                        snprintf(app.status, sizeof(app.status),
                                 app.research_mode ? "RESEARCH MODE - CLICK TECH" : "RESEARCH MODE OFF");
                    }
                    break;

                case SDLK_a:
                    // Toggle AI
                    app.ai_enabled = !app.ai_enabled;
                    snprintf(app.status, sizeof(app.status),
                             "AI %s", app.ai_enabled ? "ON" : "OFF");
                    break;

                case SDLK_n:
                    // New game with same seed
                    {
                        uint32_t seed = app.state.map_seed;
                        app_shutdown:;
                        app_init(app, seed);
                        if (!app.renderer.init(1280, 800, "CONQUEST")) {
                            running = false;
                        }
                        app.renderer.set_camera(0.0f, 0.0f, 1.0f);
                        snprintf(app.status, sizeof(app.status),
                                 "NEW GAME - SEED %u", seed);
                    }
                    break;

                case SDLK_f:
                    // Fortify selected unit
                    if (app.selected_unit >= 0) {
                        const Unit& unit = app.state.units[app.selected_unit];
                        if (unit.owner == app.state.current && unit.ap > 0 &&
                            !unit.fortified) {
                            Move m;
                            m.action = ActionType::FORTIFY;
                            m.unit_id = unit.id;
                            m.from = unit.pos;
                            m.to = unit.pos;
                            m.cost_ap = 1;
                            game_apply_move(app.state, m);
                            game_compute_hash(app.state, app.zobrist);
                            app.selected_unit = -1;
                            app.valid_move_count = 0;
                            snprintf(app.status, sizeof(app.status), "FORTIFIED");
                        }
                    }
                    break;

                // Spawn unit types (1-5)
                case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_5:
                    if (app.spawn_mode) {
                        int key = event.key.keysym.sym - SDLK_0;
                        app.spawn_type = key_to_spawn_type(key);
                        if (app.spawn_type != UnitType::NONE) {
                            // Regenerate spawn moves for this type only
                            app.valid_move_count = 0;
                            Move all_moves[1024];
                            int all_count = game_generate_moves(app.state, all_moves, 1024);
                            for (int i = 0; i < all_count; i++) {
                                if (all_moves[i].action == ACTION_SPAWN) {
                                    // Decode spawn type from the move's unit_id field
                                    // Convention: unit_id = -(type+1) for spawn moves
                                    int decoded_type = -(all_moves[i].unit_id) - 1;
                                    if (decoded_type == static_cast<int>(app.spawn_type)) {
                                        app.valid_moves[app.valid_move_count++] = all_moves[i];
                                    }
                                }
                            }
                            snprintf(app.status, sizeof(app.status),
                                     "SPAWN %s - CLICK ADJACENT HEX (COST: %d)",
                                     unit_type_name(app.spawn_type),
                                     get_base_stats(app.spawn_type).cost);
                        }
                    }
                    break;

                case SDLK_h:
                    // Show help
                    snprintf(app.status, sizeof(app.status),
                             "LCLICK:SELECT/ACT  RCLICK:CANCEL  ENTER:END TURN  F:FORTIFY  A:AI  1-5:SPAWN");
                    break;

                default:
                    break;
                }
            }
        }

        // --- AI TURN ---
        if (app.ai_enabled && app.state.phase == GamePhase::ACTION_P2 &&
            app.state.phase != GamePhase::GAME_OVER) {
            if (!app.ai_thinking) {
                ai_timer += 0.016f; // ~60fps delay before AI acts
                if (ai_timer > 0.5f) {
                    do_ai_turn(app);
                    ai_timer = 0.0f;

                    // Check if AI has no more AP - end its turn
                    bool has_ap = false;
                    for (int i = MAX_UNITS; i < MAX_UNITS * 2; i++) {
                        if (app.state.units[i].type != UnitType::NONE &&
                            app.state.units[i].owner == Player::P2 &&
                            app.state.units[i].ap > 0) {
                            has_ap = true;
                            break;
                        }
                    }
                    if (!has_ap) {
                        end_turn(app);
                    }
                }
            }
        } else {
            ai_timer = 0.0f;
        }

        // --- RENDER ---
        app.renderer.begin_frame();

        if (app.state.phase == GamePhase::DRAFT_P1 ||
            app.state.phase == GamePhase::DRAFT_P2) {
            // Draft UI
            Player drafting_player = (app.state.phase == GamePhase::DRAFT_P1)
                                     ? Player::P1 : Player::P2;
            app.renderer.render_game(app.state, app.hex_size, app.elapsed_time,
                                     -1, nullptr, 0);
            app.renderer.render_draft_ui(app.state, app.hovered_tech, drafting_player);
        }
        else if (app.state.phase == GamePhase::RESEARCH_P1 ||
                 app.state.phase == GamePhase::RESEARCH_P2) {
            // Research UI (same grid as draft, but for spending energy)
            Player researcher = (app.state.phase == GamePhase::RESEARCH_P1)
                                ? Player::P1 : Player::P2;
            app.renderer.render_game(app.state, app.hex_size, app.elapsed_time,
                                     -1, nullptr, 0);
            app.renderer.render_draft_ui(app.state, app.hovered_tech, researcher);
        }
        else {
            // Action phase - normal game view
            app.renderer.render_game(app.state, app.hex_size, app.elapsed_time,
                                     app.selected_unit,
                                     app.valid_moves, app.valid_move_count);
        }

        // Always render UI overlay
        app.renderer.render_ui(app.state, app.selected_unit, app.status);

        app.renderer.end_frame();

        // Check if renderer was closed
        if (app.renderer.should_close()) {
            running = false;
        }
    }

    app.renderer.shutdown();
    return 0;
}
