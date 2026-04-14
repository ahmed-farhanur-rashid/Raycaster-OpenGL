#include "map.h"
#include "../entities/enemy.h"
#include "../renderer/sprite_registry.h"
#include <fstream>
#include <string>
#include <cstdio>
#include <cstdint>
#include <cstring>

namespace map {

int mapWidth  = 0;
int mapHeight = 0;
int worldMap[MAX_MAP_DIMENSION][MAX_MAP_DIMENSION];

// Face texture indices per map cell
uint8_t faceMap[MAX_MAP_DIMENSION][MAX_MAP_DIMENSION][4];

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
                    int texId = sprite::getSpriteIndex("barrel");
                    map::mapSprites[numSprites++] = { (float)x + 0.5f, (float)row + 0.5f, 0, texId };
                }
                worldMap[row][x] = 0;
            }
            else if (c == 'L') {  // Lamp
                if (numSprites < MAX_SPRITES) {
                    int texId = sprite::getSpriteIndex("lamp");
                    map::mapSprites[numSprites++] = { (float)x + 0.5f, (float)row + 0.5f, 1, texId };
                }
                worldMap[row][x] = 0;
            }
            else if (c == 'C') {  // Column
                if (numSprites < MAX_SPRITES) {
                    int texId = sprite::getSpriteIndex("column");
                    map::mapSprites[numSprites++] = { (float)x + 0.5f, (float)row + 0.5f, 2, texId };
                }
                worldMap[row][x] = 0;
            }
            else if (c == 'T') {  // Torch
                if (numSprites < MAX_SPRITES) {
                    int texId = sprite::getSpriteIndex("torch");
                    map::mapSprites[numSprites++] = { (float)x + 0.5f, (float)row + 0.5f, 3, texId };
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
                    int texId = sprite::getSpriteIndex("health");
                    map::mapSprites[numSprites++] = { (float)x + 0.5f, (float)row + 0.5f, 5, texId };
                }
                worldMap[row][x] = 0;
            }
            else if (c == 'A') {  // Ammo
                if (numSprites < MAX_SPRITES) {
                    int texId = sprite::getSpriteIndex("ammo");
                    map::mapSprites[numSprites++] = { (float)x + 0.5f, (float)row + 0.5f, 6, texId };
                }
                worldMap[row][x] = 0;
            }
            else if (c == 'K') {  // Key
                if (numSprites < MAX_SPRITES) {
                    int texId = sprite::getSpriteIndex("key");
                    map::mapSprites[numSprites++] = { (float)x + 0.5f, (float)row + 0.5f, 7, texId };
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

bool loadFaceMap(const char* path) {
    // Default: all faces use texture 0 (matching existing wallType-1 behaviour)
    memset(faceMap, 0, sizeof(faceMap));

    // Also mirror worldMap values into faceMap as default
    // (so existing maps work without a faceMap file)
    for (int y = 0; y < mapHeight; y++)
        for (int x = 0; x < mapWidth; x++) {
            uint8_t t = (uint8_t)(worldMap[y][x] > 0 ? worldMap[y][x] - 1 : 0);
            faceMap[y][x][FACE_N] = t;
            faceMap[y][x][FACE_S] = t;
            faceMap[y][x][FACE_E] = t;
            faceMap[y][x][FACE_W] = t;
        }

    std::ifstream f(path);
    if (!f) return true;  // not an error — defaults applied above

    // Format: each line is "x y N S E W" (e.g. "3 4 0 0 1 2")
    // Skip comment lines starting with #
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        int x, y, n, s, e, w;
        if (sscanf(line.c_str(), "%d %d %d %d %d %d", &x, &y, &n, &s, &e, &w) == 6) {
            if (x < 0 || x >= mapWidth || y < 0 || y >= mapHeight) continue;
            faceMap[y][x][FACE_N] = (uint8_t)n;
            faceMap[y][x][FACE_S] = (uint8_t)s;
            faceMap[y][x][FACE_E] = (uint8_t)e;
            faceMap[y][x][FACE_W] = (uint8_t)w;
        }
    }
    
    printf("Loaded face map from %s\n", path);
    return true;
}

void setFaceTexture(int x, int y, int face, uint8_t texIdx) {
    if (x >= 0 && x < mapWidth && y >= 0 && y < mapHeight && face >= 0 && face < 4)
        faceMap[y][x][face] = texIdx;
}

} // namespace map