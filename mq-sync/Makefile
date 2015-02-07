TARGET   = mq-sync
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -Lnedgz -lnedgz -Lnet -lnet -lm -lz -lpthread
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) net nedgz
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: net nedgz

nedgz:
	$(MAKE) -C nedgz

net:
	$(MAKE) -C net

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C nedgz clean
	$(MAKE) -C net clean
	rm net nedgz

$(OBJECTS): $(HFILES)
