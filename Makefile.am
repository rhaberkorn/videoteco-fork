# NOTE: There are currently only a few language features like bitfields,
# which require C99.
AM_CFLAGS = -std=c99 -Wall -Wsign-compare

# OpenWatcom build system (for MS-DOS port)
EXTRA_DIST = Makefile.wcc

EXTRA_DIST += README.md README.OLD

bin_PROGRAMS = teco
teco_SOURCES = tecbuf.c teccmd.c tecdebug.c tecdisp.c tecexec.c \
               tecmem.c teco.c teco.h tecparse.c tecparse.h \
               tecstate.c tecterm.c tecundo.c tecvms.h

#
# Doxygen processing (do not install or distribute)
#
devdoc : doc/html/

.PHONY: doc/html/
doc/html/ : doc/Doxyfile
	-rm -rf doc/html/
	@DOXYGEN@ doc/Doxyfile

clean-local:
	-rm -rf doc/html/
