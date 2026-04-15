#include "player.h"
#include "../map/map.h"
#include "../settings/settings.h"
#include <cmath>

namespace player {

PlayerState player;
float GRAVITY    = 14.0f;
float JUMP_SPEED = 5.0f;

void initPlayer() {
    GRAVITY    = settings::getFloat("gravity",    14.0f);
    JUMP_SPEED = settings::getFloat("jump_speed", 5.0f);

    player.posX = 1.5f;
    player.posY = 1.5f;
    player.dirX = 1.0f;
    player.dirY = 0.0f;
    player.planeX = 0.0f;
    player.planeY = 0.666f;   /* W/(2H) → correct 1:1 aspect at 4:3 */  
    player.moveSpeed  = settings::getFloat("move_speed",     3.0f);
    player.sprintSpeed = settings::getFloat("sprint_speed",  4.5f);
    player.rotSpeed   = settings::getFloat("rotation_speed", 2.5f);
    player.posZ = 0.0f;
    player.velZ = 0.0f;
}

void movePlayer(float forward, float strafe, float deltaTime, bool sprinting) {
    float speed = (sprinting ? player.sprintSpeed : player.moveSpeed) * deltaTime;
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

void startJump() {
    if (player.posZ == 0.0f)        // only jump when grounded
        player.velZ = JUMP_SPEED;
}

void updatePhysics(float deltaTime) {
    // Apply gravity
    player.velZ -= GRAVITY * deltaTime;

    // Integrate vertical position
    player.posZ += player.velZ * deltaTime;

    // Ground collision — clamp to floor
    if (player.posZ <= 0.0f) {
        player.posZ = 0.0f;
        player.velZ = 0.0f;
    }
}

void jump() {
    if (player.posZ == 0.0f)        // only jump when grounded
        player.velZ = JUMP_SPEED;
}

} // namespace player
