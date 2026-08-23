#!/usr/bin/env python3
"""Repository hygiene checks used by GitHub workflows."""

import argparse
import subprocess
import sys
from pathlib import Path


GENERATED_DATA_PATHS = [
    "data/animationpacks",
    "data/bulletsprites",
    "data/gamestate_common",
    "data/imagepacks",
    "data/maps",
    "data/tilesets",
    "data/mods/base/base_gamestate",
    "data/mods/base/data/submods/org.openapoc.base/difficulty0",
    "data/mods/base/data/submods/org.openapoc.base/difficulty1",
    "data/mods/base/data/submods/org.openapoc.base/difficulty2",
    "data/mods/base/data/submods/org.openapoc.base/difficulty3",
    "data/mods/base/data/submods/org.openapoc.base/difficulty4",
    "data/mods/base/modinfo.xml",
    "data/mods/crashing_vehicles/crashing_vehicles_gamestate",
]


def plans(args: argparse.Namespace) -> int:
    failed = False
    for relative_path in args.forbid:
        path = args.workspace / relative_path
        if path.exists():
            print(f"::error::{relative_path}/ is fork-local and must be removed before opening an upstream PR.")
            failed = True
    return 1 if failed else 0


def data(args: argparse.Namespace) -> int:
    paths = args.path or GENERATED_DATA_PATHS
    result = subprocess.run(
        ["git", "status", "--short", "--untracked-files=all", "--", *paths],
        cwd=args.workspace,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    if not result.stdout:
        return 0
    print("::error::Build wrote generated data into the source data tree.")
    print(result.stdout, end="")
    return 1


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser()
    sub = root.add_subparsers(dest="command", required=True)

    plans_parser = sub.add_parser("plans")
    plans_parser.add_argument("--workspace", default=".", type=Path)
    plans_parser.add_argument("--forbid", action="append", default=["plans"])
    plans_parser.set_defaults(func=plans)

    data_parser = sub.add_parser("data")
    data_parser.add_argument("--workspace", default=".", type=Path)
    data_parser.add_argument("--path", action="append")
    data_parser.set_defaults(func=data)

    return root


def main() -> int:
    args = parser().parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
