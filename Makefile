
TARGET := meh
TESTTARGET := test/test

SRCFILES := $(wildcard src/*.c)
OBJFILES := $(SRCFILES:%.c=%.o)
DEPFILES := $(OBJFILES:%.o=%.d)
TESTCLEAN := $(TESTTARGET) $(TESTTARGET).d $(TESTTARGET).o
CLEANFILES := $(CLEANFILES) $(DEPFILES) $(OBJFILES) test/test.o test/test.d test/test $(TARGET)
LIBS ?= -lX11 -lXext -ljpeg -lpng -lgif
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin

# User configuration
CONFIG ?= ../config
-include configs/$(CONFIG).mk

CFLAGS ?= -O3 -DNDEBUG

meh: $(OBJFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

test: $(TESTTARGET)
	./$(TESTTARGET)

test/test: test/test.o $(filter-out src/main.o, $(OBJFILES))
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

-include $(DEPFILES)

%.o: %.c Makefile
	$(CC) $(CFLAGS) -MMD -MP -MT "$*.d" -c -o $@ $<

install:
	install -Dm 755 meh $(BINDIR)

# Clean
clean:
	$(RM) $(CLEANFILES)

.PHONY: clean test

