#include "projectile.h"
#include "../map/map.h"
#include <cstdio>
#include <cmath>

namespace projectile {

Projectile projectiles[MAX_PROJECTILES];
int numProjectiles = 0;

static const float PROJECTILE_SPEED = 15.0f;  // world units per second (much faster)
static const int BULLET_SPRITE = 8;          // sprite type index (needs new texture slot)

void spawnProjectile(float x, float y, float dirX, float dirY) {
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
    p.dirX = dirX;
    p.dirY = dirY;
    p.speed = PROJECTILE_SPEED;
    p.active = true;
    p.spriteType = BULLET_SPRITE;
}

void updateProjectiles(float deltaTime) {
    for (int i = 0; i < numProjectiles; i++) {
        Projectile& p = projectiles[i];
        if (!p.active) continue;
        
        // Move projectile
        float nx = p.x + p.dirX * p.speed * deltaTime;
        float ny = p.y + p.dirY * p.speed * deltaTime;
        
        // Check wall collision
        int mapX = (int)nx;
        int mapY = (int)ny;
        
        if (mapX < 0 || mapX >= map::mapWidth || 
            mapY < 0 || mapY >= map::mapHeight ||
            map::worldMap[mapY][mapX] > 0) {
            // Hit wall - deactivate
            p.active = false;
            continue;
        }
        
        // TODO: Add enemy collision check when enemy system is implemented
        
        // Check sprite collision (map props)
        bool hitSprite = false;
        for (int s = 0; s < map::numSprites; s++) {
            float dx = map::mapSprites[s].x - nx;
            float dy = map::mapSprites[s].y - ny;
            float dist = sqrtf(dx*dx + dy*dy);
            
            if (dist < 0.4f) {  // Sprite hit radius
                hitSprite = true;
                break;
            }
        }
        
        if (hitSprite) {
            p.active = false;
            continue;
        }
        
        p.x = nx;
        p.y = ny;
    }
}

} // namespace projectile
