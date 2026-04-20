#ifndef SPATIAL_GRID_H
#define SPATIAL_GRID_H

#define GRID_CELL_SIZE 4        // world units per grid cell
#define GRID_W         64       // max grid width
#define GRID_H         64       // max grid height
#define MAX_CELL_SPRITES 32

namespace grid {

    struct Cell {
        int  spriteIdx[MAX_CELL_SPRITES];
        int  count;
    };

    extern Cell cells[GRID_H][GRID_W];

    void clear();
    void insert(int spriteIdx, float x, float y);

    // Returns indices of sprites in cells within `radius` world units
    // of (cx, cy). Writes to `out`, returns count.
    int query(float cx, float cy, float radius, int* out, int maxOut);
}

#endif
