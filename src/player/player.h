#ifndef PLAYER_H
#define PLAYER_H

namespace player {

    // Physics constants — tune these freely
    constexpr float GRAVITY    = 14.0f;
    constexpr float JUMP_SPEED = 3.0f;

    struct PlayerState {
        float posX, posY;
        float dirX, dirY;
        float planeX, planeY;
        float moveSpeed;
        float sprintSpeed;
        float rotSpeed;
        float posZ;       // vertical position (0 = ground)
        float velZ;       // vertical velocity
    };

    extern PlayerState player;

    void initPlayer();
    void movePlayer(float forward, float strafe, float deltaTime, bool sprinting = false);
    void rotatePlayer(float angle);
    void updatePhysics(float deltaTime);
    void jump();
}

#endif
