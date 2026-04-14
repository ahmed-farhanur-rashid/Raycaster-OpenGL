#ifndef SETTINGS_H
#define SETTINGS_H

namespace settings {
    bool load(const char* configPath, const char* keybindPath);

    float getFloat(const char* key, float defaultVal);
    int   getInt(const char* key, int defaultVal);

    // Returns a GLFW key code for the named action, or the provided default
    int   getKey(const char* action, int defaultKey);
}

#endif
