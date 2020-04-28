TARGET   = subcitylights
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
#OPT      = -g -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -Ltexgz -ltexgz -Lnedgz -lnedgz -Llibcc -lcc -lpng -lm -lz
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) libcc texgz nedgz
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: libcc texgz nedgz

libcc:
	$(MAKE) -C libcc

texgz:
	$(MAKE) -C texgz

nedgz:
	$(MAKE) -C nedgz

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C libcc clean
	$(MAKE) -C texgz clean
	$(MAKE) -C nedgz clean
	rm libcc texgz nedgz

$(OBJECTS): $(HFILES)
