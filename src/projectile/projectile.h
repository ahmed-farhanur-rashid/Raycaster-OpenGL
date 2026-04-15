#ifndef PROJECTILE_H
#define PROJECTILE_H

#include <vector>

/* ================================================================== */
/*  Projectile types                                                   */
/* ================================================================== */

enum class ProjOwner { PLAYER };

struct Projectile {
    float x, y;
    float dx, dy;           /* direction (normalised)                  */
    float speed;
    float lifetime;         /* seconds remaining                       */
    float range;            /* max travel distance                     */
    float traveled;         /* distance traveled so far                */
    float visualScale;      /* sprite size multiplier                  */
    float spawnZ;           /* player height when fired (for rendering)*/
    ProjOwner owner;
    bool  alive;
    int   sprIdx;           /* 0=bullet, 1=beam, 2=shotgun             */
};

/* ================================================================== */
/*  projectile namespace — global state                                */
/* ================================================================== */

namespace projectile {
    extern std::vector<Projectile> projectiles;

    void initProjectiles();

    void spawnProjectile(float x, float y, float dirX, float dirY,
                         float speed, ProjOwner owner, int sprIdx,
                         float range, float visualScale, float spawnZ);
    void updateProjectiles(float dt);
}

#endif
