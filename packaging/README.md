# Packaging

This directory contains files used to assemble platform-specific distributable packages.

Current layout:

```text
packaging/
  windows/
```

Use one subdirectory per target platform when packaging files exist:

- `packaging/windows/` for NSIS, WiX, or other Windows installer resources.
- `packaging/linux/` for AppImage, Flatpak, distro package specs, or other Linux package resources.
- `packaging/macos/` for bundle, DMG, signing, notarization, or package resources.

Do not put ordinary CMake install rules here. CMake install rules should stay near their targets or in a dedicated CMake module. This directory is for packaging artifacts and platform package definitions.
