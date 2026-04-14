#include "input.h"
#include "../player/player.h"
#include "../renderer/hud_renderer.h"

namespace input {

bool minimapEnabled = true;
bool lightingEnabled = true;

static bool lKeyWasPressed = false;
static bool mKeyWasPressed = false;
static bool rKeyWasPressed = false;
static bool key1WasPressed = false;
static bool key2WasPressed = false;
static bool key3WasPressed = false;
static bool key4WasPressed = false;
static bool rmbWasPressed  = false;
static double lastMouseX = 0.0;
static bool   mousePrimed = false;          // skip the first frame's garbage delta

void initMouse(GLFWwindow* window) {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void processInput(GLFWwindow* window, float deltaTime) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // run physics every frame
    player::updatePhysics(deltaTime);

    // space bar to jump
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        player::jump();

    // horizontal mouse look
    {
        const float MOUSE_SENSITIVITY = 0.002f;

        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        if (!mousePrimed) {
            lastMouseX  = mouseX;
            mousePrimed = true;
        } else {
            double deltaX = mouseX - lastMouseX;
            lastMouseX    = mouseX;

            if (deltaX != 0.0)
                player::rotatePlayer((float)(deltaX * MOUSE_SENSITIVITY));
        }
    }

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

    bool sprinting = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
                   || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

    if (forward != 0.0f || strafe != 0.0f)
        player::movePlayer(forward, strafe, deltaTime, sprinting);

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

    /* weapon controls */
    bool key1Now = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
    if (key1Now && !key1WasPressed) hud::equipAssaultRifle();
    key1WasPressed = key1Now;

    bool key2Now = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
    if (key2Now && !key2WasPressed) hud::equipShotgun();
    key2WasPressed = key2Now;

    bool key3Now = glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS;
    if (key3Now && !key3WasPressed) hud::equipEnergyWeapon();
    key3WasPressed = key3Now;

    bool key4Now = glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS;
    if (key4Now && !key4WasPressed) hud::equipHandgun();
    key4WasPressed = key4Now;

    bool lmbNow = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    bool rmbNow = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    /* for non-energy weapons, RMB fires grenade on press */
    if (hud::currentWeapon() != hud::WeaponType::ENERGY_WEAPON) {
        if (rmbNow && !rmbWasPressed) hud::fireGrenade();
    }
    rmbWasPressed = rmbNow;

    bool rNow = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    if (rNow && !rKeyWasPressed) hud::reload();
    rKeyWasPressed = rNow;

    /* update HUD bob + animation */
    bool isMoving = (forward != 0.0f || strafe != 0.0f);
    hud::updateHUD(isMoving, deltaTime, lmbNow, rmbNow);
}

} // namespace input