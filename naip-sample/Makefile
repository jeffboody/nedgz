TARGET   = naip-sample
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -Llibpak -lpak -Ltexgz -ltexgz -ljpeg -Lterrain -lterrain -La3d -la3d -lm -lz -Llibexpat/expat/lib -lexpat
CCC      = gcc
ifeq ($(TEXGZ_USE_JP2),1)
	CFLAGS  += -DTEXGZ_USE_JP2
	LDFLAGS += -lopenjp2
endif

all: $(TARGET)

$(TARGET): $(OBJECTS) libpak terrain texgz a3d libexpat
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: libpak terrain texgz a3d libexpat

libpak:
	$(MAKE) -C libpak

terrain:
	$(MAKE) -C terrain

texgz:
	$(MAKE) -C texgz

a3d:
	$(MAKE) -C a3d -f Makefile.sdl

libexpat:
	$(MAKE) -C libexpat/expat/lib

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C libpak clean
	$(MAKE) -C terrain clean
	$(MAKE) -C texgz clean
	$(MAKE) -C a3d -f Makefile.sdl clean
	$(MAKE) -C libexpat/expat/lib clean
	rm libpak
	rm terrain
	rm texgz
	rm a3d
	rm libexpat

$(OBJECTS): $(HFILES)
