/*
 * projectile.cpp — Projectile spawning and movement
 */

#include "projectile.h"
#include "../map/map.h"
#include "../settings/settings.h"
#include <cmath>
#include <algorithm>

namespace projectile {

std::vector<Projectile> projectiles;

void initProjectiles() {
    projectiles.clear();
}

void spawnProjectile(float x, float y, float dirX, float dirY,
                     float speed, ProjOwner owner, int sprIdx,
                     float range, float visualScale, float spawnZ) {
    float len = sqrtf(dirX * dirX + dirY * dirY);
    if (len < 0.001f) return;
    Projectile p{};
    p.x           = x;
    p.y           = y;
    p.dx          = dirX / len;
    p.dy          = dirY / len;
    p.speed       = speed;
    p.lifetime    = settings::getFloat("projectile_lifetime", 5.0f);
    p.range       = range;
    p.traveled    = 0.0f;
    p.visualScale = visualScale;
    p.spawnZ      = spawnZ;
    p.owner       = owner;
    p.alive       = true;
    p.sprIdx      = sprIdx;
    projectiles.push_back(p);
}

void updateProjectiles(float dt) {
    for (auto& p : projectiles) {
        if (!p.alive) continue;

        float step = p.speed * dt;
        p.x += p.dx * step;
        p.y += p.dy * step;
        p.traveled += step;
        p.lifetime -= dt;
        if (p.lifetime <= 0.0f || p.traveled >= p.range) { p.alive = false; continue; }

        int ix = (int)p.x, iy = (int)p.y;
        if (ix < 0 || ix >= map::mapWidth || iy < 0 || iy >= map::mapHeight ||
            map::worldMap[iy][ix] != 0) {
            p.alive = false;
            continue;
        }
    }

    /* remove dead projectiles (erase-remove idiom) */
    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
                       [](const Projectile& p) { return !p.alive; }),
        projectiles.end());
}

} // namespace projectile
