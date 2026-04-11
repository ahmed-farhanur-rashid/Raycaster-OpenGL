#ifndef PROJECTILE_H
#define PROJECTILE_H

#define MAX_PROJECTILES 64

namespace projectile {

    struct Projectile {
        float x, y;       // world position
        float dirX, dirY; // direction
        float speed;      // movement speed
        bool  active;     // is this projectile alive?
        int   spriteType; // which sprite to render
    };

    extern Projectile projectiles[MAX_PROJECTILES];
    extern int numProjectiles;

    void spawnProjectile(float x, float y, float dirX, float dirY);
    void updateProjectiles(float deltaTime);
}

#endif
