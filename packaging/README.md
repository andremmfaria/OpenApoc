# Packaging

This directory contains files used to assemble platform-specific distributable packages.

Current layout:

```text
packaging/
  linux/
  windows/
```

Use one subdirectory per target platform when packaging files exist:

- `packaging/windows/` for NSIS, WiX, or other Windows installer resources.
- `packaging/linux/` for the portable Linux tarball launcher and metadata. It can later grow AppImage, Flatpak, distro package specs, or other Linux package resources.
- `packaging/macos/` for bundle, DMG, signing, notarization, or package resources.

## Linux

The Linux package is a portable tarball. It contains the OpenApoc binaries, tracked OpenApoc data, generated extractor output, and a small launcher script that runs the game from the package root.

It does not include original X-COM: Apocalypse game data such as `cd.iso`.

## Windows

The Windows package workflow creates the portable ZIP, debug symbols ZIP, and NSIS installer from `packaging/windows/installer.nsi`.

Do not put ordinary CMake install rules here. CMake install rules should stay near their targets or in a dedicated CMake module. This directory is for packaging artifacts and platform package definitions.
