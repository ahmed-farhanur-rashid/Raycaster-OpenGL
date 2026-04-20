/*
 * tools/map_editor.cpp — Visual map editor for the raycaster
 *
 * Usage:  build/map_editor [size]
 *         size = side length (map will be size × size), default 32, max 256
 *
 * Controls:
 *   Left-click / drag    — place wall/sprite (DRAW mode) or erase (ERASE mode)
 *   Right-click / drag   — erase wall/sprite (always)
 *   E                    — toggle DRAW / ERASE mode
 *   Scroll Wheel         — cycle through textures/sprites
 *   Tab                  — switch between WALL and SPRITE brush mode
 *   G                    — toggle grid overlay
 *   C                    — clear interior (keep perimeter)
 *   Ctrl+S               — save to resource/maps/map.txt
 *   Ctrl+Z               — undo (up to 256 steps)
 *   Ctrl+Y               — redo
 *   ESC                  — quit
 *
 * The perimeter is always walls (type 1) and cannot be erased.
 * Sprites: B=boss, G=ghost, L=lamp, T=torch, H=health, A=ammo, K=key, C=column, R=barrel
 */

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

/* ---- constants ---- */
#define MAX_DIM       256
#define UNDO_DEPTH    256
#define WINDOW_SIZE   800

/* ---- map state ---- */
static int mapSz = 32;
static int grid[MAX_DIM][MAX_DIM];
static GLFWwindow* window = NULL; /* Global window reference for render */

/* ---- editor state ---- */
static int  brushType  = 1;         /* wall type 1-9                     */
static char spriteChar = 'L';       /* current sprite character          */
static bool showGrid   = true;      /* grid overlay toggle               */
static bool eraserMode = false;     /* E key toggles DRAW / ERASE        */
static bool spriteMode = false;     /* Tab toggles WALL / SPRITE mode    */
static bool painting   = false;     /* left mouse dragging               */
static bool erasing    = false;     /* right mouse dragging              */
static int  lastCellX  = -1, lastCellY = -1; /* for drag de-dup */

/* ---- texture resources ---- */
static GLuint wallTexGL[9] = {0};   /* wall textures 1-9                 */
static const char* wallTexPaths[] = {
    "resource/textures/wall/texture_1_dirt_moss_stones.png",
    "resource/textures/wall/texture_2_full_moss.png",
    "resource/textures/wall/texture_3_dark_dirt_stones.png",
    "resource/textures/floor/texture_1_grass_yellow_flowers.png",
    "resource/textures/floor/texture_2_grass_white_daisies.png",
    "resource/textures/floor/texture_3_grass_blue_flowers.png",
    "resource/textures/wall/texture_1_dirt_moss_stones.png", /* fallback */
    "resource/textures/wall/texture_2_full_moss.png",         /* fallback */
    "resource/textures/wall/texture_3_dark_dirt_stones.png"    /* fallback */
};

struct SpriteInfo {
    char ch;
    const char* name;
    const char* path;
    GLuint texID;
};

static SpriteInfo sprites[] = {
    {'B', "Boss", "resource/textures/entities/boss01/boss_01_idle.png"},
    {'G', "Ghost", "resource/textures/entities/ghost/sprite_enemy_01_00idle.png"},
    {'L', "Lamp", "resource/textures/sprite_lamp.png"},
    {'T', "Torch", "resource/textures/sprite_torch.png"},
    {'H', "Health", "resource/textures/sprite_health.png"},
    {'A', "Ammo", "resource/textures/sprite_ammo.png"},
    {'K', "Key", "resource/textures/sprite_key.png"},
    {'C', "Column", "resource/textures/sprite_column.png"},
    {'R', "Barrel", "resource/textures/sprite_barrel.png"}
};
static const int NUM_SPRITES = sizeof(sprites) / sizeof(sprites[0]);

/* ---- undo / redo ---- */
struct Snapshot { int data[MAX_DIM][MAX_DIM]; };
static Snapshot undoStack[UNDO_DEPTH];
static int undoTop  = 0;            /* points to next free slot           */
static int undoCur  = 0;            /* current position in undo history   */

static void pushUndo() {
    /* save current grid to undo stack */
    if (undoCur >= UNDO_DEPTH) {
        /* shift everything down */
        memmove(&undoStack[0], &undoStack[1], sizeof(Snapshot) * (UNDO_DEPTH - 1));
        undoCur = UNDO_DEPTH - 1;
    }
    memcpy(undoStack[undoCur].data, grid, sizeof(grid));
    undoCur++;
    undoTop = undoCur; /* discard redo history after new edit */
}

static void undo() {
    if (undoCur <= 1) return;
    undoCur--;
    memcpy(grid, undoStack[undoCur - 1].data, sizeof(grid));
}

static void redo() {
    if (undoCur >= undoTop) return;
    memcpy(grid, undoStack[undoCur].data, sizeof(grid));
    undoCur++;
}

/* ---- map helpers ---- */
static void initMap() {
    memset(grid, 0, sizeof(grid));
    /* perimeter walls */
    for (int i = 0; i < mapSz; i++) {
        grid[0][i]          = 1;
        grid[mapSz - 1][i]  = 1;
        grid[i][0]          = 1;
        grid[i][mapSz - 1]  = 1;
    }
    undoCur = 0;
    undoTop = 0;
    pushUndo();
}

static void clearInterior() {
    pushUndo();
    for (int y = 1; y < mapSz - 1; y++)
        for (int x = 1; x < mapSz - 1; x++)
            grid[y][x] = 0;
}

static bool saveMap(const char* path) {
    std::ofstream f(path);
    if (!f) { fprintf(stderr, "ERROR: cannot write %s\n", path); return false; }
    for (int y = 0; y < mapSz; y++) {
        for (int x = 0; x < mapSz; x++) {
            int v = grid[y][x];
            if (v >= 0 && v <= 9) {
                /* Wall type - save as digit */
                f << (char)('0' + v);
            } else if (v >= 'A' && v <= 'Z') {
                /* Sprite character - save as letter */
                f << (char)v;
            } else {
                /* Empty */
                f << '0';
            }
        }
        f << '\n';
    }
    printf("Saved map (%dx%d) to %s\n", mapSz, mapSz, path);
    return true;
}

static bool loadExistingMap(const char* path) {
    std::ifstream f(path);
    if (!f) return false;
    int row = 0;
    std::string line;
    while (std::getline(f, line) && row < mapSz) {
        for (int x = 0; x < mapSz && x < (int)line.size(); x++) {
            char c = line[x];
            /* Load walls (digits) and sprites (letters) */
            if (c >= '0' && c <= '9') {
                grid[row][x] = c - '0';
            } else if (c >= 'A' && c <= 'Z') {
                grid[row][x] = c; /* Store sprite character directly */
            } else {
                grid[row][x] = 0;
            }
        }
        row++;
    }
    /* enforce perimeter */
    for (int i = 0; i < mapSz; i++) {
        grid[0][i]          = 1;
        grid[mapSz - 1][i]  = 1;
        grid[i][0]          = 1;
        grid[i][mapSz - 1]  = 1;
    }
    undoCur = 0; undoTop = 0;
    pushUndo();
    return true;
}

/* ---- rendering (OpenGL with texture support) ---- */
static unsigned int shaderProg, texShaderProg, vao, vbo;

static const char* vsSrc = R"(
#version 330 core
layout(location=0) in vec2 aPos;
uniform vec2 offset;
uniform vec2 scale;
uniform vec3 color;
out vec3 fCol;
void main() {
    gl_Position = vec4(aPos * scale + offset, 0.0, 1.0);
    fCol = color;
}
)";

static const char* fsSrc = R"(
#version 330 core
in vec3 fCol;
out vec4 fragColor;
void main() { fragColor = vec4(fCol, 1.0); }
)";

static const char* texVsSrc = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aTexCoord;
uniform vec2 offset;
uniform vec2 scale;
out vec2 fTexCoord;
void main() {
    gl_Position = vec4(aPos * scale + offset, 0.0, 1.0);
    fTexCoord = aTexCoord;
}
)";

static const char* texFsSrc = R"(
#version 330 core
in vec2 fTexCoord;
uniform sampler2D texSampler;
uniform float alpha;
out vec4 fragColor;
void main() {
    vec4 texColor = texture(texSampler, fTexCoord);
    fragColor = vec4(texColor.rgb, texColor.a * alpha);
}
)";

static void initGL() {
    /* compile color shaders */
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

    /* compile texture shaders */
    unsigned int texVs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(texVs, 1, &texVsSrc, NULL);
    glCompileShader(texVs);
    unsigned int texFs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(texFs, 1, &texFsSrc, NULL);
    glCompileShader(texFs);
    texShaderProg = glCreateProgram();
    glAttachShader(texShaderProg, texVs);
    glAttachShader(texShaderProg, texFs);
    glLinkProgram(texShaderProg);
    glDeleteShader(texVs);
    glDeleteShader(texFs);

    /* unit quad with texture coordinates */
    float quad[] = {
        0,0, 0,0,
        1,0, 1,0,
        1,1, 1,1,
        0,0, 0,0,
        1,1, 1,1,
        0,1, 0,1
    };
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

static void drawRect(float x, float y, float w, float h, float r, float g, float b) {
    glUseProgram(shaderProg);
    float sx = 2.0f * w / WINDOW_SIZE;
    float sy = 2.0f * h / WINDOW_SIZE;
    float ox = 2.0f * x / WINDOW_SIZE - 1.0f;
    float oy = 1.0f - 2.0f * (y + h) / WINDOW_SIZE;
    glUniform2f(glGetUniformLocation(shaderProg, "offset"), ox, oy);
    glUniform2f(glGetUniformLocation(shaderProg, "scale"),  sx, sy);
    glUniform3f(glGetUniformLocation(shaderProg, "color"),  r, g, b);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static void drawTexturedRect(float x, float y, float w, float h, GLuint texID, float alpha = 1.0f) {
    glUseProgram(texShaderProg);
    float sx = 2.0f * w / WINDOW_SIZE;
    float sy = 2.0f * h / WINDOW_SIZE;
    float ox = 2.0f * x / WINDOW_SIZE - 1.0f;
    float oy = 1.0f - 2.0f * (y + h) / WINDOW_SIZE;
    glUniform2f(glGetUniformLocation(texShaderProg, "offset"), ox, oy);
    glUniform2f(glGetUniformLocation(texShaderProg, "scale"),  sx, sy);
    glUniform1f(glGetUniformLocation(texShaderProg, "alpha"), alpha);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texID);
    glUniform1i(glGetUniformLocation(texShaderProg, "texSampler"), 0);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static GLuint loadTexture(const char* path) {
    int w, h, channels;
    unsigned char* data = stbi_load(path, &w, &h, &channels, 4);
    if (!data) {
        fprintf(stderr, "Warning: Could not load %s\n", path);
        /* create fallback 1x1 white texture */
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        unsigned char white[] = {255, 255, 255, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        return tex;
    }
    
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(data);
    return tex;
}

static void loadAllTextures() {
    /* Load wall textures */
    for (int i = 0; i < 9; i++) {
        wallTexGL[i] = loadTexture(wallTexPaths[i]);
    }
    
    /* Load sprite textures */
    for (int i = 0; i < NUM_SPRITES; i++) {
        sprites[i].texID = loadTexture(sprites[i].path);
    }
}

static void wallColor(int type, float& r, float& g, float& b) {
    switch (type) {
        case 1: r=0.60f; g=0.60f; b=0.60f; break;
        case 2: r=0.55f; g=0.30f; b=0.15f; break;
        case 3: r=0.25f; g=0.50f; b=0.25f; break;
        case 4: r=0.50f; g=0.40f; b=0.20f; break;
        case 5: r=0.30f; g=0.30f; b=0.70f; break;
        case 6: r=0.70f; g=0.20f; b=0.20f; break;
        case 7: r=0.70f; g=0.70f; b=0.20f; break;
        case 8: r=0.40f; g=0.20f; b=0.50f; break;
        case 9: r=0.85f; g=0.85f; b=0.85f; break;
        default: r=0.0f; g=0.0f; b=0.0f;   break;
    }
}

static bool isPerimeter(int cx, int cy) {
    return cx == 0 || cy == 0 || cx == mapSz - 1 || cy == mapSz - 1;
}

static void render() {
    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    float cellPx = (float)WINDOW_SIZE / mapSz;

    /* cells */
    for (int y = 0; y < mapSz; y++)
        for (int x = 0; x < mapSz; x++) {
            int v = grid[y][x];
            if (v >= 1 && v <= 9) {
                drawTexturedRect(x * cellPx, y * cellPx, cellPx, cellPx, wallTexGL[v-1]);
            } else if (v >= 'A' && v <= 'Z') {
                char ch = (char)v;
                for (int s = 0; s < NUM_SPRITES; s++) {
                    if (sprites[s].ch == ch) {
                        drawRect(x * cellPx, y * cellPx, cellPx, cellPx, 0.3f, 0.6f, 0.3f);
                        drawTexturedRect(x * cellPx, y * cellPx, cellPx, cellPx, sprites[s].texID, 0.7f);
                        break;
                    }
                }
            }
        }

    /* grid lines */
    if (showGrid) {
        float lineW = (cellPx > 8.0f) ? 1.0f : 0.5f;
        for (int i = 0; i <= mapSz; i++) {
            float pos = i * cellPx;
            drawRect(pos, 0, lineW, (float)WINDOW_SIZE, 0.3f, 0.3f, 0.3f);
            drawRect(0, pos, (float)WINDOW_SIZE, lineW, 0.3f, 0.3f, 0.3f);
        }
    }

    /* brush preview at mouse position */
    double mx, my;
    int mouseCx = -1, mouseCy = -1;
    {
        glfwGetCursorPos(window, &mx, &my);
        if (mx >= 0 && mx < WINDOW_SIZE && my >= 0 && my < WINDOW_SIZE) {
            mouseCx = (int)(mx / cellPx);
            mouseCy = (int)(my / cellPx);
        }
    }
    
    if (mouseCx >= 0 && mouseCy >= 0 && mouseCx < mapSz && mouseCy < mapSz && !isPerimeter(mouseCx, mouseCy)) {
        if (!eraserMode) {
            if (spriteMode) {
                for (int s = 0; s < NUM_SPRITES; s++) {
                    if (sprites[s].ch == spriteChar) {
                        drawRect(mouseCx * cellPx, mouseCy * cellPx, cellPx, cellPx, 0.3f, 0.6f, 0.3f);
                        drawTexturedRect(mouseCx * cellPx, mouseCy * cellPx, cellPx, cellPx, sprites[s].texID, 0.5f);
                        break;
                    }
                }
            } else {
                drawTexturedRect(mouseCx * cellPx, mouseCy * cellPx, cellPx, cellPx, wallTexGL[brushType-1], 0.5f);
            }
        } else {
            drawRect(mouseCx * cellPx, mouseCy * cellPx, cellPx, cellPx, 0.8f, 0.2f, 0.2f);
        }
    }

    /* HUD - current brush info */
    float hx = WINDOW_SIZE - 50.0f, hy = 8.0f;
    if (eraserMode) {
        drawRect(hx, hy, 40.0f, 40.0f, 0.8f, 0.15f, 0.15f);
        drawRect(hx + 3, hy + 3, 34.0f, 34.0f, 0.12f, 0.12f, 0.14f);
    } else if (spriteMode) {
        drawRect(hx, hy, 40.0f, 40.0f, 0.2f, 0.2f, 0.2f);
        for (int s = 0; s < NUM_SPRITES; s++) {
            if (sprites[s].ch == spriteChar) {
                drawRect(hx + 2, hy + 2, 36.0f, 36.0f, 0.9f, 0.9f, 0.9f);
                drawTexturedRect(hx + 2, hy + 2, 36.0f, 36.0f, sprites[s].texID);
                break;
            }
        }
    } else {
        drawRect(hx, hy, 40.0f, 40.0f, 0.2f, 0.2f, 0.2f);
        drawTexturedRect(hx + 2, hy + 2, 36.0f, 36.0f, wallTexGL[brushType-1]);
    }
}

/* ---- input ---- */

static void cellFromMouse(GLFWwindow* w, int& cx, int& cy) {
    double mx, my;
    glfwGetCursorPos(w, &mx, &my);
    float cellPx = (float)WINDOW_SIZE / mapSz;
    cx = (int)(mx / cellPx);
    cy = (int)(my / cellPx);
    if (cx < 0) cx = 0; if (cx >= mapSz) cx = mapSz - 1;
    if (cy < 0) cy = 0; if (cy >= mapSz) cy = mapSz - 1;
}

/* Bresenham line from (x0,y0) to (x1,y1)—fills all cells along the path */
static void paintLine(int x0, int y0, int x1, int y1, int value) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        if (!isPerimeter(x0, y0))
            grid[y0][x0] = value;
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void mouseButtonCB(GLFWwindow* w, int button, int action, int mods) {
    (void)mods;
    
    int cx, cy;
    cellFromMouse(w, cx, cy);

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            painting = true;
            pushUndo();
            if (!isPerimeter(cx, cy)) {
                if (eraserMode) {
                    grid[cy][cx] = 0;
                } else if (spriteMode) {
                    grid[cy][cx] = spriteChar;
                } else {
                    grid[cy][cx] = brushType;
                }
            }
            lastCellX = cx; lastCellY = cy;
        } else {
            painting = false;
            lastCellX = lastCellY = -1;
        }
    }
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            erasing = true;
            pushUndo();
            if (!isPerimeter(cx, cy)) grid[cy][cx] = 0;
            lastCellX = cx; lastCellY = cy;
        } else {
            erasing = false;
            lastCellX = lastCellY = -1;
        }
    }
}

static void cursorPosCB(GLFWwindow* w, double mx, double my) {
    (void)mx; (void)my;
    if (!painting && !erasing) return;
    int cx, cy;
    cellFromMouse(w, cx, cy);
    if (cx == lastCellX && cy == lastCellY) return;

    if (!isPerimeter(cx, cy)) {
        if (eraserMode) {
            grid[cy][cx] = 0;
        } else if (spriteMode) {
            grid[cy][cx] = spriteChar;
        } else {
            grid[cy][cx] = brushType;
        }
    }

    lastCellX = cx; lastCellY = cy;
}

static void scrollCB(GLFWwindow* w, double xoffset, double yoffset) {
    (void)xoffset;
    if (yoffset > 0) {
        /* Scroll up */
        if (spriteMode) {
            /* Cycle through sprites forward */
            int idx = 0;
            for (int i = 0; i < NUM_SPRITES; i++) {
                if (sprites[i].ch == spriteChar) { idx = i; break; }
            }
            idx = (idx + 1) % NUM_SPRITES;
            spriteChar = sprites[idx].ch;
            printf("Sprite: %s (%c)\n", sprites[idx].name, spriteChar);
        } else {
            /* Cycle through wall types forward */
            brushType = (brushType % 9) + 1;
            printf("Wall type: %d\n", brushType);
        }
    } else if (yoffset < 0) {
        /* Scroll down */
        if (spriteMode) {
            /* Cycle through sprites backward */
            int idx = 0;
            for (int i = 0; i < NUM_SPRITES; i++) {
                if (sprites[i].ch == spriteChar) { idx = i; break; }
            }
            idx = (idx - 1 + NUM_SPRITES) % NUM_SPRITES;
            spriteChar = sprites[idx].ch;
            printf("Sprite: %s (%c)\n", sprites[idx].name, spriteChar);
        } else {
            /* Cycle through wall types backward */
            brushType = ((brushType - 2 + 9) % 9) + 1;
            printf("Wall type: %d\n", brushType);
        }
    }
}

static void keyCB(GLFWwindow* w, int key, int scancode, int action, int mods) {
    (void)scancode;
    if (action != GLFW_PRESS) return;

    /* 1-9 = brush type */
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
        spriteMode = false;
        brushType = key - GLFW_KEY_0;
        printf("Wall type: %d\n", brushType);
        return;
    }

    bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;

    switch (key) {
        case GLFW_KEY_TAB:
            spriteMode = !spriteMode;
            printf("Mode: %s\n", spriteMode ? "SPRITE" : "WALL");
            break;
        case GLFW_KEY_E:
            eraserMode = !eraserMode;
            printf("Brush: %s\n", eraserMode ? "ERASE" : "DRAW");
            break;
        case GLFW_KEY_G:
            showGrid = !showGrid;
            break;
        case GLFW_KEY_C:
            clearInterior();
            printf("Cleared interior\n");
            break;
        case GLFW_KEY_S:
            if (ctrl) {
                saveMap("resource/maps/map.txt");
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
    /* parse map size from command line, or prompt interactively */
    if (argc >= 2) {
        int a = atoi(argv[1]);
        if (a >= 4 && a <= MAX_DIM) mapSz = a;
        else fprintf(stderr, "Size must be 4-%d, using default %d\n", MAX_DIM, mapSz);
    } else {
        printf("Enter map side length (4-%d, map will be NxN): ", MAX_DIM);
        fflush(stdout);
        int a = 0;
        if (scanf("%d", &a) == 1 && a >= 4 && a <= MAX_DIM)
            mapSz = a;
        else
            printf("Invalid input, using default %d\n", mapSz);
    }

    printf("=== Raycaster Map Editor ===\n");
    printf("Map size: %d x %d\n", mapSz, mapSz);
    printf("Controls:\n");
    printf("  Left-click/drag    — place wall/sprite (DRAW) or erase (ERASE)\n");
    printf("  Right-click/drag   — erase wall/sprite (always)\n");
    printf("  Scroll Wheel       — cycle through textures/sprites\n");
    printf("  Tab                — switch between WALL and SPRITE mode\n");
    printf("  E                  — toggle DRAW / ERASE mode\n");
    printf("  1-9                — select wall type (quick select)\n");
    printf("  G                  — toggle grid\n");
    printf("  C                  — clear interior\n");
    printf("  Ctrl+S             — save map\n");
    printf("  Ctrl+Z / Ctrl+Y    — undo / redo\n");
    printf("  ESC                — quit\n");
    printf("============================\n");

    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_STICKY_KEYS, GLFW_TRUE);

    char title[128];
    snprintf(title, sizeof(title), "Map Editor  [%dx%d]  brush: 1", mapSz, mapSz);
    window = glfwCreateWindow(WINDOW_SIZE, WINDOW_SIZE, title, NULL, NULL);
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
    loadAllTextures();

    glfwSetMouseButtonCallback(window, mouseButtonCB);
    glfwSetCursorPosCallback(window, cursorPosCB);
    glfwSetKeyCallback(window, keyCB);
    glfwSetScrollCallback(window, scrollCB);

    /* try loading existing map (will be cropped/padded to mapSz) */
    initMap();
    if (loadExistingMap("resource/maps/map.txt"))
        printf("Loaded existing map.txt\n");

    while (!glfwWindowShouldClose(window)) {
        render();

        /* update title with brush info */
        const char* modeStr = eraserMode ? "ERASE" : (spriteMode ? "SPRITE" : "DRAW");
        if (spriteMode) {
            snprintf(title, sizeof(title), "Map Editor  [%dx%d]  %s  sprite: %c",
                     mapSz, mapSz, modeStr, spriteChar);
        } else {
            snprintf(title, sizeof(title), "Map Editor  [%dx%d]  %s  wall: %d",
                     mapSz, mapSz, modeStr, brushType);
        }
        glfwSetWindowTitle(window, title);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
