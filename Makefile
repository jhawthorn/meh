
SRCFILES := $(wildcard src/*.c)
OBJFILES := $(SRCFILES:%.c=%.o)
DEPFILES := $(OBJFILES:%.o=%.d)
CLEANFILES := $(CLEANFILES) $(DEPFILES) $(OBJFILES) meh
LIBS ?= -lX11 -lXext -ljpeg -lpng -lgif
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

# User configuration
-include config.mk




meh: $(OBJFILES)
	$(CC) $(LDFLAGS) -o $@ $(OBJFILES) $(LIBS)

-include $(DEPFILES)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -MMD -MP -MT "$*.d" -c -o $@ $<

install:
	install -m 755 meh $(BINDIR)

# Clean
clean:
	$(RM) $(CLEANFILES)

.PHONY: clean

