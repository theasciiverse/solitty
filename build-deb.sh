#!/bin/sh
# Build the Debian package and collect the artifacts into dist/, instead of
# letting dpkg-buildpackage scatter them into the parent directory.
set -e

cd "$(dirname "$0")"
mkdir -p dist

dpkg-buildpackage -us -uc "$@"

mv -f ../solitty_*.deb ../solitty_*.changes ../solitty_*.buildinfo dist/ 2>/dev/null || true
mv -f ../solitty-dbgsym_*.ddeb dist/ 2>/dev/null || true

echo "Packages moved to dist/:"
ls -la dist/
