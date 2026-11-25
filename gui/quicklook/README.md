# ToolShed Quick Look

A macOS Quick Look extension that shows what is on a CoCo or Dragon disk
image. Select an image in the Finder and press Space to see:

- the image format and track layout, as `os9 info` reports them, and
- a directory listing for each filesystem the image could hold: OS-9
  (`os9 dir -e -r`, every directory), Disk BASIC (`decb dir`), Dragon DOS
  (`dgndos dir`) and FLEX (`cflex dir`).

The listings come from ToolShed's own code, compiled into the extension, so
they match what the command line tools print.

It previews files ending in `.dsk`, `.dmk`, `.vdk`, `.sdf`, `.jvc` and `.os9`.
An image with another name, or none, isn't recognized by Quick Look. A file
with one of those names that isn't a disk image gets Quick Look's usual
preview.

## Requirements

- macOS 12 or later.
- The Xcode Command Line Tools (`xcode-select --install`). Xcode itself isn't
  needed.

## Building and installing

From this directory:

```sh
make
make check
make install
```

`make install` copies `ToolShedQuickLook.app` to `~/Applications`, registers
it and turns the extension on. Open the app once if previews don't appear; it
only says it is installed. The extension can be turned off or on again in
System Settings > General > Login Items & Extensions > Quick Look.

`make uninstall` removes it.

Options:

- `ARCHS="arm64 x86_64"` builds a universal app. The default is the Mac's own
  architecture.
- `SIGN="Developer ID Application: ..."` signs with that identity. The default
  is an ad hoc signature, which is enough on the Mac that built it.
- `INSTALLDIR=/Applications` installs somewhere else.

## Testing

`make check` runs two sets of tests on the unit tests' images in
`unittest/testdata`:

- `tests/qltest.c` tests the C code that runs ToolShed's listings and
  captures what they print.
- `build/listing` writes the page the extension would show for each image
  named, and the Makefile checks its contents. It is also useful for looking
  at a page in a browser: `build/listing image.dsk > page.html`.

To see what the extension does inside Quick Look:

```sh
qlmanage -p image.dsk
log show --last 5m --info --predicate 'subsystem == "org.nitros9.toolshed.quicklook"'
```

The log has a line for each preview made, or the reason there wasn't one.

## How it works

- `Preview/toolshed_ql.c` runs a command's function (`os9dir()`,
  `decbdir()`, ...) with stdout and stderr pointed at temporary files, and
  returns what was printed. A lock keeps one command at a time, as the file
  descriptors are the whole process's.
- `Preview/PreviewProvider.swift` is the Quick Look extension. It decides
  which listings to show and makes them into an HTML page, with the image's
  full path replaced by its name.
- `App/` is the app that carries the extension and declares the disk image
  type, `org.nitros9.toolshed.disk-image`. It is an imported declaration, so
  an app that declares these extensions itself takes precedence.

ToolShed's pathlists end the image name at the first comma, so an image whose
path has a comma is reached through a link without one.
