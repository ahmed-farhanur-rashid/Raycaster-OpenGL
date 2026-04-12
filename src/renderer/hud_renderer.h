#ifndef HUD_RENDERER_H
#define HUD_RENDERER_H

namespace hud {
    void initHUD(int screenW, int screenH);
    void updateBob(bool isMoving, float deltaTime);
    void updateRecoil(bool isFiring, float deltaTime);
    void triggerDamageFlash();
    void renderWeapon();          // call after renderer::renderFrame()
    void renderHUD();             // <-- ADD: health bar, ammo, crosshair
    void renderGameOver();        // <-- ADD: game over screen
    void cleanupHUD();
}

#endif
