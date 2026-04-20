#ifndef MAP_H
#define MAP_H

#define MAX_MAP_DIMENSION 256

namespace map {
    extern int mapWidth;
    extern int mapHeight;
    extern int worldMap[MAX_MAP_DIMENSION][MAX_MAP_DIMENSION];

    bool loadMap(const char* path);
}

#endif