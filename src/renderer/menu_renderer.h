#ifndef MENU_RENDERER_H
#define MENU_RENDERER_H

namespace menu_renderer {
    void initMenu(int screenW, int screenH);
    void renderMainMenu();
    void renderPauseMenu(int selectedItem);
    void renderMainMenuSelect(int selectedItem);
    void cleanupMenu();
}

#endif
