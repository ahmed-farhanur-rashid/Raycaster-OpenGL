#include "input.h"
#include "../player/player.h"

namespace input {

bool minimapEnabled = true;
bool lightingEnabled = true;

static bool lKeyWasPressed = false;
static bool mKeyWasPressed = false;

void processInput(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float forward = 0.0f, strafe = 0.0f;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        forward += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        forward -= 1.0f;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        strafe += 1.0f;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        strafe -= 1.0f;

    if (forward != 0.0f || strafe != 0.0f)
        player::movePlayer(forward, strafe, dt);

    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
        player::rotatePlayer(-player::player.rotSpeed * dt);
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
        player::rotatePlayer(player::player.rotSpeed * dt);

    bool mNow = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
    if (mNow && !mKeyWasPressed) minimapEnabled = !minimapEnabled;
    mKeyWasPressed = mNow;

    bool lNow = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
    if (lNow && !lKeyWasPressed) lightingEnabled = !lightingEnabled;
    lKeyWasPressed = lNow;
}

} // namespace input