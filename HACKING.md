# HACKING

1. [Overview](#overview)
2. [Coding Style](#coding-style)
3. [Writing a Psys Library Backend](#writing-a-psys-library-backend)

## Overview

A complete psys library implementation consists of a
distribution-independent fronted and a distribution- (or
package-manager)-specific backend. The psys xlibrary source code
provides a complete frontend implementation and a fallback
implementation for the backend (if the backend is not provided by the
system itself; more on that later).

The frontend can be found in the `lib/` directory. `<psys.h>` is the psys
interface declaration, whereas `<psys_impl.h>` declares a set of functions
which are handy for implementing psys backends. They are implemented in
the `psys.c` and `psys_impl.c` source files, respectively.

The fallback backends are in `fallback/`. `fallback.c` contains the
general dispatching logic for calling the correct fallback
implementation for the installation target machine. Each source file
of the form `fallback_*.c` implements a fallback backend for a
specific package manager. Currently, there are RPM (`fallback_rpm.c`)
and DPKG (`fallback_dpkg.c`) backends.

The fallback backend is built to a shared library named
`libpsys_impl`; this is the library name by which the psys library
frontend will look for when loading the backend via `dlopen`.

`man/` contains the man pages which make up the psys library's
interface documentation.

## Coding Style

The psys library source code consistently follows the Linux Coding Style
as close as possible. See here for the current version:

http://www.kernel.org/doc/Documentation/CodingStyle

The most important thing to know is that tabs are used for indentation and
have have a width of 8 characters.

## Writing a Psys Library Backend

If you would like your package manager or distribution to support the psys
library interface, you can relatively easily write a backend for it.

A psys library backend is basically just a shared library named
"libpsys_impl" which exports the following functions:

    extern int _psys_announce(psys_pkg_t pkg, psys_err_t *err);
    extern int _psys_register(psys_pkg_t pkg, psys_err_t *err);

    extern int _psys_announce_update(psys_pkg_t pkg, psys_err_t *err);
    extern int _psys_register_update(psys_pkg_t pkg, psys_err_t *err);

    extern int _psys_unannounce(const char *vendor, const char *name,
                                psys_err_t *err);
    extern int _psys_unregister(const char *vendor, const char *name,
                                psys_err_t *err);

These functions should have the semantics documented for the equally-named
psys library interface functions (without the leading underscore) in the
respective functions' man pages or online at:

http://gitorious.org/libpsys/pages/ManPages

When implementing a *fallback backend* directly in the psys library source
code, the mentioned functions must be prefixed with an identifier which
is unique to the backend. For instance, all RPM fallback backend functions
are prefixed with `rpm_` (e.g. `rpm_psys_announce`). The chosen identifier
must be added to the `_fallbacks` array defined in `fallback.c`:

    const char *_fallbacks = {
	    "rpm",
	    /* ... */
	    "yourbackend",
	    NULL
    };

Additionally, fallback backends must export two more functions:

    extern int <backend>_fallback_match(void);
    extern int <backend>_fallback_match_fuzzy(void);

(where `<backend>` is your fallback backend's identifier.) The former
should return 1 if the backend *definitely* (or at least very
probably) is the correct one to use for the current system, and 0
otherwise. The latter should return 1 if the backend would work, but
only *may* be the correct one, and 0 otherwise. For instance, the RPM
fallback backend checks in `rpm_fallback_match()` if the system is a
known RPM-based distribution, and `rpm_fallback_match_fuzzy()` only
looks if the `rpm` command is available.

When implementing a psys library backend, whether fallback or
external, the `<psys_impl.h>` header should be imported. It declares
many functions which are essential or at least very handy for backend
implementation, such as functions for reporting errors to the caller,
validating package objects, finding out the distribution currently
running etc. See the `<psys_impl.h>` header for a list of the
available functions and the source code of the RPM fallback backend
(`fallback/fallback_rpm.c`) for examples of their usage.

Last but not least, some general advice:

* At the beginning of `<backend>_psys_announce()`,
  `<backend>_psys_announce_update()`, `<backend>_psys_register()` and
  `<backend>_psys_register_update()` (or the correspondinbg fallback
  backend functions), *always* copy the passeed package object with
  `psys_pkg_copy()` and validate it with `psys_pkg_validate()` before
  doing anything with it. This saves you from malicious input and
  concurrent modifications of the package object from other possibly
  running threads. The code for doing this could look like this:

        pkg = psys_pkg_copy(pkg);
        if (!pkg) {
                psys_err_set_nomem(err);
                return -1;
        }
        psys_pkg_assert_valid(pkg);

* When acquiring a package database lock in
  `<backend>_psys_announce()`, *never* keep the lock after return,
  assuming `<backend>_psys_register()` can release it later. This
  opens up all kinds of possibilities for denial-of-service
  attacks. (What if a buggy or malicious installation program
  announces a package but never registers it?)  Same for
  `<backend>_psys_announce_update()` /
  `<backend>_psys_register_update()`.

Happy hacking!
