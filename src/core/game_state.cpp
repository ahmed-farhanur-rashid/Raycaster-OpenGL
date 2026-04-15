#include "game_state.h"
#include "input.h"
#include "../player/player.h"
#include "../projectile/projectile.h"
#include "../renderer/menu_renderer.h"

namespace game {

static GameState state = GameState::MAIN_MENU;

/* menu navigation */
static int  menuSel   = 0;
static int  pauseSel  = 0;

/* edge-detection state */
static bool escWasPressed   = false;
static bool upWasPressed    = false;
static bool downWasPressed  = false;
static bool enterWasPressed = false;

void setState(GameState s) { state = s; }
GameState getState()       { return state; }

int menuSelection()  { return menuSel; }
int pauseSelection() { return pauseSel; }

static void resetGame() {
    player::initPlayer();
    projectile::initProjectiles();
}

void updateMenuInput(GLFWwindow* window) {
    /* edge-detect menu keys */
    bool escNow   = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    bool upNow    = glfwGetKey(window, GLFW_KEY_UP)     == GLFW_PRESS ||
                    glfwGetKey(window, GLFW_KEY_W)      == GLFW_PRESS;
    bool downNow  = glfwGetKey(window, GLFW_KEY_DOWN)   == GLFW_PRESS ||
                    glfwGetKey(window, GLFW_KEY_S)      == GLFW_PRESS;
    bool enterNow = glfwGetKey(window, GLFW_KEY_ENTER)  == GLFW_PRESS;

    bool escEdge   = escNow   && !escWasPressed;
    bool upEdge    = upNow    && !upWasPressed;
    bool downEdge  = downNow  && !downWasPressed;
    bool enterEdge = enterNow && !enterWasPressed;

    escWasPressed   = escNow;
    upWasPressed    = upNow;
    downWasPressed  = downNow;
    enterWasPressed = enterNow;

    switch (state) {

    case GameState::MAIN_MENU:
        if (upEdge)   menuSel = (menuSel - 1 + 2) % 2;
        if (downEdge) menuSel = (menuSel + 1) % 2;
        if (enterEdge) {
            if (menuSel == 0) {
                resetGame();
                setState(GameState::PLAYING);
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                input::initMouse(window);
            } else {
                glfwSetWindowShouldClose(window, true);
            }
        }
        if (escEdge)
            glfwSetWindowShouldClose(window, true);
        break;

    case GameState::PAUSED:
        if (upEdge)   pauseSel = (pauseSel - 1 + 3) % 3;
        if (downEdge) pauseSel = (pauseSel + 1) % 3;
        if (enterEdge) {
            if (pauseSel == 0) {
                setState(GameState::PLAYING);
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                input::initMouse(window);
            } else if (pauseSel == 1) {
                setState(GameState::MAIN_MENU);
                menuSel = 0;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                glfwSetWindowShouldClose(window, true);
            }
        }
        if (escEdge) {
            setState(GameState::PLAYING);
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            input::initMouse(window);
        }
        break;

    case GameState::PLAYING:
        if (escEdge) {
            setState(GameState::PAUSED);
            pauseSel = 0;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        break;
    }
}

} // namespace game
