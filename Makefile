CC ?= cc
CFLAGS ?= -O2 -pipe
LDFLAGS ?= -Wl,-s
DESTDIR ?=
BIN_DIR ?= /bin
MAN_DIR ?= /usr/share/man
DOC_DIR ?= /usr/share/doc

CFLAGS += -Wall -pedantic
LIBS = -lm

INSTALL = install -v

all: fbim

fbim: fbim.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS) $(LIBS)

install: all
	$(INSTALL) -D -m 755 fbim $(DESTDIR)/$(BIN_DIR)/fbim
	$(INSTALL) -D -m 644 fbim.1 $(DESTDIR)/$(MAN_DIR)/man1/fbim.1
	$(INSTALL) -D -m 644 README $(DESTDIR)/$(DOC_DIR)/fbim/README
	$(INSTALL) -m 644 AUTHORS $(DESTDIR)/$(DOC_DIR)/fbim/AUTHORS
	$(INSTALL) -m 644 COPYING $(DESTDIR)/$(DOC_DIR)/fbim/COPYING

clean:
	rm -f fbim
