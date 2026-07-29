# ToolShed Changelog

## [v2.6.0] - 2026-07-29

### 🆕 New Features & Utilities

* **DiskShed Graphical Disk Image Editor**: Introduced **DiskShed**, a new native GUI application for managing and inspecting disk images interactively.
  * Added contextual OS-9 module identification and `Ident` display for OS-9 modules.
  * Added native application icons, improved macOS compatibility, and optimized Windows builds with static wxWidgets caching.
  * Included complete documentation and CI workflows for prebuilt binaries across supported platforms.
* **`os9 reveal`**: Added a new command to inspect and explain OS-9 RBF disk image contents at specific LSNs or byte offsets (`#77`).
  * Translates LSNs, byte offsets, and directory structures into clear, human-readable descriptions.
  * Traverses active directory trees, identifies LSN0/bitmap/file descriptor locations, and reports deleted file remnants or free space.
  * Detects and reports filesystem corruption, truncated images, and invalid sector references.
* **`decb binbust`**: Added a new utility to strip DECB binary record headers and trailers (`#62`).
  * Concatenates multi-record DECB binaries into contiguous raw binary files (ideal for ROM extraction and disassembly).
  * Validates record headers/trailers and operates natively across host, DECB, OS-9, and CECB paths.
* **`cecb dumpblock`**: Added a new `cecb` subcommand to dump raw cassette block structures with terminal-aware color formatting (`#64`).

---

### ⚡ Enhancements & Updates

* **`os9 gen` Refactoring**: Major update to support copying boot structures directly from existing OS-9 disk images (`#71`).
  * Added support for embedding kernel track (`-t`) and `OS9Boot` (`-b`) components sourced directly from existing disk images.
  * Centralized boot track LSN calculation via a unified `get_boottrack_lsn()` helper.
  * Added strict image validation, file position tracking in `_os9_seek()`, and updated manual documentation.
* **`os9 copy` Attributes**: Added the `-a` flag to assign OS-9 file attributes directly during copy operations (e.g., `-a=erpepr`) without needing a secondary `os9 attr` step (`#60`).
* **Reproducible Builds**: Integrated standard `SOURCE_DATE_EPOCH` environment variable support across `libcoco`, `librbf`, and `libtoolshed` (`#76`).
  * Ensures deterministic file creation and modification timestamps using `gmtime()`.
  * Improved OS-9 2-digit year handling, `mktime()`/`localtime()` null-checks, and error reporting.
* **`os9 makdir`**: Improved handling when creating existing directories—reports existence gracefully without returning a hard error (`#70`).
* **`cecb` Improvements**:
  * `cecb copy`: Enforced WAV extension for bulk erase, stripped DECB headers when copying binary BASIC, defaulted copy destinations to end-of-file, and improved CLI help text (`#67`).
  * `cecb dir`: Resolved issue where directory listings stopped after reading the first file (`#65`, `#67`).

---

### 🛠️ Bug Fixes & Infrastructure Updates

* **Core Library & Memory Fixes**:
  * Fixed a heap use-after-free issue in `qDeleteLastNode` (`#73`).
  * Fixed uninitialized variable and checksum error handling in `_cecb_read_next_block` and `_cecb_read_next_dir_entry` (`#67`).
* **Build System & Tooling**:
  * Honored system `LDFLAGS` across Makefiles and ensured proper trailing newlines (`#73`, `#74`).
  * Updated `cocoroms` scripts to dynamically detect available MD5 tools (`md5` vs. `md5sum`) across BSD, macOS, and Linux (`#59`).
* **CI & Release Automation**:
  * Adjusted Windows CI builds to link runtime statically (`#75`).
  * Upgraded GitHub Actions dependencies (`actions/checkout` to v5, `upload-artifact` to v7, and Node.js runtime to v24).



## [v2.5.1] - 2026-05-03

### New Features
- Added automatic Makefile header and library dependency generation ([#57](https://github.com/nitros9project/toolshed/pull/57))
- Added automatic dependency support to library Makefiles ([#56](https://github.com/nitros9project/toolshed/pull/56))
- Prepared Homebrew tap support ([#58](https://github.com/nitros9project/toolshed/pull/58))

### Bug Fixes
- Fixed token bug ([#55](https://github.com/nitros9project/toolshed/pull/55))
- Fixed metadata copy when converting OS-9 to Native format ([#54](https://github.com/nitros9project/toolshed/pull/54))

---

## [v2.5] - 2026-04-18

### New Features
- Revised WAV internals: improved leader command output, implemented dead zone, added bit depth support (24-bit writing), increased delay compensation between blocks ([#45](https://github.com/nitros9project/toolshed/pull/45))
- Changed DECB COPY to use stream load data ([#53](https://github.com/nitros9project/toolshed/pull/53))

### Bug Fixes
- Revised structure of `DD.OPT` to work for both 6809 and 68000 architectures, enabling `os9 format` to create disk images matching MM/1 bootable floppy LSN 0 layout ([#49](https://github.com/nitros9project/toolshed/pull/49))
- Fixed const conversion errors with GCC 15 (`strchr`/`strrchr` now const-correct) ([#51](https://github.com/nitros9project/toolshed/pull/51))
- Fixed `cocofuse` to link against `fuse-t` on macOS ([#45](https://github.com/nitros9project/toolshed/pull/45))

---

## [v2.4.2] - 2025-09-10

### New Features
- Added partial `_cecb_seek()` to get `cecb dump` working ([#39](https://github.com/nitros9project/toolshed/pull/39))

### Bug Fixes
- Fixed `cecb dump` ([#39](https://github.com/nitros9project/toolshed/pull/39))
- Consistently call `_coco_close()` in `do_os9gen()` ([#48](https://github.com/nitros9project/toolshed/pull/48))
- Added `-iX` interleave option to `os9 format`; default is 3 for backwards compatibility ([#47](https://github.com/nitros9project/toolshed/pull/47))
- Accommodated OS-9/68000 releases accepting either explicit boot file length/LSN or zero length/LSN of the file descriptor ([#46](https://github.com/nitros9project/toolshed/pull/46))

---

## [v2.4.1] - 2025-05-29

### Bug Fixes
- Fixed Track 0 sector size when formatting hard drive images — it should not differ from other tracks ([#29](https://github.com/nitros9project/toolshed/pull/29), [#34](https://github.com/nitros9project/toolshed/pull/34)); see also [issue #28](https://github.com/nitros9project/toolshed/issues/28)
- Refactored `sectorsTrack0` variable handling; added unit test ([#34](https://github.com/nitros9project/toolshed/pull/34))
- Fixed `modbust` so it works on native files, not just OS-9 ones ([#21](https://github.com/nitros9project/toolshed/pull/21))
- Dropped quotes from make release workflow ([#36](https://github.com/nitros9project/toolshed/pull/36))

### Build
- Added new build targets and updated CI workflows ([#33](https://github.com/nitros9project/toolshed/pull/33))
- Renamed Unix package name for clarity ([#32](https://github.com/nitros9project/toolshed/pull/32))
- Included `cocofuse` for Unix builds ([#30](https://github.com/nitros9project/toolshed/pull/30))
- Added GitHub automated build support ([#25](https://github.com/nitros9project/toolshed/pull/25), [#27](https://github.com/nitros9project/toolshed/pull/27))

---

## [v2.4] - 2025-05-25

### New Features
- Added `lst2cmt` command: converts an lwasm assembly listing to a MAME-compatible comment file ([#12](https://github.com/nitros9project/toolshed/pull/12))

### Bug Fixes
- Improved BASIC tokenizer; added token end-of-line zero handling ([#11](https://github.com/nitros9project/toolshed/pull/11))
- `padrom` now returns an error code if the pad size is insufficient ([#13](https://github.com/nitros9project/toolshed/pull/13))
- Patched `os9 format` to support disks with a different number of sectors on track zero, head zero (required for Dragon Beta/128 OS-9 L2 disks) ([#14](https://github.com/nitros9project/toolshed/pull/14))
- Freed allocated buffers during `decb copy` and `cecb copy` on error ([#15](https://github.com/nitros9project/toolshed/pull/15))
- Fixed `librbf` buffer leaks: bitmap, lsn0, filename, and file descriptor freed on error ([#16](https://github.com/nitros9project/toolshed/pull/16))
- Improved bounds checking during BASIC tokenisation using `tok_strncmp()` ([#17](https://github.com/nitros9project/toolshed/pull/17))
- Fixed `libcecb` resource leaks: `_native_close()` on early returns, freed `buffer_1200`/`buffer_2400` ([#18](https://github.com/nitros9project/toolshed/pull/18))
- Added `ASSERT_MEM_EQUALS` macro to unit tests ([#19](https://github.com/nitros9project/toolshed/pull/19))
- Fixed `os9/os9gen.c` to call `_os9_close()` on early return ([#20](https://github.com/nitros9project/toolshed/pull/20))
- Removed win32 Makefiles ([#22](https://github.com/nitros9project/toolshed/pull/22))
- Changed `stpcpy` to `strcpy` for better cross-platform compatibility ([#23](https://github.com/nitros9project/toolshed/pull/23))
- Fixed assert in testing suite for MinGW64 ([#24](https://github.com/nitros9project/toolshed/pull/24))
- Fixed C23/GCC 15 build support by prototyping function pointers ([#38](https://github.com/nitros9project/toolshed/pull/38))

---

## [v2.3 / Pre-v2.4 era] - 2024 – early 2025

### New Features
- Added `cecb` support for C10 and MC10 tokenization, plus `cecb` tests and documentation
- Added detokenization option to `decb dsave`
- Added Disk Basic binary decoder to `decb dump`
- Added `os9dump` C format output option
- Added `dis68` — a 68000 disassembler for OSK by Dan Poole and Carl Kreider
- Added `os9modbust` support for OS-9/68K modules
- `os9 dir` now escapes non-printing characters in filenames
- `os9 ident` now reports the correctly calculated CRC on mismatch
- `os9dcheck` warns about file segments spanning multiple allocation map sectors
- `os9dir` adds recursion detection
- `os9fstat` displays file size in hex as well as decimal
- `makewav`: added optional block grouping with pause + leader between groups (Bret Gordon)

### Bug Fixes
- Fixed `librbf` use-after-free in `_os9_freefile` (caused crash on macOS due to division by zero); see [SourceForge bug #52](https://sourceforge.net/p/toolshed/bugs/52/)
- Fixed HDBDOS sources to work with recent LWASM changes to RMB and re-ORG statements ([#10](https://github.com/nitros9project/toolshed/pull/10))
- Fixed `dwinit` code placement in `dw3dos` (must occur before padding) ([#43](https://github.com/nitros9project/toolshed/pull/43))
- Fixed short seek adjustment in `librbf` RBF (unnecessary for raw images) ([#8](https://github.com/nitros9project/toolshed/pull/8))
- Removed `strncpy` in `libcecb` when filling in the filename buffer ([#7](https://github.com/nitros9project/toolshed/pull/7))
- Fixed `librbf` not creating file segments spanning multiple allocation map sectors; see [NitrOS-9 bug #36](https://sourceforge.net/p/nitros9/bugs/36/)
- Fixed off-by-one when checking for full segment list in `librbf`
- Fixed `librbf` going beyond end of disk when searching for free clusters
- Fixed `libtoolshed` modifying source buffer in `Native2Coco`/`Coco2Native`
- Fixed corner-case EOL detection for `os9 copy -l` when CR-NL is split across buffer chunks; see [SourceForge bug #51](https://sourceforge.net/p/toolshed/bugs/51/)
- Fixed HDB-DOS sources to avoid RMB for addresses outside the code; see [SourceForge bug #50](https://sourceforge.net/p/toolshed/bugs/50/)
- Fixed decb image corruption for HDB-DOS virtual floppy images (granule limit enforced); see [SourceForge bug #39](https://sourceforge.net/p/toolshed/bugs/39/)
- Fixed `libdecb` infinite loop on HDB-DOS floppy image read beyond file limits
- Fixed `libdecb` path parsing for colon-delimited HDB-DOS paths
- Fixed `os9 dsave` stack overflow on Linux ext2/3/4 (`.` and `..` entries now skipped)
- Fixed `os9 dsave` shell quoting
- Fixed `libnative` opening file in append mode instead of write mode for `FAM_WRITE`
- Fixed `librbfdelete` double free; see [SourceForge bug #37](https://sourceforge.net/p/toolshed/bugs/37/)
- Fixed `os9 deldir` to correctly call `_os9_delete_directory()`
- Fixed `cecb` question mark token
- Fixed tokenizing the question mark abbreviation in DECB
- Fixed `os9 list` double free on failed file open
- Fixed `ar2`, `mamou`, `dis68` incomplete prototypes (clang 14 compatibility)
- Fixed unused variables in `os9dsave`, `cocodsave`, `os9dir`, `libdecbsrec` breaking clang 14 build
- Fixed `libdecb` to print a warning if an unusual postamble is encountered
- Fixed `cocofuse` pointer and offset size casts for 32-bit and 64-bit portability
- Fixed `mamou` string length overflows
- Fixed `librbfformat` to report calculated cluster size back to caller
- Fixed build on Cygwin and 64-bit MinGW
- Used `unsigned char` with `isdigit`/`toupper` per C standard requirements
- Fixed `makewav` Cygwin build detection
- Fixed `os9format` cluster size for large capacity HDD images
- Revamped DECB FAT handling; changed libdecb allocation algorithm to mimic ROM behavior
- Fixed `os9 format` to use `-h` for help; see [SourceForge bug #15](https://sourceforge.net/p/toolshed/bugs/15/)
- Fixed `cocoroms`: all ROM images now match original checksums
- Fixed `librbfformat` to fill sectors with `$E5` instead of zeroes

---

## [v2.2] - 2017-09-09

- Initial tagged release on GitHub migration from SourceForge.
