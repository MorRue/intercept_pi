#!/usr/bin/env bash
#
# Install a built plugin into the user's OpenCPN directories, so it can be
# tested without going through the plugin manager.
#
# Usage: scripts/install-local.sh [build-tree]
#
#   build-tree defaults to "build". Pass another tree, e.g. "build-debug",
#   to install that build instead -- the Debug tree is the one you want when
#   attaching a debugger to OpenCPN, since only it carries debug_info.
#
set -euo pipefail

cd "$(dirname "$0")/.."

tree="${1:-build}"

if [[ "$tree" == "-h" || "$tree" == "--help" ]]; then
    sed -n '3,11p' "$0" | sed 's/^# \?//'
    exit 0
fi

if [[ ! -d "$tree" ]]; then
    echo "No such build tree: $tree" >&2
    echo "Existing trees:" >&2
    find . -maxdepth 1 -type d \( -name build -o -name 'build-*' \) \
        ! -name build-deps \
        -printf '    %P\n' >&2
    exit 1
fi

lib_dst="$HOME/.local/lib/opencpn"
data_dst="$HOME/.local/share/opencpn/plugins"
mkdir -p "$lib_dst" "$data_dst"

# The 'tarball' target stages a full install tree under app/, including
# compiled translations. A plain target build only leaves the bare .so in
# the tree root, so fall back to that plus the data/ files from source.
stage=$(find "$tree/app" -maxdepth 1 -type d -name 'Intercept-*' 2>/dev/null | head -1)

if [[ -n "$stage" ]]; then
    echo "Installing staged tree from $stage"
    cp "$stage"/lib/opencpn/libintercept_pi.so "$lib_dst/"
    cp -r "$stage"/share/opencpn/plugins/intercept_pi "$data_dst/"
elif [[ -f "$tree/libintercept_pi.so" ]]; then
    echo "Installing bare library from $tree (no staged tree; data/ from source)"
    cp "$tree/libintercept_pi.so" "$lib_dst/"
    mkdir -p "$data_dst/intercept_pi/data"
    cp data/*.svg "$data_dst/intercept_pi/data/"
else
    echo "Nothing built in '$tree'. Build first, for example:" >&2
    echo "    cmake --build $tree --target tarball -j\$(nproc)" >&2
    exit 1
fi

installed="$lib_dst/libintercept_pi.so"
printf 'Installed %s (%s bytes, %s)\n' \
    "$installed" "$(stat -c%s "$installed")" "$(date -r "$installed" '+%H:%M:%S')"
echo "Restart OpenCPN, then Options > Plugins > Intercept."
