#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psys.h>

#define HELLOWORLD_SCRIPT_2 \
	"#!/bin/sh \n" \
	"if [ $# -gt 0 ]; then \n" \
	"  echo \"Hello $1!\" \n" \
	"else \n" \
	"  echo \"Hello World!\" \n" \
	"fi"

static int update(void)
{
	FILE *f;

	f = fopen("/opt/example.com/helloworld/bin/helloworld", "w");
	if (!f) {
		perror("fopen()");
		return -1;		
	}
	if (fputs(HELLOWORLD_SCRIPT_2, f) == EOF) {
		perror("fputs()");
		return -1;
	}
	fclose(f);

	return 0;
}

int main(int argc, char **argv)
{
	psys_pkg_t pkg;
	psys_err_t err = NULL;

	pkg = psys_pkg_new(
		"example.com", "helloworld", "0.2", "3.0", "noarch");

	if (!pkg) {
		fprintf(stderr, "%s\n", strerror(ENOMEM));
		return EXIT_FAILURE;
	}

	psys_pkg_add_summary(pkg, "C", "A simple Hello World program");
	psys_pkg_add_description(pkg, "C",
		 "This is a simple  \"Hello World\" program. "
		 "In its second version, the program can say "
		 "given a thing as argument it can say \"Hello\" to!\n"
		 "\n"
		 "This is an example of a  program installed and "
		 "updated using the psys library.");

	if (psys_announce_update(pkg, &err)) {
		fprintf(stderr,
			"psys_announce_update(): %s\n", psys_err_msg(err));
		psys_err_free(err);
		psys_pkg_free(pkg);
		return EXIT_FAILURE;
	}

	if (update())
		return EXIT_FAILURE;

	if (psys_register_update(pkg, &err)) {
		fprintf(stderr,
			"psys_register_update(): %s\n", psys_err_msg(err));
		psys_err_free(err);
		psys_pkg_free(pkg);
		return EXIT_FAILURE;
	}

	psys_pkg_free(pkg);
	return EXIT_SUCCESS;
}
