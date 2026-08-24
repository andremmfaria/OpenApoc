# Local Builds

These scripts run OpenApoc builds directly on the local platform. They reuse the Python helpers under `.github/scripts/` so local and CI builds stay close without duplicating build logic.

Layout:

```text
scripts/local/
  linux/
  mac/
  windows/
```

Each script checks for the basic tools it needs before starting. It does not install packages by default. The first argument is a target.

Common targets:

- `deps`: install or prepare platform dependencies
- `minimal-cd`: download the minimal CI `cd.iso` if `data/cd.iso` is missing
- `configure`: configure CMake
- `build`: build the configured tree
- `test`: run CTest
- `package`: create platform package artifacts
- `lint`: run the formatting lint target
- `all`: run the usual local build path

## Linux

```sh
scripts/local/linux/run.sh deps
scripts/local/linux/run.sh all
scripts/local/linux/run.sh build
scripts/local/linux/run.sh test
scripts/local/linux/run.sh package
```

The `deps` target uses the same apt-based dependency installer used by the Linux GitHub workflow.

## mac

```sh
scripts/local/mac/run.sh deps
scripts/local/mac/run.sh all
scripts/local/mac/run.sh build
scripts/local/mac/run.sh test
scripts/local/mac/run.sh package
```

The `deps` target uses the macOS package helper, which installs Homebrew packages used by the macOS GitHub workflow.

## Windows

Run from a Visual Studio Developer PowerShell or another shell where `cl.exe` is available:

```powershell
scripts/local/windows/run.ps1 deps
scripts/local/windows/run.ps1 all
scripts/local/windows/run.ps1 build
scripts/local/windows/run.ps1 test
scripts/local/windows/run.ps1 package
```

The Windows `deps` target bootstraps vcpkg into `%USERPROFILE%\.openapoc\vcpkg` by default and installs dependencies into the local build directory. The `package` target creates the portable ZIP. If NSIS is installed, it also creates the installer.
