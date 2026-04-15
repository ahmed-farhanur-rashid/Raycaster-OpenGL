/*
 * spawn.cpp — Entity spawning system
 *
 * Spawns entities at configurable intervals around the player,
 * respecting per-type max counts, spawn distance, and map collision.
 */

#include "entity.h"
#include "../map/map.h"
#include "../settings/settings.h"
#include <cmath>
#include <cstdlib>
#include <cstdio>

namespace entity {

std::vector<Entity>     entities;
std::vector<Projectile> projectiles;
float playerHp    = 100.0f;
float playerMaxHp = 100.0f;

/* per-type spawn state */
static SpawnConfig ghostCfg;
static float       ghostTimer = 0.0f;

void initSpawner() {
    ghostCfg.enabled  = settings::getInt("ghost_spawn_enabled", 1) != 0;
    ghostCfg.maxCount = settings::getInt("ghost_max_count", 5);
    ghostCfg.interval = settings::getFloat("ghost_spawn_interval", 6.0f);
    ghostCfg.distance = settings::getFloat("ghost_spawn_distance", 8.0f);
    ghostTimer = ghostCfg.interval;  /* spawn first batch after one interval */

    playerHp    = 100.0f;
    playerMaxHp = 100.0f;
    entities.clear();
    projectiles.clear();
}

/* count alive entities of a given type */
static int countAlive(EntityType t) {
    int n = 0;
    for (auto& e : entities)
        if (e.alive && e.type == t) n++;
    return n;
}

/* pick a random open floor tile at approximately `dist` units from the player */
static bool findSpawnPos(float px, float py, float dist, float& ox, float& oy) {
    for (int attempt = 0; attempt < 30; attempt++) {
        float angle = (float)(rand() % 3600) / 3600.0f * 6.2831853f;
        float d = dist + ((float)(rand() % 100) / 100.0f - 0.5f) * 3.0f;
        float tx = px + cosf(angle) * d;
        float ty = py + sinf(angle) * d;
        int ix = (int)tx, iy = (int)ty;
        if (ix < 1 || ix >= map::mapWidth - 1 || iy < 1 || iy >= map::mapHeight - 1)
            continue;
        if (map::worldMap[iy][ix] != 0) continue;
        ox = tx;
        oy = ty;
        return true;
    }
    return false;
}

static Entity makeGhost(float x, float y, int sprIdle, int sprAgro, int sprWounded, int sprDead) {
    Entity e{};
    e.type          = EntityType::GHOST;
    e.x             = x;
    e.y             = y;
    e.dx            = 0; e.dy = 0;
    e.speed         = settings::getFloat("ghost_speed", 1.4f);
    e.hp            = settings::getFloat("ghost_hp", 50.0f);
    e.maxHp         = e.hp;
    e.alive         = true;
    e.agro          = false;
    e.agroRange     = settings::getFloat("ghost_agro_range", 8.0f);
    e.attackRange   = settings::getFloat("ghost_attack_range", 1.0f);
    e.attackCooldown = settings::getFloat("ghost_attack_cooldown", 1.2f);
    e.attackTimer   = 0.0f;
    e.damage        = settings::getFloat("ghost_damage", 8.0f);
    e.hitFlash      = 0.0f;
    e.deadTimer     = 0.0f;
    e.roamDirX      = 0.0f;
    e.roamDirY      = 0.0f;
    e.roamTimer     = 0.0f;
    e.roamPause     = 0.0f;
    e.sprIdle       = sprIdle;
    e.sprAgro       = sprAgro;
    e.sprWounded    = sprWounded;
    e.sprDead       = sprDead;
    e.spriteScale   = settings::getFloat("ghost_size", 1.0f);
    return e;
}

void updateSpawner(float dt, float px, float py,
                   int sprIdle, int sprAgro, int sprWounded, int sprDead) {
    /* Ghost spawning */
    if (ghostCfg.enabled) {
        ghostTimer -= dt;
        if (ghostTimer <= 0.0f) {
            ghostTimer = ghostCfg.interval;
            if (countAlive(EntityType::GHOST) < ghostCfg.maxCount) {
                float sx, sy;
                if (findSpawnPos(px, py, ghostCfg.distance, sx, sy)) {
                    entities.push_back(makeGhost(sx, sy, sprIdle, sprAgro, sprWounded, sprDead));
                }
            }
        }
    }
}

} // namespace entity
