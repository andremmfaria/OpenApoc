#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../../.." && pwd)
build_dir=${OPENAPOC_BUILD_DIR:-"$repo_root/build/local-mac"}
dist_dir=${OPENAPOC_DIST_DIR:-"$repo_root/dist/local-mac"}
build_type=${OPENAPOC_BUILD_TYPE:-Release}

usage() {
  cat <<'USAGE'
usage: scripts/local/mac/run.sh <target> [options]

targets:
  deps        install Homebrew dependencies using the workflow helper
  minimal-cd  download the minimal CI cd.iso if data/cd.iso is missing
  configure   configure CMake
  build       build the configured tree
  test        run CTest
  package     create macOS package artifacts
  lint        run format-sources through the workflow lint helper
  all         minimal-cd, configure, build, and test

options:
  --build-dir PATH   build directory, defaults to build/local-mac
  --dist-dir PATH    package output directory, defaults to dist/local-mac
  --build-type TYPE  CMake build type, defaults to Release
  -h, --help         show this help
USAGE
}

target=${1:-}
if [ -z "$target" ]; then
  usage >&2
  exit 2
fi
shift

while [ "$#" -gt 0 ]; do
  case "$1" in
    --build-dir)
      build_dir=$2
      shift 2
      ;;
    --dist-dir)
      dist_dir=$2
      shift 2
      ;;
    --build-type)
      build_type=$2
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

need() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "missing dependency: $1" >&2
    missing=1
  fi
}

check_python() {
  missing=0
  need python3
  if [ "$missing" -ne 0 ]; then
    echo "Install Python 3 before using this script." >&2
    exit 127
  fi
}

check_build_deps() {
  check_python
  missing=0
  for tool in cmake ctest git ninja clang clang++; do
    need "$tool"
  done
  if [ "$missing" -ne 0 ]; then
    echo "Install missing tools, or run: scripts/local/mac/run.sh deps" >&2
    exit 127
  fi
}

check_cd() {
  if [ ! -f data/cd.iso ]; then
    echo "data/cd.iso is missing. Add original game data or run: scripts/local/mac/run.sh minimal-cd" >&2
    exit 2
  fi
}

do_deps() {
  check_python
  missing=0
  need brew
  if [ "$missing" -ne 0 ]; then
    echo "Install Homebrew before using this target." >&2
    exit 127
  fi
  python3 .github/scripts/package.py macos-tools
}

do_minimal_cd() {
  check_python
  if [ ! -f data/cd.iso ]; then
    python3 .github/scripts/cmake.py minimal --output "$repo_root/data/cd.iso"
  fi
}

do_configure() {
  check_build_deps
  check_cd
  python3 .github/scripts/cmake.py mkdir --path "$build_dir"
  python3 .github/scripts/cmake.py configure \
    --source "$repo_root" \
    --build-dir "$build_dir" \
    --build-type "$build_type" \
    --cc clang \
    --cxx clang++ \
    --option=-DUSE_SYSTEM_QT=ON \
    --option=-DUSE_PCH=ON
}

do_build() {
  check_build_deps
  python3 .github/scripts/cmake.py build --build-dir "$build_dir" --config "$build_type"
}

do_test() {
  check_build_deps
  python3 .github/scripts/cmake.py test --build-dir "$build_dir" --config "$build_type"
}

do_package() {
  check_build_deps
  python3 .github/scripts/package.py macos \
    --workspace "$repo_root" \
    --build-dir "$build_dir" \
    --dist-dir "$dist_dir"
}

do_lint() {
  check_build_deps
  python3 .github/scripts/lint.py format --build-dir "$build_dir" --workspace "$repo_root"
}

cd "$repo_root"

case "$target" in
  deps)
    do_deps
    ;;
  minimal-cd)
    do_minimal_cd
    ;;
  configure)
    do_configure
    ;;
  build)
    do_build
    ;;
  test)
    do_test
    ;;
  package)
    do_package
    ;;
  lint)
    do_lint
    ;;
  all)
    do_minimal_cd
    do_configure
    do_build
    do_test
    ;;
  -h|--help|help)
    usage
    ;;
  *)
    echo "unknown target: $target" >&2
    usage >&2
    exit 2
    ;;
esac
