#include "projectile.h"
#include "../map/map.h"
#include "enemy.h"
#include "../renderer/sprite_registry.h"
#include <cstdio>
#include <cmath>

namespace projectile {

Projectile projectiles[MAX_PROJECTILES];
int numProjectiles = 0;

static const float PROJECTILE_SPEED = 15.0f;  // world units per second (much faster)

void spawnProjectile(float x, float y, float z, float dirX, float dirY, bool fromPlayer) {
    // Find inactive slot or create new
    int slot = -1;
    for (int i = 0; i < numProjectiles; i++) {
        if (!projectiles[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1 && numProjectiles < MAX_PROJECTILES) {
        slot = numProjectiles++;
    }
    
    if (slot == -1) return;  // No slots available
    
    Projectile& p = projectiles[slot];
    p.x = x;
    p.y = y;
    p.z = z;
    p.dirX = dirX;
    p.dirY = dirY;
    p.speed = fromPlayer ? 15.0f : 8.0f;  // Player bullets faster than enemy bullets
    p.active = true;
    p.spriteType = fromPlayer ? sprite::getSpriteIndex("bullet") : sprite::getSpriteIndex("bullet");
    p.fromPlayer = fromPlayer;
}

void updateProjectiles(float deltaTime) {
    for (int i = 0; i < numProjectiles; i++) {
        Projectile& p = projectiles[i];
        if (!p.active) continue;
        
        // Move projectile with multiple collision checks along path
        float totalDist = p.speed * deltaTime;
        int steps = (int)(totalDist / 0.2f) + 1;  // Check every 0.2 units
        float stepSize = totalDist / steps;
        
        bool hitSomething = false;
        
        for (int step = 0; step < steps && !hitSomething; step++) {
            float nx = p.x + p.dirX * stepSize;
            float ny = p.y + p.dirY * stepSize;
            
            // Check wall collision
            int mapX = (int)nx;
            int mapY = (int)ny;
            
            if (mapX < 0 || mapX >= map::mapWidth || 
                mapY < 0 || mapY >= map::mapHeight ||
                map::worldMap[mapY][mapX] > 0) {
                // Hit wall - deactivate
                p.active = false;
                hitSomething = true;
                break;
            }
            
            // Check enemy collision
            for (int e = 0; e < enemy::numEnemies; e++) {
                if (enemy::enemies[e].state == enemy::State::Dead) continue;
                
                float dx = enemy::enemies[e].x - nx;
                float dy = enemy::enemies[e].y - ny;
                float dist = sqrtf(dx*dx + dy*dy);
                
                if (dist < 0.5f) {  // Enemy hit radius
                    printf("Bullet hit enemy %d at distance %.2f\n", e, dist);
                    enemy::damageEnemy(e, 10);  // 10 damage per bullet (10 hits to kill)
                    p.active = false;
                    hitSomething = true;
                    break;
                }
            }
            
            if (!hitSomething) {
                p.x = nx;
                p.y = ny;
            }
        }
    }
}

} // namespace projectile
