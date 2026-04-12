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
    };

    extern PlayerState player;

    void initPlayer();
    void movePlayer(float forward, float strafe, float deltaTime);
    void rotatePlayer(float angle);
    void updatePhysics(float deltaTime);  // <-- ADD
    void jump();                          // <-- ADD
}

#endif
