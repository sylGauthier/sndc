#DEBUG := 1

NAME := sndc
VERSION := 0.1
DEPS := fftw3f

PREFIX ?= /usr/local
BINDIR ?= bin
LIBDIR ?= lib
INCDIR ?= include

CFLAGS ?= -std=c89 -pedantic -march=native -fPIC -Wall -Wno-unused-function $(if $(DEBUG),-g -DDEBUG,-O3) -Ilib$(NAME) -D_POSIX_C_SOURCE=200112L
CFLAGS += $(shell pkg-config --cflags $(DEPS))

LDFLAGS += -lm
LDFLAGS += $(shell pkg-config --libs $(DEPS))

MODSRC := $(shell find lib$(NAME)/modules -name '*.c')
MODLIST := lib$(NAME)/modules.h

LIBOBJS := $(patsubst %.c,%.o,$(wildcard lib$(NAME)/*.c))
LIBOBJS += $(patsubst %.c,%.o,$(MODSRC))
LIBOBJS += $(patsubst %.l,%.o,lib$(NAME)/lexer.l)

LIB := lib$(NAME).a

.PHONY: all
all: sndc/$(NAME)


#### Module list ####

$(MODLIST): $(MODSRC)
	modules="$(shell cat $^ | sed -ne 's/.*DECLARE_MODULE(\(.*\)).*/\1/p')"; \
	for m in $$modules ; do printf "extern const struct Module %s;\n" "$$m" ; done > $@; \
	printf "\nconst struct Module* modules[] = {\n" >> $@; \
	for m in $$modules ; do printf "    &%s,\n" "$$m" ; done >> $@; \
	printf "    NULL\n};\n" >> $@

lib$(NAME)/modules.c: $(MODLIST)

################


#### sndc library ####

$(LIB): $(LIBOBJS)
	$(AR) rcs $@ $^

################


#### sndc executable ####

sndc/$(NAME): sndc/$(NAME).o $(LIB)
	$(CC) -o $@ $< $(LDFLAGS) -L. -l$(NAME)

$(NAME).pc:
	printf 'prefix=%s\nincludedir=%s\nlibdir=%s\n\nName: %s\nDescription: %s\nVersion: %s\nCflags: %s\nLibs: %s\nRequires: %s' \
		'$(PREFIX)' \
		'$${prefix}/$(INCDIR)' \
		'$${prefix}/$(LIBDIR)' \
		'$(NAME)' \
		'$(NAME)' \
		'$(VERSION)' \
		'-I$${includedir}' \
		'-L$${libdir} -l$(NAME)' \
		'$(DEPS)' \
		> $@

################


### install and clean ###

B = $(PREFIX)/$(BINDIR)
L = $(PREFIX)/$(LIBDIR)
I = $(PREFIX)/$(INCDIR)
P = $(L)/pkgconfig

install: $(LIB) sndc/$(NAME) $(NAME).pc
	mkdir -p "$B"
	cp sndc/$(NAME) sndc/$(NAME)_play sndc/$(NAME)_export "$B"
	cp $(LIB) "$L"
	cp libsndc/sndc.h "$I"
	cp $(NAME).pc "$P"

clean:
	rm -rf $(LIB) $(LIBOBJS) $(MODLIST) sndc/$(NAME) sndc/$(NAME).o $(NAME).pc pysndc/_pysndc*

################
