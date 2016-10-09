CC=g++
CPP=gcc
CFLAGS=-c -Wall -std=c++11 -Iinclude/ -Iexternal/syoyo-tinyobjloader-28ee1b4/ -g
LDFLAGS= -g -lGL -lGLU -lGLEW -lglut -lX11 -lXrandr -lXinerama -lXi -lXxf86vm -lXcursor -ldl
# matio compile and linker flags
CFLAGS+=#`pkg-config --cflags glfw3`
LDFLAGS+=`pkg-config --libs glfw3` -Lexternal/syoyo-tinyobjloader-28ee1b4/build/ -ltinyobjloader

SOURCES=src/viewer.cpp src/textfile.cpp
OBJECTS=$(SOURCES:.cpp=.o)

all: viewer

# Build Test
viewer: $(OBJECTS) 
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

# Build o files
.cpp.o: $(SOURCES) 
	$(CC) $(CFLAGS) -fPIC $< -o $@

clean:
	rm -f *.o src/*.o *.so viewer

indent:
	indent src/*.cpp include/* shader/*
