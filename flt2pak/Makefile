TARGET   = flt2pak
CLASSES  = flt_tile
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
#OPT      = -g -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -Llibpak -lpak -Ltexgz -ltexgz -Lnedgz -lnedgz -lm -lz
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) libpak texgz nedgz
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: libpak texgz nedgz

libpak:
	$(MAKE) -C libpak

texgz:
	$(MAKE) -C texgz

nedgz:
	$(MAKE) -C nedgz

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C libpak clean
	$(MAKE) -C texgz clean
	$(MAKE) -C nedgz clean
	rm libpak texgz nedgz flt_tile.h flt_tile.c

$(OBJECTS): $(HFILES)
