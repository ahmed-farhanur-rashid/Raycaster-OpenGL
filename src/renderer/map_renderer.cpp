/*
 * map_renderer.cpp — GPU-accelerated raycasting renderer
 *
 * The entire DDA raycasting, wall/floor/sky rendering, distance fog,
 * and minimap overlay run in a single fragment shader.  The CPU only
 * uploads map data and textures at init, then sets per-frame uniforms.
 */

#include <glad/glad.h>
#include "map_renderer.h"
#include "world_shaders.h"
#include "../map/map.h"
#include "shader.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <stb/stb_image.h>

/* ===== constants ===== */
#define TEX_SZ     512
#define N_WALL_TEX 4
#define SKY_W      512
#define SKY_H      128

/* ===== module state ===== */
static int scrW, scrH;
static unsigned int vao, vbo, prog;

/* GPU texture handles */
static unsigned int mapTexGL;       /* R8UI  – map grid                  */
static unsigned int wallTexArray;   /* RGBA8 – 2-D array (wall layers)   */
static unsigned int floorTexGL;     /* RGBA8 – floor                     */
static unsigned int skyTexGL;       /* RGBA8 – sky panorama              */

/* cached uniform locations */
static int uPlayerPos, uPlayerDir, uPlayerPlane, uPlayerHeight;
static int uScreenSize, uMapSize;
static int uLightingEnabled;



/* ================================================================== */
/*                     texture upload helpers                         */
/* ================================================================== */

/* Load a PNG, resample to destW×destH, upload to a new GL_TEXTURE_2D. */
static unsigned int loadAndUploadTex2D(const char* path,
                                       int destW, int destH,
                                       unsigned int wrapS, unsigned int wrapT) {
    int w, h, ch;
    unsigned char* data = stbi_load(path, &w, &h, &ch, 4);
    if (!data) { fprintf(stderr, "WARNING: could not load %s\n", path); return 0; }

    std::vector<uint8_t> buf(destW * destH * 4);
    for (int y = 0; y < destH; y++)
        for (int x = 0; x < destW; x++) {
            int sx = x * w / destW, sy = y * h / destH;
            memcpy(&buf[(y * destW + x) * 4], &data[(sy * w + sx) * 4], 4);
        }
    stbi_image_free(data);

    unsigned int texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, destW, destH, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
    return texID;
}

/* Create a 1×1 solid-colour fallback texture. */
static unsigned int makeFallbackTex(uint8_t r, uint8_t g, uint8_t b) {
    unsigned char px[4] = { r, g, b, 255 };
    unsigned int texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px);
    return texID;
}

/* ================================================================== */
/*                           public API                               */
/* ================================================================== */

namespace renderer {

void initRenderer(int w, int h) {
    scrW = w;
    scrH = h;

    /* ---- compile GPU program ---- */
    prog = shader::createShaderProgram(world_shaders::vertexSource,
                                       world_shaders::fragmentSource);

    uPlayerPos       = glGetUniformLocation(prog, "playerPos");
    uPlayerDir       = glGetUniformLocation(prog, "playerDir");
    uPlayerPlane     = glGetUniformLocation(prog, "playerPlane");
    uPlayerHeight    = glGetUniformLocation(prog, "playerHeight");
    uScreenSize      = glGetUniformLocation(prog, "screenSize");
    uMapSize         = glGetUniformLocation(prog, "mapSize");
    uLightingEnabled = glGetUniformLocation(prog, "lightingEnabled");

    /* ---- upload map grid as R8UI texture (unit 0) ---- */
    {
        std::vector<uint8_t> md(map::mapWidth * map::mapHeight);
        for (int y = 0; y < map::mapHeight; y++)
            for (int x = 0; x < map::mapWidth; x++)
                md[y * map::mapWidth + x] = (uint8_t)map::worldMap[y][x];

        glGenTextures(1, &mapTexGL);
        glBindTexture(GL_TEXTURE_2D, mapTexGL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, map::mapWidth, map::mapHeight, 0,
                     GL_RED_INTEGER, GL_UNSIGNED_BYTE, md.data());
    }

    /* ---- wall textures → 2-D array texture (unit 1) ---- */
    {
        glGenTextures(1, &wallTexArray);
        glBindTexture(GL_TEXTURE_2D_ARRAY, wallTexArray);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8,
                     TEX_SZ, TEX_SZ, N_WALL_TEX, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        static const char* wallPath = "resource/textures/wall/texture_3_dark_dirt_stones.png";
        int tw, th, tc;
        unsigned char* raw = stbi_load(wallPath, &tw, &th, &tc, 4);
        std::vector<uint8_t> buf(TEX_SZ * TEX_SZ * 4);

        if (raw) {
            for (int y = 0; y < TEX_SZ; y++)
                for (int x = 0; x < TEX_SZ; x++) {
                    int sx = x * tw / TEX_SZ, sy = y * th / TEX_SZ;
                    memcpy(&buf[(y * TEX_SZ + x) * 4], &raw[(sy * tw + sx) * 4], 4);
                }
            stbi_image_free(raw);
        } else {
            fprintf(stderr, "WARNING: could not load %s\n", wallPath);
            memset(buf.data(), 0x80, TEX_SZ * TEX_SZ * 4);
        }

        for (int i = 0; i < N_WALL_TEX; i++)
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i,
                            TEX_SZ, TEX_SZ, 1, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());
    }

    /* ---- floor texture (unit 2) ---- */
    floorTexGL = loadAndUploadTex2D("resource/textures/floor/texture_1_grass_yellow_flowers.png",
                                    TEX_SZ, TEX_SZ, GL_REPEAT, GL_REPEAT);
    if (!floorTexGL)
        floorTexGL = makeFallbackTex(50, 35, 15);

    /* ---- sky texture (unit 3) ---- */
    skyTexGL = loadAndUploadTex2D("resource/textures/sky/sky_0.png",
                                  SKY_W, SKY_H, GL_REPEAT, GL_CLAMP_TO_EDGE);
    if (!skyTexGL)
        skyTexGL = makeFallbackTex(10, 12, 25);

    /* ---- full-screen quad VAO/VBO ---- */
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
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    /* bind sampler units (constant) */
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "mapTex"),   0);
    glUniform1i(glGetUniformLocation(prog, "wallTex"),  1);
    glUniform1i(glGetUniformLocation(prog, "floorTex"), 2);
    glUniform1i(glGetUniformLocation(prog, "skyTex"),   3);
}

void renderFrame(const RenderState& state) {
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);

    /* per-frame uniforms */
    glUniform2f(uPlayerPos,   state.posX,   state.posY);
    glUniform2f(uPlayerDir,   state.dirX,   state.dirY);
    glUniform2f(uPlayerPlane, state.planeX, state.planeY);
    glUniform1f(uPlayerHeight, state.posZ);
    glUniform2f(uScreenSize,  (float)scrW,            (float)scrH);
    glUniform2i(uMapSize,     map::mapWidth,           map::mapHeight);
    glUniform1i(uLightingEnabled, state.lightingEnabled ? 1 : 0);

    /* bind textures to their units */
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,       mapTexGL);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D_ARRAY, wallTexArray);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D,       floorTexGL);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D,       skyTexGL);

    /* draw main scene */
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void cleanupRenderer() {
    glDeleteTextures(1, &mapTexGL);
    glDeleteTextures(1, &wallTexArray);
    glDeleteTextures(1, &floorTexGL);
    glDeleteTextures(1, &skyTexGL);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
}

} // namespace renderer