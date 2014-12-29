TARGET   = ned2stl
CLASSES  =
SOURCE   = $(TARGET).c $(CLASSES:%=%.c)
OBJECTS  = $(TARGET).o $(CLASSES:%=%.o)
HFILES   = $(CLASSES:%=%.h)
OPT      = -O2 -Wall
CFLAGS   = $(OPT) -I. -DA3D_GLESv2_LOAX
LDFLAGS  = -L/usr/lib -Lnedgz -lnedgz -La3d -la3d -Lloax -lloax -Lnet -lnet -lm -lz
CCC      = gcc

all: $(TARGET)

$(TARGET): $(OBJECTS) nedgz a3d net loax
	$(CCC) $(OPT) $(OBJECTS) -o $@ $(LDFLAGS)

.PHONY: nedgz a3d net loax

net:
	$(MAKE) -C net

nedgz:
	$(MAKE) -C nedgz

a3d:
	$(MAKE) -C a3d -f Makefile.loax

loax:
	$(MAKE) -C loax

clean:
	rm -f $(OBJECTS) *~ \#*\# $(TARGET)
	$(MAKE) -C net clean
	$(MAKE) -C a3d -f Makefile.loax clean
	$(MAKE) -C nedgz clean
	$(MAKE) -C loax clean
	rm net a3d nedgz loax

$(OBJECTS): $(HFILES)
