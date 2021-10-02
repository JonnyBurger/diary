TARGET = diary
SRCDIR = src/
_SRC = import.c utils.c caldav.c diary.c
SRC = $(addprefix $(SRCDIR), $(_SRC))
PREFIX ?= /usr/local
BINDIR ?= $(DESTDIR)$(PREFIX)/bin

MANDIR := $(DESTDIR)$(PREFIX)/share/man
MAN1 = man1/diary.1

CC = gcc
CFLAGS = -Wall \
         -DGOOGLE_OAUTH_CLIENT_ID=\"$(GOOGLE_OAUTH_CLIENT_ID)\" \
         -DGOOGLE_OAUTH_CLIENT_SECRET=\"$(GOOGLE_OAUTH_CLIENT_SECRET)\"
UNAME = ${shell uname}

ifeq ($(UNAME),FreeBSD)
	LIBS = -lncurses -lcurl -pthread
endif

ifeq ($(UNAME),Linux)
	LIBS = -lncursesw -lcurl -pthread
endif

ifeq ($(UNAME),Darwin)
	LIBS = -lncurses -lcurl -pthread -framework CoreFoundation
endif


default: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) $(BINDIR)/$(TARGET)
	install -d $(MANDIR)/man1
	install -m644 $(MAN1) $(MANDIR)/$(MAN1)

uninstall:
	rm -f $(BINDIR)/$(TARGET)
	rm -f $(MANDIR)/$(MAN1)
