#include "weapon.h"
#include "../settings/settings.h"
#include <cstdio>

/* ================================================================== */
/*  Handgun                                                            */
/* ================================================================== */

static void hgHandleFire(Weapon& w, float dt, bool lmb, bool rmb) {
    (void)dt; (void)rmb;
    if (lmb && !w.prevLmb)
        weaponFirePrimary(w);
}

static void hgAdvanceAnim(Weapon& w, float dt) {
    if (w.state != WeaponState::FIRE_BULLET) return;
    w.animTimer += dt;
    if (w.animTimer >= w.fireDur)
        w.state = WeaponState::IDLE;
}

Weapon createHandgun() {
    Weapon w;
    w.type      = WeaponType::HANDGUN;
    w.fireDur   = settings::getFloat("handgun_fire_duration", 0.15f);
    w.unlimited = true;

    w.recoilKick = settings::getFloat("handgun_recoil", 0.04f);

    w.handleFire  = hgHandleFire;
    w.advanceAnim = hgAdvanceAnim;

    /* textures */
    w.texIdle = loadWeaponTexture("resource/textures/weapons/handgun/idle/hand_gun_idle.png");
    w.texFire = loadWeaponTexture("resource/textures/weapons/handgun/fire/handgun_fire.png");

    return w;
}
