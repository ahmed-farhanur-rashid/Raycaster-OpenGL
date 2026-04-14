#include "settings.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

namespace settings {

/* ------------------------------------------------------------------ */
/*  internal data                                                      */
/* ------------------------------------------------------------------ */

struct Value {
    enum { NUM, STR } type;
    float  num;
    std::string str;
};

static std::map<std::string, Value> configMap;
static std::map<std::string, int>   keybindMap;

/* ------------------------------------------------------------------ */
/*  GLFW key-name table                                                */
/* ------------------------------------------------------------------ */

static int keyNameToGLFW(const char* name) {
    if (!name || !name[0]) return -1;

    /* single letter A-Z */
    if (strlen(name) == 1 && name[0] >= 'A' && name[0] <= 'Z')
        return GLFW_KEY_A + (name[0] - 'A');

    /* single digit 0-9 */
    if (strlen(name) == 1 && name[0] >= '0' && name[0] <= '9')
        return GLFW_KEY_0 + (name[0] - '0');

    /* special keys */
    if (strcmp(name, "SPACE")         == 0) return GLFW_KEY_SPACE;
    if (strcmp(name, "ESCAPE")        == 0) return GLFW_KEY_ESCAPE;
    if (strcmp(name, "ENTER")         == 0) return GLFW_KEY_ENTER;
    if (strcmp(name, "TAB")           == 0) return GLFW_KEY_TAB;
    if (strcmp(name, "BACKSPACE")     == 0) return GLFW_KEY_BACKSPACE;
    if (strcmp(name, "LEFT_SHIFT")    == 0) return GLFW_KEY_LEFT_SHIFT;
    if (strcmp(name, "RIGHT_SHIFT")   == 0) return GLFW_KEY_RIGHT_SHIFT;
    if (strcmp(name, "LEFT_CONTROL")  == 0) return GLFW_KEY_LEFT_CONTROL;
    if (strcmp(name, "RIGHT_CONTROL") == 0) return GLFW_KEY_RIGHT_CONTROL;
    if (strcmp(name, "LEFT_ALT")      == 0) return GLFW_KEY_LEFT_ALT;
    if (strcmp(name, "RIGHT_ALT")     == 0) return GLFW_KEY_RIGHT_ALT;
    if (strcmp(name, "LEFT")          == 0) return GLFW_KEY_LEFT;
    if (strcmp(name, "RIGHT")         == 0) return GLFW_KEY_RIGHT;
    if (strcmp(name, "UP")            == 0) return GLFW_KEY_UP;
    if (strcmp(name, "DOWN")          == 0) return GLFW_KEY_DOWN;

    /* F-keys F1..F12 */
    if (name[0] == 'F' && name[1] >= '1' && name[1] <= '9') {
        int n = atoi(name + 1);
        if (n >= 1 && n <= 12) return GLFW_KEY_F1 + (n - 1);
    }

    fprintf(stderr, "WARNING: unknown key name \"%s\"\n", name);
    return -1;
}

/* ------------------------------------------------------------------ */
/*  minimal flat-JSON parser                                           */
/* ------------------------------------------------------------------ */

static const char* skipWS(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static const char* parseString(const char* p, std::string& out) {
    if (*p != '"') return nullptr;
    p++;
    out.clear();
    while (*p && *p != '"') {
        if (*p == '\\' && *(p + 1)) { out += *(p + 1); p += 2; }
        else { out += *p; p++; }
    }
    if (*p == '"') p++;
    return p;
}

static bool parseFile(const char* path, std::map<std::string, Value>& map) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = new char[sz + 1];
    fread(buf, 1, (size_t)sz, f);
    buf[sz] = '\0';
    fclose(f);

    const char* p = skipWS(buf);
    if (*p != '{') { delete[] buf; return false; }
    p = skipWS(p + 1);

    while (*p && *p != '}') {
        std::string key;
        p = parseString(p, key);
        if (!p) break;

        p = skipWS(p);
        if (*p != ':') break;
        p = skipWS(p + 1);

        Value val;
        if (*p == '"') {
            val.type = Value::STR;
            p = parseString(p, val.str);
            if (!p) break;
        } else {
            val.type = Value::NUM;
            char* end;
            val.num = strtof(p, &end);
            p = end;
        }
        map[key] = val;

        p = skipWS(p);
        if (*p == ',') p = skipWS(p + 1);
    }

    delete[] buf;
    return true;
}

/* ------------------------------------------------------------------ */
/*  public API                                                         */
/* ------------------------------------------------------------------ */

bool load(const char* configPath, const char* keybindPath) {
    bool ok = true;

    if (configPath) {
        if (!parseFile(configPath, configMap))  {
            fprintf(stderr, "WARNING: could not load config from %s\n", configPath);
            ok = false;
        }
    }

    if (keybindPath) {
        std::map<std::string, Value> kbRaw;
        if (parseFile(keybindPath, kbRaw)) {
            for (auto& kv : kbRaw) {
                if (kv.second.type == Value::STR) {
                    int code = keyNameToGLFW(kv.second.str.c_str());
                    if (code >= 0) keybindMap[kv.first] = code;
                }
            }
        } else {
            fprintf(stderr, "WARNING: could not load keybinds from %s\n", keybindPath);
            ok = false;
        }
    }

    return ok;
}

float getFloat(const char* key, float def) {
    auto it = configMap.find(key);
    if (it != configMap.end() && it->second.type == Value::NUM)
        return it->second.num;
    return def;
}

int getInt(const char* key, int def) {
    auto it = configMap.find(key);
    if (it != configMap.end() && it->second.type == Value::NUM)
        return (int)it->second.num;
    return def;
}

int getKey(const char* action, int defaultKey) {
    auto it = keybindMap.find(action);
    if (it != keybindMap.end()) return it->second;
    return defaultKey;
}

} // namespace settings
