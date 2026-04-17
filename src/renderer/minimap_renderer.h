#ifndef MINIMAP_RENDERER_H
#define MINIMAP_RENDERER_H

#include "map_renderer.h"   /* for renderer::RenderState */

namespace minimap {
    void initMinimap(int screenW, int screenH);
    void renderMinimap(const renderer::RenderState& state);
    void cleanupMinimap();
}

#endif
