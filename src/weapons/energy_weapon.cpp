#include "weapon.h"
#include "../settings/settings.h"
#include <cmath>
#include <cstdio>

/* ================================================================== */
/*  Energy Weapon                                                      */
/* ================================================================== */

static void enHandleFire(Weapon& w, float dt, bool lmb, bool rmb) {
    /* ---------- drain while HOLDING (not first frame) ---------- */
    if (!w.depleted1 && !w.depleted2) {
        bool lmbDrain = lmb && w.prevLmb;
        bool rmbDrain = rmb && w.prevRmb;

        if (lmbDrain && w.state == WeaponState::FIRE_BULLET) {
            w.charge1 -= dt / w.drainTime1;
            if (w.charge1 <= 0.0f) {
                w.charge1 = 0.0f;
                w.depleted1 = true;
                weaponStartReload(w);
            }
        } else if (rmbDrain && w.state == WeaponState::FIRE_GRENADE) {
            w.charge2 -= dt / w.drainTime2;
            if (w.charge2 <= 0.0f) {
                w.charge2 = 0.0f;
                w.depleted2 = true;
                weaponStartReload(w);
            }
        }

        /* recharge when NOT firing that mode */
        if (!lmb && w.charge1 < 1.0f && !w.depleted1)
            w.charge1 = fminf(1.0f, w.charge1 + dt * w.rechargeRate);
        if (!rmb && w.charge2 < 1.0f && !w.depleted2)
            w.charge2 = fminf(1.0f, w.charge2 + dt * w.rechargeRate);
    }

    /* ---------- hold-to-fire ---------- */
    if (lmb && !w.depleted1) {
        if (w.state == WeaponState::IDLE || w.state == WeaponState::FIRE_BULLET) {
            w.state     = WeaponState::FIRE_BULLET;
            w.animTimer = 0.0f;
            /* continuous beam: fire at interval */
            if (!w.prevLmb) {
                w.firedThisFrame = true;
                w.autoFireTimer = 0.0f;
            } else {
                w.autoFireTimer += dt;
                float interval = settings::getFloat("energy_fire_interval", 0.08f);
                if (w.autoFireTimer >= interval) {
                    w.autoFireTimer -= interval;
                    w.firedThisFrame = true;
                }
            }
        }
    } else if (rmb && !w.depleted2) {
        if (w.state == WeaponState::IDLE || w.state == WeaponState::FIRE_GRENADE) {
            w.state     = WeaponState::FIRE_GRENADE;
            w.animTimer = 0.0f;
            /* continuous beam RMB */
            if (!w.prevRmb) {
                w.firedAltThisFrame = true;
                w.autoFireTimer = 0.0f;
            } else {
                w.autoFireTimer += dt;
                float interval = settings::getFloat("energy_fire_interval", 0.08f);
                if (w.autoFireTimer >= interval) {
                    w.autoFireTimer -= interval;
                    w.firedAltThisFrame = true;
                }
            }
        }
    } else if (!lmb && !rmb && w.state != WeaponState::RELOAD) {
        w.state = WeaponState::IDLE;
        w.autoFireTimer = 0.0f;
    }
}

static void enAdvanceAnim(Weapon& w, float dt) {
    if (w.state != WeaponState::RELOAD) return;

    /* overheat animation */
    w.animTimer += dt;
    if (w.animTimer >= w.animFrameDur) {
        w.animTimer -= w.animFrameDur;
        w.reloadFrame++;
        if (w.reloadFrame >= w.animFrameCount) {
            w.reloadFrame = 0;
            if (w.depleted1) { w.charge1 = 1.0f; w.depleted1 = false; }
            if (w.depleted2) { w.charge2 = 1.0f; w.depleted2 = false; }
            w.state = WeaponState::IDLE;
        }
    }
}

Weapon createEnergyWeapon() {
    Weapon w;
    w.type     = WeaponType::ENERGY_WEAPON;
    w.isEnergy = true;

    w.fireDur      = settings::getFloat("energy_fire_duration",      0.12f);
    w.altFireDur   = w.fireDur;
    w.animFrameDur = settings::getFloat("energy_overheat_duration",  0.25f);
    w.unlimited    = true;
    w.hasAltFire   = true;

    w.drainTime1   = settings::getFloat("energy_drain_normal",       6.0f);
    w.drainTime2   = settings::getFloat("energy_drain_overcharge",   3.0f);
    w.rechargeRate = settings::getFloat("energy_recharge_rate",      0.15f);

    w.recoilKick    = settings::getFloat("energy_recoil", 0.04f);
    w.altRecoilKick = settings::getFloat("energy_overcharge_recoil", 0.06f);

    /* bars */
    w.bar1 = { true, 0.1f, 0.8f, 0.9f };  /* cyan — normal    */
    w.bar2 = { true, 0.9f, 0.2f, 0.8f };  /* magenta — overcharge */

    w.handleFire  = enHandleFire;
    w.advanceAnim = enAdvanceAnim;

    /* textures */
    w.texIdle    = loadWeaponTexture("resource/textures/weapons/energy/idle/energy_idle.png");
    w.texFire    = loadWeaponTexture("resource/textures/weapons/energy/fire/energy_fire_normal.png");
    w.texAltFire = loadWeaponTexture("resource/textures/weapons/energy/fire/energy_fire_overcharge.png");
    w.animFrameCount = 5;
    { char buf[128];
      for (int i = 0; i < w.animFrameCount; i++) {
          snprintf(buf, sizeof(buf),
                   "resource/textures/weapons/energy/overheat/energy_overheat_%02d.png", i + 1);
          w.texAnim[i] = loadWeaponTexture(buf);
      }
    }

    return w;
}
