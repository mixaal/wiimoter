CC=gcc
DEFS=
CFLAGS=-Wall -Wstrict-prototypes $(DEFS)
RM=rm -f

WIIMOTE=wiiremote
MOUSE=mouse
WIIMOTE_OBJS=$(WIIMOTE).o $(MOUSE).o

WIIMOTE_LIBS=-lxwiimote -lm

all: $(WIIMOTE) $(MOUSE)

$(WIIMOTE): $(WIIMOTE_OBJS)
	$(CC) -o $@ $(WIIMOTE_OBJS) $(WIIMOTE_LIBS)

$(MOUSE): $(MOUSE).c
	$(CC) -DTEST_MOUSE -o $@ $<

clean:
	$(RM) $(WIIMOTE) $(MOUSE) *.o

test:
	echo "Done."

