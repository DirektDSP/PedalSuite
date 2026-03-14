#!/bin/bash
# Install PedalSuite plugins to system locations on Linux.
# Usage: ./install.sh [Debug|Release]

set -e

BUILD_TYPE="${1:-Debug}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

VST3_DIR="$HOME/.vst3"
CLAP_DIR="$HOME/.clap"

PEDALS=(Mechanica Hydraulica Pneumatica Electrica)

mkdir -p "$VST3_DIR" "$CLAP_DIR"

for pedal in "${PEDALS[@]}"; do
    base="${BUILD_DIR}/${pedal}/${pedal}_artefacts/${BUILD_TYPE}"

    # VST3
    src="${base}/VST3/${pedal}.vst3"
    if [ -d "$src" ]; then
        rm -rf "${VST3_DIR}/${pedal}.vst3"
        cp -r "$src" "$VST3_DIR/"
        echo "VST3  ${pedal} -> ${VST3_DIR}/${pedal}.vst3"
    else
        echo "VST3  ${pedal} -- not found, skipping"
    fi

    # CLAP
    src="${base}/CLAP/${pedal}.clap"
    if [ -f "$src" ]; then
        cp "$src" "$CLAP_DIR/"
        echo "CLAP  ${pedal} -> ${CLAP_DIR}/${pedal}.clap"
    else
        echo "CLAP  ${pedal} -- not found, skipping"
    fi
done

echo "Done."
