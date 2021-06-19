#DEBUG := 1

CFLAGS ?= -std=c89 -pedantic -march=native -Wall -Wno-unused-function $(if $(DEBUG),-g -DDEBUG,-O3) -Isrc -D_POSIX_C_SOURCE=200112L
LDFLAGS += -lm

OBJECTS := $(patsubst %.c,%.o,$(wildcard src/*.c src/modules/*.c src/modules/*/*.c))
OBJECTS += $(patsubst %.l,%.o,src/lexer.l)

.PHONY: all
all: sndc $(DATA)

sndc: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(OBJECTS) sndc
