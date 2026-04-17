CC       = g++
CFLAGS   = -std=c++17
INCLUDES = -Iinclude
LIBS     = -lglfw -lGL -lm -ldl -lpthread -lX11 -lXrandr -lXinerama -lXi -lXxf86vm -lXcursor
SOURCES  = src/*.cpp src/*.c src/core/*.cpp src/renderer/*.cpp src/map/*.cpp src/player/*.cpp src/weapons/*.cpp src/settings/*.cpp src/projectile/*.cpp src/circular_sprite/*.cpp

all: build/main

build/main: $(SOURCES)
	mkdir -p build
	$(CC) $(INCLUDES) $(CFLAGS) $(SOURCES) $(LIBS) -o build/main

editor: build/map_editor

build/map_editor: tools/map_editor.cpp src/glad.c
	mkdir -p build
	$(CC) $(INCLUDES) $(CFLAGS) tools/map_editor.cpp src/glad.c $(LIBS) -o build/map_editor

texeditor: build/texture_editor.exe

build/texture_editor.exe: tools/texture_editor.cpp src/glad.c
	$(CC) $(INCLUDES) $(CFLAGS) tools/texture_editor.cpp src/glad.c $(LIBS) -o build/texture_editor.exe

clean:
	-rm -f build/main build/map_editor
