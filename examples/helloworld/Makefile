CFLAGS = -Wall -Werror -O0 -g

all: installer updater uninstaller

installer: installer.c
	cc $(CFLAGS) -lpsys -o installer installer.c

updater: updater.c
	cc $(CFLAGS) -lpsys -o updater updater.c

uninstaller: uninstaller.c
	cc $(CFLAGS) -lpsys -o uninstaller uninstaller.c
	
clean:
	rm installer
	rm updater
	rm uninstaller

.PHONY: clean
