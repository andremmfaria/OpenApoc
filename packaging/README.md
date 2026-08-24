# Packaging

This directory contains files used to assemble platform-specific distributable packages.

Current layout:

```text
packaging/
  linux/
  macos/
  windows/
```

Use one subdirectory per target platform when packaging files exist:

- `packaging/windows/` for NSIS, WiX, or other Windows installer resources.
- `packaging/linux/` for the portable Linux tarball launcher and metadata. It can later grow AppImage, Flatpak, distro package specs, or other Linux package resources.
- `packaging/macos/` for bundle, DMG, signing, notarization, or package resources.

## Linux

The Linux package is a portable tarball. It contains the OpenApoc binaries, tracked OpenApoc data, generated extractor output, and a small launcher script that runs the game from the package root.

It does not include original X-COM: Apocalypse game data such as `cd.iso`.

## macOS

The macOS package workflow creates a portable tarball and a DMG when `hdiutil` is available. The package contains the OpenApoc app bundle, tracked OpenApoc data, generated extractor output, and an `openapoc.command` launcher that runs from the package root so the default `./data` path resolves correctly.

It does not include original X-COM: Apocalypse game data such as `cd.iso`.

## Windows

The Windows package workflow creates the portable ZIP, debug symbols ZIP, and NSIS installer from `packaging/windows/installer.nsi`.

## Release Assets

On pushes to `master`, each platform packaging workflow publishes its package files from `dist/` to a GitHub release named for the UTC push date, using a tag such as `2026-08-24`.

If another push happens on the same day, the date tag is force-moved to the current `master` tip and same-named release assets are replaced. A packaging job skips publishing if its commit is no longer the remote `master` tip, so a slower older run cannot move the date tag backwards.

Do not put ordinary CMake install rules here. CMake install rules should stay near their targets or in a dedicated CMake module. This directory is for packaging artifacts and platform package definitions.
