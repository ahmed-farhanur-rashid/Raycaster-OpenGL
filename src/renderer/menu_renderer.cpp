/*
 * menu_renderer.cpp — Main menu and pause menu rendering
 *
 * Uses simple solid-colour rectangles and a tiny built-in bitmap font.
 * No external font files needed.
 */

#include <glad/glad.h>
#include "menu_renderer.h"
#include "shader.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static int scrW, scrH;
static unsigned int menuProg, menuVao, menuVbo;
static unsigned int fontTex;
static int uUseSolid = -1, uSolidCol = -1;

/* ===== shaders ===== */

static const char* mvsrc = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
out vec2 uv;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    uv = aUV;
}
)";

static const char* mfsrc = R"(
#version 330 core
in  vec2 uv;
out vec4 fragColor;
uniform sampler2D fontAtlas;
uniform bool useSolidColor;
uniform vec4 solidColor;
void main() {
    if (useSolidColor) { fragColor = solidColor; return; }
    vec4 c = texture(fontAtlas, uv);
    if (c.a < 0.1) discard;
    fragColor = c;
}
)";

/*
 * ===== Bitmap Font =====
 *
 * Each character is drawn on a tiny 5-wide x 7-tall pixel grid.
 * Instead of loading a font file, we store each character as a small
 * picture made of '#' (on) and '.' (off) pixels, written as a string.
 *
 * All 64 printable ASCII characters from ' ' (32) to '_' (95) are
 * defined below.  Lowercase letters are mapped to uppercase when drawn.
 *
 * These character pictures are packed into one big image (a "texture
 * atlas") arranged in a grid of 16 columns x 4 rows.  The GPU can then
 * look up any character by its grid position.
 */

static const int GLYPH_W     = 5;
static const int GLYPH_H     = 7;
static const int GLYPH_COUNT = 64;   /* ASCII 32 (' ') through 95 ('_') */
static const int ATLAS_COLS  = 16;
static const int ATLAS_ROWS  = 4;
static const int ATLAS_PX_W  = ATLAS_COLS * GLYPH_W;   /* 80 */
static const int ATLAS_PX_H  = ATLAS_ROWS * GLYPH_H;   /* 28 */

/*
 * Each string is one row of a character (5 chars long).
 * '#' = pixel on, '.' = pixel off.
 *
 * Example – the letter 'A':
 *   ".###."      top row
 *   "#...#"
 *   "#...#"
 *   "#####"      middle bar
 *   "#...#"
 *   "#...#"
 *   "#...#"      bottom row
 */
static const char* glyphBitmaps[GLYPH_COUNT][GLYPH_H] = {
    /* 32 ' ' */ {".....", ".....", ".....", ".....", ".....", ".....", "....."},
    /* 33 '!' */ {"..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."},
    /* 34 '"' */ {".#.#.", ".#.#.", ".....", ".....", ".....", ".....", "....."},
    /* 35 '#' */ {".#.#.", "#####", ".#.#.", ".#.#.", "#####", ".#.#.", "....."},
    /* 36 '$' */ {"..#..", "####.", "#.#..", ".###.", "..#.#", ".####", "..#.."},
    /* 37 '%' */ {"##...", "##..#", "...#.", "..#..", ".#...", "#.##.", "...##"},
    /* 38 '&' */ {".#...", "#.#..", "#.#..", ".#...", "#.#.#", "#..#.", ".##.#"},
    /* 39 ''' */ {"..#..", "..#..", ".....", ".....", ".....", ".....", "....."},
    /* 40 '(' */ {"..#..", ".#...", "#....", "#....", "#....", ".#...", "..#.."},
    /* 41 ')' */ {".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#..."},
    /* 42 '*' */ {".....", "..#..", "#.#.#", ".###.", "#.#.#", "..#..", "....."},
    /* 43 '+' */ {".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."},
    /* 44 ',' */ {".....", ".....", ".....", ".....", ".....", "..#..", ".#..."},
    /* 45 '-' */ {".....", ".....", ".....", "#####", ".....", ".....", "....."},
    /* 46 '.' */ {".....", ".....", ".....", ".....", ".....", ".....", "..#.."},
    /* 47 '/' */ {".....", "....#", "...#.", "..#..", ".#...", "#....", "....."},
    /* 48 '0' */ {".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."},
    /* 49 '1' */ {"..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."},
    /* 50 '2' */ {".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"},
    /* 51 '3' */ {".###.", "#...#", "....#", "..##.", "....#", "#...#", ".###."},
    /* 52 '4' */ {"...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."},
    /* 53 '5' */ {"#####", "#....", "####.", "....#", "....#", "#...#", ".###."},
    /* 54 '6' */ {".##..", ".#...", "#....", "####.", "#...#", "#...#", ".###."},
    /* 55 '7' */ {"#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."},
    /* 56 '8' */ {".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."},
    /* 57 '9' */ {".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."},
    /* 58 ':' */ {".....", ".....", "..#..", ".....", ".....", "..#..", "....."},
    /* 59 ';' */ {".....", ".....", "..#..", ".....", ".....", "..#..", ".#..."},
    /* 60 '<' */ {"....#", "...#.", "..#..", ".#...", "..#..", "...#.", "....#"},
    /* 61 '=' */ {".....", ".....", "#####", ".....", "#####", ".....", "....."},
    /* 62 '>' */ {"#....", ".#...", "..#..", "...#.", "..#..", ".#...", "#...."},
    /* 63 '?' */ {".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."},
    /* 64 '@' */ {".###.", "#...#", "#.###", "#.#.#", "#.###", "#....", ".###."},
    /* 65 'A' */ {".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"},
    /* 66 'B' */ {"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."},
    /* 67 'C' */ {".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."},
    /* 68 'D' */ {"####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."},
    /* 69 'E' */ {"#####", "#....", "#....", "####.", "#....", "#....", "#####"},
    /* 70 'F' */ {"#####", "#....", "#....", "####.", "#....", "#....", "#...."},
    /* 71 'G' */ {".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###."},
    /* 72 'H' */ {"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"},
    /* 73 'I' */ {".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."},
    /* 74 'J' */ {".####", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."},
    /* 75 'K' */ {"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"},
    /* 76 'L' */ {"#....", "#....", "#....", "#....", "#....", "#....", "#####"},
    /* 77 'M' */ {"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"},
    /* 78 'N' */ {"#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#"},
    /* 79 'O' */ {".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."},
    /* 80 'P' */ {"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."},
    /* 81 'Q' */ {".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"},
    /* 82 'R' */ {"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"},
    /* 83 'S' */ {".###.", "#...#", "#....", ".###.", "....#", "#...#", ".###."},
    /* 84 'T' */ {"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."},
    /* 85 'U' */ {"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."},
    /* 86 'V' */ {"#...#", "#...#", "#...#", "#...#", ".#.#.", ".#.#.", "..#.."},
    /* 87 'W' */ {"#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#"},
    /* 88 'X' */ {"#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"},
    /* 89 'Y' */ {"#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."},
    /* 90 'Z' */ {"#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"},
    /* 91 '[' */ {".###.", ".#...", ".#...", ".#...", ".#...", ".#...", ".###."},
    /* 92 '\' */ {".....", "#....", ".#...", "..#..", "...#.", "....#", "....."},
    /* 93 ']' */ {".###.", "...#.", "...#.", "...#.", "...#.", "...#.", ".###."},
    /* 94 '^' */ {"..#..", ".#.#.", "#...#", ".....", ".....", ".....", "....."},
    /* 95 '_' */ {".....", ".....", ".....", ".....", ".....", ".....", "#####"},
};

/*
 * buildFontAtlas() — Converts the readable '#'/'.'' bitmaps above into a
 * GPU texture.  Each character occupies a 5x7 cell in the atlas image.
 */
static unsigned int buildFontAtlas() {
    unsigned char atlas[ATLAS_PX_H][ATLAS_PX_W][4];
    memset(atlas, 0, sizeof(atlas));

    for (int charIndex = 0; charIndex < GLYPH_COUNT; charIndex++) {
        int gridCol = charIndex % ATLAS_COLS;
        int gridRow = charIndex / ATLAS_COLS;

        for (int row = 0; row < GLYPH_H; row++) {
            const char* rowPixels = glyphBitmaps[charIndex][row];
            for (int col = 0; col < GLYPH_W; col++) {
                int atlasX = gridCol * GLYPH_W + col;
                int atlasY = gridRow * GLYPH_H + row;
                bool pixelOn = (rowPixels[col] == '#');

                atlas[atlasY][atlasX][0] = 255;   /* red   */
                atlas[atlasY][atlasX][1] = 255;   /* green */
                atlas[atlasY][atlasX][2] = 255;   /* blue  */
                atlas[atlasY][atlasX][3] = pixelOn ? 255 : 0; /* alpha */
            }
        }
    }

    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ATLAS_PX_W, ATLAS_PX_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, atlas);
    return tex;
}

/* ===== drawing helpers ===== */

/* Convert pixel coordinates to normalised device coordinates (-1..1) */
static float px2x(float px) { return 2.0f * px / scrW - 1.0f; }
static float px2y(float py) { return 1.0f - 2.0f * py / scrH; }

static void drawRect(float x0, float y0, float x1, float y1,
                     float r, float g, float b, float a) {
    glUniform1i(uUseSolid, 1);
    glUniform4f(uSolidCol, r, g, b, a);
    float v[] = {
        x0, y0, 0,0,
        x1, y0, 1,0,
        x0, y1, 0,1,
        x1, y1, 1,1,
    };
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/* Draw a string centred at pixel X = centreX, top at pixel Y = topY.
   height = pixel height of each character. */
static void drawText(const char* text, float centreX, float topY, float height,
                     float r, float g, float b) {
    glUniform1i(uUseSolid, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTex);

    int len = (int)strlen(text);
    float charW = height * ((float)GLYPH_W / GLYPH_H);
    float totalW = charW * len;
    float startX = centreX - totalW / 2.0f;

    for (int i = 0; i < len; i++) {
        /* Map ASCII to our glyph table (0-63). Unknown chars become space. */
        int glyphIndex = (unsigned char)text[i] - 32;
        if (glyphIndex < 0 || glyphIndex >= GLYPH_COUNT) glyphIndex = 0;

        /* Find this glyph's position in the atlas grid */
        int atlasCol = glyphIndex % ATLAS_COLS;
        int atlasRow = glyphIndex / ATLAS_COLS;

        /* UV coordinates: where this glyph sits in the atlas texture (0..1) */
        float u0 = (float)(atlasCol * GLYPH_W) / ATLAS_PX_W;
        float u1 = (float)(atlasCol * GLYPH_W + GLYPH_W) / ATLAS_PX_W;
        float v0 = (float)(atlasRow * GLYPH_H) / ATLAS_PX_H;
        float v1 = (float)(atlasRow * GLYPH_H + GLYPH_H) / ATLAS_PX_H;

        /* Screen position in pixels */
        float left   = startX + i * charW;
        float right  = left + charW;
        float top    = topY;
        float bottom = topY + height;

        /* Convert pixels to normalised device coordinates (-1..1) */
        float ndcL = px2x(left);
        float ndcR = px2x(right);
        float ndcT = px2y(top);
        float ndcB = px2y(bottom);

        float verts[] = {
            ndcL, ndcB, u0, v1,
            ndcR, ndcB, u1, v1,
            ndcL, ndcT, u0, v0,
            ndcR, ndcT, u1, v0,
        };
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
}

/* ===== shared menu renderer ===== */

struct MenuConfig {
    const char*  title;
    float        titleY;       /* fraction of screen height (0.0 – 1.0) */
    float        titleSize;    /* pixel height of title text */
    const char** items;
    int          itemCount;
    float        itemH;        /* pixel height of each menu item */
    float        gap;          /* pixel gap between items */
    float        startY;       /* fraction of screen height where items begin */
    const char*  hint;         /* help text at the bottom */
    float bgR, bgG, bgB, bgA; /* fullscreen overlay colour */
};

static void renderMenu(const MenuConfig& cfg, int selectedItem) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(menuProg);
    glBindVertexArray(menuVao);
    glBindBuffer(GL_ARRAY_BUFFER, menuVbo);

    /* fullscreen overlay */
    drawRect(-1, -1, 1, 1, cfg.bgR, cfg.bgG, cfg.bgB, cfg.bgA);

    /* title */
    drawText(cfg.title, (float)scrW / 2.0f, scrH * cfg.titleY,
             cfg.titleSize, 1, 1, 1);

    /* menu items */
    float baseY = scrH * cfg.startY;
    for (int i = 0; i < cfg.itemCount; i++) {
        float iy = baseY + i * (cfg.itemH + cfg.gap);

        /* highlight rectangle behind the selected item */
        if (i == selectedItem) {
            float charW  = cfg.itemH * ((float)GLYPH_W / GLYPH_H);
            float totalW = charW * (int)strlen(cfg.items[i]);
            float bx0 = (float)scrW / 2.0f - totalW / 2.0f - 10.0f;
            float bx1 = (float)scrW / 2.0f + totalW / 2.0f + 10.0f;
            drawRect(px2x(bx0), px2y(iy + cfg.itemH + 4.0f),
                     px2x(bx1), px2y(iy - 4.0f),
                     0.3f, 0.3f, 0.6f, 0.7f);
        }

        drawText(cfg.items[i], (float)scrW / 2.0f, iy, cfg.itemH, 1, 1, 1);
    }

    /* hint text */
    drawText(cfg.hint, (float)scrW / 2.0f, (float)scrH * 0.85f,
             14.0f, 0.5f, 0.5f, 0.5f);

    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

/* ===== public API ===== */

namespace menu_renderer {

void initMenu(int screenW, int screenH) {
    scrW = screenW;
    scrH = screenH;

    menuProg = shader::createShaderProgram(mvsrc, mfsrc);
    glUseProgram(menuProg);
    glUniform1i(glGetUniformLocation(menuProg, "fontAtlas"), 0);
    uUseSolid = glGetUniformLocation(menuProg, "useSolidColor");
    uSolidCol = glGetUniformLocation(menuProg, "solidColor");

    float quad[16] = {};
    glGenVertexArrays(1, &menuVao);
    glGenBuffers(1, &menuVbo);
    glBindVertexArray(menuVao);
    glBindBuffer(GL_ARRAY_BUFFER, menuVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    fontTex = buildFontAtlas();
}

void renderMainMenuSelect(int selectedItem) {
    const char* items[] = { "START", "QUIT" };
    MenuConfig cfg = {
        "RAYCASTER", 0.18f, 48.0f,
        items, 2, 32.0f, 20.0f, 0.45f,
        "UP/DOWN TO SELECT  ENTER TO CONFIRM  ESC TO QUIT",
        0.05f, 0.05f, 0.08f, 1.0f
    };
    renderMenu(cfg, selectedItem);
}

void renderPauseMenu(int selectedItem) {
    const char* items[] = { "CONTINUE", "MAIN MENU", "QUIT" };
    MenuConfig cfg = {
        "PAUSED", 0.25f, 40.0f,
        items, 3, 28.0f, 18.0f, 0.45f,
        "UP/DOWN TO SELECT  ENTER TO CONFIRM",
        0.0f, 0.0f, 0.0f, 0.6f
    };
    renderMenu(cfg, selectedItem);
}

void cleanupMenu() {
    glDeleteTextures(1, &fontTex);
    glDeleteVertexArrays(1, &menuVao);
    glDeleteBuffers(1, &menuVbo);
    glDeleteProgram(menuProg);
}

} // namespace menu_renderer
