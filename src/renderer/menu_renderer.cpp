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

/* ===== 5x7 bitmap font (ASCII 32-126) ===== */

/* Each character is 5 pixels wide, 7 pixels tall.
   Stored as 7 bytes per char, each byte's lower 5 bits = row pixels. */
static const unsigned char font5x7[][7] = {
    /* 32 ' ' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    /* 33 '!' */ {0x04,0x04,0x04,0x04,0x04,0x00,0x04},
    /* 34 '"' */ {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00},
    /* 35 '#' */ {0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0x00},
    /* 36 '$' */ {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04},
    /* 37 '%' */ {0x18,0x19,0x02,0x04,0x08,0x13,0x03},
    /* 38 '&' */ {0x08,0x14,0x14,0x08,0x15,0x12,0x0D},
    /* 39 ''' */ {0x04,0x04,0x00,0x00,0x00,0x00,0x00},
    /* 40 '(' */ {0x02,0x04,0x08,0x08,0x08,0x04,0x02},
    /* 41 ')' */ {0x08,0x04,0x02,0x02,0x02,0x04,0x08},
    /* 42 '*' */ {0x00,0x04,0x15,0x0E,0x15,0x04,0x00},
    /* 43 '+' */ {0x00,0x04,0x04,0x1F,0x04,0x04,0x00},
    /* 44 ',' */ {0x00,0x00,0x00,0x00,0x00,0x04,0x08},
    /* 45 '-' */ {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
    /* 46 '.' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x04},
    /* 47 '/' */ {0x00,0x01,0x02,0x04,0x08,0x10,0x00},
    /* 48 '0' */ {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    /* 49 '1' */ {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    /* 50 '2' */ {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
    /* 51 '3' */ {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E},
    /* 52 '4' */ {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    /* 53 '5' */ {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    /* 54 '6' */ {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
    /* 55 '7' */ {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    /* 56 '8' */ {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    /* 57 '9' */ {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    /* 58 ':' */ {0x00,0x00,0x04,0x00,0x00,0x04,0x00},
    /* 59 ';' */ {0x00,0x00,0x04,0x00,0x00,0x04,0x08},
    /* 60 '<' */ {0x01,0x02,0x04,0x08,0x04,0x02,0x01},
    /* 61 '=' */ {0x00,0x00,0x1F,0x00,0x1F,0x00,0x00},
    /* 62 '>' */ {0x10,0x08,0x04,0x02,0x04,0x08,0x10},
    /* 63 '?' */ {0x0E,0x11,0x01,0x02,0x04,0x00,0x04},
    /* 64 '@' */ {0x0E,0x11,0x17,0x15,0x17,0x10,0x0E},
    /* 65 'A' */ {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
    /* 66 'B' */ {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    /* 67 'C' */ {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
    /* 68 'D' */ {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    /* 69 'E' */ {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
    /* 70 'F' */ {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    /* 71 'G' */ {0x0E,0x11,0x10,0x17,0x11,0x11,0x0E},
    /* 72 'H' */ {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    /* 73 'I' */ {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
    /* 74 'J' */ {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
    /* 75 'K' */ {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
    /* 76 'L' */ {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    /* 77 'M' */ {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
    /* 78 'N' */ {0x11,0x11,0x19,0x15,0x13,0x11,0x11},
    /* 79 'O' */ {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    /* 80 'P' */ {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    /* 81 'Q' */ {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
    /* 82 'R' */ {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    /* 83 'S' */ {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E},
    /* 84 'T' */ {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    /* 85 'U' */ {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    /* 86 'V' */ {0x11,0x11,0x11,0x11,0x0A,0x0A,0x04},
    /* 87 'W' */ {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
    /* 88 'X' */ {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
    /* 89 'Y' */ {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
    /* 90 'Z' */ {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    /* 91 '[' */ {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E},
    /* 92 '\' */ {0x00,0x10,0x08,0x04,0x02,0x01,0x00},
    /* 93 ']' */ {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E},
    /* 94 '^' */ {0x04,0x0A,0x11,0x00,0x00,0x00,0x00},
    /* 95 '_' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x1F},
};

/* only 64 entries above (32-95). For lowercase, map to uppercase. */
#define FONT_CHARS 96
#define FONT_W 5
#define FONT_H 7
#define ATLAS_COLS 16
#define ATLAS_ROWS 6
#define ATLAS_PX_W (ATLAS_COLS * FONT_W)
#define ATLAS_PX_H (ATLAS_ROWS * FONT_H)

static unsigned int buildFontAtlas() {
    unsigned char atlas[ATLAS_PX_H][ATLAS_PX_W][4];
    memset(atlas, 0, sizeof(atlas));

    for (int c = 0; c < FONT_CHARS && c < 64; c++) {
        int col = c % ATLAS_COLS;
        int row = c / ATLAS_COLS;
        for (int y = 0; y < FONT_H; y++) {
            unsigned char bits = font5x7[c][y];
            for (int x = 0; x < FONT_W; x++) {
                int px = col * FONT_W + (FONT_W - 1 - x);
                int py = row * FONT_H + y;
                bool on = (bits >> x) & 1;
                atlas[py][px][0] = 255;
                atlas[py][px][1] = 255;
                atlas[py][px][2] = 255;
                atlas[py][px][3] = on ? 255 : 0;
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

/* draw a string at pixel coords (cx = centre X, py = top Y), scale = pixel height per glyph */
static void drawText(const char* text, float cx, float py, float scale,
                     float r, float g, float b) {
    glUniform1i(uUseSolid, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fontTex);

    int len = (int)strlen(text);
    float glyphW = scale * ((float)FONT_W / FONT_H);
    float totalW = glyphW * len;
    float startX = cx - totalW / 2.0f;

    /* tint via solidColor alpha trick — actually we just draw white glyphs.
       For coloured text, re-use solid overlay: draw glyph as white, then
       we just accept white for now. Let's modify the shader to tint. */
    /* Actually, let's just use the solid uniform as a tint multiplier.
       We'll draw with useSolidColor=0 (textured), so we get white glyphs.
       Good enough for a menu. For coloured text, we'd need a tint uniform,
       but let's keep it simple — selected items get a highlight rect behind them. */

    for (int i = 0; i < len; i++) {
        int ch = (unsigned char)text[i] - 32;
        if (ch < 0 || ch >= 64) ch = 0;  /* space for unknown */

        int col = ch % ATLAS_COLS;
        int row = ch / ATLAS_COLS;

        float u0 = (float)(col * FONT_W) / ATLAS_PX_W;
        float u1 = (float)(col * FONT_W + FONT_W) / ATLAS_PX_W;
        float v0 = (float)(row * FONT_H) / ATLAS_PX_H;
        float v1 = (float)(row * FONT_H + FONT_H) / ATLAS_PX_H;

        float px0 = startX + i * glyphW;
        float px1 = px0 + glyphW;
        float py0 = py;
        float py1 = py + scale;

        /* pixel to NDC */
        float nx0 = 2.0f * px0 / scrW - 1.0f;
        float nx1 = 2.0f * px1 / scrW - 1.0f;
        float ny0 = 1.0f - 2.0f * py0 / scrH;
        float ny1 = 1.0f - 2.0f * py1 / scrH;

        float verts[] = {
            nx0, ny1, u0, v1,
            nx1, ny1, u1, v1,
            nx0, ny0, u0, v0,
            nx1, ny0, u1, v0,
        };
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

        /* tint — use solid color overlay at low alpha for coloring */
        glUniform1i(uUseSolid, 0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
}

/* pixel helpers */
static float px2x(float px) { return 2.0f * px / scrW - 1.0f; }
static float px2y(float py) { return 1.0f - 2.0f * py / scrH; }

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
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(menuProg);
    glBindVertexArray(menuVao);
    glBindBuffer(GL_ARRAY_BUFFER, menuVbo);

    /* dark fullscreen overlay */
    drawRect(-1, -1, 1, 1, 0.05f, 0.05f, 0.08f, 1.0f);

    /* title */
    float titleY = (float)scrH * 0.18f;
    drawText("RAYCASTER", (float)scrW / 2.0f, titleY, 48.0f, 1, 1, 1);

    /* menu items */
    const char* items[] = { "START", "QUIT" };
    int count = 2;
    float itemH = 32.0f;
    float gap = 20.0f;
    float startY = (float)scrH * 0.45f;

    for (int i = 0; i < count; i++) {
        float iy = startY + i * (itemH + gap);

        /* highlight selected */
        if (i == selectedItem) {
            int len = (int)strlen(items[i]);
            float glyphW = itemH * ((float)FONT_W / FONT_H);
            float totalW = glyphW * len;
            float bx0 = (float)scrW / 2.0f - totalW / 2.0f - 10.0f;
            float bx1 = (float)scrW / 2.0f + totalW / 2.0f + 10.0f;
            drawRect(px2x(bx0), px2y(iy + itemH + 4.0f), px2x(bx1), px2y(iy - 4.0f),
                     0.3f, 0.3f, 0.6f, 0.7f);
        }

        drawText(items[i], (float)scrW / 2.0f, iy, itemH, 1, 1, 1);
    }

    /* hint */
    drawText("UP/DOWN TO SELECT  ENTER TO CONFIRM  ESC TO QUIT",
             (float)scrW / 2.0f, (float)scrH * 0.85f, 14.0f, 0.5f, 0.5f, 0.5f);

    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

void renderPauseMenu(int selectedItem) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(menuProg);
    glBindVertexArray(menuVao);
    glBindBuffer(GL_ARRAY_BUFFER, menuVbo);

    /* semi-transparent overlay */
    drawRect(-1, -1, 1, 1, 0.0f, 0.0f, 0.0f, 0.6f);

    /* title */
    float titleY = (float)scrH * 0.25f;
    drawText("PAUSED", (float)scrW / 2.0f, titleY, 40.0f, 1, 1, 1);

    /* menu items */
    const char* items[] = { "CONTINUE", "MAIN MENU", "QUIT" };
    int count = 3;
    float itemH = 28.0f;
    float gap = 18.0f;
    float startY = (float)scrH * 0.45f;

    for (int i = 0; i < count; i++) {
        float iy = startY + i * (itemH + gap);

        if (i == selectedItem) {
            int len = (int)strlen(items[i]);
            float glyphW = itemH * ((float)FONT_W / FONT_H);
            float totalW = glyphW * len;
            float bx0 = (float)scrW / 2.0f - totalW / 2.0f - 10.0f;
            float bx1 = (float)scrW / 2.0f + totalW / 2.0f + 10.0f;
            drawRect(px2x(bx0), px2y(iy + itemH + 4.0f), px2x(bx1), px2y(iy - 4.0f),
                     0.3f, 0.3f, 0.6f, 0.7f);
        }

        drawText(items[i], (float)scrW / 2.0f, iy, itemH, 1, 1, 1);
    }

    drawText("UP/DOWN TO SELECT  ENTER TO CONFIRM",
             (float)scrW / 2.0f, (float)scrH * 0.85f, 14.0f, 0.5f, 0.5f, 0.5f);

    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

void cleanupMenu() {
    glDeleteTextures(1, &fontTex);
    glDeleteVertexArrays(1, &menuVao);
    glDeleteBuffers(1, &menuVbo);
    glDeleteProgram(menuProg);
}

} // namespace menu_renderer
