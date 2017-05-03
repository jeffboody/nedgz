TARGET   = subbluemarble
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
#OPT      = -g -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -Ltexgz -ltexgz -Lnedgz -lnedgz -lpng -lm -lz
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
	rm texgz nedgz

$(OBJECTS): $(HFILES)
