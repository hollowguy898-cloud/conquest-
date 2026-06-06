// ============================================================================
// CONQUEST - SDL2+OpenGL Renderer Implementation
// Full 2D hex strategy game renderer using OpenGL immediate mode.
// ============================================================================

#include "renderer.h"
#include "game.h"
#include <SDL2/SDL_opengl.h>
#include <cmath>
#include <cstring>
#include <cstdio>

static const float PI = 3.14159265358979323846f;

// ============================================================================
// 5x7 BITMAP FONT DATA
// Each character: 7 rows, 5 bits per row (bit4=leftmost, bit0=rightmost)
// ============================================================================

// Indices: 0-25 = A-Z, 26-35 = 0-9, 36=space, 37=., 38=,, 39=:,
//          40=-, 41=/, 42=(, 43=), 44=!, 45=+, 46==
static const uint8_t FONT_DATA[47][7] = {
    // A
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    // B
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
    // C
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
    // D
    {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
    // E
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
    // F
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
    // G
    {0x0E, 0x11, 0x10, 0x13, 0x11, 0x11, 0x0E},
    // H
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    // I
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
    // J
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C},
    // K
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
    // L
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    // M
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
    // N
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    // O
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    // P
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    // Q
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
    // R
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    // S
    {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E},
    // T
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    // U
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    // V
    {0x11, 0x11, 0x11, 0x11, 0x0A, 0x0A, 0x04},
    // W
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A},
    // X
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    // Y
    {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},
    // Z
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},
    // 0
    {0x0E, 0x13, 0x15, 0x15, 0x15, 0x19, 0x0E},
    // 1
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    // 2
    {0x0E, 0x11, 0x01, 0x04, 0x08, 0x10, 0x1F},
    // 3
    {0x0E, 0x11, 0x01, 0x06, 0x01, 0x11, 0x0E},
    // 4
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    // 5
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    // 6
    {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    // 7
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    // 8
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    // 9
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E},
    // space
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // .
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04},
    // ,
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x08},
    // :
    {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00},
    // -
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},
    // /
    {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10},
    // (
    {0x06, 0x08, 0x10, 0x10, 0x10, 0x08, 0x06},
    // )
    {0x0C, 0x02, 0x01, 0x01, 0x01, 0x02, 0x0C},
    // !
    {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04},
    // +
    {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00},
    // =
    {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00},
};

static int font_index(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a'; // lowercase maps to uppercase
    if (c >= '0' && c <= '9') return 26 + (c - '0');
    switch (c) {
        case ' ':  return 36;
        case '.':  return 37;
        case ',':  return 38;
        case ':':  return 39;
        case '-':  return 40;
        case '/':  return 41;
        case '(':  return 42;
        case ')':  return 43;
        case '!':  return 44;
        case '+':  return 45;
        case '=':  return 46;
        default:   return -1;
    }
}

// ============================================================================
// RENDERER CONSTRUCTOR / DESTRUCTOR
// ============================================================================

Renderer::Renderer()
    : window_(nullptr)
    , gl_context_(nullptr)
    , width_(0)
    , height_(0)
    , cam_x_(0.0f)
    , cam_y_(0.0f)
    , cam_zoom_(1.0f)
    , should_close_(false)
{
}

Renderer::~Renderer() {
    shutdown();
}

// ============================================================================
// INIT / SHUTDOWN
// ============================================================================

bool Renderer::init(int width, int height, const char* title) {
    width_ = width;
    height_ = height;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return false;
    }

    // Request OpenGL 2.1 compatibility context
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    window_ = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    if (!window_) {
        SDL_Quit();
        return false;
    }

    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_Quit();
        return false;
    }

    SDL_GL_MakeCurrent(window_, gl_context_);
    SDL_GL_SetSwapInterval(1); // vsync

    // Center camera on the board (hex 0,0 is the center)
    cam_x_ = 0.0f;
    cam_y_ = 0.0f;
    cam_zoom_ = 1.0f;
    should_close_ = false;

    return true;
}

void Renderer::shutdown() {
    if (gl_context_) {
        SDL_GL_DeleteContext(gl_context_);
        gl_context_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

// ============================================================================
// FRAME BEGIN / END
// ============================================================================

void Renderer::begin_frame() {
    // Poll events for close detection
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            should_close_ = true;
        }
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE) {
            should_close_ = true;
        }
    }

    // Update window size
    SDL_GetWindowSize(window_, &width_, &height_);

    // Set viewport
    glViewport(0, 0, width_, height_);

    // Clear with dark background
    Color bg = Color::dark();
    glClearColor(bg.r, bg.g, bg.b, bg.a);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set up 2D orthographic projection (Y-down: top-left origin)
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width_, height_, 0, -1, 1);

    // Set up modelview with camera transform
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    // Camera: translate screen center to world position, then scale
    glTranslatef((float)width_ * 0.5f, (float)height_ * 0.5f, 0.0f);
    glScalef(cam_zoom_, cam_zoom_, 1.0f);
    glTranslatef(-cam_x_, -cam_y_, 0.0f);

    // Enable blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
}

void Renderer::end_frame() {
    SDL_GL_SwapWindow(window_);
}

// ============================================================================
// CAMERA
// ============================================================================

void Renderer::set_camera(float cx, float cy, float zoom) {
    cam_x_ = cx;
    cam_y_ = cy;
    cam_zoom_ = zoom;
}

void Renderer::get_camera(float& cx, float& cy, float& zoom) {
    cx = cam_x_;
    cy = cam_y_;
    zoom = cam_zoom_;
}

// ============================================================================
// GL HELPERS
// ============================================================================

void Renderer::draw_triangle(float x1, float y1, float x2, float y2,
                              float x3, float y3, const Color& color) {
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_TRIANGLES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glVertex2f(x3, y3);
    glEnd();
}

void Renderer::draw_quad(float x1, float y1, float x2, float y2,
                          float x3, float y3, float x4, float y4,
                          const Color& color) {
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glVertex2f(x3, y3);
    glVertex2f(x4, y4);
    glEnd();
}

void Renderer::draw_line(float x1, float y1, float x2, float y2,
                          const Color& color, float width) {
    glLineWidth(width);
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

void Renderer::draw_circle(float cx, float cy, float r, const Color& color, int segments) {
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= segments; i++) {
        float angle = 2.0f * PI * i / segments;
        glVertex2f(cx + r * cosf(angle), cy + r * sinf(angle));
    }
    glEnd();
}

void Renderer::draw_ring(float cx, float cy, float r, const Color& color,
                          float width, int segments) {
    glLineWidth(width);
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; i++) {
        float angle = 2.0f * PI * i / segments;
        glVertex2f(cx + r * cosf(angle), cy + r * sinf(angle));
    }
    glEnd();
}

// ============================================================================
// HEX VERTEX / CENTER CALCULATION
// ============================================================================

void Renderer::hex_center(Hex h, float hex_size, float& cx, float& cy) {
    hex_to_pixel(h, hex_size, cx, cy);
}

void Renderer::hex_vertex(Hex h, float hex_size, int corner, float& vx, float& vy) {
    float cx, cy;
    hex_center(h, hex_size, cx, cy);
    // Pointy-top hex: corner 0 at top (-90 degrees)
    float angle = (60.0f * corner - 90.0f) * PI / 180.0f;
    vx = cx + hex_size * cosf(angle);
    vy = cy + hex_size * sinf(angle);
}

// ============================================================================
// HEX DRAWING
// ============================================================================

void Renderer::draw_filled_hex(Hex h, float hex_size, const Color& color) {
    float cx, cy;
    hex_center(h, hex_size, cx, cy);

    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_POLYGON);
    for (int i = 0; i < 6; i++) {
        float angle = (60.0f * i - 90.0f) * PI / 180.0f;
        glVertex2f(cx + hex_size * cosf(angle), cy + hex_size * sinf(angle));
    }
    glEnd();
}

void Renderer::draw_hex_outline(Hex h, float hex_size, const Color& color, float width) {
    float cx, cy;
    hex_center(h, hex_size, cx, cy);

    glLineWidth(width);
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 6; i++) {
        float angle = (60.0f * i - 90.0f) * PI / 180.0f;
        glVertex2f(cx + hex_size * cosf(angle), cy + hex_size * sinf(angle));
    }
    glEnd();
}

void Renderer::draw_hex(Hex h, float hex_size, const Color& fill,
                         const Color& border, float border_width) {
    draw_filled_hex(h, hex_size, fill);
    draw_hex_outline(h, hex_size, border, border_width);
}

// ============================================================================
// UNIT SHAPE HELPERS
// All drawn relative to (cx, cy) with a given size scale factor.
// ============================================================================

void Renderer::draw_arrow_shape(float cx, float cy, float size, const Color& color) {
    // Scout: sharp arrowhead pointing up
    float s = size;
    // Arrowhead triangle
    draw_triangle(
        cx,        cy - s * 0.7f,   // tip
        cx - s*0.4f, cy - s * 0.1f,  // left base of head
        cx + s*0.4f, cy - s * 0.1f,  // right base of head
        color
    );
    // Shaft rectangle
    draw_quad(
        cx - s*0.12f, cy - s*0.1f,
        cx + s*0.12f, cy - s*0.1f,
        cx + s*0.12f, cy + s*0.5f,
        cx - s*0.12f, cy + s*0.5f,
        color
    );
    // Small tail fins
    draw_triangle(
        cx - s*0.12f, cy + s*0.3f,
        cx - s*0.35f, cy + s*0.55f,
        cx - s*0.12f, cy + s*0.55f,
        color
    );
    draw_triangle(
        cx + s*0.12f, cy + s*0.3f,
        cx + s*0.35f, cy + s*0.55f,
        cx + s*0.12f, cy + s*0.55f,
        color
    );
}

void Renderer::draw_shield_shape(float cx, float cy, float size, const Color& color) {
    // Soldier: chunky shield/tower shape
    float s = size;
    // Main body rectangle
    draw_quad(
        cx - s*0.35f, cy - s*0.35f,
        cx + s*0.35f, cy - s*0.35f,
        cx + s*0.35f, cy + s*0.35f,
        cx - s*0.35f, cy + s*0.35f,
        color
    );
    // Top cap (wider)
    draw_quad(
        cx - s*0.45f, cy - s*0.5f,
        cx + s*0.45f, cy - s*0.5f,
        cx + s*0.45f, cy - s*0.35f,
        cx - s*0.45f, cy - s*0.35f,
        color
    );
    // Shield boss (center circle)
    Color darker = {color.r * 0.6f, color.g * 0.6f, color.b * 0.6f, color.a};
    draw_circle(cx, cy, s * 0.15f, darker);
}

void Renderer::draw_fortress_shape(float cx, float cy, float size, const Color& color) {
    // Fortress: wide base with 3 battlements on top
    float s = size;
    // Main body (wider)
    draw_quad(
        cx - s*0.45f, cy - s*0.2f,
        cx + s*0.45f, cy - s*0.2f,
        cx + s*0.45f, cy + s*0.45f,
        cx - s*0.45f, cy + s*0.45f,
        color
    );
    // Left battlement
    draw_quad(
        cx - s*0.45f, cy - s*0.5f,
        cx - s*0.2f,  cy - s*0.5f,
        cx - s*0.2f,  cy - s*0.2f,
        cx - s*0.45f, cy - s*0.2f,
        color
    );
    // Center battlement
    draw_quad(
        cx - s*0.12f, cy - s*0.6f,
        cx + s*0.12f, cy - s*0.6f,
        cx + s*0.12f, cy - s*0.2f,
        cx - s*0.12f, cy - s*0.2f,
        color
    );
    // Right battlement
    draw_quad(
        cx + s*0.2f,  cy - s*0.5f,
        cx + s*0.45f, cy - s*0.5f,
        cx + s*0.45f, cy - s*0.2f,
        cx + s*0.2f,  cy - s*0.2f,
        color
    );
    // Door
    Color darker = {color.r * 0.5f, color.g * 0.5f, color.b * 0.5f, 1.0f};
    draw_quad(
        cx - s*0.1f, cy + s*0.1f,
        cx + s*0.1f, cy + s*0.1f,
        cx + s*0.1f, cy + s*0.45f,
        cx - s*0.1f, cy + s*0.45f,
        darker
    );
}

void Renderer::draw_cannon_shape(float cx, float cy, float size, const Color& color) {
    // Cannon: rectangle base with a barrel
    float s = size;
    // Base platform
    draw_quad(
        cx - s*0.35f, cy + s*0.1f,
        cx + s*0.35f, cy + s*0.1f,
        cx + s*0.35f, cy + s*0.45f,
        cx - s*0.35f, cy + s*0.45f,
        color
    );
    // Barrel (protruding upward-right)
    draw_quad(
        cx - s*0.08f, cy - s*0.5f,
        cx + s*0.15f, cy - s*0.5f,
        cx + s*0.25f, cy + s*0.15f,
        cx + s*0.02f, cy + s*0.15f,
        color
    );
    // Wheels
    Color darker = {color.r * 0.5f, color.g * 0.5f, color.b * 0.5f, 1.0f};
    draw_circle(cx - s*0.2f, cy + s*0.35f, s*0.1f, darker);
    draw_circle(cx + s*0.2f, cy + s*0.35f, s*0.1f, darker);
}

void Renderer::draw_commander_shape(float cx, float cy, float size, const Color& color) {
    // Commander: tall diamond with a ring around it
    float s = size;
    // Diamond body
    draw_triangle(
        cx,        cy - s*0.65f,   // top
        cx - s*0.35f, cy,          // left
        cx + s*0.35f, cy,          // right
        color
    );
    draw_triangle(
        cx,        cy + s*0.65f,   // bottom
        cx - s*0.35f, cy,          // left
        cx + s*0.35f, cy,          // right
        color
    );
    // Ring around diamond
    Color gold_c = Color::gold();
    draw_ring(cx, cy, s * 0.5f, gold_c, 2.0f);
    // Star dot in center
    draw_circle(cx, cy, s * 0.1f, gold_c);
}

void Renderer::draw_hq_shape(float cx, float cy, float size, const Color& color) {
    // HQ: large hexagonal shape, prominent
    float s = size;
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_POLYGON);
    for (int i = 0; i < 6; i++) {
        float angle = (60.0f * i - 90.0f) * PI / 180.0f;
        glVertex2f(cx + s * 0.65f * cosf(angle), cy + s * 0.65f * sinf(angle));
    }
    glEnd();
    // Inner hexagon outline
    Color lighter = {color.r + 0.2f, color.g + 0.2f, color.b + 0.2f, 1.0f};
    if (lighter.r > 1.0f) lighter.r = 1.0f;
    if (lighter.g > 1.0f) lighter.g = 1.0f;
    if (lighter.b > 1.0f) lighter.b = 1.0f;
    glLineWidth(2.0f);
    glColor4f(lighter.r, lighter.g, lighter.b, lighter.a);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 6; i++) {
        float angle = (60.0f * i - 90.0f) * PI / 180.0f;
        glVertex2f(cx + s * 0.45f * cosf(angle), cy + s * 0.45f * sinf(angle));
    }
    glEnd();
    // Cross/plus in center
    draw_line(cx - s*0.2f, cy, cx + s*0.2f, cy, lighter, 2.0f);
    draw_line(cx, cy - s*0.2f, cx, cy + s*0.2f, lighter, 2.0f);
}

// ============================================================================
// UNIT DRAWING FUNCTIONS
// ============================================================================

static Color player_color(Player p) {
    return (p == Player::P1) ? Color::p1() : Color::p2();
}

static Color player_light_color(Player p) {
    return (p == Player::P1) ? Color::p1_light() : Color::p2_light();
}

void Renderer::draw_scout(Hex h, float hex_size, Player owner, int hp, int max_hp,
                           bool selected, bool fortified) {
    float cx, cy;
    hex_center(h, hex_size, cx, cy);
    float us = hex_size * 0.5f; // unit scale

    if (selected) {
        draw_ring(cx, cy, hex_size * 0.85f, Color::gold(), 3.0f, 32);
    }
    if (fortified) {
        draw_ring(cx, cy, hex_size * 0.75f, Color::green(), 2.0f, 24);
    }

    draw_arrow_shape(cx, cy, us, player_color(owner));
    draw_hp_bar(cx, cy - hex_size * 0.85f, hex_size * 0.6f, hex_size * 0.08f, hp, max_hp);
}

void Renderer::draw_soldier(Hex h, float hex_size, Player owner, int hp, int max_hp,
                             bool selected, bool fortified) {
    float cx, cy;
    hex_center(h, hex_size, cx, cy);
    float us = hex_size * 0.5f;

    if (selected) {
        draw_ring(cx, cy, hex_size * 0.85f, Color::gold(), 3.0f, 32);
    }
    if (fortified) {
        draw_ring(cx, cy, hex_size * 0.75f, Color::green(), 2.0f, 24);
    }

    draw_shield_shape(cx, cy, us, player_color(owner));
    draw_hp_bar(cx, cy - hex_size * 0.85f, hex_size * 0.6f, hex_size * 0.08f, hp, max_hp);
}

void Renderer::draw_fortress(Hex h, float hex_size, Player owner, int hp, int max_hp,
                              bool selected, bool fortified) {
    float cx, cy;
    hex_center(h, hex_size, cx, cy);
    float us = hex_size * 0.5f;

    if (selected) {
        draw_ring(cx, cy, hex_size * 0.85f, Color::gold(), 3.0f, 32);
    }
    if (fortified) {
        draw_ring(cx, cy, hex_size * 0.75f, Color::green(), 2.0f, 24);
    }

    Color c = player_color(owner);
    Color darker = {c.r * 0.7f, c.g * 0.7f, c.b * 0.7f, 1.0f};
    draw_fortress_shape(cx, cy, us, darker);
    draw_hp_bar(cx, cy - hex_size * 0.85f, hex_size * 0.6f, hex_size * 0.08f, hp, max_hp);
}

void Renderer::draw_cannon(Hex h, float hex_size, Player owner, int hp, int max_hp,
                            bool selected, bool fortified) {
    float cx, cy;
    hex_center(h, hex_size, cx, cy);
    float us = hex_size * 0.5f;

    if (selected) {
        draw_ring(cx, cy, hex_size * 0.85f, Color::gold(), 3.0f, 32);
    }
    if (fortified) {
        draw_ring(cx, cy, hex_size * 0.75f, Color::green(), 2.0f, 24);
    }

    draw_cannon_shape(cx, cy, us, player_color(owner));
    draw_hp_bar(cx, cy - hex_size * 0.85f, hex_size * 0.6f, hex_size * 0.08f, hp, max_hp);
}

void Renderer::draw_commander(Hex h, float hex_size, Player owner, int hp, int max_hp,
                               bool selected, bool fortified) {
    float cx, cy;
    hex_center(h, hex_size, cx, cy);
    float us = hex_size * 0.5f;

    if (selected) {
        draw_ring(cx, cy, hex_size * 0.85f, Color::gold(), 3.0f, 32);
    }
    if (fortified) {
        draw_ring(cx, cy, hex_size * 0.75f, Color::green(), 2.0f, 24);
    }

    draw_commander_shape(cx, cy, us, player_color(owner));
    draw_hp_bar(cx, cy - hex_size * 0.85f, hex_size * 0.6f, hex_size * 0.08f, hp, max_hp);
}

void Renderer::draw_hq(Hex h, float hex_size, Player owner, int hp, int max_hp,
                        bool selected) {
    float cx, cy;
    hex_center(h, hex_size, cx, cy);
    float us = hex_size * 0.5f;

    if (selected) {
        draw_ring(cx, cy, hex_size * 0.9f, Color::gold(), 3.0f, 32);
    }

    draw_hq_shape(cx, cy, us, player_color(owner));
    draw_hp_bar(cx, cy - hex_size * 0.9f, hex_size * 0.7f, hex_size * 0.1f, hp, max_hp);
}

void Renderer::draw_unit(Hex h, float hex_size, UnitType type, Player owner,
                          int hp, int max_hp, bool selected, bool fortified) {
    switch (type) {
    case UnitType::SCOUT:     draw_scout(h, hex_size, owner, hp, max_hp, selected, fortified); break;
    case UnitType::SOLDIER:   draw_soldier(h, hex_size, owner, hp, max_hp, selected, fortified); break;
    case UnitType::FORTRESS:  draw_fortress(h, hex_size, owner, hp, max_hp, selected, fortified); break;
    case UnitType::CANNON:    draw_cannon(h, hex_size, owner, hp, max_hp, selected, fortified); break;
    case UnitType::COMMANDER: draw_commander(h, hex_size, owner, hp, max_hp, selected, fortified); break;
    case UnitType::HQ:        draw_hq(h, hex_size, owner, hp, max_hp, selected); break;
    default: break;
    }
}

// ============================================================================
// VICTORY NODE CRYSTAL
// ============================================================================

void Renderer::draw_victory_node(Hex h, float hex_size, float time) {
    float cx, cy;
    hex_center(h, hex_size, cx, cy);
    float s = hex_size * 0.35f;

    // Pulsing size
    float pulse = 1.0f + 0.15f * sinf(time * 3.0f);
    s *= pulse;

    // Rotation angle
    float rot = time * 1.5f;

    // Diamond shape (rotated square)
    float cos_r = cosf(rot) * s;
    float sin_r = sinf(rot) * s;

    // Glow (larger, semi-transparent)
    Color glow = {1.0f, 0.9f, 0.3f, 0.3f};
    float gs = s * 1.4f;
    float gcos = cosf(rot) * gs;
    float gsin = sinf(rot) * gs;
    draw_quad(
        cx + gcos, cy,
        cx, cy + gsin,
        cx - gcos, cy,
        cx, cy - gsin,
        glow
    );

    // Main crystal
    Color crystal = {1.0f, 0.85f, 0.0f, 0.9f};
    draw_quad(
        cx + cos_r, cy,
        cx, cy + sin_r,
        cx - cos_r, cy,
        cx, cy - sin_r,
        crystal
    );

    // Bright center dot
    Color bright = {1.0f, 1.0f, 0.8f, 1.0f};
    draw_circle(cx, cy, s * 0.2f, bright);
}

// ============================================================================
// TERRITORY BORDERS
// ============================================================================

void Renderer::draw_territory_border(Hex h, float hex_size, Player owner) {
    if (owner == Player::NONE) return;

    Color c = player_color(owner);
    // For each edge, check if the neighbor has a different owner or no owner
    // Edge i (vertex i to vertex (i+1)%6) corresponds to neighbor direction
    static const int edge_to_dir[6] = {1, 0, 5, 4, 3, 2};

    float vx[6], vy[6];
    for (int i = 0; i < 6; i++) {
        hex_vertex(h, hex_size, i, vx[i], vy[i]);
    }

    glLineWidth(3.0f);
    glColor4f(c.r, c.g, c.b, c.a);
    glBegin(GL_LINES);
    for (int i = 0; i < 6; i++) {
        int dir = edge_to_dir[i];
        Hex neighbor = hex_neighbor(h, dir);
        // Check if neighbor is on the board and has a different owner
        bool is_border = false;
        if (!neighbor.valid()) {
            is_border = true;
        } else {
            // Without access to GameState we cannot check neighbor ownership.
            // Draw all edges where the neighbor exists on the board.
            // The render_game() method handles territory borders properly
            // with full neighbor-owner checking.
            if (neighbor.valid()) {
                is_border = true;
            }
        }
        if (is_border) {
            int j = (i + 1) % 6;
            glVertex2f(vx[i], vy[i]);
            glVertex2f(vx[j], vy[j]);
        }
    }
    glEnd();
}

// ============================================================================
// HP BAR
// ============================================================================

void Renderer::draw_hp_bar(float cx, float cy, float w, float h, int hp, int max_hp) {
    if (max_hp <= 0) return;

    float ratio = (float)hp / (float)max_hp;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    float left = cx - w * 0.5f;
    float top = cy - h * 0.5f;

    // Background (dark red)
    draw_rect(left, top, w, h, {0.3f, 0.0f, 0.0f, 0.8f});

    // HP fill (green -> yellow -> red gradient based on ratio)
    Color fill_color;
    if (ratio > 0.5f) {
        float t = (ratio - 0.5f) * 2.0f;
        fill_color = {1.0f - t * 0.8f, 0.8f, 0.1f, 1.0f};
    } else {
        float t = ratio * 2.0f;
        fill_color = {0.9f, t * 0.8f, 0.1f, 1.0f};
    }
    draw_rect(left, top, w * ratio, h, fill_color);

    // Border
    draw_rect_outline(left, top, w, h, {0.0f, 0.0f, 0.0f, 0.6f}, 1.0f);
}

// ============================================================================
// INFLUENCE OVERLAY
// ============================================================================

void Renderer::draw_influence_overlay(Hex h, float hex_size, int p1_inf, int p2_inf) {
    if (p1_inf == 0 && p2_inf == 0) return;

    Color overlay;
    if (p1_inf > p2_inf) {
        float strength = (float)(p1_inf - p2_inf) * 0.1f;
        if (strength > 0.5f) strength = 0.5f;
        overlay = {0.2f, 0.5f, 1.0f, strength};
    } else if (p2_inf > p1_inf) {
        float strength = (float)(p2_inf - p1_inf) * 0.1f;
        if (strength > 0.5f) strength = 0.5f;
        overlay = {0.9f, 0.2f, 0.2f, strength};
    } else {
        overlay = {0.5f, 0.5f, 0.5f, 0.1f};
    }

    draw_filled_hex(h, hex_size * 0.95f, overlay);
}

// ============================================================================
// MOVE HIGHLIGHTS
// ============================================================================

void Renderer::draw_move_highlight(Hex h, float hex_size) {
    draw_filled_hex(h, hex_size * 0.9f, {0.2f, 0.8f, 0.3f, 0.35f});
    draw_hex_outline(h, hex_size * 0.9f, {0.2f, 0.8f, 0.3f, 0.7f}, 2.0f);
}

void Renderer::draw_attack_highlight(Hex h, float hex_size) {
    draw_filled_hex(h, hex_size * 0.9f, {0.9f, 0.2f, 0.2f, 0.35f});
    draw_hex_outline(h, hex_size * 0.9f, {0.9f, 0.2f, 0.2f, 0.7f}, 2.0f);
}

void Renderer::draw_capture_highlight(Hex h, float hex_size) {
    draw_filled_hex(h, hex_size * 0.9f, {1.0f, 0.85f, 0.0f, 0.35f});
    draw_hex_outline(h, hex_size * 0.9f, {1.0f, 0.85f, 0.0f, 0.7f}, 2.0f);
}

// ============================================================================
// FONT RENDERING
// ============================================================================

void Renderer::draw_char(char c, float x, float y, float size, const Color& color) {
    int idx = font_index(c);
    if (idx < 0) return; // unsupported character

    float pw = size / 7.0f;  // pixel width
    float ph = size / 7.0f;  // pixel height

    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    for (int row = 0; row < 7; row++) {
        uint8_t bits = FONT_DATA[idx][row];
        for (int col = 0; col < 5; col++) {
            if (bits & (0x10 >> col)) {
                float px = x + col * pw;
                float py = y + row * ph;
                glVertex2f(px,       py);
                glVertex2f(px + pw,  py);
                glVertex2f(px + pw,  py + ph);
                glVertex2f(px,       py + ph);
            }
        }
    }
    glEnd();
}

void Renderer::draw_text(const char* text, float x, float y, float size, const Color& color) {
    if (!text) return;
    float char_w = size * 5.0f / 7.0f;  // width of one character
    float spacing = char_w * 1.25f;       // spacing between characters
    float cursor = x;
    for (const char* p = text; *p; p++) {
        char c = *p;
        // Convert lowercase to uppercase for our bitmap font
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        draw_char(c, cursor, y, size, color);
        cursor += spacing;
    }
}

// ============================================================================
// SCREEN-SPACE UI RECTANGLES
// ============================================================================

void Renderer::draw_rect(float x, float y, float w, float h, const Color& color) {
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    glVertex2f(x,     y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x,     y + h);
    glEnd();
}

void Renderer::draw_rect_outline(float x, float y, float w, float h,
                                  const Color& color, float width) {
    glLineWidth(width);
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x,     y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x,     y + h);
    glEnd();
}

// ============================================================================
// COORDINATE CONVERSION
// ============================================================================

void Renderer::world_to_screen(float wx, float wy, float& sx, float& sy) {
    sx = (wx - cam_x_) * cam_zoom_ + width_ * 0.5f;
    sy = (wy - cam_y_) * cam_zoom_ + height_ * 0.5f;
}

void Renderer::screen_to_world(float sx, float sy, float& wx, float& wy) {
    wx = (sx - width_ * 0.5f) / cam_zoom_ + cam_x_;
    wy = (sy - height_ * 0.5f) / cam_zoom_ + cam_y_;
}

Hex Renderer::screen_to_hex(float sx, float sy, float hex_size) {
    float wx, wy;
    screen_to_world(sx, sy, wx, wy);
    return pixel_to_hex(wx, wy, hex_size);
}

// ============================================================================
// WINDOW QUERIES
// ============================================================================

int Renderer::window_width() const  { return width_; }
int Renderer::window_height() const { return height_; }
bool Renderer::should_close() const { return should_close_; }

// ============================================================================
// FULL GAME BOARD RENDER
// ============================================================================

void Renderer::render_game(const GameState& state, float hex_size, float time,
                            int selected_unit, const Move* valid_moves, int move_count) {
    // Push modelview for camera
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();

    // Apply camera transform
    glLoadIdentity();
    glTranslatef((float)width_ * 0.5f, (float)height_ * 0.5f, 0.0f);
    glScalef(cam_zoom_, cam_zoom_, 1.0f);
    glTranslatef(-cam_x_, -cam_y_, 0.0f);

    // --- 1. Draw all hex tiles (colored by terrain) ---
    for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        if (!state.tile_valid[i]) continue;
        Hex h = Hex::from_index(i);
        const Tile& tile = state.tiles[i];

        Color fill;
        switch (tile.terrain) {
        case Terrain::PLAINS:      fill = {0.25f, 0.50f, 0.22f, 1.0f}; break;
        case Terrain::MOUNTAIN:    fill = Color::mountain(); break;
        case Terrain::WATER:       fill = Color::water(); break;
        case Terrain::HIGH_GROUND: fill = Color::high(); break;
        case Terrain::VICTORY:     fill = {0.25f, 0.50f, 0.22f, 1.0f}; break; // drawn separately
        default:                   fill = {0.25f, 0.50f, 0.22f, 1.0f}; break;
        }

        // Slightly tint based on owner
        if (tile.owner == Player::P1) {
            fill.r = fill.r * 0.7f + 0.2f * 0.7f;
            fill.g = fill.g * 0.7f + 0.5f * 0.7f;
            fill.b = fill.b * 0.7f + 1.0f * 0.7f;
        } else if (tile.owner == Player::P2) {
            fill.r = fill.r * 0.7f + 0.9f * 0.7f;
            fill.g = fill.g * 0.7f + 0.2f * 0.7f;
            fill.b = fill.b * 0.7f + 0.2f * 0.7f;
        }

        // Clamp
        if (fill.r > 1.0f) fill.r = 1.0f;
        if (fill.g > 1.0f) fill.g = 1.0f;
        if (fill.b > 1.0f) fill.b = 1.0f;

        Color border = {fill.r * 0.6f, fill.g * 0.6f, fill.b * 0.6f, 1.0f};
        draw_hex(h, hex_size * 0.97f, fill, border, 1.5f);
    }

    // --- 2. Draw territory borders ---
    // Edge-to-neighbor direction mapping for pointy-top hexes
    static const int edge_to_dir[6] = {1, 0, 5, 4, 3, 2};

    for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        if (!state.tile_valid[i]) continue;
        Hex h = Hex::from_index(i);
        const Tile& tile = state.tiles[i];
        if (tile.owner == Player::NONE) continue;

        Color c = player_color(tile.owner);
        float vx[6], vy[6];
        for (int e = 0; e < 6; e++) {
            hex_vertex(h, hex_size * 0.97f, e, vx[e], vy[e]);
        }

        glLineWidth(3.0f);
        glColor4f(c.r, c.g, c.b, c.a);
        glBegin(GL_LINES);
        for (int e = 0; e < 6; e++) {
            int dir = edge_to_dir[e];
            Hex neighbor = hex_neighbor(h, dir);
            bool is_border = false;
            if (!neighbor.valid()) {
                is_border = true;
            } else {
                int nidx = neighbor.index();
                if (nidx >= 0 && nidx < BOARD_SIZE * BOARD_SIZE) {
                    if (state.tiles[nidx].owner != tile.owner) {
                        is_border = true;
                    }
                } else {
                    is_border = true;
                }
            }
            if (is_border) {
                int j = (e + 1) % 6;
                glVertex2f(vx[e], vy[e]);
                glVertex2f(vx[j], vy[j]);
            }
        }
        glEnd();
    }

    // --- 3. Draw influence overlays ---
    for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        if (!state.tile_valid[i]) continue;
        Hex h = Hex::from_index(i);
        const Tile& tile = state.tiles[i];
        if (tile.p1_influence > 0 || tile.p2_influence > 0) {
            draw_influence_overlay(h, hex_size, tile.p1_influence, tile.p2_influence);
        }
    }

    // --- 4. Draw move/attack/capture highlights ---
    if (valid_moves && move_count > 0) {
        for (int m = 0; m < move_count; m++) {
            Hex target = valid_moves[m].to;
            switch (valid_moves[m].action) {
            case ActionType::MOVE:
                draw_move_highlight(target, hex_size);
                break;
            case ActionType::ATTACK:
                draw_attack_highlight(target, hex_size);
                break;
            case ActionType::CAPTURE:
                draw_capture_highlight(target, hex_size);
                break;
            default:
                break;
            }
        }
    }

    // --- 5. Draw victory nodes (animated) ---
    for (int v = 0; v < VICTORY_NODES; v++) {
        draw_victory_node(state.victory_hexes[v], hex_size, time);
    }

    // --- 6. Draw all units ---
    for (int u = 0; u < MAX_UNITS * 2; u++) {
        const Unit& unit = state.units[u];
        if (unit.type == UnitType::NONE) continue;

        bool is_selected = (selected_unit >= 0 && u == selected_unit);
        draw_unit(unit.pos, hex_size, unit.type, unit.owner,
                  unit.hp, unit.max_hp, is_selected, unit.fortified);
    }

    // Pop modelview
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

// ============================================================================
// UI RENDERING
// ============================================================================

void Renderer::render_ui(const GameState& state, int selected_unit, const char* status_text) {
    // Switch to screen-space (identity modelview)
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    float font_size = 14.0f;
    float small_font = 11.0f;
    float line_h = font_size * 1.5f;

    // --- Top bar ---
    float top_bar_h = 40.0f;
    draw_rect(0, 0, (float)width_, top_bar_h, {0.1f, 0.1f, 0.12f, 0.9f});

    // Turn number
    char buf[128];
    snprintf(buf, sizeof(buf), "TURN %d", state.turn);
    draw_text(buf, 10, 10, font_size, Color::white());

    // Current player
    const char* phase_str = "";
    Color phase_color = Color::white();
    switch (state.phase) {
    case GamePhase::DRAFT_P1:    phase_str = "DRAFT - P1"; phase_color = Color::p1(); break;
    case GamePhase::DRAFT_P2:    phase_str = "DRAFT - P2"; phase_color = Color::p2(); break;
    case GamePhase::RESEARCH_P1: phase_str = "RESEARCH - P1"; phase_color = Color::p1(); break;
    case GamePhase::RESEARCH_P2: phase_str = "RESEARCH - P2"; phase_color = Color::p2(); break;
    case GamePhase::ACTION_P1:   phase_str = "ACTION - P1"; phase_color = Color::p1(); break;
    case GamePhase::ACTION_P2:   phase_str = "ACTION - P2"; phase_color = Color::p2(); break;
    case GamePhase::GAME_OVER:   phase_str = "GAME OVER"; phase_color = Color::gold(); break;
    }

    float phase_x = 140.0f;
    draw_text(phase_str, phase_x, 10, font_size, phase_color);

    // Player 1 info
    float p1_x = 380.0f;
    draw_text("P1", p1_x, 5, font_size, Color::p1());
    snprintf(buf, sizeof(buf), "E:%d  N:%d/%d",
             state.energy[1], state.nodes_controlled[1], VICTORY_NEEDED);
    draw_text(buf, p1_x, 22, small_font, Color::p1());

    // Player 2 info
    float p2_x = 550.0f;
    draw_text("P2", p2_x, 5, font_size, Color::p2());
    snprintf(buf, sizeof(buf), "E:%d  N:%d/%d",
             state.energy[2], state.nodes_controlled[2], VICTORY_NEEDED);
    draw_text(buf, p2_x, 22, small_font, Color::p2());

    // --- Side panel (selected unit info) ---
    if (selected_unit >= 0 && selected_unit < MAX_UNITS * 2) {
        const Unit& unit = state.units[selected_unit];
        if (unit.type != UnitType::NONE) {
            float panel_w = 200.0f;
            float panel_h = 220.0f;
            float panel_x = (float)width_ - panel_w - 10;
            float panel_y = top_bar_h + 10;

            draw_rect(panel_x, panel_y, panel_w, panel_h, {0.08f, 0.08f, 0.1f, 0.9f});
            draw_rect_outline(panel_x, panel_y, panel_w, panel_h,
                              player_color(unit.owner), 2.0f);

            float tx = panel_x + 10;
            float ty = panel_y + 10;

            // Unit name
            draw_text(unit_type_name(unit.type), tx, ty, font_size,
                      player_color(unit.owner));
            ty += line_h;

            // Owner
            draw_text(unit.owner == Player::P1 ? "PLAYER 1" : "PLAYER 2",
                      tx, ty, small_font, player_color(unit.owner));
            ty += line_h * 0.8f;

            // HP
            snprintf(buf, sizeof(buf), "HP: %d/%d", unit.hp, unit.max_hp);
            draw_text(buf, tx, ty, small_font, Color::white());
            ty += line_h * 0.8f;

            // AP
            snprintf(buf, sizeof(buf), "AP: %d", unit.ap);
            draw_text(buf, tx, ty, small_font, Color::white());
            ty += line_h * 0.8f;

            // Stats (with tech modifiers)
            ModifiedStats ms = get_modified_stats(unit.type, unit.owner, state);
            snprintf(buf, sizeof(buf), "ATK: %d  DEF: %d", ms.attack, ms.defense);
            draw_text(buf, tx, ty, small_font, Color::white());
            ty += line_h * 0.8f;

            snprintf(buf, sizeof(buf), "MOV: %d  RNG: %d", ms.move, ms.range);
            draw_text(buf, tx, ty, small_font, Color::white());
            ty += line_h * 0.8f;

            snprintf(buf, sizeof(buf), "INFL: %d", ms.influence);
            draw_text(buf, tx, ty, small_font, Color::white());
            ty += line_h * 0.8f;

            // Status effects
            if (unit.fortified) {
                draw_text("FORTIFIED", tx, ty, small_font, Color::green());
                ty += line_h * 0.8f;
            }
            if (game_is_isolated(state, selected_unit)) {
                draw_text("ISOLATED", tx, ty, small_font, {0.9f, 0.5f, 0.1f, 1.0f});
                ty += line_h * 0.8f;
            }

            // Formation
            FormationType form = game_check_formation(state, selected_unit);
            if (form == FormationType::LINE) {
                draw_text("LINE FORM", tx, ty, small_font, Color::green());
            } else if (form == FormationType::TRIANGLE) {
                draw_text("TRI FORM", tx, ty, small_font, Color::gold());
            }

            // Position
            ty += line_h * 0.8f;
            snprintf(buf, sizeof(buf), "POS: %d,%d", unit.pos.q, unit.pos.r);
            draw_text(buf, tx, ty, small_font, Color::gray());
        }
    }

    // --- Bottom status bar ---
    if (status_text && status_text[0] != '\0') {
        float bar_h = 30.0f;
        float bar_y = (float)height_ - bar_h;
        draw_rect(0, bar_y, (float)width_, bar_h, {0.1f, 0.1f, 0.12f, 0.9f});
        draw_text(status_text, 10, bar_y + 8, small_font, Color::white());
    }

    // --- Victory progress bar ---
    {
        float bar_y = (float)height_ - 60.0f;
        float bar_w = 200.0f;
        float bar_h = 8.0f;
        float bar_x = (float)width_ * 0.5f - bar_w * 0.5f;

        // P1 progress
        draw_rect(bar_x, bar_y, bar_w, bar_h, {0.2f, 0.2f, 0.2f, 0.8f});
        float p1_ratio = (float)state.nodes_controlled[1] / (float)VICTORY_NEEDED;
        if (p1_ratio > 1.0f) p1_ratio = 1.0f;
        draw_rect(bar_x, bar_y, bar_w * p1_ratio, bar_h, Color::p1());

        // P2 progress
        bar_y += bar_h + 2;
        draw_rect(bar_x, bar_y, bar_w, bar_h, {0.2f, 0.2f, 0.2f, 0.8f});
        float p2_ratio = (float)state.nodes_controlled[2] / (float)VICTORY_NEEDED;
        if (p2_ratio > 1.0f) p2_ratio = 1.0f;
        draw_rect(bar_x, bar_y, bar_w * p2_ratio, bar_h, Color::p2());
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

// ============================================================================
// DRAFT UI
// ============================================================================

void Renderer::render_draft_ui(const GameState& state, int hover_tech, Player drafting_player) {
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    float font_size = 13.0f;
    float small_font = 10.0f;
    float line_h = font_size * 1.3f;

    // Overlay background
    draw_rect(0, 0, (float)width_, (float)height_, {0.0f, 0.0f, 0.0f, 0.7f});

    // Title
    Color pcolor = player_color(drafting_player);
    const char* player_name = (drafting_player == Player::P1) ? "PLAYER 1" : "PLAYER 2";
    char buf[128];
    snprintf(buf, sizeof(buf), "%s - DRAFT TECHS", player_name);
    float title_w = 400.0f;
    draw_text(buf, (float)width_ * 0.5f - title_w * 0.5f, 20.0f, font_size, pcolor);

    // Tech grid: 4 columns x 5 rows
    int cols = 4;
    float cell_w = 220.0f;
    float cell_h = 50.0f;
    float grid_w = cols * cell_w;
    float start_x = (float)width_ * 0.5f - grid_w * 0.5f;
    float start_y = 55.0f;

    const TechUpgrade* techs = (drafting_player == Player::P1) ? state.techs_p1 : state.techs_p2;

    for (int t = 1; t <= 20; t++) {
        int col = (t - 1) % cols;
        int row = (t - 1) / cols;
        float cx = start_x + col * cell_w;
        float cy = start_y + row * cell_h;

        TechID tid = static_cast<TechID>(t);
        bool drafted = techs[t].active;
        bool hovered = (t == hover_tech);

        // Cell background
        Color bg;
        if (drafted) {
            bg = {pcolor.r * 0.3f, pcolor.g * 0.3f, pcolor.b * 0.3f, 0.9f};
        } else if (hovered) {
            bg = {0.25f, 0.25f, 0.3f, 0.9f};
        } else {
            bg = {0.15f, 0.15f, 0.18f, 0.9f};
        }
        draw_rect(cx, cy, cell_w - 4, cell_h - 4, bg);

        // Border
        Color border_c = drafted ? pcolor : (hovered ? Color::gold() : Color::gray());
        draw_rect_outline(cx, cy, cell_w - 4, cell_h - 4, border_c, drafted ? 2.0f : 1.0f);

        // Tech name
        draw_text(tech_name(tid), cx + 8, cy + 5, small_font,
                  drafted ? Color::white() : Color::gray());

        // Status
        if (drafted) {
            draw_text("DRAFTED", cx + 8, cy + 25, small_font, Color::green());
        }
    }

    // Hovered tech detail panel
    if (hover_tech >= 1 && hover_tech <= 20) {
        float detail_y = start_y + 5 * cell_h + 10;
        float detail_w = grid_w;
        float detail_h = 60.0f;
        float detail_x = start_x;

        draw_rect(detail_x, detail_y, detail_w, detail_h, {0.08f, 0.08f, 0.1f, 0.95f});
        draw_rect_outline(detail_x, detail_y, detail_w, detail_h, Color::gold(), 2.0f);

        TechID tid = static_cast<TechID>(hover_tech);
        draw_text(tech_name(tid), detail_x + 10, detail_y + 5, font_size, Color::gold());

        // Description (simplified - just show the tech name and research cost)
        int cost = 3; // default research cost
        snprintf(buf, sizeof(buf), "RESEARCH COST: %d ENERGY", cost);
        draw_text(buf, detail_x + 10, detail_y + 25, small_font, Color::white());

        const char* desc = "";
        switch (tid) {
        case TechID::SIEGE:             desc = "CANNON RANGE +2"; break;
        case TechID::RAPID_LOGISTICS:   desc = "SCOUT MOVE +1"; break;
        case TechID::ARMOR:             desc = "SOLDIER HP +2"; break;
        case TechID::FORTIFICATION:     desc = "FORTRESS DEF +1"; break;
        case TechID::COMMAND_AUTHORITY: desc = "COMMANDER RADIUS +1"; break;
        case TechID::ARTILLERY:         desc = "CANNON ATK +1"; break;
        case TechID::MOBILIZATION:      desc = "SOLDIER MOVE +1"; break;
        case TechID::SCOUT_ARMOR:       desc = "SCOUT HP +1"; break;
        case TechID::HEAVY_FORTRESSES:  desc = "FORTRESS HP +2"; break;
        case TechID::WAR_COLLEGE:       desc = "ALL UNITS ATK +1"; break;
        case TechID::EXTENDED_RANGE:    desc = "CANNON RANGE +1"; break;
        case TechID::SCOUT_TACTICS:     desc = "SCOUT ATK +1"; break;
        case TechID::SHIELD_WALL:       desc = "FORMATION DEF +1"; break;
        case TechID::SPEAR_FORMATION:   desc = "FORMATION ATK +1"; break;
        case TechID::LOGISTICS_NETWORK: desc = "ISOLATED PENALTY -1"; break;
        case TechID::ENERGY_GRID:       desc = "+1 ENERGY PER NODE"; break;
        case TechID::RAPID_DEPLOY:      desc = "UNITS +1 AP FIRST TURN"; break;
        case TechID::FORTIFIED_POS:     desc = "HIGH GROUND DEF +2"; break;
        case TechID::OVERWATCH:         desc = "FORTRESS GAINS ATK 1"; break;
        case TechID::DEEP_STRIKE:       desc = "COMMANDER INFLUENCE +1"; break;
        default: break;
        }
        draw_text(desc, detail_x + 10, detail_y + 40, small_font,
                  {0.7f, 0.9f, 1.0f, 1.0f});
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

// ============================================================================
// TECH PANEL
// ============================================================================

void Renderer::render_tech_panel(const GameState& state, Player player, float x, float y) {
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    float font_size = 11.0f;
    float small_font = 9.0f;
    float line_h = font_size * 1.2f;

    Color pcolor = player_color(player);
    const TechUpgrade* techs = (player == Player::P1) ? state.techs_p1 : state.techs_p2;
    const char* player_name = (player == Player::P1) ? "P1 TECHS" : "P2 TECHS";

    float panel_w = 180.0f;
    float panel_h = 400.0f;

    draw_rect(x, y, panel_w, panel_h, {0.08f, 0.08f, 0.1f, 0.9f});
    draw_rect_outline(x, y, panel_w, panel_h, pcolor, 1.5f);

    float tx = x + 5;
    float ty = y + 5;

    draw_text(player_name, tx, ty, font_size, pcolor);
    ty += line_h + 2;

    for (int t = 1; t <= 20; t++) {
        if (!techs[t].active) continue; // not drafted

        TechID tid = static_cast<TechID>(t);
        if (techs[t].researched) {
            draw_text(tech_name(tid), tx, ty, small_font, Color::green());
        } else {
            draw_text(tech_name(tid), tx, ty, small_font, Color::gray());
        }
        ty += line_h;

        if (ty > y + panel_h - 10) break; // overflow protection
    }

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}
