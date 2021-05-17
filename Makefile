CFLAGS ?= -std=c89 -pedantic -march=native -Wall -Wno-unused-function -O3
LDLIBS += -lm
LDFLAGS += -lm

OBJECTS := $(patsubst %.c,%.o,$(wildcard src/*.c src/modules/*.c))
OBJECTS += $(patsubst %.l,%.o,src/lexer.l)

.PHONY: all
all: sndc $(DATA)

sndc: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(OBJECTS) sndc
