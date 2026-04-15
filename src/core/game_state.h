#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <GLFW/glfw3.h>

enum class GameState { MAIN_MENU, PLAYING, PAUSED };

namespace game {
    void     setState(GameState s);
    GameState getState();

    int menuSelection();
    int pauseSelection();

    /* per-frame menu/state-transition logic — call once per frame */
    void updateMenuInput(GLFWwindow* window);
}

#endif
