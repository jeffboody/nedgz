TARGET   = tiletex
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
#OPT      = -g -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -Ltexgz -ltexgz -lpng -ljpeg -lz
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) texgz
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: texgz

texgz:
	$(MAKE) -C texgz

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C texgz clean
	rm texgz

$(OBJECTS): $(HFILES)
