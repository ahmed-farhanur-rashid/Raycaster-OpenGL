#ifndef GAME_STATE_H
#define GAME_STATE_H

enum class GameState { MAIN_MENU, PLAYING, PAUSED };

namespace game {
    extern GameState state;

    void setState(GameState s);
    GameState getState();
}

#endif
