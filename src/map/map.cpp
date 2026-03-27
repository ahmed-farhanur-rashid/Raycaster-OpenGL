#include "map.h"
#include <fstream>
#include <string>
#include <cstdio>

namespace map {

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

    numSprites = 0;

    return true;
}

} // namespace map