#ifndef MAP_RENDERER_H
#define MAP_RENDERER_H

namespace renderer {
    void initRenderer(int screenW, int screenH);
    void renderFrame();
    void cleanupRenderer();
}

#endif