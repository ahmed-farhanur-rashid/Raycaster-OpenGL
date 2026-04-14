#include "input.h"
#include "../player/player.h"

namespace input {

bool minimapEnabled = true;
bool lightingEnabled = true;

static bool lKeyWasPressed = false;
static bool mKeyWasPressed = false;
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
}

} // namespace input