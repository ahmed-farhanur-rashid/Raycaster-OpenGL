#ifndef PLAYER_H
#define PLAYER_H

namespace player {
    struct PlayerState {
        float posX, posY;
        float dirX, dirY;
        float planeX, planeY;
        float moveSpeed;
        float rotSpeed;
    };

    extern PlayerState player;

    void initPlayer();
    void movePlayer(float forward, float strafe, float deltaTime);
    void rotatePlayer(float angle);
}

#endif
