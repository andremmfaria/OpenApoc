#!/bin/sh
set -eu

package_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$package_root"

if [ -x "$package_root/OpenApoc.app/Contents/MacOS/OpenApoc" ]; then
	exec "$package_root/OpenApoc.app/Contents/MacOS/OpenApoc"
fi

exec "$package_root/bin/OpenApoc"
