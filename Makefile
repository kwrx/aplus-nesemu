.PHONY: all clean

CC ?= cc
LD ?= cc

OUTPUT=nesemu

SRCS=$(shell find nes -name '*.c')
HDRS=$(shell find nes -name '*.h')


all: $(OUTPUT)

$(OUTPUT): main.c $(SRCS) $(HDRS)
	$(CC) -o $(OUTPUT) main.c $(SRCS) -I aplus/include -I nes -Wl,--gc-sections -D_DEFAULT_SOURCE

install: $(OUTPUT)
	mkdir -p $(DESTDIR)/usr
	mkdir -p $(DESTDIR)/usr/bin
	mkdir -p $(DESTDIR)/.pkg
	install -m 0755 $(OUTPUT) $(DESTDIR)/usr/bin/$(OUTPUT)