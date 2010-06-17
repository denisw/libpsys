CFLAGS = -Wall -O0 -g

all: helloworld-gui helloworld-gui2 installer updater uninstaller

helloworld-gui: helloworld-gui.c
	cc $(CFLAGS) -o helloworld-gui helloworld-gui.c \
		`pkg-config --cflags gtk+-2.0` \
		`pkg-config --libs gtk+-2.0`

helloworld-gui2: helloworld-gui2.c
	cc $(CFLAGS) -o helloworld-gui2 helloworld-gui2.c \
		`pkg-config --cflags gtk+-2.0` \
		`pkg-config --libs gtk+-2.0`

installer: installer.c
	cc $(CFLAGS) -o installer -lpsys installer.c

updater: updater.c
	cc $(CFLAGS) -o updater -lpsys updater.c

uninstaller: uninstaller.c
	cc $(CFLAGS) -o uninstaller -lpsys uninstaller.c

clean:
	rm helloworld-gui
	rm helloworld-gui2
	rm installer
	rm updater
	rm uninstaller

.PHONY: clean
