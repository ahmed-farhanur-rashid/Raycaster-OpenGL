#include "game_state.h"

namespace game {

GameState state = GameState::MAIN_MENU;

void setState(GameState s) { state = s; }
GameState getState() { return state; }

} // namespace game
