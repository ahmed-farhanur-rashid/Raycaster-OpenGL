CC       = g++
CFLAGS   =
INCLUDES = -Iinclude
LIBS     = -Llib \
           -lglfw3 \
           -lopengl32 -lgdi32 -lwinmm -luser32 \
           -static-libgcc -static-libstdc++
SOURCES  = src/*.cpp src/*.c src/core/*.cpp src/renderer/*.cpp src/map/*.cpp src/player/*.cpp src/weapons/*.cpp src/settings/*.cpp

all: build/main.exe

build/main.exe: $(SOURCES)
	$(CC) $(INCLUDES) $(CFLAGS) $(SOURCES) $(LIBS) -o build/main.exe

editor: build/map_editor.exe

build/map_editor.exe: tools/map_editor.cpp src/glad.c
	$(CC) $(INCLUDES) $(CFLAGS) tools/map_editor.cpp src/glad.c $(LIBS) -o build/map_editor.exe

texeditor: build/texture_editor.exe

build/texture_editor.exe: tools/texture_editor.cpp src/glad.c
	$(CC) $(INCLUDES) $(CFLAGS) tools/texture_editor.cpp src/glad.c $(LIBS) -o build/texture_editor.exe

clean:
	-rm -f build/main.exe build/map_editor.exe build/texture_editor.exe
