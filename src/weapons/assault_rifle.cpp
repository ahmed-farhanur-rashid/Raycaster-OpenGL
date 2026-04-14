#include "weapon.h"
#include "../settings/settings.h"
#include <cstdio>

/* ================================================================== */
/*  Assault Rifle                                                      */
/* ================================================================== */

static void arHandleFire(Weapon& w, float dt, bool lmb, bool rmb) {
    (void)rmb;
    if (lmb) {
        if (!w.prevLmb) {
            weaponFirePrimary(w);
            w.autoFireTimer = 0.0f;
        } else {
            w.autoFireTimer += dt;
            if (w.autoFireTimer >= w.autoFireInterval && w.state == WeaponState::IDLE) {
                w.autoFireTimer -= w.autoFireInterval;
                weaponFirePrimary(w);
            }
        }
    }
    if (!lmb) w.autoFireTimer = 0.0f;
}

static void arAdvanceAnim(Weapon& w, float dt) {
    switch (w.state) {
    case WeaponState::FIRE_BULLET:
        w.animTimer += dt;
        if (w.animTimer >= w.fireDur) {
            if (w.ammo1 <= 0) weaponStartReload(w);
            else              w.state = WeaponState::IDLE;
        }
        break;
    case WeaponState::FIRE_GRENADE:
        w.animTimer += dt;
        if (w.animTimer >= w.altFireDur) {
            if (w.ammo2 <= 0) weaponStartReload(w);
            else              w.state = WeaponState::IDLE;
        }
        break;
    case WeaponState::RELOAD:
        w.animTimer += dt;
        if (w.animTimer >= w.animFrameDur) {
            w.animTimer -= w.animFrameDur;
            w.reloadFrame++;
            if (w.reloadFrame >= w.animFrameCount) {
                w.reloadFrame = 0;
                w.ammo1 = w.maxAmmo1;
                w.ammo2 = w.maxAmmo2;
                w.state = WeaponState::IDLE;
            }
        }
        break;
    default: break;
    }
}

Weapon createAssaultRifle() {
    Weapon w;
    w.type = WeaponType::ASSAULT_RIFLE;

    /* timing from config */
    w.fireDur          = settings::getFloat("ar_fire_duration",      0.12f);
    w.altFireDur       = settings::getFloat("ar_grenade_duration",   0.25f);
    w.animFrameDur     = settings::getFloat("ar_reload_duration",    0.10f);
    w.autoFire         = true;
    w.autoFireInterval = settings::getFloat("ar_auto_fire_interval", 0.07f);

    /* ammo */
    w.unlimited = false;
    w.maxAmmo1  = settings::getInt("ar_max_bullets",  30);
    w.ammo1     = w.maxAmmo1;
    w.maxAmmo2  = settings::getInt("ar_max_grenades", 5);
    w.ammo2     = w.maxAmmo2;
    w.hasAltFire = true;

    /* recoil */
    w.recoilKick    = 0.04f;
    w.altRecoilKick = 0.06f;

    /* bars */
    w.bar1 = { true, 0.9f, 0.7f, 0.1f };  /* gold — bullets  */
    w.bar2 = { true, 0.1f, 0.8f, 0.2f };  /* green — grenades */

    /* callbacks */
    w.handleFire  = arHandleFire;
    w.advanceAnim = arAdvanceAnim;

    /* textures */
    w.texIdle    = loadWeaponTexture("resource/textures/weapons/assault/idle/assault_idle.png");
    w.texFire    = loadWeaponTexture("resource/textures/weapons/assault/fire/assault_fire_bullet_left_click.png");
    w.texAltFire = loadWeaponTexture("resource/textures/weapons/assault/fire/assault_fire_grenade_right_click.png");
    w.animFrameCount = 11;
    { char buf[128];
      for (int i = 0; i < w.animFrameCount; i++) {
          snprintf(buf, sizeof(buf),
                   "resource/textures/weapons/assault/reload/assault_reload_%02d.png", i + 1);
          w.texAnim[i] = loadWeaponTexture(buf);
      }
    }

    return w;
}
