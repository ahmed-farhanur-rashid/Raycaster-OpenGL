/*
 * combat.cpp — Combat system: projectile spawning, hit detection, damage
 */

#include "entity.h"
#include "../map/map.h"
#include <cmath>
#include <cstdio>

namespace entity {

/* ---- projectile helpers ---- */

void spawnProjectile(float x, float y, float dirX, float dirY,
                     float speed, float damage, ProjOwner owner, int sprIdx,
                     float range, float visualScale, float spawnZ) {
    float len = sqrtf(dirX * dirX + dirY * dirY);
    if (len < 0.001f) return;
    Projectile p{};
    p.x           = x;
    p.y           = y;
    p.dx          = dirX / len;
    p.dy          = dirY / len;
    p.speed       = speed;
    p.damage      = damage;
    p.lifetime    = 5.0f;
    p.range       = range;
    p.traveled    = 0.0f;
    p.visualScale = visualScale;
    p.spawnZ      = spawnZ;
    p.owner       = owner;
    p.alive       = true;
    p.sprIdx      = sprIdx;
    projectiles.push_back(p);
}

/* ---- per-frame update ---- */

void updateCombat(float dt, float px, float py) {
    /* update projectiles */
    for (auto& p : projectiles) {
        if (!p.alive) continue;

        float step = p.speed * dt;
        p.x += p.dx * step;
        p.y += p.dy * step;
        p.traveled += step;
        p.lifetime -= dt;
        if (p.lifetime <= 0.0f || p.traveled >= p.range) { p.alive = false; continue; }

        /* wall collision */
        int ix = (int)p.x, iy = (int)p.y;
        if (ix < 0 || ix >= map::mapWidth || iy < 0 || iy >= map::mapHeight ||
            map::worldMap[iy][ix] != 0) {
            p.alive = false;
            continue;
        }

        /* hit entities (player projectiles hit enemies) */
        if (p.owner == ProjOwner::PLAYER) {
            for (auto& e : entities) {
                if (!e.alive) continue;
                float ddx = e.x - p.x;
                float ddy = e.y - p.y;
                if (ddx * ddx + ddy * ddy < 0.4f * 0.4f) {
                    e.hp -= p.damage;
                    e.hitFlash = 0.15f;
                    if (e.hp <= 0.0f) {
                        e.hp = 0.0f;
                        e.alive = false;
                    }
                    p.alive = false;
                    break;
                }
            }
        }

        /* hit player (enemy projectiles) */
        if (p.owner == ProjOwner::ENEMY) {
            float ddx = px - p.x;
            float ddy = py - p.y;
            if (ddx * ddx + ddy * ddy < 0.35f * 0.35f) {
                playerHp -= p.damage;
                if (playerHp < 0.0f) playerHp = 0.0f;
                p.alive = false;
            }
        }
    }

    /* remove dead projectiles */
    for (int i = (int)projectiles.size() - 1; i >= 0; i--) {
        if (!projectiles[i].alive)
            projectiles.erase(projectiles.begin() + i);
    }

    /* dead body cleanup */
    for (int i = (int)entities.size() - 1; i >= 0; i--) {
        Entity& e = entities[i];
        if (!e.alive) {
            e.deadTimer += dt;
            if (e.deadTimer >= 3.0f)
                entities.erase(entities.begin() + i);
        }
    }
}

/* ---- hitscan helper for weapons that don't use projectiles ---- */

void hitscanFire(float px, float py, float dirX, float dirY, float damage, float range) {
    float len = sqrtf(dirX * dirX + dirY * dirY);
    if (len < 0.001f) return;
    float ndx = dirX / len, ndy = dirY / len;

    /* find closest entity along the ray within range */
    Entity* best = nullptr;
    float bestDist = range;

    for (auto& e : entities) {
        if (!e.alive) continue;
        /* project entity onto ray */
        float ex = e.x - px, ey = e.y - py;
        float t = ex * ndx + ey * ndy;  /* distance along ray */
        if (t < 0.0f || t > bestDist) continue;

        /* perpendicular distance to ray */
        float perpX = ex - ndx * t;
        float perpY = ey - ndy * t;
        float perp = sqrtf(perpX * perpX + perpY * perpY);
        if (perp < 0.5f) {
            /* check wall occlusion with simple step-through */
            bool blocked = false;
            for (float s = 0.5f; s < t; s += 0.5f) {
                int wx = (int)(px + ndx * s);
                int wy = (int)(py + ndy * s);
                if (wx >= 0 && wx < map::mapWidth && wy >= 0 && wy < map::mapHeight) {
                    if (map::worldMap[wy][wx] != 0) { blocked = true; break; }
                }
            }
            if (!blocked) {
                bestDist = t;
                best = &e;
            }
        }
    }

    if (best) {
        best->hp -= damage;
        best->hitFlash = 0.15f;
        if (best->hp <= 0.0f) {
            best->hp = 0.0f;
            best->alive = false;
        }
    }
}

} // namespace entity
