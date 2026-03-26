#ifndef MAP_H
#define MAP_H

#define MAX_MAP_DIM 256

extern int mapWidth;
extern int mapHeight;
extern int worldMap[MAX_MAP_DIM][MAX_MAP_DIM];

struct MapSprite {
    float x, y;
    int type;
};

#define MAX_SPRITES 64
extern MapSprite mapSprites[MAX_SPRITES];
extern int numSprites;

bool loadMap(const char* path);

#endif