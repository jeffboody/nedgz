TARGET   = setacl
CLASSES  = nedgz_log
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
CFLAGS   = $(OPT) -I.
LDFLAGS  = -lpthread
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	rm nedgz_log.h nedgz_log.c

$(OBJECTS): $(HFILES)
