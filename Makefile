CC       = g++
CFLAGS   =
INCLUDES = -Iinclude
# lib/x64 comes first so the 64-bit freeGLUT is picked up before anything in lib/
LIBS     = -Llib/x64 -Llib \
           -lglfw3 -lfreeglut \
           -lopengl32 -lglu32 -lgdi32 -lwinmm -luser32 \
           -static-libgcc -static-libstdc++
SOURCES  = src/*.cpp src/*.c src/core/*.cpp src/renderer/*.cpp src/map/*.cpp src/player/*.cpp

all: build/main.exe

build/main.exe: $(SOURCES)
	$(CC) $(INCLUDES) $(CFLAGS) $(SOURCES) $(LIBS) -o build/main.exe

clean:
	-rm -f build/main.exe
