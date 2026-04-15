/*
 * entity_renderer.cpp — Billboard sprite renderer for projectiles
 *
 * Uses a CPU raycaster Z-buffer to correctly depth-test sprites against walls.
 * Projectiles are sorted back-to-front, then rendered as screen-space quads
 * with alpha-test.
 */

#include <glad/glad.h>
#include "entity_renderer.h"
#include "shader.h"
#include "../entity/entity.h"
#include "../map/map.h"
#include "../settings/settings.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

/* ===== constants ===== */
#define PROJ_TEX_SZ 16

/* ===== module state ===== */
static int scrW, scrH;
static unsigned int entProg, entVao, entVbo;
static unsigned int projTexGL;      /* bullet projectile sprite            */
static unsigned int projBeamTexGL;  /* energy beam sprite                  */
static unsigned int projShotTexGL;  /* shotgun blast sprite                */
static std::vector<float> zBuffer;  /* per-column wall distance            */

/* ===== shaders ===== */
static const char* evsrc = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
out vec2 uv;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    uv = aUV;
}
)";

static const char* efsrc = R"(
#version 330 core
in  vec2 uv;
out vec4 fragColor;
uniform sampler2D spriteTex;
uniform vec4 tintColor;
void main() {
    vec4 c = texture(spriteTex, uv);
    if (c.a < 0.1) discard;
    fragColor = vec4(c.rgb * tintColor.rgb, c.a * tintColor.a);
}
)";

/* ===== texture generation ===== */

/* Generate a simple hardcoded projectile texture (bright circle) */
static unsigned int makeProjectileTex() {
    const int sz = PROJ_TEX_SZ;
    std::vector<uint8_t> px(sz * sz * 4, 0);
    float half = (float)sz / 2.0f;
    for (int y = 0; y < sz; y++)
        for (int x = 0; x < sz; x++) {
            float dx = (float)x - half + 0.5f;
            float dy = (float)y - half + 0.5f;
            float d = sqrtf(dx * dx + dy * dy);
            int idx = (y * sz + x) * 4;
            if (d < half * 0.7f) {
                float t = d / (half * 0.7f);
                px[idx + 0] = 255;
                px[idx + 1] = (uint8_t)(255 - (int)(t * 80));
                px[idx + 2] = (uint8_t)(100 - (int)(t * 80));
                if (px[idx + 2] > 255) px[idx + 2] = 0;
                px[idx + 3] = 255;
            } else if (d < half) {
                float t = (d - half * 0.7f) / (half * 0.3f);
                px[idx + 0] = (uint8_t)(255 * (1.0f - t));
                px[idx + 1] = (uint8_t)(120 * (1.0f - t));
                px[idx + 2] = 0;
                px[idx + 3] = (uint8_t)(200 * (1.0f - t));
            }
        }
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, sz, sz, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    return tex;
}

/* Generate blue energy beam texture */
static unsigned int makeBeamTex() {
    const int sz = PROJ_TEX_SZ;
    std::vector<uint8_t> px(sz * sz * 4, 0);
    float half = (float)sz / 2.0f;
    for (int y = 0; y < sz; y++)
        for (int x = 0; x < sz; x++) {
            float dx = (float)x - half + 0.5f;
            float dy = (float)y - half + 0.5f;
            float d = sqrtf(dx * dx + dy * dy);
            int idx = (y * sz + x) * 4;
            if (d < half * 0.5f) {
                /* bright white-blue core */
                px[idx + 0] = 180;
                px[idx + 1] = 220;
                px[idx + 2] = 255;
                px[idx + 3] = 255;
            } else if (d < half * 0.8f) {
                float t = (d - half * 0.5f) / (half * 0.3f);
                px[idx + 0] = (uint8_t)(80 * (1.0f - t));
                px[idx + 1] = (uint8_t)(140 * (1.0f - t));
                px[idx + 2] = (uint8_t)(255 * (1.0f - t * 0.3f));
                px[idx + 3] = (uint8_t)(255 * (1.0f - t * 0.5f));
            } else if (d < half) {
                float t = (d - half * 0.8f) / (half * 0.2f);
                px[idx + 0] = 0;
                px[idx + 1] = 0;
                px[idx + 2] = (uint8_t)(180 * (1.0f - t));
                px[idx + 3] = (uint8_t)(120 * (1.0f - t));
            }
        }
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, sz, sz, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    return tex;
}

/* Generate orange shotgun blast texture */
static unsigned int makeShotgunTex() {
    const int sz = PROJ_TEX_SZ;
    std::vector<uint8_t> px(sz * sz * 4, 0);
    float half = (float)sz / 2.0f;
    for (int y = 0; y < sz; y++)
        for (int x = 0; x < sz; x++) {
            float dx = (float)x - half + 0.5f;
            float dy = (float)y - half + 0.5f;
            float d = sqrtf(dx * dx + dy * dy);
            int idx = (y * sz + x) * 4;
            if (d < half * 0.6f) {
                px[idx + 0] = 255;
                px[idx + 1] = 200;
                px[idx + 2] = 80;
                px[idx + 3] = 255;
            } else if (d < half) {
                float t = (d - half * 0.6f) / (half * 0.4f);
                px[idx + 0] = (uint8_t)(255 * (1.0f - t));
                px[idx + 1] = (uint8_t)(120 * (1.0f - t));
                px[idx + 2] = 0;
                px[idx + 3] = (uint8_t)(220 * (1.0f - t));
            }
        }
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, sz, sz, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    return tex;
}

/* ===== CPU-side wall distance z-buffer ===== */

static void buildZBuffer(const renderer::RenderState& s) {
    zBuffer.resize(scrW);
    for (int x = 0; x < scrW; x++) {
        float camX = 2.0f * (float)x / (float)scrW - 1.0f;
        float rdx = s.dirX + s.planeX * camX;
        float rdy = s.dirY + s.planeY * camX;

        int mapX = (int)s.posX, mapY = (int)s.posY;
        float ddX = (rdx == 0.0f) ? 1e30f : fabsf(1.0f / rdx);
        float ddY = (rdy == 0.0f) ? 1e30f : fabsf(1.0f / rdy);
        float sdX, sdY;
        int stepX, stepY, side = 0;

        if (rdx < 0.0f) { stepX = -1; sdX = (s.posX - (float)mapX) * ddX; }
        else             { stepX =  1; sdX = ((float)mapX + 1.0f - s.posX) * ddX; }
        if (rdy < 0.0f) { stepY = -1; sdY = (s.posY - (float)mapY) * ddY; }
        else             { stepY =  1; sdY = ((float)mapY + 1.0f - s.posY) * ddY; }

        float perpDist = 1e30f;
        for (int i = 0; i < 256; i++) {
            if (sdX < sdY) { sdX += ddX; mapX += stepX; side = 0; }
            else            { sdY += ddY; mapY += stepY; side = 1; }
            if (mapX < 0 || mapX >= map::mapWidth || mapY < 0 || mapY >= map::mapHeight) break;
            if (map::worldMap[mapY][mapX] > 0) {
                if (side == 0) perpDist = ((float)mapX - s.posX + (float)(1 - stepX) * 0.5f) / rdx;
                else           perpDist = ((float)mapY - s.posY + (float)(1 - stepY) * 0.5f) / rdy;
                break;
            }
        }
        zBuffer[x] = perpDist;
    }
}

/* helper: draw a single sprite (accounts for z-buffer) */
static void drawSprite(float screenX, float screenY, float sprW, float sprH,
                       float perpDist, unsigned int texID,
                       float tintR, float tintG, float tintB, float tintA) {
    /* compute screen rect */
    float x0 = screenX - sprW / 2.0f;
    float x1 = screenX + sprW / 2.0f;
    float y0 = screenY - sprH / 2.0f;
    float y1 = screenY + sprH / 2.0f;

    /* clip to screen */
    if (x1 < 0 || x0 >= scrW || y1 < 0 || y0 >= scrH) return;

    /* column-based z-buffer check: skip if all columns are behind walls */
    int colStart = (int)fmaxf(x0, 0.0f);
    int colEnd   = (int)fminf(x1, (float)(scrW - 1));
    bool anyVisible = false;
    for (int c = colStart; c <= colEnd; c++) {
        if (perpDist < zBuffer[c]) { anyVisible = true; break; }
    }
    if (!anyVisible) return;

    /* convert pixel to NDC */
    float ndcX0 = 2.0f * x0 / (float)scrW - 1.0f;
    float ndcX1 = 2.0f * x1 / (float)scrW - 1.0f;
    float ndcY0 = 1.0f - 2.0f * y1 / (float)scrH;  /* flip Y */
    float ndcY1 = 1.0f - 2.0f * y0 / (float)scrH;

    float verts[] = {
        ndcX0, ndcY0,  0.0f, 1.0f,
        ndcX1, ndcY0,  1.0f, 1.0f,
        ndcX0, ndcY1,  0.0f, 0.0f,
        ndcX1, ndcY1,  1.0f, 0.0f,
    };

    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    int tintLoc = glGetUniformLocation(entProg, "tintColor");
    glUniform4f(tintLoc, tintR, tintG, tintB, tintA);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texID);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

/* sortable sprite entry */
struct SpriteEntry {
    float dist;
    float x, y;        /* world pos */
    float spriteZ;     /* vertical offset */
    int   projSprIdx;  /* 0=bullet, 1=beam, 2=shotgun */
    float projScale;   /* per-projectile visual scale  */
};

/* ===== public API ===== */

namespace entity_renderer {

void initEntityRenderer(int w, int h) {
    scrW = w;
    scrH = h;

    entProg = shader::createShaderProgram(evsrc, efsrc);

    float quad[16] = {};
    glGenVertexArrays(1, &entVao);
    glGenBuffers(1, &entVbo);
    glBindVertexArray(entVao);
    glBindBuffer(GL_ARRAY_BUFFER, entVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glUseProgram(entProg);
    glUniform1i(glGetUniformLocation(entProg, "spriteTex"), 0);

    /* generate projectile textures */
    projTexGL     = makeProjectileTex();
    projBeamTexGL = makeBeamTex();
    projShotTexGL = makeShotgunTex();
}

void renderProjectiles(const renderer::RenderState& state) {
    if (entity::projectiles.empty()) return;

    buildZBuffer(state);

    /* collect all projectile sprites */
    std::vector<SpriteEntry> sprites;
    sprites.reserve(entity::projectiles.size());

    for (auto& p : entity::projectiles) {
        float ddx = p.x - state.posX;
        float ddy = p.y - state.posY;
        float dist = ddx * ddx + ddy * ddy;
        SpriteEntry se{};
        se.dist = dist; se.x = p.x; se.y = p.y; se.spriteZ = p.spawnZ;
        se.projSprIdx = p.sprIdx;
        se.projScale  = p.visualScale;
        sprites.push_back(se);
    }

    /* sort back to front */
    std::sort(sprites.begin(), sprites.end(),
              [](const SpriteEntry& a, const SpriteEntry& b) { return a.dist > b.dist; });

    /* render */
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(entProg);
    glBindVertexArray(entVao);
    glBindBuffer(GL_ARRAY_BUFFER, entVbo);

    float invDet = 1.0f / (state.planeX * state.dirY - state.dirX * state.planeY);

    for (auto& sp : sprites) {
        float sx = sp.x - state.posX;
        float sy = sp.y - state.posY;

        /* transform to camera space */
        float transformX = invDet * (state.dirY * sx - state.dirX * sy);
        float transformY = invDet * (-state.planeY * sx + state.planeX * sy);

        if (transformY <= 0.1f) continue; /* behind camera */

        int sprScreenX = (int)((float)scrW / 2.0f * (1.0f + transformX / transformY));

        float sprHeight = fabsf((float)scrH / transformY);
        float sprWidth  = sprHeight;  /* square sprites */

        sprHeight *= sp.projScale;
        sprWidth  *= sp.projScale;

        float screenY = (float)scrH / 2.0f;
        /* apply player height offset */
        float vertOffset = (state.posZ / transformY) * ((float)scrH / 2.0f);
        screenY += vertOffset;
        /* apply projectile's vertical position */
        float sprZ = sp.spriteZ;
        sprZ -= settings::getFloat("proj_offset_down", 0.15f);
        if (sprZ != 0.0f) {
            float zOffset = (sprZ / transformY) * ((float)scrH / 2.0f);
            screenY -= zOffset;
        }

        unsigned int texID;
        if (sp.projSprIdx == 1)      texID = projBeamTexGL;
        else if (sp.projSprIdx == 2) texID = projShotTexGL;
        else                         texID = projTexGL;

        drawSprite((float)sprScreenX, screenY, sprWidth, sprHeight,
                   transformY, texID,
                   1.0f, 1.0f, 1.0f, 1.0f);
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

void cleanupEntityRenderer() {
    glDeleteTextures(1, &projTexGL);
    glDeleteTextures(1, &projBeamTexGL);
    glDeleteTextures(1, &projShotTexGL);
    glDeleteVertexArrays(1, &entVao);
    glDeleteBuffers(1, &entVbo);
    glDeleteProgram(entProg);
}

} // namespace entity_renderer
