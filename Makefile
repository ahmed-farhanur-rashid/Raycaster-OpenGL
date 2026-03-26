CC       = g++
CFLAGS   =
INCLUDES = -Iinclude
LIBS     = -Llib \
           -lglfw3 \
           -lopengl32 -lgdi32 -lwinmm -luser32 \
           -static-libgcc -static-libstdc++
SOURCES  = src/*.cpp src/*.c src/core/*.cpp src/renderer/*.cpp src/map/*.cpp src/player/*.cpp

all: build/main.exe

build/main.exe: $(SOURCES)
	$(CC) $(INCLUDES) $(CFLAGS) $(SOURCES) $(LIBS) -o build/main.exe

clean:
	-rm -f build/main.exe
