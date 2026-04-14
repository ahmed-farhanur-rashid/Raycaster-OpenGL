#ifndef HUD_RENDERER_H
#define HUD_RENDERER_H

#include "../weapons/weapon.h"

namespace hud {

    void initHUD(int screenW, int screenH);
    void updateHUD(bool isMoving, float deltaTime, bool lmbHeld, bool rmbHeld);
    void fireBullet();
    void fireGrenade();
    void reload();
    bool hasWeaponEquipped();
    WeaponType currentWeapon();
    void equipAssaultRifle();       // press 1
    void equipShotgun();            // press 2
    void equipEnergyWeapon();       // press 3
    void equipHandgun();            // press 4
    void renderWeapon();            // call after renderer::renderFrame()
    void cleanupHUD();

    int getBullets();
    int getMaxBullets();
    int getGrenades();
    int getMaxGrenades();
}

#endif
