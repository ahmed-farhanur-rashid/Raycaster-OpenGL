#include "map.h"
#include "../entities/enemy.h"
#include "../renderer/sprite_registry.h"
#include <fstream>
#include <string>
#include <cstdio>

namespace map {

int mapWidth  = 0;
int mapHeight = 0;
int worldMap[MAX_MAP_DIMENSION][MAX_MAP_DIMENSION];

MapSprite mapSprites[MAX_SPRITES];
int numSprites = 0;

bool loadMap(const char* path) {
    std::ifstream f(path);
    if (!f) {
        fprintf(stderr, "Failed to open map: %s\n", path);
        return false;
    }

    numSprites = 0;  // Reset before loading

    int row = 0;
    std::string line;
    while (std::getline(f, line) && row < MAX_MAP_DIMENSION) {
        if (line.empty()) continue;
        int cols = (int)line.size();
        if (cols > MAX_MAP_DIMENSION) cols = MAX_MAP_DIMENSION;
        if (cols > mapWidth) mapWidth = cols;
        for (int x = 0; x < cols; x++) {
            char c = line[x];
            if (c >= '0' && c <= '9')
                worldMap[row][x] = c - '0';
            else if (c == 'B') {  // Barrel
                if (numSprites < MAX_SPRITES) {
                    map::mapSprites[numSprites++] = { (float)x + 0.5f, (float)row + 0.5f, 0, 0 };
                }
                worldMap[row][x] = 0;
            }
            else if (c == 'L') {  // Lamp
                if (numSprites < MAX_SPRITES) {
                    map::mapSprites[numSprites++] = { (float)x + 0.5f, (float)row + 0.5f, 1, 1 };
                }
                worldMap[row][x] = 0;
            }
            else if (c == 'C') {  // Column
                if (numSprites < MAX_SPRITES) {
                    map::mapSprites[numSprites++] = { (float)x + 0.5f, (float)row + 0.5f, 2, 2 };
                }
                worldMap[row][x] = 0;
            }
            else if (c == 'T') {  // Torch
                if (numSprites < MAX_SPRITES) {
                    map::mapSprites[numSprites++] = { (float)x + 0.5f, (float)row + 0.5f, 3, 3 };
                }
                worldMap[row][x] = 0;
            }
            else if (c == 'E') {  // Enemy
                // Spawn enemy entity with named sprite
                enemy::spawnEnemy((float)x + 0.5f, (float)row + 0.5f, "enemy");
                worldMap[row][x] = 0;
            }
            else if (c == 'H') {  // Health
                if (numSprites < MAX_SPRITES) {
                    map::mapSprites[numSprites++] = { (float)x + 0.5f, (float)row + 0.5f, 5, 5 };
                }
                worldMap[row][x] = 0;
            }
            else if (c == 'A') {  // Ammo
                if (numSprites < MAX_SPRITES) {
                    map::mapSprites[numSprites++] = { (float)x + 0.5f, (float)row + 0.5f, 6, 6 };
                }
                worldMap[row][x] = 0;
            }
            else if (c == 'K') {  // Key
                if (numSprites < MAX_SPRITES) {
                    map::mapSprites[numSprites++] = { (float)x + 0.5f, (float)row + 0.5f, 7, 7 };
                }
                worldMap[row][x] = 0;
            }
            else
                worldMap[row][x] = 0;
        }
        row++;
    }
    mapHeight = row;

    printf("Loaded map: %dx%d from %s\n", mapWidth, mapHeight, path);
    printf("Loaded %d sprites\n", numSprites);

    return true;
}

} // namespace map