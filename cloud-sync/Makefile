TARGET   = cloud-sync
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -Lnedgz -lnedgz -lm -lz -lpthread
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) nedgz
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: nedgz

nedgz:
	$(MAKE) -C nedgz

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C nedgz clean
	rm nedgz

$(OBJECTS): $(HFILES)
