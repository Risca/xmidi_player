CC=gcc
TARGET=xmidi_player
CFLAGS=-g `sdl-config --cflags --libs sdl` -lSDL_mixer

SRC=$(wildcard *.c)

all: $(TARGET)

clean:
	rm $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

