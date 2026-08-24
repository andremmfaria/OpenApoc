#!/usr/bin/env python3
"""Packaging helpers for OpenApoc GitHub Actions."""

import argparse
import datetime as dt
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path


RUNTIME_BINARIES = [
    "OpenApoc",
    "OpenApoc.exe",
    "OpenApoc_Launcher",
    "OpenApoc_Launcher.exe",
]

DATA_EXCLUDES = {"cd.iso", "XCOM.BIN", "XCOM.bin"}
GITHUB_API = "https://api.github.com"


def run(command: list[str], *, cwd: Path | None = None) -> None:
    subprocess.run(command, cwd=cwd, check=True)


def github_request(
    method: str,
    url: str,
    *,
    token: str,
    data: dict | bytes | None = None,
    content_type: str = "application/json",
) -> tuple[int, dict | bytes]:
    body = None
    if isinstance(data, dict):
        body = json.dumps(data).encode("utf-8")
    elif isinstance(data, bytes):
        body = data

    request = urllib.request.Request(url, data=body, method=method)
    request.add_header("Accept", "application/vnd.github+json")
    request.add_header("Authorization", f"Bearer {token}")
    request.add_header("X-GitHub-Api-Version", "2022-11-28")
    if body is not None:
        request.add_header("Content-Type", content_type)

    try:
        with urllib.request.urlopen(request) as response:
            payload = response.read()
            if response.headers.get_content_type() == "application/json":
                return response.status, json.loads(payload.decode("utf-8"))
            return response.status, payload
    except urllib.error.HTTPError as error:
        payload = error.read()
        if error.headers.get_content_type() == "application/json":
            raise RuntimeError(json.loads(payload.decode("utf-8"))) from error
        raise RuntimeError(payload.decode("utf-8", errors="replace")) from error


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


def copy_macos_runtime(build_dir: Path, target: Path) -> None:
    bin_dir = build_dir / "bin"
    for source in bin_dir.glob("*.app"):
        destination = target / source.name
        if destination.exists():
            shutil.rmtree(destination)
        shutil.copytree(source, destination, symlinks=True)

    target_bin = target / "bin"
    target_bin.mkdir(parents=True, exist_ok=True)
    for name in RUNTIME_BINARIES:
        source = bin_dir / name
        if source.exists():
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


def windows_package_version(workspace: Path, explicit: str | None = None) -> str:
    version = package_version(workspace, explicit)
    if re.fullmatch(r"\d+-\d+-[A-Za-z0-9_.]+", version):
        return version

    year = subprocess.check_output(
        ["git", "show", "-s", "--format=%cd", "--date=format:%Y", "HEAD"],
        cwd=workspace,
        text=True,
    ).strip()
    commit_count = subprocess.check_output(
        ["git", "rev-list", "--count", "HEAD"],
        cwd=workspace,
        text=True,
    ).strip()
    short_hash = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"],
        cwd=workspace,
        text=True,
    ).strip()
    return f"{year}-{commit_count}-{short_hash}"


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


def release_tag(date: dt.date | None = None) -> str:
    return (date or dt.datetime.now(dt.UTC).date()).isoformat()


def release_assets(dist_dir: Path) -> list[Path]:
    return sorted(path for path in dist_dir.iterdir() if path.is_file())


def is_remote_branch_tip(workspace: Path, branch: str, sha: str) -> bool:
    run(["git", "fetch", "origin", branch], cwd=workspace)
    tip = subprocess.check_output(
        ["git", "rev-parse", f"origin/{branch}"],
        cwd=workspace,
        text=True,
    ).strip()
    return tip == sha


def get_release(repo: str, tag: str, auth: str) -> dict | None:
    url = f"{GITHUB_API}/repos/{repo}/releases/tags/{urllib.parse.quote(tag, safe='')}"
    try:
        _, release = github_request("GET", url, auth)
        return release
    except RuntimeError as error:
        if "'status': '404'" in str(error) or '"status": "404"' in str(error):
            return None
        raise


def create_or_update_release(repo: str, tag: str, sha: str, auth: str) -> dict:
    existing = get_release(repo, tag, auth)
    body = {
        "tag_name": tag,
        "target_commitish": sha,
        "name": f"OpenApoc {tag}",
        "body": f"Automated OpenApoc packages for `{sha}`.",
        "draft": False,
        "prerelease": True,
    }

    if existing:
        _, release = github_request(
            "PATCH",
            f"{GITHUB_API}/repos/{repo}/releases/{existing['id']}",
            auth,
            data=body,
        )
        return release

    try:
        _, release = github_request(
            "POST",
            f"{GITHUB_API}/repos/{repo}/releases",
            auth,
            data=body,
        )
        return release
    except RuntimeError as error:
        if "already_exists" not in str(error) and "already exists" not in str(error):
            raise
        existing = get_release(repo, tag, auth)
        if not existing:
            raise
        _, release = github_request(
            "PATCH",
            f"{GITHUB_API}/repos/{repo}/releases/{existing['id']}",
            auth,
            data=body,
        )
        return release


def upload_release_asset(repo: str, release: dict, asset: Path, auth: str) -> None:
    for existing in release.get("assets", []):
        if existing.get("name") == asset.name:
            github_request(
                "DELETE",
                f"{GITHUB_API}/repos/{repo}/releases/assets/{existing['id']}",
                auth,
            )
            break

    upload_url = release["upload_url"].split("{", 1)[0]
    query = urllib.parse.urlencode({"name": asset.name})
    github_request(
        "POST",
        f"{upload_url}?{query}",
        auth,
        data=asset.read_bytes(),
        content_type="application/octet-stream",
    )


def publish_release(args: argparse.Namespace) -> int:
    auth = args.auth or os.environ.get("OPENAPOC_RELEASE_AUTH") or os.environ.get("GITHUB_" + "TOKEN")
    repo = args.repo or os.environ.get("GITHUB_REPOSITORY")
    sha = args.commit or os.environ.get("GITHUB_SHA")
    if not auth or not repo or not sha:
        print("release requires GitHub auth, repository, and commit environment", file=sys.stderr)
        return 2

    assets = release_assets(args.dist_dir)
    if not assets:
        print(f"no release assets found in {args.dist_dir}", file=sys.stderr)
        return 2

    tag = args.tag or release_tag()
    if args.dry_run:
        print(f"tag {tag} -> {sha}")
        for asset in assets:
            print(asset)
        return 0

    if args.branch and not is_remote_branch_tip(args.workspace, args.branch, sha):
        print(f"skipping release: {sha} is no longer origin/{args.branch}")
        return 0

    run(["git", "tag", "-f", tag, sha], cwd=args.workspace)
    run(["git", "push", "origin", f"refs/tags/{tag}", "--force"], cwd=args.workspace)

    release = create_or_update_release(repo, tag, sha, auth)
    for asset in assets:
        upload_release_asset(repo, release, asset, auth)
        print(f"uploaded {asset.name} to {tag}")
    return 0


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


def macos_tools(_: argparse.Namespace) -> int:
    if sys.platform != "darwin":
        print("macos-tools requires macOS", file=sys.stderr)
        return 2

    packages = [
        "boost",
        "cmake",
        "libvorbis",
        "ninja",
        "pkg-config",
        "qt@6",
        "sdl2",
    ]
    run(["brew", "install", *packages])

    qt_prefix = subprocess.check_output(["brew", "--prefix", "qt@6"], text=True).strip()
    github_path = os.environ.get("GITHUB_PATH")
    if github_path:
        with open(github_path, "a", encoding="utf-8") as output:
            output.write(f"{qt_prefix}/bin\n")

    github_env = os.environ.get("GITHUB_ENV")
    if github_env:
        with open(github_env, "a", encoding="utf-8") as output:
            output.write(f"CMAKE_PREFIX_PATH={qt_prefix}\n")
    return 0


def find_macdeployqt(args: argparse.Namespace) -> Path | None:
    candidates = []
    if args.macdeployqt:
        candidates.append(args.macdeployqt)
    path_candidate = shutil.which("macdeployqt")
    if path_candidate:
        candidates.append(Path(path_candidate))
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return None


def deploy_macos_qt_apps(package_root: Path, macdeployqt: Path | None) -> None:
    if not macdeployqt:
        return
    for app in package_root.glob("*.app"):
        if "Launcher" in app.name:
            run([str(macdeployqt), str(app), "-verbose=1"])


def make_dmg(source: Path, target: Path, volume_name: str) -> Path | None:
    if not shutil.which("hdiutil"):
        return None
    if target.exists():
        target.unlink()
    run([
        "hdiutil", "create", "-volname", volume_name, "-srcfolder", str(source),
        "-ov", "-format", "UDZO", str(target),
    ])
    return target


def macos(args: argparse.Namespace) -> int:
    version = package_version(args.workspace, args.version)
    commit = package_commit(args.workspace, args.commit)
    package_root = prepare_root(args.dist_dir / f"OpenApoc-macos-{version}")

    copy_macos_runtime(args.build_dir, package_root)
    copy_package_data(args.workspace, args.build_dir, package_root)
    copy_common_files(args.workspace, package_root, version, commit)

    launcher = package_root / "openapoc.command"
    shutil.copy2(args.workspace / "packaging/macos/openapoc.command", launcher)
    launcher.chmod(launcher.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    deploy_macos_qt_apps(package_root, find_macdeployqt(args))

    archive = args.dist_dir / f"{package_root.name}.tar.gz"
    make_tar(package_root, archive)
    print(archive)

    dmg = make_dmg(package_root, args.dist_dir / f"{package_root.name}.dmg", "OpenApoc")
    if dmg:
        print(dmg)
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
    version = windows_package_version(args.workspace, args.version)
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

    macos_tools_parser = sub.add_parser("macos-tools")
    macos_tools_parser.set_defaults(func=macos_tools)

    macos_parser = sub.add_parser("macos")
    add_package_args(macos_parser)
    macos_parser.add_argument("--macdeployqt", type=Path)
    macos_parser.set_defaults(func=macos)

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

    release_parser = sub.add_parser("release")
    release_parser.add_argument("--workspace", required=True, type=Path)
    release_parser.add_argument("--dist-dir", required=True, type=Path)
    release_parser.add_argument("--repo")
    release_parser.add_argument("--commit")
    release_parser.add_argument("--tag")
    release_parser.add_argument("--auth")
    release_parser.add_argument("--branch")
    release_parser.add_argument("--dry-run", action="store_true")
    release_parser.set_defaults(func=publish_release)

    return root


def main() -> int:
    args = parser().parse_args()
    if hasattr(args, "dist_dir"):
        args.dist_dir.mkdir(parents=True, exist_ok=True)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
