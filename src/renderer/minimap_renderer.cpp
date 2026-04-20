/*
 * minimap_renderer.cpp — CPU-side minimap overlay
 *
 * Fills a 160×160 RGBA buffer each frame and renders it as a
 * screen-space quad in the top-left corner.
 */

#include <glad/glad.h>
#include "minimap_renderer.h"
#include "../map/map.h"
#include "../settings/settings.h"
#include "shader.h"
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <vector>

/* ===== constants ===== */
static int mmSz = 160;

/* ===== module state ===== */
static int scrW, scrH;
static unsigned int minimapTexGL;
static unsigned int mmVao, mmVbo, mmProg;
static std::vector<uint8_t> mmPixels;

/* ===== shaders ===== */
static const char* mmVsrc = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
out vec2 uv;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    uv = aUV;
}
)";

static const char* mmFsrc = R"(
#version 330 core
in  vec2 uv;
out vec4 fragColor;
uniform sampler2D mmTex;
void main() {
    fragColor = texture(mmTex, uv);
}
)";

/* ===== helpers ===== */

static int cpuGetMap(int x, int y) {
    if (x < 0 || x >= map::mapWidth || y < 0 || y >= map::mapHeight) return 1;
    return map::worldMap[y][x];
}

/* CPU DDA ray cast — returns perpendicular distance (1e30 on miss). */
static float cpuCastRay(float posX, float posY, float rdx, float rdy) {
    int mapX = (int)posX, mapY = (int)posY;

    float ddX = (rdx == 0.0f) ? 1e30f : fabsf(1.0f / rdx);
    float ddY = (rdy == 0.0f) ? 1e30f : fabsf(1.0f / rdy);
    float sdX, sdY;
    int   stepX, stepY;
    int   side = 0;

    if (rdx < 0.0f) { stepX = -1; sdX = (posX - (float)mapX) * ddX; }
    else             { stepX =  1; sdX = ((float)mapX + 1.0f - posX) * ddX; }
    if (rdy < 0.0f) { stepY = -1; sdY = (posY - (float)mapY) * ddY; }
    else             { stepY =  1; sdY = ((float)mapY + 1.0f - posY) * ddY; }

    for (int i = 0; i < 256; i++) {
        if (sdX < sdY) { sdX += ddX; mapX += stepX; side = 0; }
        else            { sdY += ddY; mapY += stepY; side = 1; }
        if (cpuGetMap(mapX, mapY) > 0) {
            float dist;
            if (side == 0) dist = ((float)mapX - posX + (float)(1 - stepX) * 0.5f) / rdx;
            else           dist = ((float)mapY - posY + (float)(1 - stepY) * 0.5f) / rdy;
            if (dist < 0.001f) dist = 0.001f;
            return dist;
        }
    }
    return 1e30f;
}

static void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || x >= mmSz || y < 0 || y >= mmSz) return;
    int idx = (y * mmSz + x) * 4;
    mmPixels[idx + 0] = r;
    mmPixels[idx + 1] = g;
    mmPixels[idx + 2] = b;
    mmPixels[idx + 3] = 255;
}

static void updateMinimapTexture(const renderer::RenderState& state) {
    float mw = (float)map::mapWidth;
    float mh = (float)map::mapHeight;
    float cpp = (float)(map::mapWidth > map::mapHeight ? map::mapWidth : map::mapHeight) / (float)mmSz;

    /* background: wall cells light grey, floor cells black */
    for (int y = 0; y < mmSz; y++)
        for (int x = 0; x < mmSz; x++) {
            float wx = (float)x / (float)mmSz * mw;
            float wy = (float)y / (float)mmSz * mh;
            bool wall = cpuGetMap((int)wx, (int)wy) > 0;
            uint8_t c = wall ? 199 : 0; /* 0.78 * 255 ≈ 199 */
            setPixel(x, y, c, c, c);
        }

    /* 11 FOV ray lines — green */
    for (int r = 0; r <= 10; r++) {
        float rc  = 2.0f * (float)r / 10.0f - 1.0f;
        float rdx = state.dirX + state.planeX * rc;
        float rdy = state.dirY + state.planeY * rc;
        float rLen = sqrtf(rdx * rdx + rdy * rdy);
        if (rLen < 0.001f) continue;

        float dist = cpuCastRay(state.posX, state.posY, rdx, rdy);
        if (dist > 15.0f) dist = 15.0f;

        /* walk along the ray in small steps and paint pixels */
        float step = cpp * 0.5f;
        float nrdx = rdx / rLen, nrdy = rdy / rLen;
        for (float t = 0.0f; t < dist * rLen; t += step) {
            float wx = state.posX + nrdx * t;
            float wy = state.posY + nrdy * t;
            int px = (int)(wx / mw * (float)mmSz);
            int py = (int)(wy / mh * (float)mmSz);
            setPixel(px, py, 0, 178, 0); /* vec3(0.0, 0.7, 0.0) */
        }
    }

    /* player dot — red, 2-pixel radius */
    int pcx = (int)(state.posX / mw * (float)mmSz);
    int pcy = (int)(state.posY / mh * (float)mmSz);
    int rad = (int)(cpp * 2.5f + 0.5f);
    if (rad < 2) rad = 2;
    for (int dy = -rad; dy <= rad; dy++)
        for (int dx = -rad; dx <= rad; dx++)
            if (dx * dx + dy * dy <= rad * rad)
                setPixel(pcx + dx, pcy + dy, 255, 0, 0);

    /* direction line — yellow, 3 cells long */
    {
        float lineLen = 3.0f;
        float dLen = sqrtf(state.dirX * state.dirX + state.dirY * state.dirY);
        if (dLen < 0.001f) dLen = 0.001f;
        float ndx = state.dirX / dLen, ndy = state.dirY / dLen;
        float step = cpp * 0.5f;
        for (float t = 0.0f; t < lineLen; t += step) {
            float wx = state.posX + ndx * t;
            float wy = state.posY + ndy * t;
            int px = (int)(wx / mw * (float)mmSz);
            int py = (int)(wy / mh * (float)mmSz);
            setPixel(px, py, 255, 255, 0);
        }
    }

    /* upload to GPU */
    glBindTexture(GL_TEXTURE_2D, minimapTexGL);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mmSz, mmSz,
                    GL_RGBA, GL_UNSIGNED_BYTE, mmPixels.data());
}

/* ===== public API ===== */

namespace minimap {

void initMinimap(int w, int h) {
    scrW = w;
    scrH = h;

    mmSz = settings::getInt("minimap_size", 160);
    mmPixels.resize(mmSz * mmSz * 4);

    /* texture */
    glGenTextures(1, &minimapTexGL);
    glBindTexture(GL_TEXTURE_2D, minimapTexGL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mmSz, mmSz, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    /* shader */
    mmProg = shader::createShaderProgram(mmVsrc, mmFsrc);
    glUseProgram(mmProg);
    glUniform1i(glGetUniformLocation(mmProg, "mmTex"), 0);

    /* quad — top-left corner */
    float mmOx = settings::getFloat("minimap_margin", 10.0f);
    float mmOy = mmOx;
    float l = mmOx / (float)scrW * 2.0f - 1.0f;
    float r = (mmOx + (float)mmSz) / (float)scrW * 2.0f - 1.0f;
    float t = 1.0f - mmOy / (float)scrH * 2.0f;
    float b = 1.0f - (mmOy + (float)mmSz) / (float)scrH * 2.0f;

    float mmQuad[] = {
        l, b,  0, 1,
        r, b,  1, 1,
        l, t,  0, 0,
        r, t,  1, 0,
    };
    glGenVertexArrays(1, &mmVao);
    glGenBuffers(1, &mmVbo);
    glBindVertexArray(mmVao);
    glBindBuffer(GL_ARRAY_BUFFER, mmVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(mmQuad), mmQuad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

void renderMinimap(const renderer::RenderState& state) {
    if (!state.minimapEnabled) return;

    updateMinimapTexture(state);

    glUseProgram(mmProg);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, minimapTexGL);
    glBindVertexArray(mmVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void cleanupMinimap() {
    glDeleteTextures(1, &minimapTexGL);
    glDeleteVertexArrays(1, &mmVao);
    glDeleteBuffers(1, &mmVbo);
    glDeleteProgram(mmProg);
}

} // namespace minimap
