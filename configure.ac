AC_PREREQ([2.65])
# Original bug report address: paul@copters.com
AC_INIT([VTECO], [7.0],
        [robin.haberkorn@googlemail.com],
        [vteco],
        [https://github.com/rhaberkorn/videoteco-fork])
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE

AC_CONFIG_SRCDIR([teco.c])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_BUILD
AC_CANONICAL_HOST

AC_PROG_CC([cc gcc clang])
AC_PROG_INSTALL

# Doxygen is not necessarily required as long as
# you do not run `make devdoc`.
AC_CHECK_PROG(DOXYGEN, doxygen, doxygen)
AC_CHECK_PROG(DOT, dot, dot)
DOXYGEN_HAVE_DOT=YES
if [[ x$DOT = x ]]; then DOXYGEN_HAVE_DOT=NO; fi
AC_SUBST(DOXYGEN_HAVE_DOT)

AC_SYS_LONG_FILE_NAMES
AC_SYS_POSIX_TERMIOS

# The original build system also checked for libncurses.
PKG_CHECK_MODULES(LIBTINFO, [tinfo], [
	CFLAGS="$CFLAGS $LIBTINFO_CFLAGS"
	LIBS="$LIBS $LIBTINFO_LIBS"
	AC_DEFINE([HAVE_LIBTERMINFO], 1, [Define to 1 if you have the `terminfo' library.])
], [
	AC_SEARCH_LIBS(setupterm, [tinfo terminfo], [
		AC_DEFINE([HAVE_LIBTERMINFO], 1, [Define to 1 if you have the `terminfo' library.])
	], [
		AC_SEARCH_LIBS(tgetstr, [termcap], [
			AC_DEFINE([HAVE_LIBTERMCAP], 1, [Define to 1 if you have the `termcap' library.])
		])
	])
])

AC_HEADER_TIOCGWINSZ
AC_HEADER_DIRENT
AC_HEADER_STAT
#AC_HEADER_STDC
AC_HEADER_SYS_WAIT

AC_CHECK_HEADERS([ctype.h errno.h fcntl.h memory.h poll.h pwd.h sgtty.h signal.h])
AC_CHECK_HEADERS([sys/file.h sys/filio.h sys/ioctl.h sys/param.h sys/select.h sys/socket.h])
AC_CHECK_HEADERS([termcap.h termios.h termio.h])
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

AC_FUNC_FORK
AC_FUNC_MALLOC

AC_CHECK_FUNCS([alarm bzero fork mkdir sbrk select poll])
AC_CHECK_FUNCS([tcgetattr tcsetattr])

AC_CHECK_DECLS([cfgetospeed], , , [
	#ifdef HAVE_TERMIOS_H
	#include <termios.h>
	#endif
	#ifdef HAVE_TERMIO_H
	#include <termio.h>
	#endif
])
AC_CHECK_MEMBERS([struct termios.c_cflag, struct termios.c_ospeed, struct termios.sg_ospeed], , , [
	#ifdef HAVE_TERMIOS_H
	#include <termios.h>
	#endif
	#ifdef HAVE_TERMIO_H
	#include <termio.h>
	#endif
])

AC_DEFINE_UNQUOTED([AUTO_DATE], ["`date '+$Date: %Y/%m/%d %T $'`"], [Configuration time])

AC_CONFIG_FILES([Makefile doc/Doxyfile])
AC_OUTPUT
