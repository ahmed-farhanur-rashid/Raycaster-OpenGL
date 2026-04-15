#ifndef ENTITY_RENDERER_H
#define ENTITY_RENDERER_H

#include "map_renderer.h"

namespace entity_renderer {
    void initEntityRenderer(int screenW, int screenH);
    void renderProjectiles(const renderer::RenderState& state);
    void cleanupEntityRenderer();
}

#endif
