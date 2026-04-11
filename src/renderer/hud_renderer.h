#ifndef HUD_RENDERER_H
#define HUD_RENDERER_H

namespace hud {
    void initHUD(int screenW, int screenH);
    void updateBob(bool isMoving, float deltaTime);
    void renderWeapon();          // call after renderer::renderFrame()
    void cleanupHUD();
}

#endif
