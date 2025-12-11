
ToolShed is a package of utilities to perform cross-development from Windows,
Linux or Mac OS X computers to the Tandy Color Computer and Dragon
microcomputers.

https://github.com/nitros9project/toolshed


== HOW TO BUILD ON WINDOWS ==

The recommended build environment is MingW32 or MingW64 (http://mingw.org/),
MSYS2 (http://msys2.github.io/), or the WSL subsystem (for Windows 10+)
(https://en.wikipedia.org/wiki/Windows_Subsystem_for_Linux).

The easiest way to install MingW is using a mingw-get-inst.*.exe from
http://mingw.org/wiki/Getting_Started

Inside MingW, make sure you have "make" installed. There are several options,
but the simpler mingw-make should be good enough:
$ mingw-get install mingw-make

Enter the unpackaged toolshed directory and run:
$ make -C build/unix install CC=gcc

== HOW TO BUILD FOR WINDOWS ON UNIX or WSL ==

$ NOTEST=1 make -C build/unix CC="x86_64-w64-mingw32-gcc -Wl,--force-exe-suffix"

== HOW TO BUILD ON UNIX ==

To build cocofuse (LINUX only) you will need to have FUSE libraries and
header files installed. On Debian-based systems:
$ sudo apt-get install libfuse-dev

Enter the unpackaged toolshed directory and run:
$ make -C build/unix install


== HOW TO BUILD hdbdos and dwdos ==

It is recommended to have lwtools installed (http://lwtools.projects.l-w.ca/).
You will also need "makewav" from Toolshed installed to build WAV files.
See hdbdos/README.txt and the makefiles for different build options.

To build all default flavors:
$ make -C dwdos
$ make -C hdbdos
$ make -C superdos

Instead of lwtools the deprecated mamou can still be used (YMMV):
$ make -C dwdos AS="mamou -r -q"
$ make -C hdbdos AS="mamou -r -q"
Note that superdos still builds with mamou by default.
