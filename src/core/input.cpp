#include "input.h"
#include "../player/player.h"
#include "../renderer/hud_renderer.h"
#include "../entities/projectile.h"

namespace input {

bool minimapEnabled = true;
bool lightingEnabled = true;

static bool lKeyWasPressed = false;
static bool mKeyWasPressed = false;

// --- ADD THESE TWO ---
static double lastMouseX = 0.0;
static bool   mousePrimed = false;          // skip the first frame's garbage delta
// ----------------------

void processInput(GLFWwindow* window, float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // ---- ADD: run physics every frame ----
    player::updatePhysics(deltaTime);

    // ---- ADD: space bar to jump ----
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        player::jump();

    // --- ADD THIS BLOCK ---
    {
        const float MOUSE_SENSITIVITY = 0.002f; // radians per pixel — tune to taste

        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        if (!mousePrimed) {
            lastMouseX  = mouseX;   // initialise on first frame, don't rotate
            mousePrimed = true;
        } else {
            double deltaX = mouseX - lastMouseX;
            lastMouseX    = mouseX;

            if (deltaX != 0.0)
                player::rotatePlayer((float)(deltaX * MOUSE_SENSITIVITY));
        }
    }
    // ----------------------

    float forward = 0.0f;
    float strafe = 0.0f;
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        forward += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        forward -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        strafe += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        strafe -= 1.0f;

    if (forward != 0.0f || strafe != 0.0f)
        player::movePlayer(forward, strafe, deltaTime);

    // Update weapon bob based on movement
    bool moving = (forward != 0.0f || strafe != 0.0f);
    hud::updateBob(moving, deltaTime);

    // Automatic fire with F key or left mouse
    static float fireCooldown = 0.0f;
    const float FIRE_RATE = 0.1f;  // 10 bullets per second (automatic)
    
    bool fireNow = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS ||
                   glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    
    if (fireNow && fireCooldown <= 0.0f && player::player.ammo > 0) {
        projectile::spawnProjectile(
            player::player.posX,
            player::player.posY,
            player::player.posZ,  // Pass player height for bullet jumping
            player::player.dirX,
            player::player.dirY,
            true  // fromPlayer
        );
        player::player.ammo--;  // Decrease ammo
        fireCooldown = FIRE_RATE;
    }
    
    if (fireCooldown > 0.0f) {
        fireCooldown -= deltaTime;
    }
    
    // Update weapon recoil
    hud::updateRecoil(fireNow && fireCooldown > 0.0f, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        player::rotatePlayer(-player::player.rotSpeed * deltaTime);
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        player::rotatePlayer(player::player.rotSpeed * deltaTime);

    bool mNow = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
    if (mNow && !mKeyWasPressed) minimapEnabled = !minimapEnabled;
    mKeyWasPressed = mNow;

    bool lNow = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
    if (lNow && !lKeyWasPressed) lightingEnabled = !lightingEnabled;
    lKeyWasPressed = lNow;
}

} // namespace input