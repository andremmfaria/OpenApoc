#!/usr/bin/env python3
"""Packaging helpers for OpenApoc GitHub Actions."""

import argparse
import shutil
import stat
import subprocess
import sys
import tarfile
import zipfile
from pathlib import Path


RUNTIME_BINARIES = [
    "OpenApoc",
    "OpenApoc.exe",
    "OpenApoc_Launcher",
    "OpenApoc_Launcher.exe",
]

DATA_EXCLUDES = {"cd.iso", "XCOM.BIN", "XCOM.bin"}


def run(command: list[str], *, cwd: Path | None = None) -> None:
    subprocess.run(command, cwd=cwd, check=True)


def copytree(source: Path, target: Path, *, ignore_names: set[str] | None = None) -> None:
    if not source.exists():
        return
    ignore_names = ignore_names or set()
    for path in source.rglob("*"):
        relative = path.relative_to(source)
        if any(part in ignore_names for part in relative.parts):
            continue
        destination = target / relative
        if path.is_dir():
            destination.mkdir(parents=True, exist_ok=True)
        else:
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, destination)


def copy_tracked_data(workspace: Path, target: Path) -> None:
    result = subprocess.check_output(["git", "ls-files", "data"], cwd=workspace, text=True)
    for filename in result.splitlines():
        source = workspace / filename
        relative = Path(filename).relative_to("data")
        if any(part in DATA_EXCLUDES for part in relative.parts):
            continue
        destination = target / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)


def copy_linux_runtime(build_dir: Path, target: Path) -> None:
    bin_dir = build_dir / "bin"
    target_bin = target / "bin"
    target_bin.mkdir(parents=True, exist_ok=True)
    for name in RUNTIME_BINARIES:
        source = bin_dir / name
        if source.exists():
            shutil.copy2(source, target_bin / source.name)
    for source in bin_dir.glob("*.dll"):
        shutil.copy2(source, target_bin / source.name)


def copy_windows_runtime(build_dir: Path, target: Path) -> None:
    bin_dir = build_dir / "bin"
    for source in bin_dir.iterdir():
        if source.is_file():
            shutil.copy2(source, target / source.name)


def copy_common_files(workspace: Path, package_root: Path, version: str, commit: str) -> None:
    for source_name, destination_name in [
        ("README.md", "README.txt"),
        ("README_HOTKEYS.md", "README_HOTKEYS.md"),
        ("portable.txt", "portable.txt"),
    ]:
        source = workspace / source_name
        if source.exists():
            shutil.copy2(source, package_root / destination_name)

    (package_root / "build-id").write_text(f"{version}\n", encoding="utf-8")
    (package_root / "git-commit").write_text(f"{commit}\n", encoding="utf-8")


def package_version(workspace: Path, explicit: str | None = None) -> str:
    return explicit or subprocess.check_output(
        ["git", "describe", "--tags", "--long", "--always"],
        cwd=workspace,
        text=True,
    ).strip()


def package_commit(workspace: Path, explicit: str | None = None) -> str:
    return explicit or subprocess.check_output(
        ["git", "rev-parse", "HEAD"],
        cwd=workspace,
        text=True,
    ).strip()


def prepare_root(package_root: Path) -> Path:
    if package_root.exists():
        shutil.rmtree(package_root)
    package_root.mkdir(parents=True)
    return package_root


def copy_package_data(workspace: Path, build_dir: Path, package_root: Path) -> None:
    copy_tracked_data(workspace, package_root / "data")
    copytree(build_dir / "data", package_root / "data")


def make_tar(source: Path, target: Path) -> None:
    if target.exists():
        target.unlink()
    with tarfile.open(target, "w:gz") as archive:
        archive.add(source, arcname=source.name)


def make_zip(source: Path, target: Path) -> None:
    if target.exists():
        target.unlink()
    with zipfile.ZipFile(target, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in source.rglob("*"):
            archive.write(path, path.relative_to(source.parent))


def linux(args: argparse.Namespace) -> int:
    version = package_version(args.workspace, args.version)
    commit = package_commit(args.workspace, args.commit)
    package_root = prepare_root(args.dist_dir / f"OpenApoc-linux-{version}")

    copy_linux_runtime(args.build_dir, package_root)
    copy_package_data(args.workspace, args.build_dir, package_root)
    copy_common_files(args.workspace, package_root, version, commit)

    launcher = package_root / "openapoc"
    shutil.copy2(args.workspace / "packaging/linux/openapoc.sh", launcher)
    launcher.chmod(launcher.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    desktop_dir = package_root / "share/applications"
    desktop_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(args.workspace / "packaging/linux/openapoc.desktop", desktop_dir / "openapoc.desktop")

    archive = args.dist_dir / f"{package_root.name}.tar.gz"
    make_tar(package_root, archive)
    print(archive)
    return 0


def windows_tools(_: argparse.Namespace) -> int:
    run(["choco", "install", "nsis", "ninja", "-y", "--no-progress"])
    return 0


def windows_vcpkg(args: argparse.Namespace) -> int:
    if not args.root.exists():
        run(["git", "clone", "https://github.com/microsoft/vcpkg.git", str(args.root)])
    run(["git", "fetch", "--tags"], cwd=args.root)
    run(["git", "checkout", args.ref], cwd=args.root)
    run(["cmd", "/c", str(args.root / "bootstrap-vcpkg.bat"), "-disableMetrics"], cwd=args.root)
    command = [str(args.root / "vcpkg.exe"), "install"]
    if args.install_root:
        command.append(f"--x-install-root={args.install_root}")
    run(command, cwd=args.workspace)
    return 0


def debug_symbols(build_dir: Path, dist_dir: Path, version: str) -> Path | None:
    pdb_files = list((build_dir / "bin").glob("*.pdb"))
    if not pdb_files:
        return None
    archive = dist_dir / f"OpenApoc-debug-windows-{version}.zip"
    if archive.exists():
        archive.unlink()
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zip_file:
        for path in pdb_files:
            zip_file.write(path, path.name)
    return archive


def find_windeployqt(args: argparse.Namespace) -> Path | None:
    candidates = []
    if args.windeployqt:
        candidates.append(args.windeployqt)
    candidates.extend(args.build_dir.glob("vcpkg_installed/**/windeployqt.exe"))
    if args.vcpkg_root:
        candidates.extend(args.vcpkg_root.glob("installed/**/windeployqt.exe"))
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return None


def strip_windows_dev_files(package_root: Path) -> None:
    for suffix in [".pdb", ".exp", ".ilk", ".lib"]:
        for path in package_root.rglob(f"*{suffix}"):
            path.unlink()


def windows(args: argparse.Namespace) -> int:
    version = package_version(args.workspace, args.version)
    commit = package_commit(args.workspace, args.commit)
    package_root = prepare_root(args.workspace / f"OpenApoc-{version}")

    copy_windows_runtime(args.build_dir, package_root)
    copy_package_data(args.workspace, args.build_dir, package_root)
    copy_common_files(args.workspace, package_root, version, commit)

    debug_archive = debug_symbols(args.build_dir, args.dist_dir, version)
    if debug_archive:
        print(debug_archive)

    windeployqt = find_windeployqt(args)
    launcher = package_root / "OpenApoc_Launcher.exe"
    if windeployqt and launcher.exists():
        run([str(windeployqt), "--no-opengl-sw", "--no-compiler-runtime", str(launcher)])

    strip_windows_dev_files(package_root)
    archive = args.dist_dir / f"OpenApoc-windows-{version}.zip"
    make_zip(package_root, archive)
    print(archive)

    if args.nsis:
        run([str(args.nsis), f"/DGAME_VERSION={version}", str(args.workspace / "packaging/windows/installer.nsi")])
        installer_name = f"install-openapoc-{version}.exe"
        for installer in [
            args.workspace / "packaging/windows" / installer_name,
            args.workspace / installer_name,
        ]:
            if installer.exists():
                shutil.copy2(installer, args.dist_dir / installer.name)
                print(args.dist_dir / installer.name)
                break
    return 0


def add_package_args(command: argparse.ArgumentParser) -> None:
    command.add_argument("--workspace", required=True, type=Path)
    command.add_argument("--build-dir", required=True, type=Path)
    command.add_argument("--dist-dir", required=True, type=Path)
    command.add_argument("--version")
    command.add_argument("--commit")


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser()
    sub = root.add_subparsers(dest="command", required=True)

    linux_parser = sub.add_parser("linux")
    add_package_args(linux_parser)
    linux_parser.set_defaults(func=linux)

    tools_parser = sub.add_parser("windows-tools")
    tools_parser.set_defaults(func=windows_tools)

    vcpkg_parser = sub.add_parser("windows-vcpkg")
    vcpkg_parser.add_argument("--root", required=True, type=Path)
    vcpkg_parser.add_argument("--workspace", required=True, type=Path)
    vcpkg_parser.add_argument("--install-root", type=Path)
    vcpkg_parser.add_argument("--ref", default="2025.09.17")
    vcpkg_parser.set_defaults(func=windows_vcpkg)

    windows_parser = sub.add_parser("windows")
    add_package_args(windows_parser)
    windows_parser.add_argument("--nsis", type=Path)
    windows_parser.add_argument("--vcpkg-root", type=Path)
    windows_parser.add_argument("--windeployqt", type=Path)
    windows_parser.set_defaults(func=windows)

    return root


def main() -> int:
    args = parser().parse_args()
    if hasattr(args, "dist_dir"):
        args.dist_dir.mkdir(parents=True, exist_ok=True)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
