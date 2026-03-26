#include <glad/glad.h>
#include "map_renderer.h"
#include "../map/map.h"
#include "../player/player.h"
#include "../core/input.h"
#include "shader.h"
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <malloc.h>
#include <stb/stb_image.h>

/* ===== constants ===== */
#define TEX_SZ 512
#define N_WALL_TEX 4
#define SKY_W  512
#define SKY_H  128

/* ===== module state ===== */
static int scrW, scrH;
static uint32_t* screenBuf = nullptr;
static float*    zBuf      = nullptr;

static unsigned int glTex, vao, vbo, prog;

static uint32_t* wallTex[N_WALL_TEX];
static uint32_t* floorTex = nullptr;
static uint32_t  skyTex[SKY_W * SKY_H];

/* ===== shaders ===== */
static const char* vsrc = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
out vec2 uv;
void main(){ gl_Position = vec4(aPos, 0.0, 1.0); uv = aUV; }
)";

static const char* fsrc = R"(
#version 330 core
in vec2 uv;
out vec4 fragColor;
uniform sampler2D tex;
void main(){ fragColor = texture(tex, uv); }
)";

/* ===== helpers ===== */
static inline uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
}

static inline void setPixel(int x, int y, uint32_t c) {
    if (x >= 0 && x < scrW && y >= 0 && y < scrH)
        screenBuf[y * scrW + x] = c;
}

/* ===== texture loading from resource/textures ===== */

/* Load a PNG and resample into a dest buffer of destW x destH. */
static bool loadTexture(const char* path, uint32_t* dest, int destW, int destH) {
    int w, h, channels;
    unsigned char* data = stbi_load(path, &w, &h, &channels, 4);
    if (!data) { fprintf(stderr, "WARNING: could not load %s\n", path); return false; }

    for (int y = 0; y < destH; y++)
        for (int x = 0; x < destW; x++) {
            int srcX = x * w / destW;
            int srcY = y * h / destH;
            unsigned char* px = data + (srcY * w + srcX) * 4;
            dest[y * destW + x] = (uint32_t)px[0]
                                | ((uint32_t)px[1] << 8)
                                | ((uint32_t)px[2] << 16)
                                | ((uint32_t)px[3] << 24);
        }

    stbi_image_free(data);
    return true;
}

static void genTextures() {
    /* allocate texture buffers */
    for (int i = 0; i < N_WALL_TEX; i++)
        wallTex[i] = new uint32_t[TEX_SZ * TEX_SZ];
    floorTex = new uint32_t[TEX_SZ * TEX_SZ];

    /* all walls use the same dark dirt stones texture */
    static const char* wallPath = "resource/textures/texture_3_dark_dirt_stones.png";

    for (int i = 0; i < N_WALL_TEX; i++)
        if (!loadTexture(wallPath, wallTex[i], TEX_SZ, TEX_SZ))
            memset(wallTex[i], 0x80, TEX_SZ * TEX_SZ * sizeof(uint32_t));
    if (!loadTexture("resource/textures/texture_3_grass_blue_flowers.png", floorTex, TEX_SZ, TEX_SZ))
        for (int i = 0; i < TEX_SZ * TEX_SZ; i++) floorTex[i] = rgba(50, 35, 15);
    if (!loadTexture("resource/textures/sky_0.png", skyTex, SKY_W, SKY_H))
        for (int i = 0; i < SKY_W * SKY_H; i++) skyTex[i] = rgba(10, 12, 25);
}

/* ===== textured ceiling (panoramic sky) & floor ===== */
static void drawBackground() {
    int half = scrH / 2;

    /* --- sky: panoramic mapping based on view direction --- */
    /* precompute per-column angle (avoids atan2f per pixel) */
    float* colAngle = (float*)alloca(scrW * sizeof(float));
    for (int x = 0; x < scrW; x++) {
        float camX = 2.0f * x / (float)scrW - 1.0f;
        float rdX  = player.dirX + player.planeX * camX;
        float rdY  = player.dirY + player.planeY * camX;
        float a    = atan2f(rdY, rdX);          /* -PI .. PI  */
        colAngle[x] = (a + 3.14159265f) / (2.0f * 3.14159265f); /* 0..1 */
    }
    for (int y = 0; y < half; y++) {
        float vt  = (float)y / (float)half;     /* 0 = top, 1 = horizon */
        int skyY  = (int)(vt * (SKY_H - 1));
        for (int x = 0; x < scrW; x++) {
            int skyX = (int)(colAngle[x] * SKY_W) % SKY_W;
            if (skyX < 0) skyX += SKY_W;
            screenBuf[y * scrW + x] = skyTex[skyY * SKY_W + skyX];
        }
    }

    /* --- floor: perspective-correct texture mapping --- */
    float rdX0 = player.dirX - player.planeX;
    float rdY0 = player.dirY - player.planeY;
    float rdX1 = player.dirX + player.planeX;
    float rdY1 = player.dirY + player.planeY;

    for (int y = half + 1; y < scrH; y++) {
        int p = y - half;
        float rowDist = (0.5f * scrH) / (float)p;

        float fStepX = rowDist * (rdX1 - rdX0) / (float)scrW;
        float fStepY = rowDist * (rdY1 - rdY0) / (float)scrW;

        float fX = player.posX + rowDist * rdX0;
        float fY = player.posY + rowDist * rdY0;

        for (int x = 0; x < scrW; x++) {
            int tx = (int)(TEX_SZ * (fX - floorf(fX))) & (TEX_SZ - 1);
            int ty = (int)(TEX_SZ * (fY - floorf(fY))) & (TEX_SZ - 1);

            uint32_t c = floorTex[ty * TEX_SZ + tx];

            if (lightingEnabled) {
                float shade = 1.0f / (1.0f + rowDist * rowDist * 0.04f);
                uint8_t r = (uint8_t)((c & 0xFF) * shade);
                uint8_t g = (uint8_t)(((c >> 8) & 0xFF) * shade);
                uint8_t b = (uint8_t)(((c >> 16) & 0xFF) * shade);
                c = rgba(r, g, b);
            }

            screenBuf[y * scrW + x] = c;
            fX += fStepX;
            fY += fStepY;
        }
    }
}

/* ===== DDA raycasting + textured walls (Stages 2-3-5) ===== */
static void castWalls() {
    for (int x = 0; x < scrW; x++) {
        float camX = 2.0f * x / (float)scrW - 1.0f;
        float rdX  = player.dirX + player.planeX * camX;
        float rdY  = player.dirY + player.planeY * camX;

        int mapX = (int)player.posX;
        int mapY = (int)player.posY;

        float ddX = (rdX == 0) ? 1e30f : fabsf(1.0f / rdX);
        float ddY = (rdY == 0) ? 1e30f : fabsf(1.0f / rdY);

        float sdX, sdY;
        int stepX, stepY;

        if (rdX < 0) { stepX = -1; sdX = (player.posX - mapX) * ddX; }
        else          { stepX =  1; sdX = (mapX + 1.0f - player.posX) * ddX; }
        if (rdY < 0) { stepY = -1; sdY = (player.posY - mapY) * ddY; }
        else          { stepY =  1; sdY = (mapY + 1.0f - player.posY) * ddY; }

        int side = 0, hit = 0;
        while (!hit) {
            if (sdX < sdY) { sdX += ddX; mapX += stepX; side = 0; }
            else            { sdY += ddY; mapY += stepY; side = 1; }
            if (mapX < 0 || mapX >= mapWidth || mapY < 0 || mapY >= mapHeight) { hit = 1; break; }
            if (worldMap[mapY][mapX] > 0) hit = 1;
        }

        float perpDist;
        if (side == 0) perpDist = (mapX - player.posX + (1 - stepX) / 2.0f) / rdX;
        else           perpDist = (mapY - player.posY + (1 - stepY) / 2.0f) / rdY;
        if (perpDist < 0.001f) perpDist = 0.001f;
        zBuf[x] = perpDist;

        int lineH = (int)(scrH / perpDist);
        int drawStart = -lineH / 2 + scrH / 2; if (drawStart < 0) drawStart = 0;
        int drawEnd   =  lineH / 2 + scrH / 2; if (drawEnd >= scrH) drawEnd = scrH - 1;

        /* texture coordinate */
        float wallX;
        if (side == 0) wallX = player.posY + perpDist * rdY;
        else           wallX = player.posX + perpDist * rdX;
        wallX -= floorf(wallX);

        int texX = (int)(wallX * TEX_SZ);
        if (side == 0 && rdX > 0) texX = TEX_SZ - texX - 1;
        if (side == 1 && rdY < 0) texX = TEX_SZ - texX - 1;

        int texIdx = worldMap[mapY][mapX] - 1;
        if (texIdx < 0 || texIdx >= N_WALL_TEX) texIdx = 0;

        float step   = (float)TEX_SZ / lineH;
        float texPos = (drawStart - scrH / 2.0f + lineH / 2.0f) * step;

        for (int y = drawStart; y <= drawEnd; y++) {
            int texY = (int)texPos & (TEX_SZ - 1);
            texPos += step;

            uint32_t c = wallTex[texIdx][texY * TEX_SZ + texX];

            /* EW-side darkening */
            if (side == 1) {
                uint8_t r = (c & 0xFF) >> 1;
                uint8_t g = ((c >> 8) & 0xFF) >> 1;
                uint8_t b = ((c >> 16) & 0xFF) >> 1;
                c = rgba(r, g, b);
            }

            /* distance-based shading (Stage 5) */
            if (lightingEnabled) {
                float shade = 1.0f / (1.0f + perpDist * perpDist * 0.04f);
                uint8_t r = (uint8_t)((c & 0xFF) * shade);
                uint8_t g = (uint8_t)(((c >> 8) & 0xFF) * shade);
                uint8_t b = (uint8_t)(((c >> 16) & 0xFF) * shade);
                c = rgba(r, g, b);
            }

            screenBuf[y * scrW + x] = c;
        }
    }
}

/* ===== minimap overlay (Stage 6) ===== */
static void drawMinimap() {
    int sz = 160;
    int ox = 10, oy = 10;
    float cw = (float)sz / mapWidth, ch = (float)sz / mapHeight;

    /* dark background */
    for (int y = oy; y < oy + sz && y < scrH; y++)
        for (int x = ox; x < ox + sz && x < scrW; x++)
            screenBuf[y * scrW + x] = rgba(0, 0, 0, 255);

    /* wall cells */
    for (int my = 0; my < mapHeight; my++)
        for (int mx = 0; mx < mapWidth; mx++) {
            if (worldMap[my][mx] == 0) continue;
            int sx = ox + (int)(mx * cw), sy = oy + (int)(my * ch);
            int ex = ox + (int)((mx + 1) * cw), ey = oy + (int)((my + 1) * ch);
            for (int y = sy; y < ey && y < scrH; y++)
                for (int x = sx; x < ex && x < scrW; x++)
                    screenBuf[y * scrW + x] = rgba(200, 200, 200);
        }

    /* ray lines */
    int px = ox + (int)(player.posX * cw);
    int py = oy + (int)(player.posY * ch);
    for (int col = 0; col < scrW; col += scrW / 10) {
        float camX = 2.0f * col / (float)scrW - 1.0f;
        float rx = player.dirX + player.planeX * camX;
        float ry = player.dirY + player.planeY * camX;
        float dist = zBuf[col];
        if (dist > 15) dist = 15;
        for (float t = 0; t < dist; t += 0.3f) {
            int x = px + (int)(rx * t * cw);
            int y = py + (int)(ry * t * ch);
            if (x >= ox && x < ox + sz && y >= oy && y < oy + sz)
                setPixel(x, y, rgba(0, 180, 0));
        }
    }

    /* player dot */
    for (int dy = -2; dy <= 2; dy++)
        for (int dx = -2; dx <= 2; dx++)
            setPixel(px + dx, py + dy, rgba(255, 0, 0));

    /* direction line */
    for (int i = 0; i < 12; i++)
        setPixel(px + (int)(player.dirX * i * 1.5f),
                 py + (int)(player.dirY * i * 1.5f), rgba(255, 255, 0));
}

/* ===== public API ===== */
void initRenderer(int w, int h) {
    scrW = w;
    scrH = h;
    screenBuf = new uint32_t[w * h];
    zBuf      = new float[w];

    genTextures();

    prog = createShaderProgram(vsrc, fsrc);

    glGenTextures(1, &glTex);
    glBindTexture(GL_TEXTURE_2D, glTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    float quad[] = {
        -1, -1,  0, 1,
         1, -1,  1, 1,
        -1,  1,  0, 0,
         1,  1,  1, 0,
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
    glBindVertexArray(0);

    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "tex"), 0);
}

void renderFrame() {
    drawBackground();
    castWalls();
    if (minimapEnabled) drawMinimap();

    glBindTexture(GL_TEXTURE_2D, glTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, scrW, scrH,
                    GL_RGBA, GL_UNSIGNED_BYTE, screenBuf);

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glTex);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void cleanupRenderer() {
    delete[] screenBuf;
    delete[] zBuf;
    for (int i = 0; i < N_WALL_TEX; i++) delete[] wallTex[i];
    delete[] floorTex;
    glDeleteTextures(1, &glTex);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
}