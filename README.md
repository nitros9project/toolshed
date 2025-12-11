# ToolShed

ToolShed is a cornucopia of tools and source code for the Tandy Color Computer and Dragon micro.

The repository contains:
- os9 and decb tools for copying files to/from host file systems to disk images.
- source code for CoCo and Dragon system ROMs.
- source code for custom ROMs like HDB-DOS, DriveWire DOS, and SuperDOS.
- source code for the Microware C compiler for cross-hosted compilation (currently needs work).
- other miscellaneous tools.
- assemblers to perform cross-development from Windows, Linux, and macOS (see the NOTE below on assembler recommendations).

**NOTE:** while the venerable 6809 cross-assembler, mamou, is part of the repository, it is only kept for historical value. Everyone should really be using William Astle's excellent LWTOOLS which contains the *lwasm* 6809 assembler and *lwlink* linker. [Download the latest version of the source here.](http://lwtools.projects.l-w.ca)

Find the documentation for some of the command in Toolshed's [Wiki](https://github.com/nitros9project/toolshed/wiki)
## Building on Windows

The recommended build environment is [MingW32 or MingW64](http://mingw.org/), [MSYS2](http://msys2.github.io/), or the [WSL subsystem for Windows 10+](https://en.wikipedia.org/wiki/Windows_Subsystem_for_Linux).

The easiest way to install MingW is using a mingw-get-inst.*.exe [from here](http://mingw.org/wiki/Getting_Started).

Inside MingW, make sure you have "make" installed. There are several options, but the simpler mingw-make should be good enough:
```
$ mingw-get install mingw-make
```

Enter the unpackaged toolshed directory and run:
```
$ make -C build/unix install CC=gcc
```

## Building on Linux

To build cocofuse you will need to have FUSE libraries and header files installed. On Debian-based systems:
```
$ sudo apt-get install libfuse-dev
```

Enter the unpackaged toolshed directory and run:
```
$ make -C build/unix install
```

## Building on macOS

To build cocofuse for the Mac, you will need to have FUSE libraries and header files installed.

The best way to do this is to first [visit the Homebrew page](https://brew.sh) and use the simple one-line ruby command to install Homebrew on your Mac.

Currently the makefile for cocofuse requires fuse-t on macos. Historically we used to use macfuse, and osxfuse.

```
brew install fuse-t
```

If you have previously install osxfuse, you can use brew command to uninstall `oxsfuse` and install `fuse-t`

```
brew uninstall osxfuse
brew uninstall macfuse
brew install fuse-t
```

Then, enter the unpackaged toolshed directory and run:
```
$ make -C build/unix install
```

## Building HDB-DOS and DriveWire DOS

It is highly recommended to have [LWTOOLS](http://lwtools.projects.l-w.ca/) installed.  You will also need "makewav" from ToolShed installed to build WAV files.  See hdbdos/README.txt and the makefiles for different build options.

To build all default flavors:
```
$ make -C dwdos
$ make -C hdbdos
$ make -C superdos
```

Instead of LWTOOLS the deprecated mamou can still be used (YMMV):
```
$ make -C dwdos AS="mamou -r -q"
$ make -C hdbdos AS="mamou -r -q"
```

Note that SuperDOS still builds with mamou by default.
