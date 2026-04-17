/*
 * circular_sprite.cpp — Midpoint-circle-drawn billboard sprite
 *
 * Uses the midpoint circle drawing algorithm to procedurally generate
 * a filled-circle sprite texture, renders it as a billboard in 3D.
 * Spawns at a random empty tile; respawns elsewhere when shot.
 *
 * To remove: delete this folder, remove 4 lines from main.cpp,
 * the Makefile glob, and the config.json sprite_circle_* entries.
 */

#include <glad/glad.h>
#include "circular_sprite.h"
#include "../renderer/shader.h"
#include "../projectile/projectile.h"
#include "../map/map.h"
#include "../settings/settings.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

/* ================================================================== */
/*  Module state                                                      */
/* ================================================================== */

static int scrW, scrH;
static unsigned int prog, vao, vbo, tex;
static float sprX, sprY;
static bool  alive = false;

/* config */
static int   texSz, radius, outlineW;
static float hitR, scale;
static uint8_t fR, fG, fB, oR, oG, oB;

/* ================================================================== */
/*  Shaders                                                           */
/* ================================================================== */

static const char* vs = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
out vec2 uv;
void main() { gl_Position = vec4(aPos, 0.0, 1.0); uv = aUV; }
)";

static const char* fs = R"(
#version 330 core
in vec2 uv; out vec4 fragColor;
uniform sampler2D spriteTex;
void main() {
    vec4 c = texture(spriteTex, uv);
    if (c.a < 0.1) discard;
    fragColor = c;
}
)";

/* ================================================================== */
/*  Midpoint Circle Drawing Algorithm                                 */
/*                                                                    */
/*  Attempt to draw a filled circle using the midpoint circle drawing */
/*  algorithm. For each octant point we fill horizontal spans to      */
/*  create a solid disc, then stroke the outline on top.              */
/* ================================================================== */

static void setPixel(std::vector<uint8_t>& buf, int sz,
                     int px, int py, uint8_t r, uint8_t g, uint8_t b) {
    if (px < 0 || px >= sz || py < 0 || py >= sz) return;
    int i = (py * sz + px) * 4;
    buf[i] = r; buf[i+1] = g; buf[i+2] = b; buf[i+3] = 255;
}

static void hline(std::vector<uint8_t>& buf, int sz,
                  int x0, int x1, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (y < 0 || y >= sz) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    for (int x = (x0 < 0 ? 0 : x0); x <= (x1 >= sz ? sz-1 : x1); x++)
        setPixel(buf, sz, x, y, r, g, b);
}

/*
 * midpointCircleFilled — The core algorithm.
 * Pass 1: fill interior via horizontal spans at each octant step.
 * Pass 2: draw outline ring(s) via 8-way symmetric pixel plotting.
 */
static void midpointCircleFilled(std::vector<uint8_t>& buf, int sz,
                                 int cx, int cy, int rad) {
    /* Pass 1 — fill */
    int x = 0, y = rad, p = 1 - rad;
    while (x <= y) {
        hline(buf, sz, cx-x, cx+x, cy+y, fR, fG, fB);
        hline(buf, sz, cx-x, cx+x, cy-y, fR, fG, fB);
        hline(buf, sz, cx-y, cx+y, cy+x, fR, fG, fB);
        hline(buf, sz, cx-y, cx+y, cy-x, fR, fG, fB);
        x++;
        if (p < 0) p += 2*x + 1;
        else      { y--; p += 2*(x-y) + 1; }
    }

    /* Pass 2 — outline */
    for (int w = 0; w < outlineW; w++) {
        int r = rad - w;
        if (r < 0) break;
        x = 0; y = r; p = 1 - r;
        while (x <= y) {
            setPixel(buf,sz, cx+x,cy+y, oR,oG,oB);
            setPixel(buf,sz, cx-x,cy+y, oR,oG,oB);
            setPixel(buf,sz, cx+x,cy-y, oR,oG,oB);
            setPixel(buf,sz, cx-x,cy-y, oR,oG,oB);
            setPixel(buf,sz, cx+y,cy+x, oR,oG,oB);
            setPixel(buf,sz, cx-y,cy+x, oR,oG,oB);
            setPixel(buf,sz, cx+y,cy-x, oR,oG,oB);
            setPixel(buf,sz, cx-y,cy-x, oR,oG,oB);
            x++;
            if (p < 0) p += 2*x + 1;
            else      { y--; p += 2*(x-y) + 1; }
        }
    }
}

/* ================================================================== */
/*  Helpers                                                           */
/* ================================================================== */

static unsigned int genTex() {
    std::vector<uint8_t> px(texSz * texSz * 4, 0);
    midpointCircleFilled(px, texSz, texSz/2, texSz/2, radius);
    unsigned int t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texSz, texSz, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    return t;
}

static void spawn() {
    struct Tile { int x, y; };
    std::vector<Tile> empty;
    for (int y = 1; y < map::mapHeight - 1; y++)
        for (int x = 1; x < map::mapWidth - 1; x++)
            if (map::worldMap[y][x] == 0)
                empty.push_back({x, y});
    if (empty.empty()) { alive = false; return; }
    auto& t = empty[rand() % empty.size()];
    sprX = t.x + 0.5f;  sprY = t.y + 0.5f;  alive = true;
}

/* DDA wall distance for a single screen column (for z-buffer check) */
static float wallDist(const renderer::RenderState& s, float camX) {
    float rdx = s.dirX + s.planeX * camX;
    float rdy = s.dirY + s.planeY * camX;
    int mx = (int)s.posX, my = (int)s.posY;
    float ddX = (rdx == 0.f) ? 1e30f : fabsf(1.f/rdx);
    float ddY = (rdy == 0.f) ? 1e30f : fabsf(1.f/rdy);
    float sdX, sdY; int stX, stY, side = 0;
    if (rdx < 0) { stX=-1; sdX=(s.posX-mx)*ddX; } else { stX=1; sdX=(mx+1.f-s.posX)*ddX; }
    if (rdy < 0) { stY=-1; sdY=(s.posY-my)*ddY; } else { stY=1; sdY=(my+1.f-s.posY)*ddY; }
    for (int i = 0; i < 256; i++) {
        if (sdX < sdY) { sdX+=ddX; mx+=stX; side=0; } else { sdY+=ddY; my+=stY; side=1; }
        if (mx<0||mx>=map::mapWidth||my<0||my>=map::mapHeight) break;
        if (map::worldMap[my][mx] > 0) {
            if (side==0) return (mx-s.posX+(1-stX)*.5f)/rdx;
            else         return (my-s.posY+(1-stY)*.5f)/rdy;
        }
    }
    return 1e30f;
}

/* ================================================================== */
/*  Public API                                                        */
/* ================================================================== */

namespace circular_sprite {

void init(int w, int h) {
    scrW = w; scrH = h;
    srand((unsigned)time(nullptr));

    texSz    = settings::getInt("sprite_circle_tex_size", 32);
    radius   = settings::getInt("sprite_circle_radius", 14);
    hitR     = settings::getFloat("sprite_circle_hit_radius", 0.5f);
    scale    = settings::getFloat("sprite_circle_visual_scale", 0.4f);
    fR = (uint8_t)settings::getInt("sprite_circle_fill_r", 220);
    fG = (uint8_t)settings::getInt("sprite_circle_fill_g", 50);
    fB = (uint8_t)settings::getInt("sprite_circle_fill_b", 50);
    oR = (uint8_t)settings::getInt("sprite_circle_outline_r", 255);
    oG = (uint8_t)settings::getInt("sprite_circle_outline_g", 255);
    oB = (uint8_t)settings::getInt("sprite_circle_outline_b", 255);
    outlineW = settings::getInt("sprite_circle_outline_width", 2);

    prog = shader::createShaderProgram(vs, fs);
    float q[16] = {};
    glGenVertexArrays(1, &vao); glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,16,(void*)0);  glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,16,(void*)8);  glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog,"spriteTex"), 0);

    tex = genTex();
    spawn();
}

void update(float) {
    if (!alive) return;
    for (auto& p : projectile::projectiles) {
        if (!p.alive) continue;
        float dx = p.x - sprX, dy = p.y - sprY;
        if (dx*dx + dy*dy < hitR*hitR) {
            p.alive = false;
            spawn();
            break;
        }
    }
}

void render(const renderer::RenderState& s) {
    if (!alive) return;

    float sx = sprX - s.posX, sy = sprY - s.posY;
    float invDet = 1.f / (s.planeX*s.dirY - s.dirX*s.planeY);
    float txfX = invDet * (s.dirY*sx - s.dirX*sy);
    float txfY = invDet * (-s.planeY*sx + s.planeX*sy);
    if (txfY <= 0.1f) return;

    float scrCX = scrW/2.f * (1.f + txfX/txfY);
    float sz = fabsf((float)scrH / txfY) * scale;
    float scrCY = scrH/2.f + (s.posZ/txfY)*(scrH/2.f);

    float x0 = scrCX - sz/2, x1 = scrCX + sz/2;
    float y0 = scrCY - sz/2, y1 = scrCY + sz/2;
    if (x1<0||x0>=scrW||y1<0||y0>=scrH) return;

    /* z-buffer check: is sprite visible in front of walls? */
    int c0 = (int)fmaxf(x0,0), c1 = (int)fminf(x1,(float)(scrW-1));
    bool vis = false;
    for (int c = c0; c <= c1 && !vis; c++) {
        float camX = 2.f*c/scrW - 1.f;
        if (txfY < wallDist(s, camX)) vis = true;
    }
    if (!vis) return;

    float nX0=2.f*x0/scrW-1, nX1=2.f*x1/scrW-1;
    float nY0=1-2.f*y1/scrH, nY1=1-2.f*y0/scrH;
    float v[]={nX0,nY0,0,1, nX1,nY0,1,1, nX0,nY1,0,0, nX1,nY1,1,0};

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(prog);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

void cleanup() {
    glDeleteTextures(1, &tex);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
}

} // namespace circular_sprite
