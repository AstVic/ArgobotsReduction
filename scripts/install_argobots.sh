#!/usr/bin/env bash

set -euo pipefail

PREFIX="${1:-$HOME/argobots-install}"
SOURCE_DIR="${SOURCE_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/argobots_framework}"

need_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "[ERROR] Missing required tool: $1" >&2
        return 1
    fi
}

echo "[INFO] Installing Argobots from: $SOURCE_DIR"
echo "[INFO] Installation prefix: $PREFIX"

if [ ! -d "$SOURCE_DIR" ]; then
    echo "[ERROR] Source directory not found: $SOURCE_DIR" >&2
    exit 1
fi

for cmd in autoconf automake aclocal libtoolize make gcc; do
    need_cmd "$cmd"
done

cd "$SOURCE_DIR"
./autogen.sh
./configure --prefix="$PREFIX"
make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"
make install

echo "[OK] Argobots installed to $PREFIX"
echo "[INFO] If pkg-config cannot find argobots, run:"
echo "export ARGOBOTS_INSTALL_DIR="$PREFIX""
echo "export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:\$PKG_CONFIG_PATH""