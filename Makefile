.PHONY: all clean

SRCS=$(shell find nes -name '*.c')
HDRS=$(shell find nes -name '*.h')

all: nesemu
nesemu: $(SRCS) $(HDRS)
	$(CC) -o $@ $(SRCS) $(CFLAGS) $(LDFLAGS) $(LIBS)

install: nesemu
	mkdir -p $(DESTDIR)/usr/bin
	install -m 0755 nesemu $(DESTDIR)/usr/bin/nesemu