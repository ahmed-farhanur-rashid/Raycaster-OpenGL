#ifndef MAP_RENDERER_H
#define MAP_RENDERER_H

namespace renderer {

    struct RenderState {
        float posX, posY;
        float dirX, dirY;
        float planeX, planeY;
        float posZ;
        bool  lightingEnabled;
        bool  minimapEnabled;
    };

    void initRenderer(int screenW, int screenH);
    void renderFrame(const RenderState& state);
    void cleanupRenderer();

    RenderState buildRenderState();
}

#endif