#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <psys.h>

static int uninstall(void)
{
	if (system("rm -rf /opt/example.com/helloworld-gui")) {
		return -1;
	}
	if (system("rm "
	           "/usr/share/applications/example.com-helloworld-gui.desktop")) {
		return -1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	psys_err_t err = NULL;

	if (psys_unannounce("example.com", "helloworld-gui", &err)) {
		fprintf(stderr, "psys_unannounce(): %s\n", psys_err_msg(err));
		psys_err_free(err);
		return EXIT_FAILURE;
	}

	if (uninstall())
		return EXIT_FAILURE;

	if (psys_unregister("example.com", "helloworld-gui", &err)) {
		fprintf(stderr, "psys_unregister(): %s\n", psys_err_msg(err));
		psys_err_free(err);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
