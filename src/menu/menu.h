#ifndef MENU_H
#define MENU_H

#include <GLFW/glfw3.h>

void initMenu(int screenW, int screenH);
bool updateMenu(GLFWwindow* window);   /* returns true when Start triggered */
void renderMenu();
void cleanupMenu();

#endif
