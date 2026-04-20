#include "spatial_grid.h"
#include <cmath>
#include <cstring>

namespace grid {

Cell cells[GRID_H][GRID_W];

void clear() {
    memset(cells, 0, sizeof(cells));
}

void insert(int idx, float x, float y) {
    int gx = (int)(x / GRID_CELL_SIZE);
    int gy = (int)(y / GRID_CELL_SIZE);
    if (gx < 0 || gx >= GRID_W || gy < 0 || gy >= GRID_H) return;
    Cell& c = cells[gy][gx];
    if (c.count < MAX_CELL_SPRITES)
        c.spriteIdx[c.count++] = idx;
}

int query(float cx, float cy, float radius, int* out, int maxOut) {
    int count = 0;
    int gx0 = (int)((cx - radius) / GRID_CELL_SIZE);
    int gx1 = (int)((cx + radius) / GRID_CELL_SIZE);
    int gy0 = (int)((cy - radius) / GRID_CELL_SIZE);
    int gy1 = (int)((cy + radius) / GRID_CELL_SIZE);

    gx0 = gx0 < 0 ? 0 : gx0;  gx1 = gx1 >= GRID_W ? GRID_W-1 : gx1;
    gy0 = gy0 < 0 ? 0 : gy0;  gy1 = gy1 >= GRID_H ? GRID_H-1 : gy1;

    for (int gy = gy0; gy <= gy1 && count < maxOut; gy++)
        for (int gx = gx0; gx <= gx1 && count < maxOut; gx++)
            for (int i = 0; i < cells[gy][gx].count && count < maxOut; i++)
                out[count++] = cells[gy][gx].spriteIdx[i];

    return count;
}

} // namespace grid
