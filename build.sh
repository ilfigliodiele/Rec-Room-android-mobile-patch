#!/bin/bash
# github / LAN build: discovery + Photon -> 192.168.1.91:8805
set -eu

: "${NDK:?set NDK to your Android NDK path (r25c+)}"

cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build -j"$(nproc 2>/dev/null || echo 4)"
echo "-> build/librecroom_patch.so"
