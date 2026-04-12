/*
 * map_renderer.cpp — GPU-accelerated raycasting renderer
 *
 * The entire DDA raycasting, wall/floor/sky rendering, distance fog,
 * and minimap overlay run in a single fragment shader.  The CPU only
 * uploads map data and textures at init, then sets per-frame uniforms.
 */

#include <glad/glad.h>
#include "map_renderer.h"
#include "../map/map.h"
#include "../player/player.h"
#include "../core/input.h"
#include "../entities/projectile.h"
#include "../entities/enemy.h"
#include "sprite_registry.h"
#include "shader.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
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
static unsigned int spriteTexArray; // RGBA8 2D array — one layer per sprite type

/* cached uniform locations */
static int uPlayerPos, uPlayerDir, uPlayerPlane;
static int uScreenSize, uMapSize;
static int uLightingEnabled, uMinimapEnabled;
static int uPlayerHeight;
static int uSpritePos, uSpriteType, uNumSprites;

/* ================================================================== */
/*                           GLSL shaders                             */
/* ================================================================== */

static const char* vsrc = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
out vec2 uv;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    uv = aUV;
}
)";

static const char* fsrc = R"(
#version 330 core
in  vec2 uv;
out vec4 fragColor;

uniform vec2  playerPos;
uniform vec2  playerDir;
uniform vec2  playerPlane;
uniform vec2  screenSize;
uniform ivec2 mapSize;
uniform bool  lightingEnabled;
uniform bool  minimapEnabled;
uniform float playerHeight;   // Player's Z position (jumping)

uniform usampler2D    mapTex;
uniform sampler2DArray wallTex;
uniform sampler2D     floorTex;
uniform sampler2D     skyTex;
uniform sampler2DArray spriteTex;
uniform vec2  spritePos[64];
uniform float spriteZ[64];   // vertical offset for jumping
uniform int   spriteType[64];
uniform int   numSprites;

#define PI        3.14159265359
#define MAX_STEPS 256

/* ---- helpers ---- */
int getMap(int x, int y) {
    if (x < 0 || x >= mapSize.x || y < 0 || y >= mapSize.y) return 1;
    return int(texelFetch(mapTex, ivec2(x, y), 0).r);
}

/* DDA ray cast.  Returns wall type (>0 on hit, 0 on miss). */
int castRay(vec2 rd, out int side, out float perpDist) {
    int mapX = int(playerPos.x), mapY = int(playerPos.y);

    float ddX = (rd.x == 0.0) ? 1e30 : abs(1.0 / rd.x);
    float ddY = (rd.y == 0.0) ? 1e30 : abs(1.0 / rd.y);
    float sdX, sdY;
    int   stepX, stepY;

    if (rd.x < 0.0) { stepX = -1; sdX = (playerPos.x - float(mapX)) * ddX; }
    else             { stepX =  1; sdX = (float(mapX) + 1.0 - playerPos.x) * ddX; }
    if (rd.y < 0.0) { stepY = -1; sdY = (playerPos.y - float(mapY)) * ddY; }
    else             { stepY =  1; sdY = (float(mapY) + 1.0 - playerPos.y) * ddY; }

    side = 0;
    for (int i = 0; i < MAX_STEPS; i++) {
        if (sdX < sdY) { sdX += ddX; mapX += stepX; side = 0; }
        else            { sdY += ddY; mapY += stepY; side = 1; }
        int cell = getMap(mapX, mapY);
        if (cell > 0) {
            if (side == 0) perpDist = (float(mapX) - playerPos.x + float(1 - stepX) * 0.5) / rd.x;
            else           perpDist = (float(mapY) - playerPos.y + float(1 - stepY) * 0.5) / rd.y;
            if (perpDist < 0.001) perpDist = 0.001;
            return cell;
        }
    }
    perpDist = 1e30;
    return 0;
}

/* ---- main ---- */
void main() {
    /* pixel coords: (0,0) = top-left */
    float px = uv.x * screenSize.x;
    float py = uv.y * screenSize.y;
    int   ix = int(px);
    int   iy = int(py);
    int   halfH = int(screenSize.y) / 2;
    int   horizon = halfH;  // Horizon at screen center (no pitch offset)

    /* minimap bounds */
    const int mmSz = 160, mmOx = 10, mmOy = 10;
    bool inMinimap = minimapEnabled
                  && ix >= mmOx && ix < mmOx + mmSz
                  && iy >= mmOy && iy < mmOy + mmSz;

    vec3 color;

    if (inMinimap) {
        /* ============ MINIMAP ============ */
        float worldX = float(ix - mmOx) / float(mmSz) * float(mapSize.x);
        float worldY = float(iy - mmOy) / float(mmSz) * float(mapSize.y);
        float cpp    = float(max(mapSize.x, mapSize.y)) / float(mmSz);

        color = vec3(0.0);                     /* black background */

        /* wall cells */
        if (getMap(int(worldX), int(worldY)) > 0)
            color = vec3(0.78);

        /* 11 sample-ray lines */
        for (int r = 0; r <= 10; r++) {
            float rc  = 2.0 * float(r) / 10.0 - 1.0;
            vec2  rDir = playerDir + playerPlane * rc;
            float rLen = length(rDir);
            if (rLen < 0.001) continue;

            int rSide; float rDist;
            castRay(rDir, rSide, rDist);
            if (rDist > 15.0) rDist = 15.0;

            vec2  d = vec2(worldX, worldY) - playerPos;
            float t = dot(d, rDir) / dot(rDir, rDir);
            float crossDist = abs(d.x * rDir.y - d.y * rDir.x) / rLen;
            if (t > 0.0 && t < rDist && crossDist < cpp * 0.7)
                color = vec3(0.0, 0.7, 0.0);
        }

        /* player dot */
        if (length(vec2(worldX, worldY) - playerPos) < cpp * 2.5)
            color = vec3(1.0, 0.0, 0.0);

        /* direction line */
        vec2  dv = vec2(worldX, worldY) - playerPos;
        float dt = dot(dv, playerDir);
        float dc = abs(dv.x * playerDir.y - dv.y * playerDir.x);
        if (dt > 0.0 && dt < 3.0 && dc < cpp * 0.7)
            color = vec3(1.0, 1.0, 0.0);

    } else {
        /* ============ MAIN RAYCASTING ============ */
        float camX = 2.0 * px / screenSize.x - 1.0;
        vec2  rd   = playerDir + playerPlane * camX;

        int   side;
        float perpDist;
        int   wallType = castRay(rd, side, perpDist);
        bool  hit      = wallType > 0;

        int lineH     = (perpDist < 1e20) ? int(screenSize.y / perpDist) : 0;
        
        // Vertical offset for jumping - creates parallax effect
        // Objects closer move more than distant objects
        float vertOffset = (playerHeight / perpDist) * halfH;
        int drawStart = -lineH / 2 + horizon + int(vertOffset);
        int drawEnd   =  lineH / 2 + horizon + int(vertOffset);

        if (iy < drawStart || !hit) {
            /* ---- SKY ---- */
            float angle = atan(rd.y, rd.x);
            float u = (angle + PI) / (2.0 * PI);
            float v = clamp(float(iy) / float(halfH), 0.0, 1.0);
            color = texture(skyTex, vec2(u, v)).rgb;

        } else if (iy > drawEnd) {
            /* ---- FLOOR ---- */
            // For floor, we need to calculate the distance this pixel represents
            // The relationship is: iy = horizon + (screenHeight/2) / rowDist + vertOffset
            // where vertOffset = (playerHeight / rowDist) * halfH
            // So: iy - horizon = (halfH / rowDist) + (playerHeight * halfH / rowDist)
            //     iy - horizon = halfH * (1 + playerHeight) / rowDist
            // Therefore: rowDist = halfH * (1 + playerHeight) / (iy - horizon)
            
            int p = iy - horizon;
            if (p < 1) p = 1;
            
            // Calculate row distance with height compensation
            float rowDist = (halfH * (1.0 + playerHeight)) / float(p);
            vec2  f = playerPos + rowDist * rd;
            color = texture(floorTex, fract(f)).rgb;

            if (lightingEnabled) {
                float shade = 1.0 / (1.0 + rowDist * rowDist * 0.04);
                color *= shade;
            }

        } else {
            /* ---- WALL ---- */
            float wallX = (side == 0)
                        ? playerPos.y + perpDist * rd.y
                        : playerPos.x + perpDist * rd.x;
            wallX = fract(wallX);

            float texU = wallX;
            if (side == 0 && rd.x > 0.0) texU = 1.0 - texU;
            if (side == 1 && rd.y < 0.0) texU = 1.0 - texU;

            float texV = clamp(float(iy - drawStart) / float(max(lineH, 1)), 0.0, 1.0);

            int texIdx = wallType - 1;
            if (texIdx < 0 || texIdx >= 4) texIdx = 0;

            color = texture(wallTex, vec3(texU, texV, float(texIdx))).rgb;

            /* EW-side darkening */
            if (side == 1) color *= 0.5;

            /* distance fog */
            if (lightingEnabled) {
                float shade = 1.0 / (1.0 + perpDist * perpDist * 0.04);
                color *= shade;
            }
        }

        // ---- SPRITE PASS ----
        for (int i = 0; i < numSprites; i++) {
            vec2 sp = spritePos[i];

            // Transform sprite into camera space
            vec2  d       = sp - playerPos;
            float invDet  = 1.0 / (playerPlane.x * playerDir.y - playerDir.x * playerPlane.y);
            float transX  = invDet * (playerDir.y * d.x - playerDir.x * d.y);
            float transY  = invDet * (-playerPlane.y * d.x + playerPlane.x * d.y);

            if (transY <= 0.0) continue;            // behind camera
            if (transY >= perpDist) continue;       // behind a wall

            // Screen X of sprite centre
            float sprScreenX = (0.5 + transX / transY * 0.5) * screenSize.x;

            // Sprite height & width on screen
            float sprH = abs(screenSize.y / transY);
            float sprW = sprH;  // square sprites

            float drawStartX = sprScreenX - sprW * 0.5;
            float drawEndX   = sprScreenX + sprW * 0.5;
            if (px < drawStartX || px >= drawEndX) continue;

            // Sprite vertical position with parallax effect
            // Sprites shift based on distance - closer sprites move more
            float sprVertOffset = (playerHeight / transY) * halfH;
            
            float drawStartY = -sprH * 0.5 + horizon + sprVertOffset;
            float drawEndY   = sprH * 0.5 + horizon + sprVertOffset;
            if (py < drawStartY || py >= drawEndY) continue;

            // UV
            float u = (px - drawStartX) / sprW;
            float v = (py - drawStartY) / sprH;
            vec4 sc = texture(spriteTex, vec3(u, v, float(spriteType[i])));

            if (sc.a > 0.5) color = sc.rgb;  // alpha clip
        }
    }

    fragColor = vec4(color, 1.0);
}
)";

/* ================================================================== */
/*                     texture upload helpers                         */
/* ================================================================== */

/* Load a PNG, resample to destW×destH, upload to a new GL_TEXTURE_2D. */
static bool loadAndUploadTex2D(const char* path, unsigned int* texID,
                                int destW, int destH,
                                unsigned int wrapS, unsigned int wrapT) {
    int w, h, ch;
    unsigned char* data = stbi_load(path, &w, &h, &ch, 4);
    if (!data) { fprintf(stderr, "WARNING: could not load %s\n", path); return false; }

    unsigned char* buf = new unsigned char[destW * destH * 4];
    for (int y = 0; y < destH; y++)
        for (int x = 0; x < destW; x++) {
            int sx = x * w / destW, sy = y * h / destH;
            memcpy(&buf[(y * destW + x) * 4], &data[(sy * w + sx) * 4], 4);
        }
    stbi_image_free(data);

    glGenTextures(1, texID);
    glBindTexture(GL_TEXTURE_2D, *texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, destW, destH, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, buf);
    delete[] buf;
    return true;
}

/* Create a 1×1 solid-colour fallback texture. */
static void makeFallbackTex(unsigned int* texID, uint8_t r, uint8_t g, uint8_t b) {
    unsigned char px[4] = { r, g, b, 255 };
    glGenTextures(1, texID);
    glBindTexture(GL_TEXTURE_2D, *texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px);
}

/* ================================================================== */
/*                           public API                               */
/* ================================================================== */

namespace renderer {

void initRenderer(int w, int h) {
    scrW = w;
    scrH = h;

    /* ---- compile GPU program ---- */
    prog = shader::createShaderProgram(vsrc, fsrc);

    uPlayerPos       = glGetUniformLocation(prog, "playerPos");
    uPlayerDir       = glGetUniformLocation(prog, "playerDir");
    uPlayerPlane     = glGetUniformLocation(prog, "playerPlane");
    uScreenSize      = glGetUniformLocation(prog, "screenSize");
    uMapSize         = glGetUniformLocation(prog, "mapSize");
    uLightingEnabled = glGetUniformLocation(prog, "lightingEnabled");
    uMinimapEnabled  = glGetUniformLocation(prog, "minimapEnabled");
    uPlayerHeight    = glGetUniformLocation(prog, "playerHeight");
    uSpritePos  = glGetUniformLocation(prog, "spritePos");
    uSpriteType = glGetUniformLocation(prog, "spriteType");
    uNumSprites = glGetUniformLocation(prog, "numSprites");

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

        static const char* wallPath = "resource/textures/texture_3_dark_dirt_stones.png";
        int tw, th, tc;
        unsigned char* raw = stbi_load(wallPath, &tw, &th, &tc, 4);
        unsigned char* buf = new unsigned char[TEX_SZ * TEX_SZ * 4];

        if (raw) {
            for (int y = 0; y < TEX_SZ; y++)
                for (int x = 0; x < TEX_SZ; x++) {
                    int sx = x * tw / TEX_SZ, sy = y * th / TEX_SZ;
                    memcpy(&buf[(y * TEX_SZ + x) * 4], &raw[(sy * tw + sx) * 4], 4);
                }
            stbi_image_free(raw);
        } else {
            fprintf(stderr, "WARNING: could not load %s\n", wallPath);
            memset(buf, 0x80, TEX_SZ * TEX_SZ * 4);
        }

        for (int i = 0; i < N_WALL_TEX; i++)
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i,
                            TEX_SZ, TEX_SZ, 1, GL_RGBA, GL_UNSIGNED_BYTE, buf);
        delete[] buf;
    }

    /* ---- floor texture (unit 2) ---- */
    if (!loadAndUploadTex2D("resource/textures/texture_3_grass_blue_flowers.png",
                            &floorTexGL, TEX_SZ, TEX_SZ, GL_REPEAT, GL_REPEAT))
        makeFallbackTex(&floorTexGL, 50, 35, 15);

    /* ---- sky texture (unit 3) ---- */
    if (!loadAndUploadTex2D("resource/textures/sky_0.png",
                            &skyTexGL, SKY_W, SKY_H, GL_REPEAT, GL_CLAMP_TO_EDGE))
        makeFallbackTex(&skyTexGL, 10, 12, 25);

    // Initialize sprite registry
    sprite::initSpriteRegistry();
    
    // Load all sprites using registry
    spriteTexArray = sprite::loadSpriteTextures();

    // Bind to unit 4
    glUniform1i(glGetUniformLocation(prog, "spriteTex"), 4);

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
    glUniform1i(glGetUniformLocation(prog, "spriteTex"), 4);
}

void uploadMapTexture() {
    /* ---- upload map grid as R8UI texture (unit 0) ---- */
    if (map::mapWidth > 0 && map::mapHeight > 0) {
        uint8_t* md = new uint8_t[map::mapWidth * map::mapHeight];
        for (int y = 0; y < map::mapHeight; y++)
            for (int x = 0; x < map::mapWidth; x++)
                md[y * map::mapWidth + x] = (uint8_t)map::worldMap[y][x];

        glGenTextures(1, &mapTexGL);
        glBindTexture(GL_TEXTURE_2D, mapTexGL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, map::mapWidth, map::mapHeight, 0,
                     GL_RED_INTEGER, GL_UNSIGNED_BYTE, md);
        delete[] md;
        printf("Uploaded map texture: %dx%d\n", map::mapWidth, map::mapHeight);
    }
}

void renderFrame() {
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);

    /* per-frame uniforms */
    glUniform2f(uPlayerPos,   player::player.posX,   player::player.posY);
    glUniform2f(uPlayerDir,   player::player.dirX,   player::player.dirY);
    glUniform2f(uPlayerPlane, player::player.planeX, player::player.planeY);
    glUniform2f(uScreenSize,  (float)scrW,            (float)scrH);
    glUniform2i(uMapSize,     map::mapWidth,           map::mapHeight);
    glUniform1i(uLightingEnabled, input::lightingEnabled ? 1 : 0);
    glUniform1i(uMinimapEnabled,  input::minimapEnabled  ? 1 : 0);

    // Pass player height for parallax jumping effect
    glUniform1f(uPlayerHeight, player::player.posZ);

    // Combine map sprites, projectiles, and enemies
    int totalSprites = 0;
    struct SpriteData {
        float x, y;
        int type;
    } allSprites[MAX_SPRITES + MAX_PROJECTILES + MAX_ENEMIES];

    // Map props
    for (int i = 0; i < map::numSprites && totalSprites < MAX_SPRITES + MAX_PROJECTILES + MAX_ENEMIES; i++) {
        allSprites[totalSprites++] = { map::mapSprites[i].x, map::mapSprites[i].y, map::mapSprites[i].textureId };
    }

    // Active projectiles
    for (int i = 0; i < projectile::numProjectiles && totalSprites < MAX_SPRITES + MAX_PROJECTILES + MAX_ENEMIES; i++) {
        if (!projectile::projectiles[i].active) continue;
        allSprites[totalSprites++] = { projectile::projectiles[i].x, projectile::projectiles[i].y, projectile::projectiles[i].spriteType };
        if (!projectile::projectiles[i].fromPlayer) {
            printf("Rendering enemy bullet at (%.1f, %.1f)\n", projectile::projectiles[i].x, projectile::projectiles[i].y);
        }
    }

    // Living and dead enemies (show dead bodies)
    for (int i = 0; i < enemy::numEnemies && totalSprites < MAX_SPRITES + MAX_PROJECTILES + MAX_ENEMIES; i++) {
        if (enemy::enemies[i].state == enemy::State::Dead && enemy::enemies[i].deathTimer >= 2.0f) continue;
        allSprites[totalSprites++] = { enemy::enemies[i].x, enemy::enemies[i].y, enemy::enemies[i].currentSprite };
    }

    // Sort farthest first
    struct SortedSprite {
        float dist;
        int   idx;
    };
    SortedSprite sorted[MAX_SPRITES + MAX_PROJECTILES + MAX_ENEMIES];
    for (int i = 0; i < totalSprites; i++) {
        float dx = allSprites[i].x - player::player.posX;
        float dy = allSprites[i].y - player::player.posY;
        sorted[i] = { dx*dx + dy*dy, i };
    }
    // Insertion sort
    for (int i = 1; i < totalSprites; i++) {
        SortedSprite key = sorted[i];
        int j = i - 1;
        while (j >= 0 && sorted[j].dist < key.dist) { sorted[j+1] = sorted[j]; j--; }
        sorted[j+1] = key;
    }

    // Upload sorted positions, types
    float spritePosData[(MAX_SPRITES + MAX_PROJECTILES + MAX_ENEMIES) * 2];
    int   spriteTypeData[MAX_SPRITES + MAX_PROJECTILES + MAX_ENEMIES];
    for (int i = 0; i < totalSprites; i++) {
        int idx = sorted[i].idx;
        spritePosData[i*2+0] = allSprites[idx].x;
        spritePosData[i*2+1] = allSprites[idx].y;
        spriteTypeData[i]    = allSprites[idx].type;
    }
    glUniform2fv(uSpritePos,  totalSprites, spritePosData);
    glUniform1iv(uSpriteType, totalSprites, spriteTypeData);
    glUniform1i (uNumSprites, totalSprites);

    /* bind textures to their units */
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,       mapTexGL);
    glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D_ARRAY, wallTexArray);
    glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D,       floorTexGL);
    glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D,       skyTexGL);
    glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D_ARRAY, spriteTexArray);

    /* draw - explicitly bind VAO to prevent state pollution */
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);  // <-- ADD: unbind after drawing
}

void cleanupRenderer() {
    glDeleteTextures(1, &mapTexGL);
    glDeleteTextures(1, &wallTexArray);
    glDeleteTextures(1, &floorTexGL);
    glDeleteTextures(1, &skyTexGL);
    glDeleteTextures(1, &spriteTexArray);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
}

} // namespace renderer