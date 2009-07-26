
SRCFILES := $(wildcard src/*.c)
OBJFILES := $(SRCFILES:%.c=%.o)
DEPFILES := $(OBJFILES:%.o=%.d)
CLEANFILES := $(CLEANFILES) $(DEPFILES) $(OBJFILES) meh
CFLAGS ?= -O3
LIBS ?= -lX11 -lXext -ljpeg -lpng -lgif
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

# User configuration
CONFIG ?= ../config
-include configs/$(CONFIG).mk

meh: $(OBJFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJFILES) $(LIBS)

-include $(DEPFILES)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -MMD -MP -MT "$*.d" -c -o $@ $<

install:
	install -Dm 755 meh $(BINDIR)

# Clean
clean:
	$(RM) $(CLEANFILES)

.PHONY: clean

