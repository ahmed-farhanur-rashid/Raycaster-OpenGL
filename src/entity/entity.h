#ifndef ENTITY_H
#define ENTITY_H

#include <vector>

/* ================================================================== */
/*  Entity / projectile / combat types                                 */
/* ================================================================== */

enum class EntityType { GHOST };

struct Entity {
    EntityType type;
    float x, y;            /* world position                          */
    float dx, dy;           /* movement direction (normalised)         */
    float speed;
    float hp;
    float maxHp;
    bool  alive;
    bool  agro;             /* chasing player?                         */
    float agroRange;        /* distance at which it starts chasing     */
    float attackRange;      /* distance at which it deals damage       */
    float attackCooldown;   /* seconds between attacks                 */
    float attackTimer;      /* time until next attack                  */
    float damage;           /* damage per hit                          */
    float hitFlash;         /* >0 while flashing from being hit        */
    float deadTimer;        /* seconds since death (for cleanup)       */

    /* roaming state (idle wander) */
    float roamDirX, roamDirY; /* current wander direction              */
    float roamTimer;        /* time until next direction change         */
    float roamPause;        /* time spent pausing (standing still)     */

    /* sprite texture indices */
    int sprIdle;
    int sprAgro;
    int sprWounded;
    int sprDead;
    float spriteScale;      /* visual size multiplier for rendering    */
};

enum class ProjOwner { PLAYER, ENEMY };

struct Projectile {
    float x, y;
    float dx, dy;           /* direction (normalised)                  */
    float speed;
    float damage;
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
/*  Spawn configuration (per entity type)                              */
/* ================================================================== */

struct SpawnConfig {
    bool  enabled;          /* toggle spawning on/off                  */
    int   maxCount;         /* max alive entities of this type         */
    float interval;         /* seconds between spawn attempts          */
    float distance;         /* spawn distance from player              */
};

/* ================================================================== */
/*  entity namespace — global state                                    */
/* ================================================================== */

namespace entity {
    extern std::vector<Entity>     entities;
    extern std::vector<Projectile> projectiles;
    extern float playerHp;
    extern float playerMaxHp;

    /* spawn.cpp */
    void initSpawner();
    void updateSpawner(float dt, float px, float py,
                       int sprIdle, int sprAgro, int sprWounded, int sprDead);

    /* ghost_behavior.cpp */
    void updateGhost(Entity& e, float dt, float px, float py);

    /* combat.cpp */
    void spawnProjectile(float x, float y, float dirX, float dirY,
                         float speed, float damage, ProjOwner owner, int sprIdx,
                         float range, float visualScale, float spawnZ);
    void updateCombat(float dt, float px, float py);
    void hitscanFire(float px, float py, float dirX, float dirY,
                     float damage, float range);
}

#endif
