#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psys.h>

#define HELLOWORLD_SCRIPT \
	"#!/bin/sh \n" \
	"echo \"Hello World!\""

static int install(void)
{
	FILE *f;

	if (mkdir("/opt/example.com", 0755) && errno != EEXIST) {
		perror("mkdir()");
		return -1;
	}
	if (mkdir("/opt/example.com/helloworld", 0755)) {
		perror("mkdir()");
		return -1;
	}
	if (mkdir("/opt/example.com/helloworld/bin", 0755)) {
		perror("mkdir()");
		return -1;
	}

	f = fopen("/opt/example.com/helloworld/bin/helloworld", "w");
	if (!f) {
		perror("fopen()");
		return -1;		
	}
	if (fputs(HELLOWORLD_SCRIPT, f) == EOF) {
		perror("fputs()");
		return -1;
	}
	fclose(f);

	if (chmod("/opt/example.com/helloworld/bin/helloworld", 0755)) {
		perror("chmod()");
		return -1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	psys_pkg_t pkg;
	psys_err_t err = NULL;

	pkg = psys_pkg_new(
		"example.com", "helloworld", "0.1", "3.0", "noarch");

	if (!pkg) {
		fprintf(stderr, "%s\n", strerror(ENOMEM));
		return EXIT_FAILURE;
	}

	psys_pkg_add_summary(pkg, "C", "A simple Hello World program");
	psys_pkg_add_description(pkg, "C",
		 "This is a simple program which prints "
		 "\"Hello World\" onto the screen. It is an "
		 "example of a  program installed using the "
		 "psys library.");

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
