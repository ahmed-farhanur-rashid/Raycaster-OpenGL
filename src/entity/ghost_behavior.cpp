/*
 * ghost_behavior.cpp — Ghost AI: roam idle, chase player when in agro range, attack in melee
 */

#include "entity.h"
#include "../map/map.h"
#include <cmath>
#include <cstdlib>

namespace entity {

/* pick a new random wander direction */
static void pickRoamDir(Entity& e) {
    float angle = (float)(rand() % 3600) / 3600.0f * 6.2831853f;
    e.roamDirX = cosf(angle);
    e.roamDirY = sinf(angle);
    e.roamTimer = 2.0f + (float)(rand() % 300) / 100.0f; /* 2-5 seconds */
    e.roamPause = 0.0f;
}

void updateGhost(Entity& e, float dt, float px, float py) {
    if (!e.alive) return;

    float ddx = px - e.x;
    float ddy = py - e.y;
    float dist = sqrtf(ddx * ddx + ddy * ddy);

    /* hit flash decay */
    if (e.hitFlash > 0.0f) e.hitFlash -= dt;

    /* agro check */
    e.agro = (dist < e.agroRange);

    if (!e.agro) {
        /* ---- idle roaming ---- */
        if (e.roamPause > 0.0f) {
            e.roamPause -= dt;
            return;
        }

        e.roamTimer -= dt;
        if (e.roamTimer <= 0.0f) {
            /* occasionally pause before picking new direction */
            e.roamPause = 1.0f + (float)(rand() % 200) / 100.0f;
            pickRoamDir(e);
            return;
        }

        float roamSpeed = e.speed * 0.4f; /* wander slower than chase */
        float step = roamSpeed * dt;
        float nx = e.x + e.roamDirX * step;
        float ny = e.y + e.roamDirY * step;

        /* wall collision — bounce off walls */
        float margin = 0.25f;
        int cx = (int)(nx + (e.roamDirX > 0 ? margin : -margin));
        bool xOk = cx >= 1 && cx < map::mapWidth - 1 && map::worldMap[(int)e.y][cx] == 0;
        int cy = (int)(ny + (e.roamDirY > 0 ? margin : -margin));
        bool yOk = cy >= 1 && cy < map::mapHeight - 1 && map::worldMap[cy][(int)e.x] == 0;

        if (xOk) e.x = nx;
        else     e.roamDirX = -e.roamDirX; /* reverse on wall hit */

        if (yOk) e.y = ny;
        else     e.roamDirY = -e.roamDirY;

        /* update facing direction for sprite */
        e.dx = e.roamDirX;
        e.dy = e.roamDirY;
        return;
    }

    /* ---- agro: chase player ---- */
    if (dist > 0.01f) {
        e.dx = ddx / dist;
        e.dy = ddy / dist;
    }

    if (dist > e.attackRange) {
        float step = e.speed * dt;
        float nx = e.x + e.dx * step;
        float ny = e.y + e.dy * step;

        /* simple wall collision */
        float margin = 0.25f;
        int cx = (int)(nx + (e.dx > 0 ? margin : -margin));
        if (cx >= 0 && cx < map::mapWidth && map::worldMap[(int)e.y][cx] == 0)
            e.x = nx;
        int cy = (int)(ny + (e.dy > 0 ? margin : -margin));
        if (cy >= 0 && cy < map::mapHeight && map::worldMap[cy][(int)e.x] == 0)
            e.y = ny;
    }

    /* attack */
    e.attackTimer -= dt;
    if (dist <= e.attackRange && e.attackTimer <= 0.0f) {
        playerHp -= e.damage;
        if (playerHp < 0.0f) playerHp = 0.0f;
        e.attackTimer = e.attackCooldown;
    }
}

} // namespace entity
