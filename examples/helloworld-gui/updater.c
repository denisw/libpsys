#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <psys.h>

static int update(void)
{
	if (system("cp helloworld-gui2 "
	           "/opt/example.com/helloworld-gui/bin/helloworld-gui")) {
		return -1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	psys_pkg_t pkg;
	psys_err_t err = NULL;

	pkg = psys_pkg_new(
		"example.com", "helloworld-gui", "0.2", "3.2", "noarch");

	if (!pkg) {
		fprintf(stderr, "%s\n", strerror(ENOMEM));
		return EXIT_FAILURE;
	}

	psys_pkg_add_summary(pkg, "C",
		"A simple graphical Hello World program");
	psys_pkg_add_description(pkg, "C",
		 "This is a simple graphical program which displays "
		 "\"Hello World!\" (or \"Hello <what you want>!\" since "
		 "version 0.2!) in a window. It is an "
		 "example of a  program installed and updated using the "
		 "psys library.");

	psys_pkg_add_extra(pkg,
		"/usr/share/applications/example.com-helloworld-gui.desktop");

	if (psys_announce_update(pkg, &err)) {
		fprintf(stderr, "psys_announce_update(): %s\n",
			psys_err_msg(err));
		psys_err_free(err);
		psys_pkg_free(pkg);
		return EXIT_FAILURE;
	}

	if (update())
		return EXIT_FAILURE;

	if (psys_register_update(pkg, &err)) {
		fprintf(stderr, "psys_register_update(): %s\n",
			psys_err_msg(err));
		psys_err_free(err);
		psys_pkg_free(pkg);
		return EXIT_FAILURE;
	}

	psys_pkg_free(pkg);
	return EXIT_SUCCESS;
}
