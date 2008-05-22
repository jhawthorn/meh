
# User configuration
-include config.mk

SRCFILES := $(wildcard src/*.c)
OBJFILES := $(SRCFILES:%.c=%.o)
DEPFILES := $(OBJFILES:%.o=%.d)
CLEANFILES := $(CLEANFILES) $(DEPFILES) $(OBJFILES) meh
CFLAGS := -O3 -Wall -g -ggdb
LIBS := -lX11 -ljpeg -lpng

meh: $(OBJFILES)
	$(CC) -o $@ $(OBJFILES) $(LIBS)

-include $(DEPFILES)

%.o: %.c Makefile
	@echo "CC $<"
	@$(CC) $(CFLAGS) -MMD -MP -MT "$*.d" -c -o $@ $<

# Clean
clean:
	$(RM) $(CLEANFILES)

.PHONY: clean

