TARGET   = bluemarble
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
#OPT      = -g -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -Lnedgz -lnedgz -Ltexgz -ltexgz -Lterrain -lterrain -lpng -lm -lz
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) nedgz terrain texgz
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: nedgz terrain texgz

nedgz:
	$(MAKE) -C nedgz

terrain:
	$(MAKE) -C terrain

texgz:
	$(MAKE) -C texgz

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C nedgz clean
	$(MAKE) -C terrain clean
	$(MAKE) -C texgz clean
	rm nedgz terrain texgz

$(OBJECTS): $(HFILES)
