TARGET   = citylights
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall -Wno-format-truncation
#OPT      = -g -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -Lnedgz -lnedgz -Ltexgz -ltexgz -Lterrain -lterrain -Llibcc -lcc -lpng -lm -lz
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) libcc nedgz terrain texgz
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: libcc nedgz terrain texgz

libcc:
	$(MAKE) -C libcc

nedgz:
	$(MAKE) -C nedgz

terrain:
	$(MAKE) -C terrain

texgz:
	$(MAKE) -C texgz

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C libcc clean
	$(MAKE) -C nedgz clean
	$(MAKE) -C terrain clean
	$(MAKE) -C texgz clean
	rm libcc nedgz terrain texgz

$(OBJECTS): $(HFILES)
