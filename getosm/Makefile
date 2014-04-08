TARGET   = getosm
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
#OPT      = -g -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -Lnet -lnet -lm -lz
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) net
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: net

net:
	$(MAKE) -C net

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C net clean
	rm net

$(OBJECTS): $(HFILES)
