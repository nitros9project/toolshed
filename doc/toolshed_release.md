# Releasing Toolshed

This document describes how to create a new release of toolshed.

New releases use the tag format:

```
v2.5
```

(Old `v2_5` format is deprecated.)

---

## 1. Pick the Version

Choose the version:

- `2.5` → minor release  
- `2.5.1` → patch release  

---

## 2. Update Version (ALL LOCATIONS)

### A. Build System (authoritative)

File:

```
build/unix/rules.mak
```

Line 3:

```
edit build/unix/rules.mak:0
VERSION = 2.5
```

---

### B. Main Documentation

File:

```
edit doc/ToolShed.md:0
```

Line 1:

```
ToolShed 2.5
```

---

### C. README

File:

```
README.md
```

Line 3:

Update version reference accordingly (example):

```
edit README.md:3
Current version: 2.5
```

---

### D. Sanity Check

```
grep -R "2\.4" .
```

---

### E. Add release notes to changelog.md

```
edit doc/changelog.md:0
```

---

## 3. Commit Changes

```
git add build/unix/rules.mak doc/ToolShed.md README.md
git commit -m "Release 2.5"
git push upstream main
```

---

## 4. Tag the Release

```
git tag -a v2.6.0 -m "Toolshed 2.6.0"
```

Do NOT use old underscore format.

---

## 5. Push

```
git push upstream v2.6.0
```

---

## 6. Create GitHub Release

- Tag: `v2.5`  
- Title: `Toolshed 2.5`  

Example:

```
## Toolshed 2.5

### Changes
- Bug fixes
- Improvements
```

---

## 7. Update Homebrew Formula

Update:

```
curl -O "https://github.com/nitros9project/toolshed/archive/refs/tags/v2.5.tar.gz"
curl -O "https://codeload.github.com/nitros9project/toolshed/archive/refs/tags/v2.5.1.tar.gz"
curl -O "https://github.com/nitros9project/toolshed/archive/refs/tags/v2.5.1.tar.gz"
curl -O "https://codeload.github.com/nitros9project/toolshed/tar.gz/refs/tags/v2.5.1"
sha256 "<NEW_SHA256>"
```

---

### Generate SHA256

```
curl -L -o toolshed-2.5.tar.gz \
  https://github.com/nitros9project/toolshed/archive/refs/tags/v2.5.tar.gz

shasum -a 256 toolshed-2.5.tar.gz
```

---

### Test

```
brew install --build-from-source ./toolshed.rb
```

---

### Commit

```
git add toolshed.rb
git commit -m "toolshed v2.5"
git push
```

---

## 8. Verify

```
brew update
brew upgrade toolshed
toolshed --version
```

Expected:

```
2.5
```

---

## 9. Tag Format Policy

- Old: `v2_4`
- New: `v2.5`

Do not modify old tags.

---

## Quick Checklist

- [ ] Update `build/unix/rules.mak` (line 3)
- [ ] Update `doc/ToolShed.md` (line 1)
- [ ] Update `README.md` (line 3)
- [ ] Commit changes
- [ ] Tag `vX.Y`
- [ ] Push tag
- [ ] Create GitHub release
- [ ] Update Homebrew formula
- [ ] Verify install
