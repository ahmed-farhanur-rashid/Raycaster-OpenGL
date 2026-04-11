#ifndef ENEMY_H
#define ENEMY_H

#include <string>

#define MAX_ENEMIES 32

namespace enemy {

    enum class State {
        Patrol,
        Alert,    // player spotted, short delay before chasing
        Chase,
        Attack,
        Dead
    };

    struct Enemy {
        float x, y;           // world position
        float dirX, dirY;     // facing direction
        float speed;
        int   health;
        State state;

        float alertTimer;     // countdown before switching to Chase
        float attackCooldown; // time until next attack
        float patrolTimer;    // time until next patrol direction change
        float shootCooldown;  // time until next shot (for ranged enemies)
        bool  isRanged;       // true = shoots projectiles, false = melee chaser

        // Sprite rendering fields (used by map_renderer)
        std::string baseSpriteName;     // "enemy"
        std::string woundedSpriteName;  // "enemy_wounded"
        std::string deadSpriteName;     // "enemy_dead"
        std::string agroSpriteName;     // "enemy_agro"
        int   currentSprite;            // current sprite index (resolved from name)
        float animTimer;
        
        float deathTimer;     // countdown before despawning after death
    };

    extern Enemy enemies[MAX_ENEMIES];
    extern int   numEnemies;

    void spawnEnemy(float x, float y, const std::string& baseSpriteName);
    void updateEnemies(float deltaTime);
    void damageEnemy(int index, int damage);
}

#endif
