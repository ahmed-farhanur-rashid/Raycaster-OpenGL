#ifndef PLAYER_H
#define PLAYER_H

struct Player {
    float posX, posY;
    float dirX, dirY;
    float planeX, planeY;
    float moveSpeed;
    float rotSpeed;
};

extern Player player;

void initPlayer();
void movePlayer(float forward, float strafe, float dt);
void rotatePlayer(float angle);

#endif
