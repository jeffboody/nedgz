TARGET   = unpakblue
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
#OPT      = -g -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -Llibpak -lpak -Lnedgz -lnedgz -lm -lz
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) libpak nedgz
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: libpak nedgz

libpak:
	$(MAKE) -C libpak

nedgz:
	$(MAKE) -C nedgz

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C libpak clean
	$(MAKE) -C nedgz clean
	rm libpak nedgz

$(OBJECTS): $(HFILES)
