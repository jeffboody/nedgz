TARGET   = hillshade
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
#OPT      = -g -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -Llibpak -lpak -Lnedgz -lnedgz -Ltexgz -ltexgz -lm -lz
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) libpak nedgz texgz
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: libpak nedgz texgz

libpak:
	$(MAKE) -C libpak

nedgz:
	$(MAKE) -C nedgz

texgz:
	$(MAKE) -C texgz

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C libpak clean
	$(MAKE) -C nedgz clean
	$(MAKE) -C texgz clean
	rm libpak nedgz texgz

$(OBJECTS): $(HFILES)
