#include "player.h"
#include "../map/map.h"
#include <cmath>
#include <cstdio>

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
    player.posZ = 0.0f;   // <-- ADD
    player.velZ = 0.0f;   // <-- ADD
    player.health    = 100;
    player.maxHealth = 100;
    player.ammo      = 30;
    player.maxAmmo   = 30;
    player.damageFlash = 0.0f;
    player.healFlash   = 0.0f;
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

void takeDamage(int amount) {
    player.health -= amount;
    if (player.health < 0) player.health = 0;
    player.damageFlash = 1.0f;   // full intensity, will decay over time
}

bool isDead() {
    return player.health <= 0;
}

void checkPickups(float deltaTime) {
    // Check collision with map sprites (pickups)
    for (int i = 0; i < map::numSprites; i++) {
        if (map::mapSprites[i].type == -1) continue;  // Already picked up
        
        float dx = map::mapSprites[i].x - player.posX;
        float dy = map::mapSprites[i].y - player.posY;
        float dist = sqrtf(dx*dx + dy*dy);
        
        if (dist < 0.5f) {  // Pickup radius
            // Type 5 = health, Type 6 = ammo
            if (map::mapSprites[i].type == 5) {  // Health pickup
                if (player.health < player.maxHealth) {
                    int oldHealth = player.health;
                    player.health += 25;
                    if (player.health > player.maxHealth) player.health = player.maxHealth;
                    printf("Picked up health! Health: %d\n", player.health);
                    map::mapSprites[i].type = -1;  // Mark as collected
                    
                    // Trigger heal flash if health was actually restored
                    if (player.health > oldHealth) {
                        player.healFlash = 1.0f;
                    }
                }
            } else if (map::mapSprites[i].type == 6) {  // Ammo pickup
                if (player.ammo < player.maxAmmo) {
                    player.ammo += 15;
                    if (player.ammo > player.maxAmmo) player.ammo = player.maxAmmo;
                    printf("Picked up ammo! Ammo: %d\n", player.ammo);
                    map::mapSprites[i].type = -1;  // Mark as collected
                }
            }
        }
    }
}

void updateEffects(float deltaTime) {
    const float FLASH_DECAY = 2.5f;   // full flash fades in ~0.4 seconds
    player.damageFlash -= FLASH_DECAY * deltaTime;
    if (player.damageFlash < 0.0f) player.damageFlash = 0.0f;

    player.healFlash -= FLASH_DECAY * deltaTime;
    if (player.healFlash < 0.0f) player.healFlash = 0.0f;
}

} // namespace player
