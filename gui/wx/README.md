# DiskShed

DiskShed is a wxWidgets desktop application for browsing and editing OS-9 RBF
and Disk BASIC images. It deliberately builds the existing ToolShed C filesystem
sources without changing the repository's current Makefiles.

The floppy-and-shed application icon is embedded in the macOS application bundle
and Windows executable; Linux installs the matching scalable icon for desktop
launchers and AppImage packaging.

## Build

Install CMake and wxWidgets 3.2 or newer, then run:

```sh
cmake -S gui/wx -B build/wx -DCMAKE_BUILD_TYPE=Release
cmake --build build/wx
```

On macOS with Homebrew, wxWidgets can be installed with:

```sh
brew install wxwidgets
```

On macOS, run `open build/wx/DiskShed.app`. On Windows, run `DiskShed.exe`; on
Linux, run `DiskShed` from the build directory. If wxWidgets is unavailable,
CMake still builds `toolshed-image-smoke`, which exercises the same image adapter
from a terminal:

```sh
build/wx/toolshed-image-smoke path/to/image.dsk
build/wx/toolshed-image-smoke path/to/image.dsk CMDS
build/wx/toolshed-image-smoke path/to/image.dsk --extract CMDS/shell shell
build/wx/toolshed-image-smoke path/to/image.dsk --ident CMDS/shell
```

## Release binaries

The `Build DiskShed` GitHub Actions workflow creates native binaries on their
respective operating systems rather than cross-compiling the GUI. Every branch
and pull request produces downloadable workflow artifacts for:

- macOS on Apple silicon (`arm64`)
- macOS on Intel (`x86_64`)
- Windows (`x86_64`)
- Linux (`x86_64` AppImage)

The macOS and Windows builds link wxWidgets statically. The Linux AppImage
bundles wxGTK and its non-system runtime dependencies. Pushing a tag beginning
with `v` builds all four artifacts, verifies their SHA-256 checksum files, and
attaches them to the corresponding GitHub release. For example:

```sh
git tag v0.1.0
git push upstream v0.1.0
```

Release tags should only be pushed after the version in this CMake project and
the `DISKSHED_VERSION` workflow variable agree. The generated macOS and Windows
packages are currently unsigned; code signing and macOS notarization can be
added when the project has the required certificates and CI secrets.

DiskShed supports opening an image, format detection, nested OS-9 directory
navigation, Back/Up/Refresh commands, native file export, file-size lookup, and
basic Disk BASIC type descriptions. The table adapts to the image format: OS-9
shows Size, Type, Attributes, Owner, and Modified, while Disk BASIC shows Size,
File Type, and Encoding. Double-click a folder to enter it or a file to export
it. Right-click an entry for a context-sensitive menu. Folders offer **Open**,
while files offer **Export**. Files beginning with an OS-9/6809 or OS-9/68K
module signature also offer **Ident**, which displays module metadata and
recognizes files containing multiple concatenated modules. Ordinary text and
data files do not show Ident. Disk BASIC files omit operations that do not apply
to that filesystem.

Opening another image creates another native window, so OS-9 and Disk BASIC
images can be browsed side by side. The first image reuses the initial empty
window. Multiple image files can be selected and dragged out together; folders
in a mixed selection are skipped.

Editing operations include file import, rename, delete, and OS-9 directory
creation. Before the first mutation in an open session, the adapter copies the
original image to a sibling file named `IMAGE.diskshed-backup` (or a numbered
variant if that name already exists). The current image is then edited in place.

Drag and drop works in both directions:

- Drop regular host files from Finder, Explorer, or a Linux file manager onto
  the file list to import them into the currently displayed image directory.
- Drag one or more selected image files from the list onto the native file
  system to export copies.
  The GUI materializes them in a private temporary directory for the duration of
  the app session so the operating system receives an ordinary file URL.

- On macOS, drop disk images (`.dsk`, `.dmk`, `.vdk`, `.sdf`, `.jvc`, `.os9`)
  on DiskShed's icon in the Dock or Finder to open them, whether or not it is
  running. DiskShed also appears under **Open With** for those files, without
  becoming their default application. Each image opens in the empty window if
  there is one, otherwise in a new window.

Host directories are ignored when dropped. Disk BASIC images remain flat and
therefore do not enable **New Folder**.

The mutation adapter also has a small command-line smoke utility:

```sh
build/wx/toolshed-image-edit-smoke image.dsk import host-file IMAGE_DIRECTORY
build/wx/toolshed-image-edit-smoke image.dsk mkdir IMAGE_DIRECTORY NAME
build/wx/toolshed-image-edit-smoke image.dsk rename IMAGE_PATH NEW_NAME
build/wx/toolshed-image-edit-smoke image.dsk delete IMAGE_PATH file
```

An image path may also be passed to the GUI executable on the command line.
