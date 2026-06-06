#pragma once
// ============================================================================
// CONQUEST - SDL2+OpenGL Renderer
// Full 2D hex strategy game renderer using OpenGL immediate mode.
// ============================================================================

#include <cmath>
#include "core.h"
#include <SDL2/SDL.h>

// Colors
struct Color {
    float r, g, b, a;
    constexpr Color(float r=0, float g=0, float b=0, float a=1) : r(r), g(g), b(b), a(a) {}
    static constexpr Color white()   { return {1,1,1,1}; }
    static constexpr Color black()   { return {0,0,0,1}; }
    static constexpr Color p1()      { return {0.2f,0.5f,1.0f,1}; }  // Blue
    static constexpr Color p2()      { return {0.9f,0.2f,0.2f,1}; }  // Red
    static constexpr Color p1_light(){ return {0.4f,0.7f,1.0f,0.5f}; }
    static constexpr Color p2_light(){ return {1.0f,0.5f,0.5f,0.5f}; }
    static constexpr Color gray()    { return {0.4f,0.4f,0.4f,1}; }
    static constexpr Color dark()    { return {0.15f,0.15f,0.18f,1}; }
    static constexpr Color green()   { return {0.2f,0.8f,0.3f,1}; }
    static constexpr Color gold()    { return {1.0f,0.85f,0.0f,1}; }
    static constexpr Color mountain(){ return {0.55f,0.45f,0.35f,1}; }
    static constexpr Color water()   { return {0.2f,0.35f,0.7f,1}; }
    static constexpr Color high()    { return {0.6f,0.55f,0.3f,1}; }
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(int width, int height, const char* title);
    void shutdown();

    void begin_frame();
    void end_frame();

    void set_camera(float cx, float cy, float zoom);
    void get_camera(float& cx, float& cy, float& zoom);

    // World-space drawing
    void draw_hex(Hex h, float hex_size, const Color& fill, const Color& border, float border_width = 1.0f);
    void draw_hex_outline(Hex h, float hex_size, const Color& color, float width = 1.0f);
    void draw_filled_hex(Hex h, float hex_size, const Color& color);

    // Units - procedural geometry
    void draw_scout(Hex h, float hex_size, Player owner, int hp, int max_hp, bool selected, bool fortified);
    void draw_soldier(Hex h, float hex_size, Player owner, int hp, int max_hp, bool selected, bool fortified);
    void draw_fortress(Hex h, float hex_size, Player owner, int hp, int max_hp, bool selected, bool fortified);
    void draw_cannon(Hex h, float hex_size, Player owner, int hp, int max_hp, bool selected, bool fortified);
    void draw_commander(Hex h, float hex_size, Player owner, int hp, int max_hp, bool selected, bool fortified);
    void draw_hq(Hex h, float hex_size, Player owner, int hp, int max_hp, bool selected);
    void draw_unit(Hex h, float hex_size, UnitType type, Player owner, int hp, int max_hp, bool selected, bool fortified);

    // Victory node crystal
    void draw_victory_node(Hex h, float hex_size, float time);

    // Territory borders
    void draw_territory_border(Hex h, float hex_size, Player owner);

    // HP bar
    void draw_hp_bar(float cx, float cy, float w, float h, int hp, int max_hp);

    // Influence overlay
    void draw_influence_overlay(Hex h, float hex_size, int p1_inf, int p2_inf);

    // Move highlights
    void draw_move_highlight(Hex h, float hex_size);
    void draw_attack_highlight(Hex h, float hex_size);
    void draw_capture_highlight(Hex h, float hex_size);

    // Screen-space UI
    void draw_text(const char* text, float x, float y, float size, const Color& color);
    void draw_rect(float x, float y, float w, float h, const Color& color);
    void draw_rect_outline(float x, float y, float w, float h, const Color& color, float width = 1.0f);

    // Full game board render
    void render_game(const GameState& state, float hex_size, float time,
                     int selected_unit, const Move* valid_moves, int move_count);

    // UI panels
    void render_ui(const GameState& state, int selected_unit, const char* status_text);
    void render_draft_ui(const GameState& state, int hover_tech, Player drafting_player);
    void render_tech_panel(const GameState& state, Player player, float x, float y);

    // Coordinate conversion
    void world_to_screen(float wx, float wy, float& sx, float& sy);
    void screen_to_world(float sx, float sy, float& wx, float& wy);
    Hex screen_to_hex(float sx, float sy, float hex_size);

    // Window
    int window_width() const;
    int window_height() const;
    bool should_close() const;

private:
    SDL_Window* window_;
    SDL_GLContext gl_context_;
    int width_, height_;
    float cam_x_, cam_y_, cam_zoom_;
    bool should_close_;

    // Simple font rendering with SDL2
    void draw_char(char c, float x, float y, float size, const Color& color);

    // GL helpers
    void draw_triangle(float x1, float y1, float x2, float y2, float x3, float y3, const Color& color);
    void draw_quad(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, const Color& color);
    void draw_line(float x1, float y1, float x2, float y2, const Color& color, float width);
    void draw_circle(float cx, float cy, float r, const Color& color, int segments = 24);
    void draw_ring(float cx, float cy, float r, const Color& color, float width, int segments = 24);

    // Hex vertex calculation
    void hex_vertex(Hex h, float hex_size, int corner, float& vx, float& vy);
    void hex_center(Hex h, float hex_size, float& cx, float& cy);

    // Unit shape helpers
    void draw_arrow_shape(float cx, float cy, float size, const Color& color);        // Scout
    void draw_shield_shape(float cx, float cy, float size, const Color& color);       // Soldier
    void draw_fortress_shape(float cx, float cy, float size, const Color& color);     // Fortress
    void draw_cannon_shape(float cx, float cy, float size, const Color& color);       // Cannon
    void draw_commander_shape(float cx, float cy, float size, const Color& color);    // Commander
    void draw_hq_shape(float cx, float cy, float size, const Color& color);           // HQ
};
