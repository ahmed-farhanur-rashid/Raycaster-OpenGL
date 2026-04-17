#include <glad/glad.h>
#include "weapon.h"
#include <stb/stb_image.h>
#include <cstdio>

/* ================================================================== */
/*  shared texture loader                                              */
/* ================================================================== */

unsigned int loadWeaponTexture(const char* path) {
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
/*  shared weapon operations                                           */
/* ================================================================== */

void weaponStartReload(Weapon& w) {
    w.state       = WeaponState::RELOAD;
    w.animTimer   = 0.0f;
    w.reloadFrame = 0;
}

void weaponFirePrimary(Weapon& w) {
    if (w.state != WeaponState::IDLE) return;
    if (w.isEnergy && w.depleted1) return;
    if (!w.unlimited && w.ammo1 <= 0) { weaponStartReload(w); return; }
    if (!w.unlimited) w.ammo1--;
    w.state     = WeaponState::FIRE_BULLET;
    w.animTimer = 0.0f;
    w.firedThisFrame = true;
}

void weaponFireSecondary(Weapon& w) {
    if (w.state != WeaponState::IDLE) return;
    if (!w.hasAltFire) return;
    if (w.isEnergy && w.depleted2) return;
    if (!w.unlimited && w.maxAmmo2 > 0 && w.ammo2 <= 0) { weaponStartReload(w); return; }
    if (!w.unlimited && w.maxAmmo2 > 0) w.ammo2--;
    w.state     = WeaponState::FIRE_GRENADE;
    w.animTimer = 0.0f;
    w.firedAltThisFrame = true;
}

void weaponReload(Weapon& w) {
    if (w.state != WeaponState::IDLE) return;
    if (w.unlimited) return;
    weaponStartReload(w);
}

void updateWeapon(Weapon& w, float dt, bool lmb, bool rmb) {
    w.firedThisFrame = false;
    w.firedAltThisFrame = false;
    if (w.handleFire)  w.handleFire(w, dt, lmb, rmb);
    w.prevLmb = lmb;
    w.prevRmb = rmb;
    if (w.advanceAnim) w.advanceAnim(w, dt);
}

unsigned int getWeaponTexture(const Weapon& w) {
    switch (w.state) {
    case WeaponState::FIRE_BULLET:  return w.texFire;
    case WeaponState::FIRE_GRENADE: return w.texAltFire ? w.texAltFire : w.texFire;
    case WeaponState::RELOAD:
        if (w.animFrameCount > 0 && w.reloadFrame < w.animFrameCount)
            return w.texAnim[w.reloadFrame];
        return w.texIdle;
    default: return w.texIdle;
    }
}

void cleanupWeapon(Weapon& w) {
    auto del = [](unsigned int& t) { if (t) { glDeleteTextures(1, &t); t = 0; } };
    del(w.texIdle);
    del(w.texFire);
    del(w.texAltFire);
    for (int i = 0; i < w.animFrameCount; i++) del(w.texAnim[i]);
}
