#ifndef MAP_RENDERER_H
#define MAP_RENDERER_H

namespace renderer {
    void initRenderer(int screenW, int screenH);
    void uploadMapTexture();  // Call after loading map
    void renderFrame();
    void cleanupRenderer();
}

#endif