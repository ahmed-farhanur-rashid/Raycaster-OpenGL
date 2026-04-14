#ifndef PLAYER_H
#define PLAYER_H

namespace player {

    // Physics constants — tune these freely
    constexpr float GRAVITY    = 9.8f;
    constexpr float JUMP_SPEED = 4.0f;

    struct PlayerState {
        float posX, posY;
        float dirX, dirY;
        float planeX, planeY;
        float moveSpeed;
        float rotSpeed;

        float posZ;   // <-- ADD: vertical position (0.0 = ground)
        float velZ;   // <-- ADD: vertical velocity (world units/sec)

        int health;     // <-- ADD (0–100)
        int maxHealth;  // <-- ADD
        int ammo;       // <-- ADD
        int maxAmmo;    // <-- ADD

        float damageFlash;  // <-- ADD: decays to 0 when no damage
        float healFlash;    // <-- ADD
    };

    extern PlayerState player;

    void initPlayer();
    void movePlayer(float forward, float strafe, float deltaTime);
    void rotatePlayer(float angle);
    void updatePhysics(float deltaTime);  // <-- ADD
    void jump();                          // <-- ADD
    void takeDamage(int amount);          // <-- ADD: damage player
    bool isDead();                        // <-- ADD: check if player is dead
    void checkPickups(float deltaTime);   // <-- ADD: check for sprite pickups
    void updateEffects(float deltaTime);  // <-- ADD: decay screen effects
}

#endif
