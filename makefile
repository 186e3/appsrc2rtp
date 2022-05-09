CC = gcc

# gstreamer-1.0 gstreamer-app-1.0 glib-2.0 gobject-2.0
STUFF = $(shell pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0 glib-2.0 gobject-2.0) -D_GNU_SOURCE
STUFF_LIBS = $(shell pkg-config --libs gstreamer-1.0 gstreamer-app-1.0 glib-2.0 gobject-2.0)
OPTS = -Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wunused #-Werror #-O2
GDB = -g -ggdb
OBJS = src/appsrc2rtp.o

all: appsrc2rtp

%.o: %.c
	$(CC) $(ASAN) $(STUFF) -fPIC $(GDB) -c $< -o $@ $(OPTS)

appsrc2rtp: $(OBJS)
	$(CC) $(GDB) -o appsrc2rtp $(OBJS) $(ASAN_LIBS) $(STUFF_LIBS)

clean:
	rm -f appsrc2rtp src/*.o