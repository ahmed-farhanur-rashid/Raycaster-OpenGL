#include "map.h"
#include <fstream>
#include <string>
#include <cstdio>

int mapWidth  = 0;
int mapHeight = 0;
int worldMap[MAX_MAP_DIM][MAX_MAP_DIM];

MapSprite mapSprites[MAX_SPRITES];
int numSprites = 0;

bool loadMap(const char* path) {
    std::ifstream f(path);
    if (!f) {
        fprintf(stderr, "Failed to open map: %s\n", path);
        return false;
    }

    int row = 0;
    std::string line;
    while (std::getline(f, line) && row < MAX_MAP_DIM) {
        if (line.empty()) continue;
        int cols = (int)line.size();
        if (cols > MAX_MAP_DIM) cols = MAX_MAP_DIM;
        if (cols > mapWidth) mapWidth = cols;
        for (int x = 0; x < cols; x++) {
            char c = line[x];
            if (c >= '0' && c <= '9')
                worldMap[row][x] = c - '0';
            else
                worldMap[row][x] = 0;
        }
        row++;
    }
    mapHeight = row;

    printf("Loaded map: %dx%d from %s\n", mapWidth, mapHeight, path);

    // Sprite auto-placement — uncomment when you want sprites
    numSprites = 0;
    /*
    int interval = (mapWidth * mapHeight) / MAX_SPRITES;
    if (interval < 4) interval = 4;
    int count = 0;
    for (int y = 1; y < mapHeight - 1 && numSprites < MAX_SPRITES; y++) {
        for (int x = 1; x < mapWidth - 1 && numSprites < MAX_SPRITES; x++) {
            if (worldMap[y][x] != 0) continue;
            count++;
            if (count % interval == 0) {
                mapSprites[numSprites].x = x + 0.5f;
                mapSprites[numSprites].y = y + 0.5f;
                mapSprites[numSprites].type = numSprites % 3;
                numSprites++;
            }
        }
    }
    printf("Placed %d sprites\n", numSprites);
    */

    return true;
}