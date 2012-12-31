GTK_PIDGIN_INCLUDES= `pkg-config --cflags gtk+-2.0 pidgin`

CFLAGS= -O2 -Wall -fpic 
CFLAGS_DEBUG= -g -Wall -fpic 
LDFLAGS= -shared

INCLUDES = \
      $(GTK_PIDGIN_INCLUDES)

all: clean closeoffline.so

closeoffline.so: closeoffline.c
	gcc closeoffline.c $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o closeoffline.so

debug: clean closeoffline.c
	clang closeoffline.c $(CFLAGS_DEBUG) $(INCLUDES) $(LDFLAGS) -o closeoffline.so

install: closeoffline.so
	mkdir -p ~/.purple/plugins
	cp closeoffline.so ~/.purple/plugins/

uninstall:
	rm -f ~/.purple/plugins/closeoffline.so

clean:
	rm -f closeoffline.so

build-dep:
	sudo apt-get build-dep pidgin
	sudo apt-get install pidgin-dev
