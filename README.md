# Video TECO: Robin Haberkorn's fork

This is a fork of Paul Cantrell's and Joyce Nishinaga's
Video TECO, as first [published on Sourceforge](https://sourceforge.net/projects/videoteco/).
It contains the history of the original CVS repository.

Video TECO is a graphical interactive dialect of the
Text Editor and Corrector (TECO) for UNIX and VMS.

This fork has the following extensions compared to the last
version published by Paul Cantrell in 2007
([v6.4](https://github.com/rhaberkorn/videoteco-fork/releases/tag/v6.4)):

* Ported to [real-mode MS-DOS](#ms-dos-port)!
* Fixed file backups.
* Avoid warnings and compatibility tweaks for newer C compilers (GCC/Clang) and current
  versions of Linux and FreeBSD.
  Actually, this fork currently requires some C99 feautures - but this might be reversed.
* Fixed possible memory corruptions.
* Reduced memory usage.
* `EC` command can be used both to insert stdout at the current buffer
  position and pipe portions of the buffer through the external process.
  The semantics are [SciTECO-compatible](https://rhaberkorn.github.io/sciteco/sciteco.7.html#Execute%20operating%20system%20command%20and%20filter%20buffer%20contents)
  (but it was first implemented in this fork).
  There is also a dumb non-UNIX implementation that works with temporary
  files, that's currently only used for the MS-DOS port.
* Control characters are echoed in reverse video just like by default in SciTECO.
* Fixed echoing control characters on the end of lines.
  Or was it broken only at the end of the document?
  Anyway, it's fixed now.
* Restore screen on exit when building for terminfo,
  which is the new default.
* Code documentation using [Doxygen](https://doxygen.nl/).
  Build with `make devdoc`.
* Rewrote the Autotools build system.
* Added the [original manual](https://www.copters.com/teco.html) to the repository
  (`doc/TECO Manual V4.html`).

Video TECO is the main inspiration of [SciTECO](https://rhaberkorn.github.io/sciteco) and most
development and innovation nowadays takes place there.
Video TECO still remains interesting for legacy and space constrained systems as it has even lower
system requirements and almost no external dependencies (except for terminfo/termcap).
Also, Video TECO supports [split screen modes](https://www.copters.com/teco.html#RTFToC132),
that will probably never be added to SciTECO.
Any new features will be SciTECO-compatible if possible.

![Split screen mode](doc/TECO%20Manual%20V4_files/teco_EP.gif)

The original README can be found in [README.OLD](README.OLD) and contains
`~/.teco_ini` examples.

## MS-DOS port

This fork also compiles for real-mode 16-bit MS-DOS - in fact it will even work on 8088/8086
processors.
It has been tested on MS-DOS 6.2, [FreeDOS](https://www.freedos.org/) 1.4 and
[SvarDOS](http://svardos.org/) (DR-DOS).
The port contains Long File Name (LFN) support and should also be useful
on Win9x (Windows 95-Me) systems.
Binaries can be downloaded in the [Releases section](https://github.com/rhaberkorn/videoteco-fork/releases).

It currently only compiles with the [Open Watcom v1.9](https://github.com/open-watcom/open-watcom-1.9)
compiler.
It can be compiled both from DOS and Linux hosts if the environment is
set up correctly (`. owsetenv.sh`):

    wmake -f Makefile.wcc

### Features and limitations

* Being ported from UNIX, it outputs escape sequences and requires an ANSI driver like
  [ANSI.SYS](https://en.wikipedia.org/wiki/ANSI.SYS),
  [NANSI.SYS](http://www.kegel.com/nansi/) or
  [ANSIPLUS](http://www.sweger.com/ansiplus/).
  ANSIPLUS is detected automatically, which enables some optimizations.
  To enable NANSI.SYS-specific optimizations, try `set TECO_TERM=nansi.sys`.
  You can also set the TERMCAP environment variable to a custom termcap definition file.
* The console size/resolution is automatically detected.
* It supports the same Csh-like wildcard expansions as on UNIX for
  command-line parameters.
* DOS linebreaks (CRLF) are automatically normalized to LF in the buffer unless
  you specify the `-8` command-line parameter.
  This is similar in spirit to but much more primitive than
  SciTECO's [automatic EOL translation](https://rhaberkorn.github.io/sciteco/sciteco.7.html#BUFFER%20RING).
  It relies on Watcom libc features.
* The initialization file is called TECO.INI and should be located in the same directory as TECO.EXE.
  If Video TECO fails to locate it, you can define the `VTECO` environment variable to
  point to the installation directory (e.g. `set VTECO=C:\VTECO`).
  See [README.OLD](README.OLD) for macro examples.
* [Open Watcom v2.0](https://github.com/open-watcom/open-watcom-v2) is not yet supported -
  generated binaries are still broken.
  The v2.0 version is obviously not mature enough.
* Programs executed via `EC` should not print to stderr as it will confuse the
  display.
  This is hard to prevent though on DOS.
* For a real-mode DOS application the binary is relatively large.
  There are of course much smaller editors written in assembly.

### TODO

- [ ] Make use of ANSIPLUS scroll regions.
- [ ] Re-evaluate Video TECO's lookaside buffers.
      But they probably don't make sense considering the memory constraints.
- [ ] Dosbox has problems with drawing the command line cursor.
      Obviously its ANSI.SYS emulation attaches attributes to character cells,
      so a moving reverse `' '` leaks the attribute to the preceding or following
      cell.
      It also cannot deal with more than 25 rows.
      Interestingly, this persists even after loading ANSIPLUS.
      But perhaps it's not so important to support Dosbox.
- [ ] ommand line help (`-h`)
- [ ] SvarDOS package
