#include "player.h"
#include "../map/map.h"
#include <cmath>

namespace player {

PlayerState player;

void initPlayer() {
    player.posX = 1.5f;
    player.posY = 1.5f;
    player.dirX = 1.0f;
    player.dirY = 0.0f;
    player.planeX = 0.0f;
    player.planeY = 0.66f;
    player.moveSpeed = 3.0f;
    player.rotSpeed = 2.5f;
}

void movePlayer(float forward, float strafe, float deltaTime) {
    float speed = player.moveSpeed * deltaTime;
    float rightX = -player.dirY;
    float rightY = player.dirX;
    float moveX = player.dirX * forward + rightX * strafe;
    float moveY = player.dirY * forward + rightY * strafe;

    float newX = player.posX + moveX * speed;
    float newY = player.posY + moveY * speed;

    float margin = 0.2f;

    int checkX = (int)(newX + (moveX > 0 ? margin : -margin));
    if (checkX >= 0 && checkX < map::mapWidth && map::worldMap[(int)player.posY][checkX] == 0)
        player.posX = newX;

    int checkY = (int)(newY + (moveY > 0 ? margin : -margin));
    if (checkY >= 0 && checkY < map::mapHeight && map::worldMap[checkY][(int)player.posX] == 0)
        player.posY = newY;
}

void rotatePlayer(float angle) {

    // Formula to rotate a vector (x, y) by an angle θ:
    //x' = x cosθ − y sinθ  
    //y' = x sinθ + y cosθ

    float c = cosf(angle);
    float s = sinf(angle);
    float oldDirX = player.dirX;
    player.dirX = player.dirX * c - player.dirY * s;
    player.dirY = oldDirX * s + player.dirY * c; // we change player.dirX before calculating player.dirY, so we need to store the old value in a temporary variable
    float oldPlaneX = player.planeX;
    player.planeX = player.planeX * c - player.planeY * s;
    player.planeY = oldPlaneX * s + player.planeY * c;
}

} // namespace player
