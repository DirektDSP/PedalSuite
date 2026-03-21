#!/usr/bin/env bash
# Downloads NSynth test set, Iowa MIS piano, and VocalSet for dictionary extraction.
# Usage: ./fetch_datasets.sh [datasets_dir]
#
# Requires: wget or curl, tar, unzip

set -euo pipefail

DATASETS_DIR="${1:-$(dirname "$0")/datasets}"
mkdir -p "$DATASETS_DIR"

# ---------------------------------------------------------------------------
# NSynth test set (~400MB, contains guitar + keyboard + vocal samples)
# ---------------------------------------------------------------------------
NSYNTH_DIR="$DATASETS_DIR/nsynth"
if [ ! -d "$NSYNTH_DIR/nsynth-test" ]; then
    echo "=== Downloading NSynth test set (~400MB) ==="
    mkdir -p "$NSYNTH_DIR"
    NSYNTH_URL="http://download.magenta.tensorflow.org/datasets/nsynth/nsynth-test.jsonwav.tar.gz"
    NSYNTH_TAR="$NSYNTH_DIR/nsynth-test.jsonwav.tar.gz"

    if command -v wget &>/dev/null; then
        wget -c -O "$NSYNTH_TAR" "$NSYNTH_URL"
    else
        curl -L -C - -o "$NSYNTH_TAR" "$NSYNTH_URL"
    fi

    echo "Extracting NSynth test set..."
    tar -xzf "$NSYNTH_TAR" -C "$NSYNTH_DIR"
    rm -f "$NSYNTH_TAR"
    echo "NSynth test set ready at $NSYNTH_DIR/nsynth-test/"
else
    echo "NSynth test set already exists at $NSYNTH_DIR/nsynth-test/, skipping."
fi

# ---------------------------------------------------------------------------
# University of Iowa MIS — Piano samples (44.1kHz AIFF, anechoic)
# The Iowa MIS site serves files individually. We grab the piano ZIP.
# ---------------------------------------------------------------------------
IOWA_DIR="$DATASETS_DIR/iowa_mis"
if [ ! -d "$IOWA_DIR/Piano" ] && [ ! -d "$IOWA_DIR/piano" ]; then
    echo "=== Downloading University of Iowa MIS Piano samples ==="
    mkdir -p "$IOWA_DIR"

    # The Iowa MIS hosts zipped instrument sets.  Piano forte+mf+pp.
    # These URLs point to the current hosting location (as of 2025).
    IOWA_BASE="https://theremin.music.uiowa.edu/MISPiano.html"

    # Piano ff (forte)
    for dyn in ff mf pp; do
        ZIP_URL="https://theremin.music.uiowa.edu/sound%20files/MIS/Piano_Other/piano/Piano.${dyn}.zip"
        ZIP_FILE="$IOWA_DIR/Piano.${dyn}.zip"
        echo "  Fetching Piano ${dyn}..."
        if command -v wget &>/dev/null; then
            wget -c -O "$ZIP_FILE" "$ZIP_URL" 2>/dev/null || true
        else
            curl -L -C - -o "$ZIP_FILE" "$ZIP_URL" 2>/dev/null || true
        fi
        if [ -f "$ZIP_FILE" ] && [ -s "$ZIP_FILE" ]; then
            unzip -qo "$ZIP_FILE" -d "$IOWA_DIR" 2>/dev/null || true
            rm -f "$ZIP_FILE"
        fi
    done

    # Fallback: if the zip URLs changed, try the alternative flat layout
    if [ ! -d "$IOWA_DIR/Piano" ] && [ -z "$(find "$IOWA_DIR" -name '*.aif*' 2>/dev/null | head -1)" ]; then
        echo "  ZIP download failed — trying alternative URL structure..."
        ALT_URL="https://theremin.music.uiowa.edu/sound%20files/MIS/Piano_Other/piano/"
        mkdir -p "$IOWA_DIR/Piano"
        echo "  NOTE: Iowa MIS may require manual download from:"
        echo "  https://theremin.music.uiowa.edu/MIS.html"
        echo "  Place piano AIFF files in: $IOWA_DIR/Piano/"
    fi
    echo "Iowa MIS piano ready at $IOWA_DIR/"
else
    echo "Iowa MIS piano already exists at $IOWA_DIR/, skipping."
fi

# ---------------------------------------------------------------------------
# VocalSet — sustained singing samples (44.1kHz WAV)
# Hosted on Zenodo (~1.4GB compressed)
# ---------------------------------------------------------------------------
VOCALSET_DIR="$DATASETS_DIR/vocalset"
if [ ! -d "$VOCALSET_DIR/FULL" ] && [ ! -d "$VOCALSET_DIR/data_by_singer" ]; then
    echo "=== Downloading VocalSet (~1.4GB) ==="
    mkdir -p "$VOCALSET_DIR"
    # Zenodo record 1193957, file VocalSet11.zip or VocalSet1-2.zip
    VS_URL="https://zenodo.org/records/1193957/files/VocalSet.zip?download=1"
    VS_ZIP="$VOCALSET_DIR/VocalSet.zip"

    if command -v wget &>/dev/null; then
        wget -c -O "$VS_ZIP" "$VS_URL"
    else
        curl -L -C - -o "$VS_ZIP" "$VS_URL"
    fi

    echo "Extracting VocalSet..."
    unzip -qo "$VS_ZIP" -d "$VOCALSET_DIR"
    rm -f "$VS_ZIP"
    echo "VocalSet ready at $VOCALSET_DIR/"
else
    echo "VocalSet already exists at $VOCALSET_DIR/, skipping."
fi

echo ""
echo "=== All datasets ready in $DATASETS_DIR ==="
echo "Run: python build_dictionaries.py --datasets-dir $DATASETS_DIR"
