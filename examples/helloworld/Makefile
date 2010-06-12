all: installer updater uninstaller

installer: installer.c
	cc -O0 -g -lpsys -o installer installer.c

updater: updater.c
	cc -O0 -g -lpsys -o updater updater.c

uninstaller: uninstaller.c
	cc -O0 -g -lpsys -o uninstaller uninstaller.c
	
clean:
	rm installer
	rm updater
	rm uninstaller

.PHONY: clean
