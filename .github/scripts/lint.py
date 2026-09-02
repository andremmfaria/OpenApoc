#!/usr/bin/env python3
"""Reusable lint workflow helpers."""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


ZERO_SHA = "0000000000000000000000000000000000000000"


def run(command: list[str], *, cwd: Path | None = None, check: bool = True) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, check=check, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def range_(args: argparse.Namespace) -> int:
    if args.event == "pull_request":
        before = args.pr_base
        after = args.pr_head
    elif args.event == "push":
        before = args.push_before or ZERO_SHA
        after = args.push_after
    else:
        print(f"::error::Unsupported event for lint range: {args.event}")
        return 1

    if not args.env_file:
        print(f"BEFORE_COMMIT_SHA={before}")
        print(f"AFTER_COMMIT_SHA={after}")
        return 0

    with open(args.env_file, "a", encoding="utf-8") as env_file:
        env_file.write(f"BEFORE_COMMIT_SHA={before}\n")
        env_file.write(f"AFTER_COMMIT_SHA={after}\n")
    return 0


def format_(args: argparse.Namespace) -> int:
    try:
        print(run(["cmake", "--build", str(args.build_dir), "-t", "format-sources"]).stdout, end="")
    except subprocess.CalledProcessError as exc:
        print(exc.stdout, end="")
        return exc.returncode

    status = run(["git", "-C", str(args.workspace), "status", "--porcelain"]).stdout
    if not status:
        return 0

    print("Format mismatch:")
    print(run(["git", "-C", str(args.workspace), "diff"]).stdout, end="")
    return 1


def revision(before: str, after: str) -> str:
    if before == ZERO_SHA:
        return after
    return f"{before}..{after}"


def sha_exists(revision_ref: str, *, cwd: Path) -> bool:
    if not revision_ref or revision_ref == ZERO_SHA:
        return False
    try:
        run(["git", "cat-file", "-e", f"{revision_ref}^{{commit}}"], cwd=cwd)
        return True
    except subprocess.CalledProcessError:
        return False


def merge_base(ref_a: str, ref_b: str, *, cwd: Path) -> str | None:
    try:
        result = run(["git", "merge-base", ref_a, ref_b], cwd=cwd)
    except subprocess.CalledProcessError:
        return None
    candidate = result.stdout.strip()
    return candidate if sha_exists(candidate, cwd=cwd) else None


def resolve_before(before: str, after: str, *, cwd: Path) -> str:
    """Resolve a usable BEFORE revision for the tidy diff range.

    A force-push (common with the stacked-branch workflow used here) can make
    github.event.before point at a commit that no longer exists anywhere in the
    repository, which makes `git diff` fail outright. Fall back through
    progressively cheaper alternatives rather than crashing.
    """
    if sha_exists(before, cwd=cwd):
        return before

    if before and before != ZERO_SHA:
        print(f"::warning::before revision {before} not found in repository, choosing a fallback lint range")

    for default_ref in ("origin/HEAD", "origin/master", "origin/main"):
        if sha_exists(default_ref, cwd=cwd):
            base = merge_base(default_ref, after, cwd=cwd)
            if base is not None:
                return base

    try:
        parent = run(["git", "rev-parse", f"{after}^"], cwd=cwd).stdout.strip()
    except subprocess.CalledProcessError:
        parent = None
    if parent and sha_exists(parent, cwd=cwd):
        return parent

    return ZERO_SHA


def tidy(args: argparse.Namespace) -> int:
    compile_commands = args.build_dir / "compile_commands.json"
    if not compile_commands.is_file():
        print(f"::error::{compile_commands} not found - is BUILD_DIR set correctly?")
        return 1
    if shutil.which(args.clang_tidy) is None:
        print(f'::error::clang-tidy binary "{args.clang_tidy}" not found')
        return 1

    if args.revision:
        git_revision = args.revision
    else:
        before = resolve_before(args.before, args.after, cwd=args.workspace)
        git_revision = revision(before, args.after)
    print(f"Running clang-tidy for {git_revision}")
    result = run(
        ["git", "diff", "--name-only", "--diff-filter=ACMRTUXB", git_revision],
        cwd=args.workspace,
    )

    failed = False
    for filename in result.stdout.splitlines():
        if Path(filename).suffix != ".cpp":
            continue
        print(f"Running clang-tidy on {filename}")
        tidy_result = subprocess.run([args.clang_tidy, "-p", str(args.build_dir), filename], cwd=args.workspace)
        if tidy_result.returncode != 0:
            failed = True
            print(f"File {filename} failed clang-tidy checks")

    print("All specified files checked")
    return 1 if failed and args.fail_on_error else 0


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser()
    sub = root.add_subparsers(dest="command", required=True)

    range_parser = sub.add_parser("range")
    range_parser.add_argument("--event", default=os.environ.get("GITHUB_EVENT_NAME", ""))
    range_parser.add_argument("--env-file", default=os.environ.get("GITHUB_ENV"))
    range_parser.add_argument("--pr-head", default=os.environ.get("PR_HEAD_SHA", ""))
    range_parser.add_argument("--pr-base", default=os.environ.get("PR_BASE_SHA", ""))
    range_parser.add_argument("--push-after", default=os.environ.get("PUSH_AFTER_SHA", ""))
    range_parser.add_argument("--push-before", default=os.environ.get("PUSH_BEFORE_SHA", ""))
    range_parser.set_defaults(func=range_)

    format_parser = sub.add_parser("format")
    format_parser.add_argument("--build-dir", required=True, type=Path)
    format_parser.add_argument("--workspace", required=True, type=Path)
    format_parser.set_defaults(func=format_)

    tidy_parser = sub.add_parser("tidy")
    tidy_parser.add_argument("--workspace", default=os.environ.get("GITHUB_WORKSPACE", "."), type=Path)
    tidy_parser.add_argument("--build-dir", default=os.environ.get("BUILD_DIR"), type=Path)
    tidy_parser.add_argument("--clang-tidy", default=os.environ.get("CLANG_TIDY", "clang-tidy"))
    tidy_parser.add_argument("--before", default=os.environ.get("BEFORE_COMMIT_SHA", ZERO_SHA))
    tidy_parser.add_argument("--after", default=os.environ.get("AFTER_COMMIT_SHA", "HEAD"))
    tidy_parser.add_argument("--fail-on-error", action="store_true")
    tidy_parser.add_argument("--revision")
    tidy_parser.set_defaults(func=tidy)

    return root


def main() -> int:
    args = parser().parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
