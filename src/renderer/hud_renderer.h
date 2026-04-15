#ifndef HUD_RENDERER_H
#define HUD_RENDERER_H

#include "../weapons/weapon.h"

namespace hud {

    void initHUD(int screenW, int screenH);
    void updateHUD(bool isMoving, float deltaTime, bool lmbHeld, bool rmbHeld);
    void fireGrenade();
    void reload();
    WeaponType currentWeapon();
    void equipAssaultRifle();
    void equipShotgun();
    void equipEnergyWeapon();
    void equipHandgun();
    void renderWeapon();
    void cleanupHUD();
    bool firedThisFrame();
    bool firedAltThisFrame();
}

#endif
