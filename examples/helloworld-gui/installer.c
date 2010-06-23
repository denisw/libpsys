#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <psys.h>

static int install(void)
{
	if (mkdir("/opt", 0755) && errno != EEXIST) {
		perror("mkdir()");
		return -1;
	}
	if (mkdir("/opt/example.com", 0755) && errno != EEXIST) {
		perror("mkdir()");
		return -1;
	}
	if (mkdir("/opt/example.com/helloworld-gui", 0755)) {
		perror("mkdir()");
		return -1;
	}
	if (mkdir("/opt/example.com/helloworld-gui/bin", 0755)) {
		perror("mkdir()");
		return -1;
	}
	if (mkdir("/opt/example.com/helloworld-gui/share", 0755)) {
		perror("mkdir()");
		return -1;
	}

	if (system("cp helloworld-gui /opt/example.com/helloworld-gui/bin/")) {
		return -1;
	}
	if (system("cp Tux.png /opt/example.com/helloworld-gui/share/")) {
		return -1;
	}
	if (system("cp Tux_small.png /opt/example.com/helloworld-gui/share/")) {
		return -1;
	}
	if (system("cp example.com-helloworld-gui.desktop "
	           "/usr/share/applications")) {
		return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	psys_pkg_t pkg;
	psys_err_t err = NULL;

	pkg = psys_pkg_new(
		"example.com", "helloworld-gui", "0.1", "3.2", "noarch");

	if (!pkg) {
		fprintf(stderr, "%s\n", strerror(ENOMEM));
		return EXIT_FAILURE;
	}

	psys_pkg_add_summary(pkg, "C",
		"A simple graphical Hello World program");
	psys_pkg_add_description(pkg, "C",
		 "This is a simple graphical program which displays "
		 "\"Hello World!\" in a window. It is an "
		 "example of a  program installed using the "
		 "psys library.");

	psys_pkg_add_extra(pkg,
		"/usr/share/applications/example.com-helloworld-gui.desktop");

	if (psys_announce(pkg, &err)) {
		fprintf(stderr, "psys_announce(): %s\n", psys_err_msg(err));
		psys_err_free(err);
		psys_pkg_free(pkg);
		return EXIT_FAILURE;
	}

	if (install())
		return EXIT_FAILURE;

	if (psys_register(pkg, &err)) {
		fprintf(stderr, "psys_register(): %s\n", psys_err_msg(err));
		psys_err_free(err);
		psys_pkg_free(pkg);
		return EXIT_FAILURE;
	}

	psys_pkg_free(pkg);
	return EXIT_SUCCESS;
}
