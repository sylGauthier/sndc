#DEBUG := 1

PREFIX ?= /usr/local
BINDIR ?= bin

CFLAGS ?= -std=c89 -pedantic -march=native -Wall -Wno-unused-function $(if $(DEBUG),-g -DDEBUG,-O3) -Isrc -D_POSIX_C_SOURCE=200112L
LDFLAGS += -lm

MODSRC := $(shell find src/modules -name *.c)
MODLIST := src/modules.h

OBJECTS := $(patsubst %.c,%.o,$(wildcard src/*.c))
OBJECTS += $(patsubst %.c,%.o,$(MODSRC))
OBJECTS += $(patsubst %.l,%.o,src/lexer.l)

.PHONY: all
all: sndc $(DATA)

$(MODLIST): $(MODSRC)
	modules="$(shell cat $^ | sed -ne 's/.*DECLARE_MODULE(\(.*\)).*/\1/p')"; \
	for m in $$modules ; do printf "extern const struct Module %s;\n" "$$m" ; done > $@; \
	printf "\nconst struct Module* modules[] = {\n" >> $@; \
	for m in $$modules ; do printf "    &%s,\n" "$$m" ; done >> $@; \
	printf "    NULL\n};\n" >> $@

src/modules.c: $(MODLIST)

sndc: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(OBJECTS) $(MODLIST) sndc

B = "$(PREFIX)/$(BINDIR)"
install: sndc
	mkdir -p $B
	cp $< $B
	cp wrappers/sndc_play wrappers/sndc_export $B
