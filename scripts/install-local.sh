#!/usr/bin/env bash
#
# Install the freshly built plugin into the user's OpenCPN directories,
# so it can be tested without going through the plugin manager.
#
set -euo pipefail

cd "$(dirname "$0")/.."

stage=$(find build/app -maxdepth 1 -type d -name 'Intercept-*' | head -1)
if [[ -z "$stage" ]]; then
    echo "No build found. Run:" >&2
    echo "    cmake -B build -DCMAKE_BUILD_TYPE=Release" >&2
    echo "    cmake --build build -j\$(nproc) --target tarball" >&2
    exit 1
fi

lib_dst="$HOME/.local/lib/opencpn"
data_dst="$HOME/.local/share/opencpn/plugins"

mkdir -p "$lib_dst" "$data_dst"
cp -v "$stage"/lib/opencpn/libintercept_pi.so "$lib_dst/"
cp -rv "$stage"/share/opencpn/plugins/intercept_pi "$data_dst/"

echo
echo "Installed. Restart OpenCPN, then Options > Plugins > Intercept."
