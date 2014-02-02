TARGET   = flt2mtex
CLASSES  = flt_tile
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
#OPT      = -g -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -Ltexgz -ltexgz -Lnedgz -lnedgz -lm -lz
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) texgz nedgz
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: texgz nedgz

texgz:
	$(MAKE) -C texgz

nedgz:
	$(MAKE) -C nedgz

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C texgz clean
	$(MAKE) -C nedgz clean
	rm texgz nedgz flt_tile.h flt_tile.c

$(OBJECTS): $(HFILES)
