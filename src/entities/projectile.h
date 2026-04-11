#ifndef PROJECTILE_H
#define PROJECTILE_H

#define MAX_PROJECTILES 64

namespace projectile {

    struct Projectile {
        float x, y;       // world position
        float z;          // vertical offset (for player jump)
        float dirX, dirY; // direction
        float speed;      // movement speed
        bool  active;     // is this projectile alive?
        int   spriteType; // which sprite to render
        bool  fromPlayer; // true = player bullet, false = enemy bullet
    };

    extern Projectile projectiles[MAX_PROJECTILES];
    extern int numProjectiles;

    void spawnProjectile(float x, float y, float z, float dirX, float dirY, bool fromPlayer);
    void updateProjectiles(float deltaTime);
}

#endif
