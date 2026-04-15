#include <glad/glad.h>
#include "hud_renderer.h"
#include "shader.h"
#include "../entity/entity.h"
#include <cstdio>
#include <cmath>

namespace hud {

/* ================================================================== */
/*                    internal state                                   */
/* ================================================================== */

static unsigned int prog, vao, vbo;
static int scrW, scrH;

/* bob */
static float bobTimer  = 0.0f;
static float bobOffsetY = 0.0f;
static float bobOffsetX = 0.0f;

/* weapon instances (all preloaded) */
static Weapon weapons[4];          /* AR, SG, EN, HG */
static Weapon* active = nullptr;

/* uniform locations for solid-colour quads (ammo bars) */
static int uUseSolidColor = -1;
static int uSolidColor    = -1;

/* ================================================================== */
/*                    shaders                                          */
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
uniform sampler2D weaponTex;
uniform bool useSolidColor;
uniform vec4 solidColor;
void main() {
    if (useSolidColor) { fragColor = solidColor; return; }
    vec4 c = texture(weaponTex, uv);
    if (c.a < 0.1) discard;
    fragColor = c;
}
)";

/* ================================================================== */
/*                    public API                                       */
/* ================================================================== */

void initHUD(int screenW, int screenH) {
    scrW = screenW;
    scrH = screenH;

    prog = shader::createShaderProgram(vsrc, fsrc);

    /* quad — will be updated per-frame */
    float quad[16] = {};
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float),
                          (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glUseProgram(prog);
    glUniform1i(glGetUniformLocation(prog, "weaponTex"), 0);
    uUseSolidColor = glGetUniformLocation(prog, "useSolidColor");
    uSolidColor    = glGetUniformLocation(prog, "solidColor");

    /* preload ALL weapon instances */
    weapons[0] = createAssaultRifle();
    weapons[1] = createShotgun();
    weapons[2] = createEnergyWeapon();
    weapons[3] = createHandgun();
}

static bool equipWeapon(int index) {
    Weapon* w = &weapons[index];
    if (active == w) return false;
    active = w;
    w->state         = WeaponState::IDLE;
    w->animTimer     = 0.0f;
    w->autoFireTimer = 0.0f;
    w->prevLmb       = false;
    w->prevRmb       = false;
    if (w->isEnergy) { w->depleted1 = false; w->depleted2 = false; }
    return true;
}

void equipAssaultRifle()  { if (equipWeapon(0)) printf("Assault Rifle equipped.\n"); }
void equipShotgun()       { if (equipWeapon(1)) printf("Shotgun equipped.\n"); }
void equipEnergyWeapon()  { if (equipWeapon(2)) printf("Energy Weapon equipped.\n"); }
void equipHandgun()       { if (equipWeapon(3)) printf("Handgun equipped.\n"); }

bool hasWeaponEquipped() { return active != nullptr; }
WeaponType currentWeapon() { return active ? active->type : WeaponType::NONE; }

int getBullets()     { return active ? active->ammo1 : 0; }
int getMaxBullets()  { return active ? active->maxAmmo1 : 0; }
int getGrenades()    { return (active && active->type == WeaponType::ASSAULT_RIFLE) ? active->ammo2 : 0; }
int getMaxGrenades() { return (active && active->type == WeaponType::ASSAULT_RIFLE) ? active->maxAmmo2 : 0; }
bool firedThisFrame() { return active && active->firedThisFrame; }
bool firedAltThisFrame() { return active && active->firedAltThisFrame; }

void fireBullet()  { if (active) weaponFirePrimary(*active); }
void fireGrenade() { if (active) weaponFireSecondary(*active); }
void reload()      { if (active) weaponReload(*active); }

void updateHUD(bool isMoving, float deltaTime, bool lmbHeld, bool rmbHeld) {
    /* bob animation — only sway horizontally and bob downward */
    if (isMoving && active && active->state == WeaponState::IDLE) {
        bobTimer += deltaTime * 7.0f;
        float raw = sinf(bobTimer);
        bobOffsetY = (raw < 0.0f) ? raw * 0.015f : 0.0f;
        bobOffsetX = cosf(bobTimer * 0.5f) * 0.012f;
    } else {
        bobOffsetY *= 0.9f;
        bobOffsetX *= 0.9f;
        if (!isMoving) bobTimer = 0.0f;
    }

    if (active)
        updateWeapon(*active, deltaTime, lmbHeld, rmbHeld);
}

/* ================================================================== */
/*  rendering helpers                                                  */
/* ================================================================== */

static void drawRect(float x0, float y0, float x1, float y1,
                     float r, float g, float b, float a) {
    glUniform1i(uUseSolidColor, 1);
    glUniform4f(uSolidColor, r, g, b, a);
    float v[] = {
        x0, y0, 0,0,  x1, y0, 1,0,
        x0, y1, 0,1,  x1, y1, 1,1,
    };
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void drawAmmoBars() {
    if (!active) return;
    if (!active->bar1.show && !active->bar2.show) return;

    auto px2x = [](float px) { return 2.0f * px / scrW - 1.0f; };
    auto px2y = [](float py) { return 1.0f - 2.0f * py / scrH; };

    const float barW = 200.0f, barH = 14.0f;
    const float margin = 20.0f, bottomMargin = 40.0f;
    const float gap = 6.0f, iconW = 14.0f, iconGap = 4.0f;

    float bxL = scrW - margin - barW;
    float bxR = scrW - margin;
    float byBot = scrH - bottomMargin;
    float byTop = byBot - barH;
    float iconLeft = bxL - iconGap - iconW;

    /* bar 1 */
    if (active->bar1.show) {
        float fill;
        if (active->isEnergy) fill = active->charge1;
        else fill = active->maxAmmo1 > 0 ? (float)active->ammo1 / active->maxAmmo1 : 0;

        auto& c = active->bar1;
        drawRect(px2x(iconLeft), px2y(byBot), px2x(iconLeft + iconW), px2y(byTop),
                 c.r, c.g, c.b, 0.9f);
        drawRect(px2x(bxL), px2y(byBot), px2x(bxR), px2y(byTop),
                 0.15f, 0.15f, 0.15f, 0.7f);
        drawRect(px2x(bxL), px2y(byBot), px2x(bxL + barW * fill), px2y(byTop),
                 c.r, c.g, c.b, 0.85f);
    }

    /* bar 2 */
    if (active->bar2.show) {
        float gyBot = byTop - gap;
        float gyTop = gyBot - barH;

        float fill;
        if (active->isEnergy) fill = active->charge2;
        else fill = active->maxAmmo2 > 0 ? (float)active->ammo2 / active->maxAmmo2 : 0;

        auto& c = active->bar2;
        drawRect(px2x(iconLeft), px2y(gyBot), px2x(iconLeft + iconW), px2y(gyTop),
                 c.r, c.g, c.b, 0.9f);
        drawRect(px2x(bxL), px2y(gyBot), px2x(bxR), px2y(gyTop),
                 0.15f, 0.15f, 0.15f, 0.7f);
        drawRect(px2x(bxL), px2y(gyBot), px2x(bxL + barW * fill), px2y(gyTop),
                 c.r, c.g, c.b, 0.85f);
    }
}

void renderWeapon() {
    if (!active) return;

    unsigned int curTex = getWeaponTexture(*active);

    /* weapon quad — positioned at bottom, shifted right.
       The base coordinates were designed for 4:3.  Correct the
       horizontal extent so the sprite keeps its proportions.     */
    float aspect = (float)scrW / (float)scrH;
    float ax = (4.0f / 3.0f) / aspect;   /* 1.0 at 4:3, 0.75 at 16:9 */
    float cx = 0.3f;   /* horizontal centre of the sprite quad */
    float hw = 0.8f * ax;   /* half-width, aspect-corrected */
    float x0 = cx - hw + bobOffsetX;
    float x1 = cx + hw + bobOffsetX;
    float y0 = -1.0f + bobOffsetY;
    float y1 =  0.2f + bobOffsetY;

    /* slight recoil kick during fire */
    if (active->state == WeaponState::FIRE_BULLET) {
        float dur = active->fireDur;
        float t = active->animTimer / dur;
        float kick = sinf(t * 3.14159f) * active->recoilKick;
        y0 -= kick;
        y1 -= kick;
    } else if (active->state == WeaponState::FIRE_GRENADE) {
        float dur = active->altFireDur;
        float t = active->animTimer / dur;
        float kick = sinf(t * 3.14159f) * active->altRecoilKick;
        y0 -= kick;
        y1 -= kick;
    }

    float verts[] = {
        x0, y0,   0.0f, 1.0f,
        x1, y0,   1.0f, 1.0f,
        x0, y1,   0.0f, 0.0f,
        x1, y1,   1.0f, 0.0f,
    };

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(prog);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    /* draw weapon sprite */
    glUniform1i(uUseSolidColor, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, curTex);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    /* draw ammo bars */
    drawAmmoBars();

    /* draw player health bar — top-centre */
    {
        float hpFill = (entity::playerMaxHp > 0.0f)
                     ? entity::playerHp / entity::playerMaxHp : 0.0f;
        if (hpFill < 0.0f) hpFill = 0.0f;
        if (hpFill > 1.0f) hpFill = 1.0f;

        auto px2x = [](float px) { return 2.0f * px / scrW - 1.0f; };
        auto px2y = [](float py) { return 1.0f - 2.0f * py / scrH; };

        float barW = 240.0f, barH = 16.0f;
        float cx = (float)scrW / 2.0f;
        float bxL = cx - barW / 2.0f;
        float bxR = cx + barW / 2.0f;
        float byTop = 12.0f;
        float byBot = byTop + barH;

        /* background */
        drawRect(px2x(bxL), px2y(byBot), px2x(bxR), px2y(byTop),
                 0.15f, 0.15f, 0.15f, 0.7f);
        /* fill — green→red */
        float hr = 1.0f - hpFill;
        float hg = hpFill;
        drawRect(px2x(bxL), px2y(byBot), px2x(bxL + barW * hpFill), px2y(byTop),
                 hr, hg, 0.0f, 0.85f);
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

void cleanupHUD() {
    for (int i = 0; i < 4; i++) cleanupWeapon(weapons[i]);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
}

} // namespace hud
