/*
 * tools/map_editor.cpp — Visual map editor for the raycaster
 *
 * Usage:  build/map_editor.exe [size]
 *         size = side length (map will be size × size), default 32, max 256
 *
 * Controls:
 *   Left-click / drag   — place wall (DRAW mode) or erase (ERASE mode)
 *   Right-click / drag   — erase wall (always)
 *   E                    — toggle DRAW / ERASE mode
 *   1-9                  — select wall type
 *   G                    — toggle grid overlay
 *   C                    — clear interior (keep perimeter)
 *   Ctrl+S               — save to resource/maps/map.txt
 *   Ctrl+Z               — undo (up to 256 steps)
 *   Ctrl+Y               — redo
 *   ESC                  — quit
 *
 * The perimeter is always walls (type 1) and cannot be erased.
 */

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

/* ---- constants ---- */
#define MAX_DIM       256
#define UNDO_DEPTH    256
#define WINDOW_SIZE   800

/* ---- map state ---- */
static int mapSz = 32;
static int grid[MAX_DIM][MAX_DIM];

/* ---- editor state ---- */
static int  brushType  = 1;         /* wall type 1-9                     */
static bool showGrid   = true;      /* grid overlay toggle               */
static bool eraserMode = false;     /* E key toggles DRAW / ERASE        */
static bool painting   = false;     /* left mouse dragging               */
static bool erasing    = false;     /* right mouse dragging              */
static int  lastCellX  = -1, lastCellY = -1; /* for drag de-dup */

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
        for (int x = 0; x < mapSz; x++)
            f << (char)('0' + grid[y][x]);
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
            grid[row][x] = (c >= '0' && c <= '9') ? c - '0' : 0;
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

/* ---- rendering (OpenGL immediate-mode style via a simple shader) ---- */
static unsigned int shaderProg, vao, vbo;

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

static void initGL() {
    /* compile shaders */
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

    /* unit quad (0,0)-(1,1) */
    float quad[] = { 0,0, 1,0, 1,1, 0,0, 1,1, 0,1 };
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);
}

static void drawRect(float x, float y, float w, float h, float r, float g, float b) {
    glUseProgram(shaderProg);
    /* convert pixel coords to NDC: x,y in [0..WINDOW_SIZE] → [-1..1] */
    float sx = 2.0f * w / WINDOW_SIZE;
    float sy = 2.0f * h / WINDOW_SIZE;
    float ox = 2.0f * x / WINDOW_SIZE - 1.0f;
    float oy = 1.0f - 2.0f * (y + h) / WINDOW_SIZE; /* flip Y */
    glUniform2f(glGetUniformLocation(shaderProg, "offset"), ox, oy);
    glUniform2f(glGetUniformLocation(shaderProg, "scale"),  sx, sy);
    glUniform3f(glGetUniformLocation(shaderProg, "color"),  r, g, b);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

/* wall type → colour */
static void wallColor(int type, float& r, float& g, float& b) {
    switch (type) {
        case 1: r=0.60f; g=0.60f; b=0.60f; break; /* gray stone   */
        case 2: r=0.55f; g=0.30f; b=0.15f; break; /* brown brick  */
        case 3: r=0.25f; g=0.50f; b=0.25f; break; /* green moss   */
        case 4: r=0.50f; g=0.40f; b=0.20f; break; /* wood         */
        case 5: r=0.30f; g=0.30f; b=0.70f; break; /* blue tile    */
        case 6: r=0.70f; g=0.20f; b=0.20f; break; /* red          */
        case 7: r=0.70f; g=0.70f; b=0.20f; break; /* yellow       */
        case 8: r=0.40f; g=0.20f; b=0.50f; break; /* purple       */
        case 9: r=0.85f; g=0.85f; b=0.85f; break; /* white        */
        default: r=0.0f; g=0.0f; b=0.0f;   break;
    }
}

static void render() {
    glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    float cellPx = (float)WINDOW_SIZE / mapSz;

    /* cells */
    for (int y = 0; y < mapSz; y++)
        for (int x = 0; x < mapSz; x++) {
            int v = grid[y][x];
            if (v > 0) {
                float r, g, b;
                wallColor(v, r, g, b);
                drawRect(x * cellPx, y * cellPx, cellPx, cellPx, r, g, b);
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

    /* brush indicator (HUD) */
    float hx = WINDOW_SIZE - 40.0f, hy = 8.0f;
    if (eraserMode) {
        /* red border to indicate eraser mode */
        drawRect(hx, hy, 30.0f, 30.0f, 0.8f, 0.15f, 0.15f);
        drawRect(hx + 3, hy + 3, 24.0f, 24.0f, 0.12f, 0.12f, 0.14f);
    } else {
        drawRect(hx, hy, 30.0f, 30.0f, 0.2f, 0.2f, 0.2f);
        float br, bg, bb;
        wallColor(brushType, br, bg, bb);
        drawRect(hx + 3, hy + 3, 24.0f, 24.0f, br, bg, bb);
    }
}

/* ---- input ---- */
static bool isPerimeter(int cx, int cy) {
    return cx == 0 || cy == 0 || cx == mapSz - 1 || cy == mapSz - 1;
}

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
            int value = eraserMode ? 0 : brushType;
            if (!isPerimeter(cx, cy)) grid[cy][cx] = value;
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

    int value = painting ? (eraserMode ? 0 : brushType) : 0;
    if (lastCellX >= 0)
        paintLine(lastCellX, lastCellY, cx, cy, value);
    else if (!isPerimeter(cx, cy))
        grid[cy][cx] = value;

    lastCellX = cx; lastCellY = cy;
}

static void keyCB(GLFWwindow* w, int key, int scancode, int action, int mods) {
    (void)scancode;
    if (action != GLFW_PRESS) return;

    /* 1-9 = brush type */
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
        brushType = key - GLFW_KEY_0;
        printf("Brush: wall type %d\n", brushType);
        return;
    }

    bool ctrl = (mods & GLFW_MOD_CONTROL) != 0;

    switch (key) {
        case GLFW_KEY_E:
            eraserMode = !eraserMode;
            printf("Mode: %s\n", eraserMode ? "ERASE" : "DRAW");
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
    printf("  Left-click/drag  — place wall (DRAW) or erase (ERASE)\n");
    printf("  Right-click/drag — erase wall (always)\n");
    printf("  E                — toggle DRAW / ERASE mode\n");
    printf("  1-9              — select wall type\n");
    printf("  G                — toggle grid\n");
    printf("  C                — clear interior\n");
    printf("  Ctrl+S           — save map\n");
    printf("  Ctrl+Z / Ctrl+Y — undo / redo\n");
    printf("  ESC              — quit\n");
    printf("============================\n");

    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    char title[128];
    snprintf(title, sizeof(title), "Map Editor  [%dx%d]  brush: 1", mapSz, mapSz);
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

    /* try loading existing map (will be cropped/padded to mapSz) */
    initMap();
    if (loadExistingMap("resource/maps/map.txt"))
        printf("Loaded existing map.txt\n");

    while (!glfwWindowShouldClose(window)) {
        render();

        /* update title with brush info */
        snprintf(title, sizeof(title), "Map Editor  [%dx%d]  %s  brush: %d",
                 mapSz, mapSz, eraserMode ? "ERASE" : "DRAW", brushType);
        glfwSetWindowTitle(window, title);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
