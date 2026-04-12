#include "enemy.h"
#include "../player/player.h"
#include "../map/map.h"
#include "../renderer/sprite_registry.h"
#include "../entities/projectile.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace enemy {

Enemy enemies[MAX_ENEMIES];
int   numEnemies = 0;

static const float PATROL_SPEED = 1.0f;
static const float CHASE_SPEED  = 2.2f;
static const float SIGHT_RANGE  = 8.0f;    // world units
static const float ATTACK_RANGE = 1.2f;
static const float ALERT_DELAY  = 0.4f;    // seconds before chasing
static const float ATTACK_CD    = 1.5f;

// ---- Line-of-sight check using DDA ----
static bool hasLineOfSight(float ax, float ay, float bx, float by) {
    float dx = bx - ax, dy = by - ay;
    float dist = sqrtf(dx*dx + dy*dy);
    if (dist < 0.001f) return true;
    float stepX = dx / dist, stepY = dy / dist;
    float cx = ax, cy = ay;
    int steps = (int)(dist / 0.2f);
    for (int i = 0; i < steps; i++) {
        cx += stepX * 0.2f;
        cy += stepY * 0.2f;
        if (map::worldMap[(int)cy][(int)cx] > 0) return false;
    }
    return true;
}

// ---- Collision-aware move ----
static void moveEnemy(Enemy& e, float dx, float dy, float dt) {
    float nx = e.x + dx * dt;
    float ny = e.y + dy * dt;
    float margin = 0.25f;
    if (map::worldMap[(int)(ny)][(int)(nx + (dx > 0 ? margin : -margin))] == 0)
        e.x = nx;
    if (map::worldMap[(int)(ny + (dy > 0 ? margin : -margin))][(int)(e.x)] == 0)
        e.y = ny;
}

void spawnEnemy(float x, float y, const std::string& baseSpriteName) {
    if (numEnemies >= MAX_ENEMIES) return;
    Enemy& e      = enemies[numEnemies++];
    e.x           = x;
    e.y           = y;
    e.dirX        = 1.0f; e.dirY = 0.0f;
    e.speed       = PATROL_SPEED;
    e.health      = 100;
    e.state       = State::Patrol;
    e.alertTimer  = 0.0f;
    e.attackCooldown = 0.0f;
    e.patrolTimer    = 2.0f;
    e.shootCooldown  = 2.0f + (rand() % 100) / 50.0f;  // Random initial cooldown
    e.isRanged       = (rand() % 2 == 0);  // 50% chance to be ranged
    
    // Set sprite names
    e.baseSpriteName     = baseSpriteName;
    e.woundedSpriteName  = baseSpriteName + "_wounded";
    e.deadSpriteName     = baseSpriteName + "_dead";
    e.agroSpriteName     = baseSpriteName + "_agro";
    
    // Resolve initial sprite index
    e.currentSprite  = sprite::getSpriteIndex(baseSpriteName);
    e.animTimer      = 0.0f;
    e.deathTimer     = 0.0f;
}

void updateEnemies(float deltaTime) {
    float px = player::player.posX;
    float py = player::player.posY;

    for (int i = 0; i < numEnemies; i++) {
        Enemy& e = enemies[i];
        
        // Handle dead enemies - despawn after 0.5 seconds
        if (e.state == State::Dead) {
            e.deathTimer += deltaTime;
            
            // Flash effect: toggle visibility every 0.08 seconds
            float flashPeriod = 0.08f;
            bool isVisible = fmodf(e.deathTimer, flashPeriod * 2.0f) < flashPeriod;
            
            if (e.deathTimer >= 0.5f) {
                // Remove this enemy by swapping with last
                if (i < numEnemies - 1) {
                    enemies[i] = enemies[numEnemies - 1];
                }
                numEnemies--;
                i--;  // Re-check this index
            } else if (!isVisible) {
                // Skip rendering during flash off period
                continue;
            }
        }

        float dx = px - e.x, dy = py - e.y;
        float distToPlayer = sqrtf(dx*dx + dy*dy);
        bool  canSee = distToPlayer < SIGHT_RANGE && hasLineOfSight(e.x, e.y, px, py);

        e.attackCooldown -= deltaTime;
        e.animTimer      += deltaTime;

        switch (e.state) {

            case State::Patrol:
                e.patrolTimer -= deltaTime;
                if (e.patrolTimer <= 0.0f) {
                    // Pick a new random-ish direction
                    e.dirX = cosf(e.animTimer * 1.3f);
                    e.dirY = sinf(e.animTimer * 1.3f);
                    e.patrolTimer = 2.0f + (e.animTimer - (int)e.animTimer);
                }
                moveEnemy(e, e.dirX * PATROL_SPEED, e.dirY * PATROL_SPEED, deltaTime);
                if (canSee) {
                    e.state      = State::Alert;
                    e.alertTimer = ALERT_DELAY;
                    e.currentSprite = sprite::getSpriteIndex(e.agroSpriteName);  // Switch to agro sprite
                }
                break;

            case State::Alert:
                e.alertTimer -= deltaTime;
                if (e.alertTimer <= 0.0f)
                    e.state = State::Chase;
                if (!canSee) {
                    e.state = State::Patrol;
                    e.currentSprite = sprite::getSpriteIndex(e.baseSpriteName);  // Back to normal sprite
                }
                break;

            case State::Chase:
                if (distToPlayer < ATTACK_RANGE) {
                    e.state = State::Attack;
                } else if (!canSee && distToPlayer > SIGHT_RANGE) {
                    e.state = State::Patrol;
                    e.currentSprite = sprite::getSpriteIndex(e.baseSpriteName);  // Back to normal sprite
                } else {
                    float len = distToPlayer > 0.001f ? distToPlayer : 1.0f;
                    
                    // Ranged enemies try to dodge player bullets
                    if (e.isRanged) {
                        // Check for incoming player projectiles
                        float dodgeX = 0.0f, dodgeY = 0.0f;
                        for (int p = 0; p < projectile::numProjectiles; p++) {
                            if (!projectile::projectiles[p].active || !projectile::projectiles[p].fromPlayer) continue;
                            
                            float pdx = projectile::projectiles[p].x - e.x;
                            float pdy = projectile::projectiles[p].y - e.y;
                            float pdist = sqrtf(pdx*pdx + pdy*pdy);
                            
                            if (pdist < 3.0f) {  // Bullet is close
                                // Dodge perpendicular to bullet direction
                                dodgeX = -projectile::projectiles[p].dirY * 2.0f;
                                dodgeY = projectile::projectiles[p].dirX * 2.0f;
                                break;
                            }
                        }
                        
                        // Move towards player with dodge offset
                        float moveX = (dx/len) * CHASE_SPEED + dodgeX;
                        float moveY = (dy/len) * CHASE_SPEED + dodgeY;
                        moveEnemy(e, moveX, moveY, deltaTime);
                        
                        // Shoot at player
                        e.shootCooldown -= deltaTime;
                        if (e.shootCooldown <= 0.0f && canSee) {
                            printf("Enemy %d shooting at player!\n", i);
                            projectile::spawnProjectile(
                                e.x, e.y, 0.0f,
                                dx/len, dy/len,
                                false  // fromPlayer = false
                            );
                            e.shootCooldown = 2.0f + (rand() % 100) / 50.0f;  // 2-4 seconds
                        }
                    } else {
                        // Melee enemy - just chase
                        moveEnemy(e, (dx/len) * CHASE_SPEED, (dy/len) * CHASE_SPEED, deltaTime);
                    }
                    
                    e.dirX = dx/len; e.dirY = dy/len;
                }
                break;

            case State::Attack:
                if (distToPlayer > ATTACK_RANGE * 1.5f) {
                    e.state = State::Chase;
                } else if (e.attackCooldown <= 0.0f) {
                    // Deal damage to player
                    printf("Enemy %d attacks player!\n", i);
                    player::takeDamage(10);  // 10 damage per attack
                    e.attackCooldown = ATTACK_CD;
                }
                break;

            case State::Dead:
                break;
        }
    }
}

void damageEnemy(int index, int damage) {
    if (index < 0 || index >= numEnemies) return;
    Enemy& e = enemies[index];
    if (e.state == State::Dead) return;
    
    printf("Enemy %d hit! Health: %d -> %d\n", index, e.health, e.health - damage);
    e.health -= damage;
    e.state   = State::Chase;   // always aggro on hit
    
    // Update sprite based on health
    if (e.health <= 0) {
        e.health = 0;
        e.state  = State::Dead;
        e.currentSprite = sprite::getSpriteIndex(e.deadSpriteName);
        e.deathTimer = 0.0f;
        printf("Enemy %d killed!\n", index);
    } else if (e.health < 70) {  // Wounded at 70 HP (3 hits with 10 damage)
        e.currentSprite = sprite::getSpriteIndex(e.woundedSpriteName);
        printf("Enemy %d wounded!\n", index);
    }
}

} // namespace enemy
