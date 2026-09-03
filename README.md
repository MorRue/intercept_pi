# Intercept — an OpenCPN plugin

Computes the course to steer to reach a reported position.

The case it is built for: a position is reported for a target you need to
close, and the target keeps drifting with wind and current between the
report and your arrival. The bearing to the last known position is then not
the course to steer. This plugin does that arithmetic.

## Status

Early development. Currently a working plugin skeleton: it registers a
toolbar button and receives own-ship position fixes from OpenCPN.

## Building

Requires wxWidgets 3.2, CMake and a C++ compiler.

    git submodule update --init --recursive
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(nproc) --target tarball

The tarball in `build/` is installable through OpenCPN's plugin manager.
For a quick local test, copy the shared library straight into the plugin
directory instead:

    cp build/libintercept_pi.so ~/.local/lib/opencpn/

Then restart OpenCPN and enable the plugin under Options > Plugins.

## Platform support

Linux and Windows, built in CI on every push. The Windows build is 32-bit
because OpenCPN's official Windows binary is 32-bit; a 64-bit plugin will
not load.

The plugin targets plugin API 1.18, so it loads on OpenCPN 5.8 and later.

## Licence

GPLv3 or later — see [COPYING](COPYING). Derived from the OpenCPN plugin
template by Mike Rossiter.
