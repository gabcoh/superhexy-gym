shim.so: shim.c SDL2-2.0.12
	gcc -ISDL2-2.0.12/include -lSDL2-2.0 -lOpenGL -lEGL -lGLX -Wall -shared -fpic shim.c -o shim.so
clean_shim:

SDL2-2.0.12:
	rm -rf SDL2-2.0.12*
	wget "https://www.libsdl.org/release/SDL2-2.0.12.tar.gz"
	tar xf SDL2-2.0.12.tar.gz
libSDL2-2.0.so.0:
	rm -rf SDL2-2.0.12*
	wget "https://www.libsdl.org/release/SDL2-2.0.12.tar.gz"
	tar xf SDL2-2.0.12.tar.gz
	mkdir SDL2-2.0.12/build
	cmake -DVIDEO_X11=ON -DVIDEO_OFFSCREEN=ON -S SDL2-2.0.12 -B SDL2-2.0.12/build
	make -C SDL2-2.0.12/build -j 8
	cp SDL2-2.0.12/build/libSDL2-2.0.so.0 .
clean_SDL:
	rm -rf SDL2-2.0.12*

all: libSDL2-2.0.so.0 shim.so
.DEFAULT_GOAL := all

.PHONY: clean remove
clean: clean_SDL clean_shim

remove:
	rm -rf shim.so
	rm -rf libSDL2-2.0.so.0
