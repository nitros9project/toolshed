# ToolShed graphical applications

This directory contains graphical applications built on ToolShed's filesystem
libraries. These applications are optional and do not replace the command-line
tools or FUSE support.

## DiskShed

[`wx/`](wx/) contains DiskShed, a native cross-platform desktop application
implemented with wxWidgets. It reads and writes both filesystem formats exposed
by ToolShed:

- OS-9 RBF disk images, including nested directory navigation and OS-9 metadata.
- CoCo Disk BASIC images, including file type and ASCII/binary encoding details.

DiskShed supports multiple image windows, native import and export dialogs,
multi-file drag and drop, rename and delete operations, and OS-9 directory
creation. Before modifying an image, it creates a sibling
`IMAGE.diskshed-backup` file.

## Quick build

Install CMake and wxWidgets 3.2 or newer, then run these commands from the
ToolShed repository root:

```sh
cmake -S gui/wx -B build/wx -DCMAKE_BUILD_TYPE=Release
cmake --build build/wx
```

Run the resulting application:

- macOS: `open build/wx/DiskShed.app`
- Windows: `build\wx\DiskShed.exe`
- Linux: `build/wx/DiskShed`

See the [DiskShed README](wx/README.md) for platform prerequisites, detailed
features, command-line image arguments, and adapter smoke-test utilities.

## ToolShed Quick Look

[`quicklook/`](quicklook/) contains a macOS Quick Look extension that shows
the directory of a disk image when it is selected in the Finder and Space is
pressed. It lists OS-9, Disk BASIC, Dragon DOS and FLEX disks with the same
code as the command line tools, and builds with the Command Line Tools alone.
See the [Quick Look README](quicklook/README.md).
