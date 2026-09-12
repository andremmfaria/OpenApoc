#!/usr/bin/env python3
"""Reusable CMake workflow helpers."""

import argparse
import lzma
import os
import shutil
import subprocess
import sys
import urllib.request
from pathlib import Path


COMMON_PACKAGES = [
    "build-essential",
    "cmake",
    "git",
    "libboost-filesystem-dev",
    "libboost-locale-dev",
    "libboost-program-options-dev",
    "libunwind8-dev",
    "libvorbis-dev",
    "libsdl2-dev",
    "ninja-build",
    "qt6-base-dev",
]

PROFILE_PACKAGES = {
    "build": ["ccache"],
    "lint": [],
}


def run(command: list[str], *, cwd: Path | None = None) -> None:
    subprocess.run(command, cwd=cwd, check=True)


def mkdir(args: argparse.Namespace) -> int:
    args.path.mkdir(parents=True, exist_ok=True)
    return 0


def install(args: argparse.Namespace) -> int:
    packages = COMMON_PACKAGES + PROFILE_PACKAGES[args.profile] + args.extra_package
    run(["sudo", "apt-get", "update"])
    run(["sudo", "apt-get", "install", "-y", *packages])
    return 0


def minimal(args: argparse.Namespace) -> int:
    args.output.parent.mkdir(parents=True, exist_ok=True)
    compressed = args.output.with_suffix(args.output.suffix + ".xz")
    urllib.request.urlretrieve(args.url, compressed)
    with lzma.open(compressed, "rb") as source, open(args.output, "wb") as target:
        shutil.copyfileobj(source, target)
    compressed.unlink()
    return 0


def ccache(args: argparse.Namespace) -> int:
    run(["ccache", f"--max-size={args.max_size}"])
    run(["ccache", "--zero-stats"])
    return 0


def compilers(args: argparse.Namespace) -> int:
    print(f"{args.name}:", flush=True)
    for tool in [args.cc, args.cxx]:
        path = shutil.which(tool)
        if path is None:
            print(f"::error::{tool} not found on PATH")
            return 1
        print(path, flush=True)
        run([tool, "--version"])
    return 0


def configure(args: argparse.Namespace) -> int:
    command = [
        "cmake",
        str(args.source),
        f"-DCMAKE_BUILD_TYPE={args.build_type}",
        f"-G{args.generator}",
        *args.option,
    ]
    env = None
    if args.cc or args.cxx:
        env = os.environ.copy()
        if args.cc:
            env["CC"] = args.cc
        if args.cxx:
            env["CXX"] = args.cxx
    subprocess.run(command, cwd=args.build_dir, check=True, env=env)
    return 0


def build(args: argparse.Namespace) -> int:
    run(["cmake", "--build", ".", "--config", args.config], cwd=args.build_dir)
    return 0


def test(args: argparse.Namespace) -> int:
    run(["ctest", "-C", args.config], cwd=args.build_dir)
    return 0


def stats(_: argparse.Namespace) -> int:
    run(["ccache", "--show-stats"])
    return 0


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser()
    sub = root.add_subparsers(dest="command", required=True)

    mkdir_parser = sub.add_parser("mkdir")
    mkdir_parser.add_argument("--path", required=True, type=Path)
    mkdir_parser.set_defaults(func=mkdir)

    install_parser = sub.add_parser("install")
    install_parser.add_argument("--profile", choices=PROFILE_PACKAGES, required=True)
    install_parser.add_argument("--extra-package", action="append", default=[])
    install_parser.set_defaults(func=install)

    minimal_parser = sub.add_parser("minimal")
    minimal_parser.add_argument("--url", default="http://s2.jonnyh.net/pub/cd_minimal.iso.xz")
    minimal_parser.add_argument("--output", required=True, type=Path)
    minimal_parser.set_defaults(func=minimal)

    ccache_parser = sub.add_parser("ccache")
    ccache_parser.add_argument("--max-size", default="1G")
    ccache_parser.set_defaults(func=ccache)

    compilers_parser = sub.add_parser("compilers")
    compilers_parser.add_argument("--name", required=True)
    compilers_parser.add_argument("--cc", required=True)
    compilers_parser.add_argument("--cxx", required=True)
    compilers_parser.set_defaults(func=compilers)

    configure_parser = sub.add_parser("configure")
    configure_parser.add_argument("--source", required=True, type=Path)
    configure_parser.add_argument("--build-dir", required=True, type=Path)
    configure_parser.add_argument("--build-type", required=True)
    configure_parser.add_argument("--generator", default="Ninja")
    configure_parser.add_argument("--cc")
    configure_parser.add_argument("--cxx")
    configure_parser.add_argument("--option", action="append", default=[])
    configure_parser.set_defaults(func=configure)

    build_parser = sub.add_parser("build")
    build_parser.add_argument("--build-dir", required=True, type=Path)
    build_parser.add_argument("--config", required=True)
    build_parser.set_defaults(func=build)

    test_parser = sub.add_parser("test")
    test_parser.add_argument("--build-dir", required=True, type=Path)
    test_parser.add_argument("--config", required=True)
    test_parser.set_defaults(func=test)

    stats_parser = sub.add_parser("stats")
    stats_parser.set_defaults(func=stats)

    return root


def main() -> int:
    args = parser().parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
