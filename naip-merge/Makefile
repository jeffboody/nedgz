TARGET   = naip-merge
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
#OPT      = -g -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -La3d -la3d
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) a3d
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: a3d

a3d:
	$(MAKE) -C a3d -f Makefile.sdl

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C a3d -f Makefile clean
	rm a3d

$(OBJECTS): $(HFILES)
