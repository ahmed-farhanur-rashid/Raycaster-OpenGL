#include "weapon.h"
#include "../settings/settings.h"
#include <cstdio>

/* ================================================================== */
/*  Shotgun                                                            */
/* ================================================================== */

static void sgHandleFire(Weapon& w, float dt, bool lmb, bool rmb) {
    (void)dt; (void)rmb;
    if (lmb && !w.prevLmb)
        weaponFirePrimary(w);
}

static void sgAdvanceAnim(Weapon& w, float dt) {
    switch (w.state) {
    case WeaponState::FIRE_BULLET:
        w.animTimer += dt;
        if (w.animTimer >= w.fireDur) {
            /* auto-reload (pump) after every shot */
            w.state       = WeaponState::RELOAD;
            w.animTimer   = 0.0f;
            w.reloadFrame = 0;
        }
        break;
    case WeaponState::RELOAD:
        w.animTimer += dt;
        if (w.animTimer >= w.animFrameDur) {
            w.animTimer -= w.animFrameDur;
            w.reloadFrame++;
            if (w.reloadFrame >= w.animFrameCount) {
                w.reloadFrame = 0;
                w.state = WeaponState::IDLE;
            }
        }
        break;
    default: break;
    }
}

Weapon createShotgun() {
    Weapon w;
    w.type = WeaponType::SHOTGUN;

    w.fireDur      = settings::getFloat("sg_fire_duration",  0.15f);
    w.animFrameDur = settings::getFloat("sg_reload_duration", 0.10f);
    w.unlimited    = true;

    w.recoilKick = 0.07f;

    w.handleFire  = sgHandleFire;
    w.advanceAnim = sgAdvanceAnim;

    /* textures */
    w.texIdle = loadWeaponTexture("resource/textures/weapons/shotgun/idle/shotgun_idle.png");
    w.texFire = loadWeaponTexture("resource/textures/weapons/shotgun/fire/shotgun_fire.png");
    w.animFrameCount = 5;
    { char buf[128];
      for (int i = 0; i < w.animFrameCount; i++) {
          snprintf(buf, sizeof(buf),
                   "resource/textures/weapons/shotgun/reload/shotgun_reload_%02d.png", i + 1);
          w.texAnim[i] = loadWeaponTexture(buf);
      }
    }

    return w;
}
