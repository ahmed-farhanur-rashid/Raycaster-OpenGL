#include <glad/glad.h>
#include "hud_renderer.h"
#include "shader.h"
#include <stb/stb_image.h>
#include <cstdio>
#include <cmath>
#include <cstring>

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

/* weapon equipped */
static bool weaponEquipped = false;

/* animation state machine */
static WeaponState state = WeaponState::IDLE;
static float       animTimer = 0.0f;
static int         reloadFrame = 0;

/* timing constants */
static const float FIRE_BULLET_DURATION   = 0.12f;   // show muzzle flash
static const float FIRE_GRENADE_DURATION  = 0.25f;
static const float RELOAD_FRAME_DURATION  = 0.10f;   // slower reload
static const int   RELOAD_FRAME_COUNT     = 11;

/* ammo — assault rifle */
static const int   MAX_BULLETS  = 30;
static const int   MAX_GRENADES = 5;
static int         bullets  = MAX_BULLETS;
static int         grenades = MAX_GRENADES;

/* weapon type */
static WeaponType curWeapon = WeaponType::NONE;

/* auto-fire (assault rifle) */
static const float AUTO_FIRE_INTERVAL = 0.10f;
static float       autoFireTimer      = 0.0f;
static bool        prevLmbHeld        = false;

/* GL texture handles for assault rifle */
static unsigned int texIdle        = 0;
static unsigned int texFireBullet  = 0;
static unsigned int texFireGrenade = 0;
static unsigned int texReload[11]  = {};

/* GL texture handles for handgun */
static unsigned int hgTexIdle = 0;
static unsigned int hgTexFire = 0;

/* GL texture handles for shotgun */
static unsigned int sgTexIdle = 0;
static unsigned int sgTexFire = 0;
static const int    SG_RELOAD_FRAME_COUNT = 5;
static unsigned int sgTexReload[5] = {};
static int          sgReloadFrame  = 0;

/* GL texture handles for energy weapon */
static unsigned int enTexIdle        = 0;
static unsigned int enTexFireNormal  = 0;
static unsigned int enTexFireOver    = 0;
static const int    EN_OVERHEAT_FRAME_COUNT = 5;
static unsigned int enTexOverheat[5] = {};
static int          enOverheatFrame  = 0;
static const float  EN_OVERHEAT_FRAME_DURATION = 0.25f;  /* slower overheat anim */

/* energy weapon ammo (0.0 = empty, 1.0 = full) */
static float enNormalCharge    = 1.0f;
static float enOverCharge      = 1.0f;
static bool  enNormalDepleted  = false;   /* triggers overheat anim */
static bool  enOverDepleted    = false;
static bool  prevRmbHeld       = false;

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
/*                    texture loading helper                           */
/* ================================================================== */

static unsigned int loadTex(const char* path) {
    int w, h, ch;
    unsigned char* data = stbi_load(path, &w, &h, &ch, 4);
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    } else {
        fprintf(stderr, "WARNING: could not load %s\n", path);
        unsigned char fb[4] = {200, 200, 200, 255};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, fb);
    }
    return tex;
}

/* ================================================================== */
/*                    public API                                       */
/* ================================================================== */

void initHUD(int screenW, int screenH) {
    scrW = screenW;
    scrH = screenH;

    prog = shader::createShaderProgram(vsrc, fsrc);

    /* quad – will be updated per-frame */
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

    /* ---- preload ALL weapon textures so equip never hitches ---- */
    texIdle       = loadTex("resource/textures/weapons/assault/idle/assault_idle.png");
    texFireBullet = loadTex("resource/textures/weapons/assault/fire/assault_fire_bullet_left_click.png");
    texFireGrenade= loadTex("resource/textures/weapons/assault/fire/assault_fire_grenade_right_click.png");
    { char buf[128];
      for (int i = 0; i < RELOAD_FRAME_COUNT; i++) {
          snprintf(buf, sizeof(buf),
                   "resource/textures/weapons/assault/reload/assault_reload_%02d.png", i + 1);
          texReload[i] = loadTex(buf);
      }
    }
    hgTexIdle = loadTex("resource/textures/weapons/handgun/idle/hand_gun_idle.png");
    hgTexFire = loadTex("resource/textures/weapons/handgun/fire/handgun_fire.png");
    sgTexIdle = loadTex("resource/textures/weapons/shotgun/idle/shotgun_idle.png");
    sgTexFire = loadTex("resource/textures/weapons/shotgun/fire/shotgun_fire.png");
    { char buf[128];
      for (int i = 0; i < SG_RELOAD_FRAME_COUNT; i++) {
          snprintf(buf, sizeof(buf),
                   "resource/textures/weapons/shotgun/reload/shotgun_reload_%02d.png", i + 1);
          sgTexReload[i] = loadTex(buf);
      }
    }
    enTexIdle       = loadTex("resource/textures/weapons/energy/idle/energy_idle.png");
    enTexFireNormal = loadTex("resource/textures/weapons/energy/fire/energy_fire_normal.png");
    enTexFireOver   = loadTex("resource/textures/weapons/energy/fire/energy_fire_overcharge.png");
    { char buf[128];
      for (int i = 0; i < EN_OVERHEAT_FRAME_COUNT; i++) {
          snprintf(buf, sizeof(buf),
                   "resource/textures/weapons/energy/overheat/energy_overheat_%02d.png", i + 1);
          enTexOverheat[i] = loadTex(buf);
      }
    }
}

void equipAssaultRifle() {
    if (curWeapon == WeaponType::ASSAULT_RIFLE) return;
    curWeapon = WeaponType::ASSAULT_RIFLE;
    weaponEquipped = true;
    state = WeaponState::IDLE;
    animTimer = 0.0f;
    autoFireTimer = 0.0f;
    prevLmbHeld = false;

    printf("Assault Rifle equipped.\n");
}

bool hasWeaponEquipped() {
    return weaponEquipped;
}

WeaponType currentWeapon() {
    return curWeapon;
}

void equipHandgun() {
    if (curWeapon == WeaponType::HANDGUN) return;
    curWeapon = WeaponType::HANDGUN;
    weaponEquipped = true;
    state = WeaponState::IDLE;
    animTimer = 0.0f;
    prevLmbHeld = false;

    printf("Handgun equipped.\n");
}

void equipShotgun() {
    if (curWeapon == WeaponType::SHOTGUN) return;
    curWeapon = WeaponType::SHOTGUN;
    weaponEquipped = true;
    state = WeaponState::IDLE;
    animTimer = 0.0f;
    prevLmbHeld = false;

    printf("Shotgun equipped.\n");
}

void equipEnergyWeapon() {
    if (curWeapon == WeaponType::ENERGY_WEAPON) return;
    curWeapon = WeaponType::ENERGY_WEAPON;
    weaponEquipped = true;
    state = WeaponState::IDLE;
    animTimer = 0.0f;
    prevLmbHeld = false;
    prevRmbHeld = false;
    enNormalDepleted = false;
    enOverDepleted   = false;

    printf("Energy Weapon equipped.\n");
}

int getBullets()     { return bullets; }
int getMaxBullets()  { return MAX_BULLETS; }
int getGrenades()    { return (curWeapon == WeaponType::ASSAULT_RIFLE) ? grenades : 0; }
int getMaxGrenades() { return (curWeapon == WeaponType::ASSAULT_RIFLE) ? MAX_GRENADES : 0; }

static void startReload() {
    state = WeaponState::RELOAD;
    animTimer = 0.0f;
    reloadFrame = 0;
}

void fireBullet() {
    if (!weaponEquipped) return;
    if (state != WeaponState::IDLE) return;

    if (curWeapon == WeaponType::ASSAULT_RIFLE) {
        if (bullets <= 0) { startReload(); return; }
        bullets--;
        state = WeaponState::FIRE_BULLET;
        animTimer = 0.0f;
    } else if (curWeapon == WeaponType::HANDGUN) {
        /* unlimited ammo */
        state = WeaponState::FIRE_BULLET;
        animTimer = 0.0f;
    } else if (curWeapon == WeaponType::SHOTGUN) {
        /* unlimited ammo — fire then auto-reload */
        state = WeaponState::FIRE_BULLET;
        animTimer = 0.0f;
    } else if (curWeapon == WeaponType::ENERGY_WEAPON) {
        /* single press doesn't consume ammo */
        if (enNormalDepleted) return;
        state = WeaponState::FIRE_BULLET;
        animTimer = 0.0f;
    }
}

void fireGrenade() {
    if (!weaponEquipped) return;
    if (state != WeaponState::IDLE) return;

    if (curWeapon == WeaponType::ASSAULT_RIFLE) {
        if (grenades <= 0) { startReload(); return; }
        grenades--;
        state = WeaponState::FIRE_GRENADE;
        animTimer = 0.0f;
    } else if (curWeapon == WeaponType::ENERGY_WEAPON) {
        /* single press overcharge — doesn't consume ammo */
        if (enOverDepleted) return;
        state = WeaponState::FIRE_GRENADE;
        animTimer = 0.0f;
    }
}

void reload() {
    if (!weaponEquipped) return;
    if (state != WeaponState::IDLE) return;
    if (curWeapon == WeaponType::HANDGUN || curWeapon == WeaponType::SHOTGUN
        || curWeapon == WeaponType::ENERGY_WEAPON) return;
    startReload();
}

void updateHUD(bool isMoving, float deltaTime, bool lmbHeld, bool rmbHeld) {
    /* bob animation — only sway horizontally and bob downward */
    if (isMoving && state == WeaponState::IDLE) {
        bobTimer += deltaTime * 7.0f;
        float raw = sinf(bobTimer);
        bobOffsetY = (raw < 0.0f) ? raw * 0.015f : 0.0f;
        bobOffsetX = cosf(bobTimer * 0.5f) * 0.012f;
    } else {
        bobOffsetY *= 0.9f;
        bobOffsetX *= 0.9f;
        if (!isMoving) bobTimer = 0.0f;
    }

    /* ---------- energy weapon charge / drain ---------- */
    if (curWeapon == WeaponType::ENERGY_WEAPON && !enNormalDepleted && !enOverDepleted) {
        /* drain while HOLDING (not first frame) */
        bool lmbDraining = lmbHeld && prevLmbHeld;
        bool rmbDraining = rmbHeld && prevRmbHeld;

        if (lmbDraining && state == WeaponState::FIRE_BULLET) {
            enNormalCharge -= deltaTime / 10.0f;     /* 10 sec to deplete */
            if (enNormalCharge <= 0.0f) {
                enNormalCharge = 0.0f;
                enNormalDepleted = true;
                state = WeaponState::RELOAD;
                animTimer = 0.0f;
                enOverheatFrame = 0;
            }
        } else if (rmbDraining && state == WeaponState::FIRE_GRENADE) {
            enOverCharge -= deltaTime / 5.0f;        /* 5 sec to deplete */
            if (enOverCharge <= 0.0f) {
                enOverCharge = 0.0f;
                enOverDepleted = true;
                state = WeaponState::RELOAD;
                animTimer = 0.0f;
                enOverheatFrame = 0;
            }
        }

        /* recharge when NOT firing that mode */
        if (!lmbHeld && enNormalCharge < 1.0f && !enNormalDepleted)
            enNormalCharge = fminf(1.0f, enNormalCharge + deltaTime * 0.15f);
        if (!rmbHeld && enOverCharge < 1.0f && !enOverDepleted)
            enOverCharge = fminf(1.0f, enOverCharge + deltaTime * 0.15f);
    }

    /* ---------- auto-fire / semi-auto / energy logic ---------- */
    if (weaponEquipped && curWeapon == WeaponType::ENERGY_WEAPON) {
        /* energy: LMB = normal fire, RMB = overcharge fire */
        if (lmbHeld && !enNormalDepleted) {
            if (state == WeaponState::IDLE || state == WeaponState::FIRE_BULLET) {
                state = WeaponState::FIRE_BULLET;
                animTimer = 0.0f;   /* keep showing fire texture */
            }
        } else if (rmbHeld && !enOverDepleted) {
            if (state == WeaponState::IDLE || state == WeaponState::FIRE_GRENADE) {
                state = WeaponState::FIRE_GRENADE;
                animTimer = 0.0f;
            }
        } else if (!lmbHeld && !rmbHeld && state != WeaponState::RELOAD) {
            state = WeaponState::IDLE;
        }
    } else if (weaponEquipped && lmbHeld) {
        if (curWeapon == WeaponType::ASSAULT_RIFLE) {
            if (!prevLmbHeld) {
                fireBullet();
                autoFireTimer = 0.0f;
            } else {
                autoFireTimer += deltaTime;
                if (autoFireTimer >= AUTO_FIRE_INTERVAL && state == WeaponState::IDLE) {
                    autoFireTimer -= AUTO_FIRE_INTERVAL;
                    fireBullet();
                }
            }
        } else if (curWeapon == WeaponType::HANDGUN || curWeapon == WeaponType::SHOTGUN) {
            if (!prevLmbHeld) fireBullet();
        }
    }
    if (!lmbHeld) autoFireTimer = 0.0f;
    prevLmbHeld = lmbHeld;
    prevRmbHeld = rmbHeld;

    /* ---------- advance animation ---------- */
    float fireDur = FIRE_BULLET_DURATION;
    if (curWeapon == WeaponType::HANDGUN) fireDur = 0.15f;
    else if (curWeapon == WeaponType::SHOTGUN) fireDur = 0.15f;

    switch (state) {
    case WeaponState::FIRE_BULLET:
        animTimer += deltaTime;
        if (animTimer >= fireDur) {
            if (curWeapon == WeaponType::ASSAULT_RIFLE && bullets <= 0)
                startReload();
            else if (curWeapon == WeaponType::SHOTGUN) {
                /* shotgun: auto-reload after every shot */
                state = WeaponState::RELOAD;
                animTimer = 0.0f;
                sgReloadFrame = 0;
            } else
                state = WeaponState::IDLE;
        }
        break;
    case WeaponState::FIRE_GRENADE:
        animTimer += deltaTime;
        if (animTimer >= FIRE_GRENADE_DURATION) {
            if (grenades <= 0) startReload();
            else state = WeaponState::IDLE;
        }
        break;
    case WeaponState::RELOAD:
        animTimer += deltaTime;
        if (curWeapon == WeaponType::SHOTGUN) {
            if (animTimer >= RELOAD_FRAME_DURATION) {
                animTimer -= RELOAD_FRAME_DURATION;
                sgReloadFrame++;
                if (sgReloadFrame >= SG_RELOAD_FRAME_COUNT) {
                    sgReloadFrame = 0;
                    state = WeaponState::IDLE;
                }
            }
        } else if (curWeapon == WeaponType::ENERGY_WEAPON) {
            if (animTimer >= EN_OVERHEAT_FRAME_DURATION) {
                animTimer -= EN_OVERHEAT_FRAME_DURATION;
                enOverheatFrame++;
                if (enOverheatFrame >= EN_OVERHEAT_FRAME_COUNT) {
                    enOverheatFrame = 0;
                    /* refill depleted bar */
                    if (enNormalDepleted)  { enNormalCharge = 1.0f; enNormalDepleted = false; }
                    if (enOverDepleted)    { enOverCharge   = 1.0f; enOverDepleted   = false; }
                    state = WeaponState::IDLE;
                }
            }
        } else {
            if (animTimer >= RELOAD_FRAME_DURATION) {
                animTimer -= RELOAD_FRAME_DURATION;
                reloadFrame++;
                if (reloadFrame >= RELOAD_FRAME_COUNT) {
                    reloadFrame = 0;
                    bullets  = MAX_BULLETS;
                    grenades = MAX_GRENADES;
                    state = WeaponState::IDLE;
                }
            }
        }
        break;
    default:
        break;
    }
}

/* helper: draw a solid-colour rectangle in NDC */
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
    if (!weaponEquipped) return;

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

    if (curWeapon == WeaponType::ASSAULT_RIFLE) {
        int cb = bullets, mb = MAX_BULLETS;
        int cg = grenades, mg = MAX_GRENADES;

        /* bullet icon */
        drawRect(px2x(iconLeft), px2y(byBot), px2x(iconLeft + iconW), px2y(byTop),
                 0.9f, 0.7f, 0.1f, 0.9f);
        /* bullet bg */
        drawRect(px2x(bxL), px2y(byBot), px2x(bxR), px2y(byTop),
                 0.15f, 0.15f, 0.15f, 0.7f);
        /* bullet fill */
        float bf = mb > 0 ? (float)cb / mb : 0;
        drawRect(px2x(bxL), px2y(byBot), px2x(bxL + barW * bf), px2y(byTop),
                 0.9f, 0.7f, 0.1f, 0.85f);

        if (mg > 0) {
            float gyBot = byTop - gap;
            float gyTop = gyBot - barH;
            drawRect(px2x(iconLeft), px2y(gyBot), px2x(iconLeft + iconW), px2y(gyTop),
                     0.1f, 0.8f, 0.2f, 0.9f);
            drawRect(px2x(bxL), px2y(gyBot), px2x(bxR), px2y(gyTop),
                     0.15f, 0.15f, 0.15f, 0.7f);
            float gf = (float)cg / mg;
            drawRect(px2x(bxL), px2y(gyBot), px2x(bxL + barW * gf), px2y(gyTop),
                     0.1f, 0.8f, 0.2f, 0.85f);
        }
    } else if (curWeapon == WeaponType::ENERGY_WEAPON) {
        /* normal charge bar (cyan) */
        drawRect(px2x(iconLeft), px2y(byBot), px2x(iconLeft + iconW), px2y(byTop),
                 0.1f, 0.8f, 0.9f, 0.9f);
        drawRect(px2x(bxL), px2y(byBot), px2x(bxR), px2y(byTop),
                 0.15f, 0.15f, 0.15f, 0.7f);
        drawRect(px2x(bxL), px2y(byBot), px2x(bxL + barW * enNormalCharge), px2y(byTop),
                 0.1f, 0.8f, 0.9f, 0.85f);

        /* overcharge bar (magenta) */
        float gyBot = byTop - gap;
        float gyTop = gyBot - barH;
        drawRect(px2x(iconLeft), px2y(gyBot), px2x(iconLeft + iconW), px2y(gyTop),
                 0.9f, 0.2f, 0.8f, 0.9f);
        drawRect(px2x(bxL), px2y(gyBot), px2x(bxR), px2y(gyTop),
                 0.15f, 0.15f, 0.15f, 0.7f);
        drawRect(px2x(bxL), px2y(gyBot), px2x(bxL + barW * enOverCharge), px2y(gyTop),
                 0.9f, 0.2f, 0.8f, 0.85f);
    }
    /* handgun / shotgun: no bars (unlimited) */
}

void renderWeapon() {
    if (!weaponEquipped) return;

    /* pick current texture based on weapon type */
    unsigned int curTex = texIdle;
    if (curWeapon == WeaponType::HANDGUN) {
        curTex = (state == WeaponState::FIRE_BULLET) ? hgTexFire : hgTexIdle;
    } else if (curWeapon == WeaponType::SHOTGUN) {
        switch (state) {
        case WeaponState::FIRE_BULLET: curTex = sgTexFire; break;
        case WeaponState::RELOAD:      curTex = sgTexReload[sgReloadFrame]; break;
        default:                       curTex = sgTexIdle;  break;
        }
    } else if (curWeapon == WeaponType::ENERGY_WEAPON) {
        switch (state) {
        case WeaponState::FIRE_BULLET:  curTex = enTexFireNormal; break;
        case WeaponState::FIRE_GRENADE: curTex = enTexFireOver;   break;
        case WeaponState::RELOAD:       curTex = enTexOverheat[enOverheatFrame]; break;
        default:                        curTex = enTexIdle;       break;
        }
    } else {
        switch (state) {
        case WeaponState::FIRE_BULLET:  curTex = texFireBullet;  break;
        case WeaponState::FIRE_GRENADE: curTex = texFireGrenade; break;
        case WeaponState::RELOAD:       curTex = texReload[reloadFrame]; break;
        default:                        curTex = texIdle;        break;
        }
    }

    /* weapon quad — positioned at bottom, shifted right */
    float x0 = -0.5f + bobOffsetX;
    float x1 =  1.1f + bobOffsetX;
    float y0 = -1.0f + bobOffsetY;
    float y1 =  0.2f + bobOffsetY;

    /* slight recoil kick during fire */
    if (state == WeaponState::FIRE_BULLET) {
        float dur = FIRE_BULLET_DURATION;
        if (curWeapon == WeaponType::HANDGUN || curWeapon == WeaponType::SHOTGUN) dur = 0.15f;
        else if (curWeapon == WeaponType::ENERGY_WEAPON) dur = 0.12f;
        float t = animTimer / dur;
        float kickAmt = (curWeapon == WeaponType::SHOTGUN) ? 0.07f : 0.04f;
        float kick = sinf(t * 3.14159f) * kickAmt;
        y0 -= kick;
        y1 -= kick;
    } else if (state == WeaponState::FIRE_GRENADE) {
        float dur = (curWeapon == WeaponType::ENERGY_WEAPON) ? 0.12f : FIRE_GRENADE_DURATION;
        float t = animTimer / dur;
        float kick = sinf(t * 3.14159f) * 0.06f;
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

    glBindVertexArray(0);
    glDisable(GL_BLEND);
}

void cleanupHUD() {
    if (texIdle)       glDeleteTextures(1, &texIdle);
    if (texFireBullet) glDeleteTextures(1, &texFireBullet);
    if (texFireGrenade)glDeleteTextures(1, &texFireGrenade);
    for (int i = 0; i < RELOAD_FRAME_COUNT; i++)
        if (texReload[i]) glDeleteTextures(1, &texReload[i]);
    if (hgTexIdle) glDeleteTextures(1, &hgTexIdle);
    if (hgTexFire) glDeleteTextures(1, &hgTexFire);
    if (sgTexIdle) glDeleteTextures(1, &sgTexIdle);
    if (sgTexFire) glDeleteTextures(1, &sgTexFire);
    for (int i = 0; i < SG_RELOAD_FRAME_COUNT; i++)
        if (sgTexReload[i]) glDeleteTextures(1, &sgTexReload[i]);
    if (enTexIdle)       glDeleteTextures(1, &enTexIdle);
    if (enTexFireNormal) glDeleteTextures(1, &enTexFireNormal);
    if (enTexFireOver)   glDeleteTextures(1, &enTexFireOver);
    for (int i = 0; i < EN_OVERHEAT_FRAME_COUNT; i++)
        if (enTexOverheat[i]) glDeleteTextures(1, &enTexOverheat[i]);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteProgram(prog);
}

} // namespace hud
