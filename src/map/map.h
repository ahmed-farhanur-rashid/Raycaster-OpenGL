#ifndef MAP_H
#define MAP_H

#include <cstdint>

#define MAX_MAP_DIMENSION 256
#define MAX_SPRITES 64

// Face texture indices per map cell
// [y][x][face]:  face 0=North, 1=South, 2=East, 3=West
#define FACE_N 0
#define FACE_S 1
#define FACE_E 2
#define FACE_W 3

namespace map {
    extern int mapWidth;
    extern int mapHeight;
    extern int worldMap[MAX_MAP_DIMENSION][MAX_MAP_DIMENSION];

    // Face texture indices per map cell
    extern uint8_t faceMap[MAX_MAP_DIMENSION][MAX_MAP_DIMENSION][4];

    struct MapSprite {
        float x, y;
        int type;
        int textureId;  // <-- ADD: index into sprite texture array
    };

    extern MapSprite mapSprites[MAX_SPRITES];
    extern int numSprites;

    bool loadMap(const char* path);
    bool loadFaceMap(const char* path);  // <-- ADD: load per-face textures
    void setFaceTexture(int x, int y, int face, uint8_t texIdx);  // <-- ADD: runtime face changes
}

#endif