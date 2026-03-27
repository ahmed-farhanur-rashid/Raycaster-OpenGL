/*
 * tools/texture_editor.cpp — Pixel art texture editor
 *
 * Usage:  build/texture_editor.exe [size]
 *         size = texture side length (up to 512), default 128
 *
 * Controls:
 *   Left-click / drag    — paint pixel with current color
 *   Right-click / drag   — erase pixel (set to transparent black)
 *   R/G/B + scroll       — adjust Red/Green/Blue channel (hold key + scroll)
 *   Scroll (no key held) — adjust brightness of all channels
 *   F                    — fill entire canvas with current color
 *   X                    — toggle grid overlay
 *   C                    — clear canvas
 *   Ctrl+S               — save (prompts for filename in console)
 *   Ctrl+Z               — undo (up to 128 steps)
 *   Ctrl+Y               — redo
 *   ESC                  — quit
 */

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../include/stb/stb_image_write.h"

/* ---- constants ---- */
#define MAX_TEX_SZ    512
#define UNDO_DEPTH    128
#define WINDOW_SIZE   800
#define PALETTE_H     40     /* bottom bar height in window pixels */

/* ---- canvas state ---- */
static int texSz = 128;
static uint8_t canvas[MAX_TEX_SZ][MAX_TEX_SZ][4]; /* RGBA */

/* ---- editor state ---- */
static uint8_t brushR = 200, brushG = 200, brushB = 200, brushA = 255;
static bool    showGrid   = true;
static bool    painting   = false;
static bool    erasing    = false;
static int     lastCellX  = -1, lastCellY = -1;

/* track which channel key is held */
static bool holdR = false, holdG = false, holdB = false;

/* ---- undo / redo ---- */
struct Snapshot { uint8_t data[MAX_TEX_SZ][MAX_TEX_SZ][4]; };
static Snapshot* undoStack = nullptr;
static int undoTop = 0, undoCur = 0;

static void pushUndo() {
    if (undoCur >= UNDO_DEPTH) {
        memmove(&undoStack[0], &undoStack[1], sizeof(Snapshot) * (UNDO_DEPTH - 1));
        undoCur = UNDO_DEPTH - 1;
    }
    memcpy(undoStack[undoCur].data, canvas, sizeof(canvas));
    undoCur++;
    undoTop = undoCur;
}

static void undo() {
    if (undoCur <= 1) return;
    undoCur--;
    memcpy(canvas, undoStack[undoCur - 1].data, sizeof(canvas));
}

static void redo() {
    if (undoCur >= undoTop) return;
    memcpy(canvas, undoStack[undoCur].data, sizeof(canvas));
    undoCur++;
}

/* ---- GL rendering ---- */
static unsigned int shaderProg, vaoQuad, vboQuad;

static const char* vsSrc = R"(
#version 330 core
layout(location=0) in vec2 aPos;
uniform vec2 offset;
uniform vec2 scale;
uniform vec4 color;
out vec4 fCol;
void main() {
    gl_Position = vec4(aPos * scale + offset, 0.0, 1.0);
    fCol = color;
}
)";

static const char* fsSrc = R"(
#version 330 core
in vec4 fCol;
out vec4 fragColor;
void main() { fragColor = fCol; }
)";

static void initGL() {
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, NULL);
    glCompileShader(vs);
    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, NULL);
    glCompileShader(fs);
    shaderProg = glCreateProgram();
    glAttachShader(shaderProg, vs);
    glAttachShader(shaderProg, fs);
    glLinkProgram(shaderProg);
    glDeleteShader(vs);
    glDeleteShader(fs);

    float quad[] = { 0,0, 1,0, 1,1, 0,0, 1,1, 0,1 };
    glGenVertexArrays(1, &vaoQuad);
    glGenBuffers(1, &vboQuad);
    glBindVertexArray(vaoQuad);
    glBindBuffer(GL_ARRAY_BUFFER, vboQuad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);
}

static void drawRect(float x, float y, float w, float h,
                     float r, float g, float b, float a = 1.0f) {
    glUseProgram(shaderProg);
    float sx = 2.0f * w / WINDOW_SIZE;
    float sy = 2.0f * h / WINDOW_SIZE;
    float ox = 2.0f * x / WINDOW_SIZE - 1.0f;
    float oy = 1.0f - 2.0f * (y + h) / WINDOW_SIZE;
    glUniform2f(glGetUniformLocation(shaderProg, "offset"), ox, oy);
    glUniform2f(glGetUniformLocation(shaderProg, "scale"),  sx, sy);
    glUniform4f(glGetUniformLocation(shaderProg, "color"),  r, g, b, a);
    glBindVertexArray(vaoQuad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

/* canvas area = WINDOW_SIZE x (WINDOW_SIZE - PALETTE_H), at top */
static float canvasAreaH() { return (float)(WINDOW_SIZE - PALETTE_H); }

static void render() {
    glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float areaH  = canvasAreaH();
    float cellPx = areaH / texSz;

    /* checkerboard background (for transparency) */
    float checkSz = cellPx;
    if (checkSz < 4.0f) checkSz = cellPx * 2;
    for (float cy = 0; cy < areaH; cy += checkSz)
        for (float cx = 0; cx < (float)WINDOW_SIZE; cx += checkSz) {
            int ix = (int)(cx / checkSz), iy = (int)(cy / checkSz);
            float v = ((ix + iy) % 2 == 0) ? 0.18f : 0.22f;
            float w = checkSz, h = checkSz;
            if (cx + w > WINDOW_SIZE) w = WINDOW_SIZE - cx;
            if (cy + h > areaH) h = areaH - cy;
            drawRect(cx, cy, w, h, v, v, v);
        }

    /* canvas pixels — center the canvas in the available area */
    float totalCanvasW = cellPx * texSz;
    float offsetX = ((float)WINDOW_SIZE - totalCanvasW) * 0.5f;
    if (offsetX < 0) offsetX = 0;

    for (int y = 0; y < texSz; y++)
        for (int x = 0; x < texSz; x++) {
            uint8_t* px = canvas[y][x];
            if (px[3] == 0) continue; /* transparent = show checkerboard */
            drawRect(offsetX + x * cellPx, y * cellPx,
                     cellPx, cellPx,
                     px[0] / 255.0f, px[1] / 255.0f, px[2] / 255.0f, px[3] / 255.0f);
        }

    /* grid lines */
    if (showGrid && cellPx >= 3.0f) {
        float lineW = 1.0f;
        for (int i = 0; i <= texSz; i++) {
            float pos = i * cellPx;
            drawRect(offsetX + pos, 0, lineW, areaH, 0.35f, 0.35f, 0.35f, 0.5f);
            drawRect(offsetX, pos, totalCanvasW, lineW, 0.35f, 0.35f, 0.35f, 0.5f);
        }
    }

    /* bottom palette bar */
    float barY = areaH;
    drawRect(0, barY, (float)WINDOW_SIZE, (float)PALETTE_H, 0.15f, 0.15f, 0.17f);

    /* current color preview */
    drawRect(10, barY + 5, 30, 30, brushR / 255.0f, brushG / 255.0f, brushB / 255.0f);

    /* RGB channel bars */
    float barW = 150, barH2 = 8;
    float bx = 55;
    /* Red */
    drawRect(bx, barY + 4, barW, barH2, 0.25f, 0.1f, 0.1f);
    drawRect(bx, barY + 4, barW * brushR / 255.0f, barH2, 0.9f, 0.15f, 0.15f);
    /* Green */
    drawRect(bx, barY + 16, barW, barH2, 0.1f, 0.25f, 0.1f);
    drawRect(bx, barY + 16, barW * brushG / 255.0f, barH2, 0.15f, 0.9f, 0.15f);
    /* Blue */
    drawRect(bx, barY + 28, barW, barH2, 0.1f, 0.1f, 0.25f);
    drawRect(bx, barY + 28, barW * brushB / 255.0f, barH2, 0.15f, 0.15f, 0.9f);

    /* preset color swatches */
    static const uint8_t presets[][3] = {
        {0,0,0}, {255,255,255}, {128,128,128},
        {255,0,0}, {0,255,0}, {0,0,255},
        {255,255,0}, {255,0,255}, {0,255,255},
        {255,128,0}, {128,0,255}, {0,128,0},
        {128,64,0}, {64,64,64}, {192,192,192},
    };
    int nPresets = sizeof(presets) / sizeof(presets[0]);
    float swSz = 24, swGap = 3;
    float swX = 220;
    for (int i = 0; i < nPresets; i++) {
        float px = swX + i * (swSz + swGap);
        if (px + swSz > WINDOW_SIZE - 10) break;
        drawRect(px, barY + 8, swSz, swSz,
                 presets[i][0] / 255.0f, presets[i][1] / 255.0f, presets[i][2] / 255.0f);
    }

    glDisable(GL_BLEND);
}

/* ---- input helpers ---- */
static bool cellFromMouse(GLFWwindow* w, int& cx, int& cy) {
    double mx, my;
    glfwGetCursorPos(w, &mx, &my);
    float areaH  = canvasAreaH();
    float cellPx = areaH / texSz;
    float totalCanvasW = cellPx * texSz;
    float offsetX = ((float)WINDOW_SIZE - totalCanvasW) * 0.5f;
    if (offsetX < 0) offsetX = 0;

    if (my >= areaH) return false; /* in palette bar */
    cx = (int)((mx - offsetX) / cellPx);
    cy = (int)(my / cellPx);
    if (cx < 0 || cx >= texSz || cy < 0 || cy >= texSz) return false;
    return true;
}

static void paintPixel(int x, int y, bool erase) {
    if (x < 0 || x >= texSz || y < 0 || y >= texSz) return;
    if (erase) {
        canvas[y][x][0] = canvas[y][x][1] = canvas[y][x][2] = canvas[y][x][3] = 0;
    } else {
        canvas[y][x][0] = brushR;
        canvas[y][x][1] = brushG;
        canvas[y][x][2] = brushB;
        canvas[y][x][3] = brushA;
    }
}

static void paintLine(int x0, int y0, int x1, int y1, bool erase) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        paintPixel(x0, y0, erase);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static bool tryPickSwatch(GLFWwindow* w) {
    double mx, my;
    glfwGetCursorPos(w, &mx, &my);
    float areaH = canvasAreaH();
    if (my < areaH) return false;

    static const uint8_t presets[][3] = {
        {0,0,0}, {255,255,255}, {128,128,128},
        {255,0,0}, {0,255,0}, {0,0,255},
        {255,255,0}, {255,0,255}, {0,255,255},
        {255,128,0}, {128,0,255}, {0,128,0},
        {128,64,0}, {64,64,64}, {192,192,192},
    };
    int nPresets = sizeof(presets) / sizeof(presets[0]);
    float swSz = 24, swGap = 3, swX = 220;
    for (int i = 0; i < nPresets; i++) {
        float px = swX + i * (swSz + swGap);
        float py = areaH + 8;
        if (mx >= px && mx < px + swSz && my >= py && my < py + swSz) {
            brushR = presets[i][0];
            brushG = presets[i][1];
            brushB = presets[i][2];
            printf("Color: (%d, %d, %d)\n", brushR, brushG, brushB);
            return true;
        }
    }
    return false;
}

/* ---- callbacks ---- */
static void mouseButtonCB(GLFWwindow* w, int button, int action, int mods) {
    (void)mods;
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        if (tryPickSwatch(w)) return;
        int cx, cy;
        if (cellFromMouse(w, cx, cy)) {
            painting = true;
            pushUndo();
            paintPixel(cx, cy, false);
            lastCellX = cx; lastCellY = cy;
        }
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        painting = false;
        lastCellX = lastCellY = -1;
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        int cx, cy;
        if (cellFromMouse(w, cx, cy)) {
            erasing = true;
            pushUndo();
            paintPixel(cx, cy, true);
            lastCellX = cx; lastCellY = cy;
        }
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
        erasing = false;
        lastCellX = lastCellY = -1;
    }
}

static void cursorPosCB(GLFWwindow* w, double mx, double my) {
    (void)mx; (void)my;
    if (!painting && !erasing) return;
    int cx, cy;
    if (!cellFromMouse(w, cx, cy)) return;
    if (cx == lastCellX && cy == lastCellY) return;

    bool erase = erasing;
    if (lastCellX >= 0)
        paintLine(lastCellX, lastCellY, cx, cy, erase);
    else
        paintPixel(cx, cy, erase);

    lastCellX = cx; lastCellY = cy;
}

static inline int clampByte(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

static void scrollCB(GLFWwindow* w, double xoff, double yoff) {
    (void)w; (void)xoff;
    int delta = (int)(yoff * 5);

    if (holdR) {
        brushR = (uint8_t)clampByte(brushR + delta);
    } else if (holdG) {
        brushG = (uint8_t)clampByte(brushG + delta);
    } else if (holdB) {
        brushB = (uint8_t)clampByte(brushB + delta);
    } else {
        /* adjust brightness of all channels equally */
        brushR = (uint8_t)clampByte(brushR + delta);
        brushG = (uint8_t)clampByte(brushG + delta);
        brushB = (uint8_t)clampByte(brushB + delta);
    }
}

static void keyCB(GLFWwindow* w, int key, int scancode, int action, int mods) {
    (void)scancode;

    /* track R/G/B hold state for scroll adjustment */
    if (key == GLFW_KEY_R) { holdR = (action != GLFW_RELEASE); return; }
    if (key == GLFW_KEY_G && !(mods & GLFW_MOD_CONTROL)) {
        if (action == GLFW_PRESS) holdG = true;
        else if (action == GLFW_RELEASE) holdG = false;
        return;
    }
    if (key == GLFW_KEY_B) { holdB = (action != GLFW_RELEASE); return; }

    if (action != GLFW_PRESS) return;
    bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;

    switch (key) {
        case GLFW_KEY_X:
            showGrid = !showGrid;
            break;
        case GLFW_KEY_C:
            if (!ctrl) {
                pushUndo();
                memset(canvas, 0, sizeof(canvas));
                printf("Canvas cleared\n");
            }
            break;
        case GLFW_KEY_F:
            pushUndo();
            for (int y = 0; y < texSz; y++)
                for (int x = 0; x < texSz; x++) {
                    canvas[y][x][0] = brushR;
                    canvas[y][x][1] = brushG;
                    canvas[y][x][2] = brushB;
                    canvas[y][x][3] = brushA;
                }
            printf("Filled canvas with (%d, %d, %d)\n", brushR, brushG, brushB);
            break;
        case GLFW_KEY_S:
            if (ctrl) {
                char filename[256];
                printf("Enter filename (without .png): ");
                fflush(stdout);
                if (scanf("%255s", filename) == 1) {
                    char path[512];
                    snprintf(path, sizeof(path), "resource/textures/%s.png", filename);
                    /* write RGBA PNG */
                    uint8_t* outBuf = new uint8_t[texSz * texSz * 4];
                    for (int y = 0; y < texSz; y++)
                        for (int x = 0; x < texSz; x++)
                            memcpy(&outBuf[(y * texSz + x) * 4], canvas[y][x], 4);
                    if (stbi_write_png(path, texSz, texSz, 4, outBuf, texSz * 4))
                        printf("Saved %dx%d texture to %s\n", texSz, texSz, path);
                    else
                        fprintf(stderr, "ERROR: failed to write %s\n", path);
                    delete[] outBuf;
                }
            }
            break;
        case GLFW_KEY_Z:
            if (ctrl) { undo(); printf("Undo\n"); }
            break;
        case GLFW_KEY_Y:
            if (ctrl) { redo(); printf("Redo\n"); }
            break;
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(w, true);
            break;
    }
}

/* ---- main ---- */
int main(int argc, char** argv) {
    if (argc >= 2) {
        int a = atoi(argv[1]);
        if (a >= 1 && a <= MAX_TEX_SZ) texSz = a;
        else fprintf(stderr, "Size must be 1-%d, using default %d\n", MAX_TEX_SZ, texSz);
    } else {
        printf("Enter texture size (1-%d, will be NxN): ", MAX_TEX_SZ);
        fflush(stdout);
        int a = 0;
        if (scanf("%d", &a) == 1 && a >= 1 && a <= MAX_TEX_SZ)
            texSz = a;
        else
            printf("Invalid input, using default %d\n", texSz);
    }

    /* allocate undo stack on heap (too large for stack) */
    undoStack = new Snapshot[UNDO_DEPTH];

    printf("=== Texture Editor ===\n");
    printf("Canvas: %d x %d\n", texSz, texSz);
    printf("Controls:\n");
    printf("  Left-click/drag   - paint pixel\n");
    printf("  Right-click/drag  - erase pixel\n");
    printf("  Click palette     - pick color\n");
    printf("  Hold R/G/B+scroll - adjust channel\n");
    printf("  Scroll (no key)   - adjust brightness\n");
    printf("  F                 - fill canvas\n");
    printf("  X                 - toggle grid\n");
    printf("  C                 - clear canvas\n");
    printf("  Ctrl+S            - save PNG (prompts name)\n");
    printf("  Ctrl+Z / Ctrl+Y   - undo / redo\n");
    printf("  ESC               - quit\n");
    printf("======================\n");

    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    char title[128];
    snprintf(title, sizeof(title), "Texture Editor  [%dx%d]", texSz, texSz);
    GLFWwindow* window = glfwCreateWindow(WINDOW_SIZE, WINDOW_SIZE, title, NULL, NULL);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return 1;
    }

    glViewport(0, 0, WINDOW_SIZE, WINDOW_SIZE);
    initGL();

    glfwSetMouseButtonCallback(window, mouseButtonCB);
    glfwSetCursorPosCallback(window, cursorPosCB);
    glfwSetKeyCallback(window, keyCB);
    glfwSetScrollCallback(window, scrollCB);

    memset(canvas, 0, sizeof(canvas));
    pushUndo();

    while (!glfwWindowShouldClose(window)) {
        render();

        snprintf(title, sizeof(title), "Texture Editor  [%dx%d]  color: (%d,%d,%d)",
                 texSz, texSz, brushR, brushG, brushB);
        glfwSetWindowTitle(window, title);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    delete[] undoStack;
    glfwTerminate();
    return 0;
}
