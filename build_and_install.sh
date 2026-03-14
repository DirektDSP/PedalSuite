#!/bin/bash
# Build all PedalSuite plugins without Moonbase auth and install to system dirs.
# Usage: ./build_and_install.sh [Release|Debug] [jobs]

set -euo pipefail

BUILD_TYPE="${1:-Release}"
JOBS="${2:-$(nproc)}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

VST3_DIR="$HOME/.vst3"
CLAP_DIR="$HOME/.clap"
STANDALONE_DIR="$HOME/.local/bin"

PEDALS=(Mechanica Hydraulica Pneumatica Electrica)

echo "=== PedalSuite Build & Install ==="
echo "Build type: ${BUILD_TYPE}"
echo "Jobs:       ${JOBS}"
echo ""

# --- Configure ---
echo "--- Configuring CMake (Moonbase disabled) ---"
cmake -B "$BUILD_DIR" -S "$SCRIPT_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DPEDALSUITE_DISABLE_MOONBASE=ON

# --- Build all targets ---
TARGETS=()
for pedal in "${PEDALS[@]}"; do
    TARGETS+=("${pedal}_Standalone" "${pedal}_VST3" "${pedal}_CLAP")
done

echo ""
echo "--- Building ${#PEDALS[@]} plugins (${#TARGETS[@]} targets) ---"
cmake --build "$BUILD_DIR" --target "${TARGETS[@]}" -j"$JOBS"

