# The Psys Library

***Note:***
*I developed this library in 2010 as an attempt of to create a
 standard interface for app installers integrate with native distro
 package managers, with the goal to eventually have that interface
 integrated into the [LSB standard](http://www.linuxbase.org/). I have
 since given up on that effort, though, and didn't develop the library
 any further in the last years, with the consequence that its
 package-manager-specific backends don't build anymore due to library
 changes (I developed this under Ubuntu 10.04!).*

*However, as the idea and code might be of interest for some, I have
 decided to migrate the code and documentation to GitHub, where it is
 probably more easily found these days than at its original place
 [on Gitorious](https://www.gitorious.org/libpsys/libpsys). The
 accompanying
 [discussions on lsb-discuss](https://lists.linux-foundation.org/pipermail/packaging/2010-June/001235.html)
 might also be of interest.*

*Below is the original README, converted to Markdown and augmented
 with an example program that was originally only in the
 documentation. The man pages in the `doc/` directory can be found in
 Markdown format in the [wiki](https://github.com/denisw/libpsys/wiki).*

1. [Introduction](#introduction)
2. [Example](#example)
2. [Design](#design)
3. [Installation](#installation)
4. [Documentation](#documentation)
5. [Contributing](#contributing)

## Introduction

The psys library provides a simple interface to the system package
manager of a Linux system. It allows installation programs to notify
the package manager of the installation, uninstallation and update of
third-party software packages, and to request these packages to be
added to or removed from the system package database. The psys library
interface is generic and not tied to a specific package management
system or Linux distribution.

Note that the psys library is only useful for adding and removing
software which complies to the
[Linux Standard Base (LSB)](http://www.linuxbase.org)
specification. Most notably, it is assumed that the data files of a
software package are installed into `/opt` as demanded by the
[Filesystem Hierarchy Standard (FHS)](http://www.pathname.com/fhs/pub/fhs-2.3.html),
and that a package only depends on the interfaces and behavior
specified by the LSB; any additional dependencies must be contained
within the package itself.

The psys library interface is strongly influenced by the
["Berlin Packaging API"]([3]
http://www.linuxfoundation.org/en/Berlin_Packaging_API) concept [3]
discussed at the 2006 LSB face-to-face meeting in Berlin.  The library
interface is meant as a proposal for a future version of the LSB
standard.

## Example

The following program installs a simple hello world program and
registers it as a packate in the distribution's package system.

    #include <errno.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/stat.h>

    #include <psys.h>

    #define HELLOWORLD_SCRIPT \\
        "#!/bin/sh \\n" \\
        "echo \\"Hello World!\\""

    static void install_dir(const char *name)
    {
        if (mkdir(name, 0755) && errno != EEXIST) {
            perror("mkdir()");
            abort();
        }
    }

    static void install(void)
    {
        FILE *f;

        install_dir("/opt/example.com");
        install_dir("/opt/example.com/helloworld");
        install_dir("/opt/example.com/helloworld/bin");

        f = fopen("/opt/example.com/helloworld/bin/helloworld", "w");
        if (!f) {
            perror("fopen()");
            abort();
        }
        if (fputs(HELLOWORLD_SCRIPT, f) == EOF) {
            perror("fputs()");
            abort();
        }
        fclose(f);

        if (chmod("/opt/example.com/helloworld/bin/helloworld", 0755)) {
            perror("chmod()");
            abort();
        }
    }

    int main(int argc, char **argv)
    {
        psys_pkg_t pkg;
        psys_err_t err = NULL;

        pkg = psys_pkg_new(
            "example.com", "helloworld", "0.1", "3.0", "noarch");

        if (!pkg) {
            fprintf(stderr, "%s\\n", strerror(ENOMEM));
            return EXIT_FAILURE;
        }

        psys_pkg_add_summary(pkg, "C", "A simple Hello World program");
        psys_pkg_add_description(pkg, "C",
             "This is a simple program which prints "
             "\\"Hello World\\" onto the screen. It is an "
             "example of a  program installed using the "
             "psys library.");

        if (psys_announce(pkg, &err)) {
            fprintf(stderr, "psys_announce(): %s\\n", psys_err_msg(err));
            psys_err_free(err);
            psys_pkg_free(pkg);
            return EXIT_FAILURE;
        }

        install();

        if (psys_register(pkg, &err)) {
            fprintf(stderr, "psys_register(): %s\\n", psys_err_msg(err));
            psys_err_free(err);
            psys_pkg_free(pkg);
            return EXIT_FAILURE;
        }

        psys_pkg_free(pkg);
        return EXIT_SUCCESS;
    }

## Design

Psys is divided into two parts, a "frontend" and a "backend".

* The **frontend** implements the distribution-independent functions of the
  psys library, e.g. for the creation of the psys data structures. This
  part is fully implemented by the psys library source code itself.

* The **backend** is specific to a package manager or Linux
  distribution and implements the actual core functionality of the
  psys library interface (that is, processing notifications and
  package registration requests from installation programs). It can
  either be implemented by the package manager / distribution directly
  or, otherwise, by a **fallback backend** built into the core psys
  library itself. Currently, the psys library comes with fallback
  backends for RPM and Debian-based distributions; others may be
  added in the future. See `HACKING` for general information about
  writing a psys library backend.

The availability of fallback backends means that the psys library can,
in principal, be used on today's distros by installation programs
which ship the library theirselves or statically link to it. As the
psys library only takes care of the commmunication with the package
manager, but leaves the actual installation / update / uninstallation
to the installation program, adopting an existing installer to use the
psys interface should require relatively little code modifications.

## Installation

The psys library can be built like any autotools-powered project with

    ./configure [options]
    make
    make install  # as super user (root, sudo)

In addition to the standard configure options, the following can be
specified:

    --enable-fallback-rpm	Build RPM fallback backend (requires rpmlib)
    --enable-fallback-dpkg  Build DPKG fallback backend (requires libdpkg)
    --enable-fallback-all   Build all fallback backends

Note that in order to build the DPKG fallback backend, you need a
version of `libdpkg.a` which is compiled to position-independent code
with `-fPIC`; otherwise, linking will fail. As the `libdpkg-dev`
package in the Debian testing/unstable repositories is currently not
built with `-fPIC`, you need to build it yourself:

    git clone git://git.debian.org/git/dpkg/dpkg.git
    cd dpkg

    ## If you have a gettext version < 0.18 (e.g. Ubuntu 10.04): ##
    git checkout 1.15.7

    ## Add -fPIC to compiler flags ##
    echo "AM_CFLAGS += -fPIC" | cat lib/dpkg/Makefile.am - > Makefile.am.tmp
    cp Makefile.am.tmp lib/dpkg/Makefile.am
    rm Makefile.am.tmp

    ## Configure and install ##
    autoreconf -i
    ./configure
    cd lib/dpkg
    make
    sudo make install

Your dpkg installation will not be affected by this.

After having installed the psys library and a backend, you can test it by
building and running the examples in the examples/ sub-directory:

    cd examples/helloworld
    make
    sudo ./installer
    sudo ./updater
    sudo ./uninstaller

## Documentation

The psys library interface is documented in man page format; see the
`sys(7)` man page distributed with this library to get started.
Alternatively, all pages are available online at:

http://gitorious.org/libpsys/pages/ManPages

Also have a look at the examples in the `examples/` sub-directory to
see the psys library in action (see [Installation](#installation)).

## Contributing

Contributions of any kind are, of course, very welcome! Please submit
bug reports, patches, feature requests and branch merge requests to
the libpsys newsgroup:

http://groups.google.com/group/libpsys

To track the development of the psys library and get the latest source
code, visit the project page on Gitorious:

http://gitorious.org/libpsys

You are free to publish any number psys library branches there if you like.

For some information about hacking on the psys library source code, and a
short overview of writing psys library backends, see `HACKING`.
