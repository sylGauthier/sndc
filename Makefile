#DEBUG := 1

PREFIX ?= /usr/local
BINDIR ?= bin

CFLAGS ?= -std=c89 -pedantic -march=native -Wall -Wno-unused-function $(if $(DEBUG),-g -DDEBUG,-O3) -Isrc -D_POSIX_C_SOURCE=200112L
LDFLAGS += -lm

OBJECTS := $(patsubst %.c,%.o,$(wildcard src/*.c))
OBJECTS += $(patsubst %.c,%.o,$(shell find src/modules -name *.c))
OBJECTS += $(patsubst %.l,%.o,src/lexer.l)

.PHONY: all
all: sndc $(DATA)

sndc: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(OBJECTS) sndc

B = "$(PREFIX)/$(BINDIR)"
install: sndc
	mkdir -p $B
	cp $< $B
	cp wrappers/sndc_play wrappers/sndc_export $B
