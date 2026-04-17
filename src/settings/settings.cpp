#include "settings.h"
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

namespace settings {

/* ------------------------------------------------------------------ */
/*  internal data                                                     */
/* ------------------------------------------------------------------ */

struct Value {
    enum { NUM, STR } type;
    float  num;
    std::string str;
};

static std::map<std::string, Value> configMap;
static std::map<std::string, int>   keybindMap;

/* ------------------------------------------------------------------ */
/*  GLFW key-name table                                               */
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
/*  minimal flat-JSON parser                                          */
/* ------------------------------------------------------------------ */

static bool parseFile(const char* path, std::map<std::string, Value>& map) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::string buf(sz, '\0');
    fread(&buf[0], 1, (size_t)sz, f);
    fclose(f);

    size_t pos = 0;

    // skip whitespace and // comments
    auto skipWSIdx = [&]() {
        while (pos < buf.size()) {
            if (buf[pos] == ' ' || buf[pos] == '\t' || buf[pos] == '\n' || buf[pos] == '\r') { pos++; continue; }
            if (pos + 1 < buf.size() && buf[pos] == '/' && buf[pos + 1] == '/') {
                pos += 2;
                while (pos < buf.size() && buf[pos] != '\n') pos++;
                continue;
            }
            break;
        }
    };

    // parse string helper using index
    auto parseStringIdx = [&](std::string& out) -> bool {
        if (pos >= buf.size() || buf[pos] != '"') return false;
        pos++;
        out.clear();
        while (pos < buf.size() && buf[pos] != '"') {
            if (buf[pos] == '\\' && pos + 1 < buf.size()) { out += buf[pos + 1]; pos += 2; }
            else { out += buf[pos]; pos++; }
        }
        if (pos < buf.size() && buf[pos] == '"') pos++;
        return true;
    };

    skipWSIdx();
    if (pos >= buf.size() || buf[pos] != '{') return false;
    pos++;
    skipWSIdx();

    while (pos < buf.size() && buf[pos] != '}') {
        std::string key;
        if (!parseStringIdx(key)) break;

        skipWSIdx();
        if (pos >= buf.size() || buf[pos] != ':') break;
        pos++;
        skipWSIdx();

        Value val;
        if (pos < buf.size() && buf[pos] == '"') {
            val.type = Value::STR;
            if (!parseStringIdx(val.str)) break;
        } else {
            val.type = Value::NUM;
            char* end;
            val.num = strtof(buf.c_str() + pos, &end);
            pos = end - buf.c_str();
        }
        map[key] = val;

        skipWSIdx();
        if (pos < buf.size() && buf[pos] == ',') { pos++; skipWSIdx(); }
    }

    return true;
}

/* ------------------------------------------------------------------ */
/*  public API                                                        */
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
