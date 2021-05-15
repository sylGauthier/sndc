CFLAGS ?= -std=c89 -pedantic -march=native -Wall -g -D_XOPEN_SOURCE=500
LDLIBS += -lm
LDFLAGS += -lm

OBJECTS := $(patsubst %.c,%.o,$(wildcard src/*.c))

.PHONY: all
all: sndc $(DATA)

sndc: $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(OBJECTS) sndc
