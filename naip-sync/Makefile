TARGET   = naip-sync
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
#OPT      = -O2 -Wall
OPT      = -g -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -La3d -la3d -lm -lz -lpthread -Llibexpat/expat/lib -lexpat
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) a3d libexpat
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: a3d libexpat

a3d:
	$(MAKE) -C a3d -f Makefile.sdl

libexpat:
	$(MAKE) -C libexpat/expat/lib

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C a3d -f Makefile.sdl clean
	$(MAKE) -C libexpat/expat/lib clean
	rm a3d
	rm libexpat

$(OBJECTS): $(HFILES)
