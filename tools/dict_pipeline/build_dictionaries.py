#!/usr/bin/env python3
"""
Builds spectral dictionaries from audio datasets for Electrica's SparsePitchDetector.

Processes NSynth (guitar, keyboard), Iowa MIS (piano), and VocalSet (voice) into
L2-normalized FFT magnitude templates.  Outputs a C++ header file with static
constexpr arrays that can be included directly in the plugin.

Usage:
    pip install -r requirements.txt
    ./fetch_datasets.sh              # download datasets first
    python build_dictionaries.py     # build dictionaries

The pipeline:
    1. Loads each audio sample, resamples to 44.1 kHz
    2. Trims silence, computes STFT (4096-point, Hann window)
    3. Averages magnitude across time frames → single spectral template
    4. L2-normalizes each template
    5. Averages multiple samples per MIDI note (across takes/velocities)
    6. Outputs GeneratedDictionaries.h with per-instrument arrays
"""

import argparse
import json
import os
import re
import sys
from pathlib import Path

import numpy as np

try:
    import librosa
except ImportError:
    print("ERROR: librosa not installed. Run: pip install -r requirements.txt")
    sys.exit(1)

try:
    from tqdm import tqdm
except ImportError:
    # Fallback if tqdm not installed
    def tqdm(iterable, **kwargs):
        desc = kwargs.get('desc', '')
        if desc:
            print(f"  {desc}...")
        return iterable


# ── Constants matching SparsePitchDetector.h ──────────────────────────────────

TARGET_SR = 44100
FFT_SIZE = 4096
HOP_LENGTH = 1024
CROP_BINS = 512       # bins up to ~5.5 kHz at 44.1 kHz
MIN_MIDI = 40         # E2
MAX_MIDI = 88         # E6
NUM_NOTES = MAX_MIDI - MIN_MIDI + 1


# ── Core spectral extraction ─────────────────────────────────────────────────

def compute_template(audio_path: str) -> np.ndarray | None:
    """Load an audio file and compute an L2-normalized average magnitude template."""
    try:
        y, sr = librosa.load(audio_path, sr=TARGET_SR, mono=True)
    except Exception as e:
        print(f"    WARN: could not load {audio_path}: {e}")
        return None

    # Trim silence
    y_trimmed, _ = librosa.effects.trim(y, top_db=30)
    if len(y_trimmed) < FFT_SIZE:
        return None

    # Compute STFT magnitude
    S = np.abs(librosa.stft(y_trimmed, n_fft=FFT_SIZE, hop_length=HOP_LENGTH,
                            window='hann'))

    # Average across time, crop to CROP_BINS
    avg_mag = np.mean(S[:CROP_BINS, :], axis=1).astype(np.float32)

    # L2-normalize
    norm = np.linalg.norm(avg_mag)
    if norm < 1e-10:
        return None
    avg_mag /= norm

    return avg_mag


def average_templates(templates: list[np.ndarray]) -> np.ndarray:
    """Average multiple templates for the same note, then re-normalize."""
    avg = np.mean(np.stack(templates), axis=0)
    norm = np.linalg.norm(avg)
    if norm < 1e-10:
        return avg
    return avg / norm


# ── NSynth processing ────────────────────────────────────────────────────────

def process_nsynth(nsynth_dir: str, families: list[str]) -> dict[str, dict[int, np.ndarray]]:
    """
    Process NSynth test set for specified instrument families.
    Returns {family_name: {midi_note: template}}.
    """
    test_dir = os.path.join(nsynth_dir, "nsynth-test")
    json_path = os.path.join(test_dir, "examples.json")
    audio_dir = os.path.join(test_dir, "audio")

    if not os.path.exists(json_path):
        print(f"  NSynth examples.json not found at {json_path}")
        return {}

    print(f"  Loading NSynth metadata from {json_path}...")
    with open(json_path) as f:
        metadata = json.load(f)

    results = {}
    for family in families:
        print(f"  Processing NSynth family: {family}")

        # Collect samples by MIDI note
        note_samples: dict[int, list[str]] = {}
        for key, info in metadata.items():
            if info.get("instrument_family_str") != family:
                continue
            pitch = info.get("pitch", -1)
            if pitch < MIN_MIDI or pitch > MAX_MIDI:
                continue
            wav_path = os.path.join(audio_dir, f"{key}.wav")
            if os.path.exists(wav_path):
                note_samples.setdefault(pitch, []).append(wav_path)

        if not note_samples:
            print(f"    No samples found for family '{family}'")
            continue

        print(f"    Found {sum(len(v) for v in note_samples.values())} samples "
              f"across {len(note_samples)} notes")

        # Compute templates
        note_templates: dict[int, np.ndarray] = {}
        for midi_note in tqdm(sorted(note_samples.keys()), desc=f"    {family}"):
            templates = []
            for path in note_samples[midi_note]:
                t = compute_template(path)
                if t is not None:
                    templates.append(t)
            if templates:
                note_templates[midi_note] = average_templates(templates)

        results[family] = note_templates
        print(f"    Built {len(note_templates)} note templates for {family}")

    return results


# ── Iowa MIS processing ──────────────────────────────────────────────────────

# Note name → MIDI note offset within octave
IOWA_NOTE_MAP = {
    'C': 0, 'Db': 1, 'D': 2, 'Eb': 3, 'E': 4, 'F': 5,
    'Gb': 6, 'G': 7, 'Ab': 8, 'A': 9, 'Bb': 10, 'B': 11
}


def iowa_filename_to_midi(filename: str) -> int | None:
    """Parse Iowa MIS filename like 'Piano.ff.C4.aiff' → MIDI note number."""
    # Various naming patterns:
    #   Piano.ff.C4.aiff
    #   Piano.ff.Db4.aiff
    #   piano_44k_Gb5_ff.aif
    name = Path(filename).stem

    # Pattern 1: Piano.ff.C4
    m = re.search(r'\.([A-G]b?)(\d)', name)
    if m:
        note_name, octave = m.group(1), int(m.group(2))
        if note_name in IOWA_NOTE_MAP:
            return (octave + 1) * 12 + IOWA_NOTE_MAP[note_name]

    # Pattern 2: piano_44k_Gb5_ff
    m = re.search(r'_([A-G]b?)(\d)_', name)
    if m:
        note_name, octave = m.group(1), int(m.group(2))
        if note_name in IOWA_NOTE_MAP:
            return (octave + 1) * 12 + IOWA_NOTE_MAP[note_name]

    # Pattern 3: just NoteName+Octave anywhere
    m = re.search(r'([A-G]b?)(\d)', name)
    if m:
        note_name, octave = m.group(1), int(m.group(2))
        if note_name in IOWA_NOTE_MAP:
            return (octave + 1) * 12 + IOWA_NOTE_MAP[note_name]

    return None


def process_iowa(iowa_dir: str) -> dict[int, np.ndarray]:
    """Process Iowa MIS piano samples. Returns {midi_note: template}."""
    # Find all AIFF/WAV files
    audio_files = []
    for ext in ('*.aiff', '*.aif', '*.wav', '*.AIFF', '*.AIF', '*.WAV'):
        audio_files.extend(Path(iowa_dir).rglob(ext))

    if not audio_files:
        print(f"  No audio files found in {iowa_dir}")
        return {}

    # Filter to piano files and group by MIDI note
    note_samples: dict[int, list[str]] = {}
    for f in audio_files:
        fname = f.name.lower()
        if 'piano' not in fname and 'piano' not in str(f.parent).lower():
            continue
        midi = iowa_filename_to_midi(f.name)
        if midi is not None and MIN_MIDI <= midi <= MAX_MIDI:
            note_samples.setdefault(midi, []).append(str(f))

    if not note_samples:
        # Try all files if no piano-specific ones found
        print("    No piano-specific files found, trying all audio files...")
        for f in audio_files:
            midi = iowa_filename_to_midi(f.name)
            if midi is not None and MIN_MIDI <= midi <= MAX_MIDI:
                note_samples.setdefault(midi, []).append(str(f))

    print(f"    Found {sum(len(v) for v in note_samples.values())} samples "
          f"across {len(note_samples)} notes")

    note_templates: dict[int, np.ndarray] = {}
    for midi_note in tqdm(sorted(note_samples.keys()), desc="    Piano"):
        templates = []
        for path in note_samples[midi_note]:
            t = compute_template(path)
            if t is not None:
                templates.append(t)
        if templates:
            note_templates[midi_note] = average_templates(templates)

    return note_templates


# ── VocalSet processing ───────────────────────────────────────────────────────

def vocalset_filename_to_midi(filename: str) -> int | None:
    """
    Try to extract MIDI note from VocalSet filename.
    VocalSet files are organized by technique — scales and arpeggios have
    note info in the path.  Long tones often have the note in the filename.
    """
    name = Path(filename).stem.lower()

    # Look for note names like c4, f#3, bb2
    m = re.search(r'([a-g])([#b]?)(\d)', name)
    if m:
        note_letter = m.group(1).upper()
        accidental = m.group(2)
        octave = int(m.group(3))

        note_map = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}
        if note_letter in note_map:
            midi = (octave + 1) * 12 + note_map[note_letter]
            if accidental == '#':
                midi += 1
            elif accidental == 'b':
                midi -= 1
            if MIN_MIDI <= midi <= MAX_MIDI:
                return midi

    return None


def process_vocalset(vocalset_dir: str) -> dict[int, np.ndarray]:
    """
    Process VocalSet — look for sustained notes ('long_tones', 'scales', 'arpeggios').
    For non-labeled files, use pitch detection to assign MIDI notes.
    Returns {midi_note: template}.
    """
    # Find all WAV files
    audio_files = list(Path(vocalset_dir).rglob('*.wav'))
    if not audio_files:
        print(f"  No WAV files found in {vocalset_dir}")
        return {}

    # Prefer long tones and scales (most sustained, cleanest for templates)
    preferred = [f for f in audio_files
                 if any(k in str(f).lower()
                        for k in ('long_tone', 'scales', 'arpeggios', 'straight'))]
    if not preferred:
        preferred = audio_files

    print(f"    Found {len(preferred)} preferred files out of {len(audio_files)} total")

    # Try filename-based MIDI assignment first
    note_samples: dict[int, list[str]] = {}
    unlabeled = []

    for f in preferred:
        midi = vocalset_filename_to_midi(f.name)
        if midi is not None:
            note_samples.setdefault(midi, []).append(str(f))
        else:
            unlabeled.append(str(f))

    # For unlabeled files, use librosa pitch detection to assign
    if unlabeled and len(note_samples) < 10:
        print(f"    Running pitch detection on {min(len(unlabeled), 200)} unlabeled files...")
        for path in tqdm(unlabeled[:200], desc="    Voice pitch detect"):
            try:
                y, sr = librosa.load(path, sr=TARGET_SR, mono=True)
                y_trimmed, _ = librosa.effects.trim(y, top_db=30)
                if len(y_trimmed) < FFT_SIZE:
                    continue

                # Use pyin for pitch detection
                f0, voiced_flag, _ = librosa.pyin(
                    y_trimmed, fmin=librosa.note_to_hz('E2'),
                    fmax=librosa.note_to_hz('E6'), sr=TARGET_SR)

                # Get median voiced pitch
                voiced_f0 = f0[voiced_flag]
                if len(voiced_f0) < 5:
                    continue
                median_f0 = np.median(voiced_f0)
                midi_note = int(round(librosa.hz_to_midi(median_f0)))

                if MIN_MIDI <= midi_note <= MAX_MIDI:
                    note_samples.setdefault(midi_note, []).append(path)
            except Exception:
                continue

    if not note_samples:
        print("    No usable vocal samples found")
        return {}

    print(f"    Assigned {sum(len(v) for v in note_samples.values())} samples "
          f"across {len(note_samples)} notes")

    note_templates: dict[int, np.ndarray] = {}
    for midi_note in tqdm(sorted(note_samples.keys()), desc="    Voice"):
        templates = []
        for path in note_samples[midi_note][:10]:  # cap per-note to avoid slowness
            t = compute_template(path)
            if t is not None:
                templates.append(t)
        if templates:
            note_templates[midi_note] = average_templates(templates)

    return note_templates


# ── Synthetic fallback (for missing notes) ────────────────────────────────────

def generate_synthetic_template(midi_note: int,
                                max_harmonics: int = 12,
                                harmonic_decay: float = 1.2) -> np.ndarray:
    """Generate a synthetic harmonic template for a MIDI note (same as current SparsePitchDetector)."""
    f0 = 440.0 * (2.0 ** ((midi_note - 69) / 12.0))
    bin_res = TARGET_SR / FFT_SIZE
    sigma = 1.5

    spectrum = np.zeros(CROP_BINS, dtype=np.float32)
    for h in range(1, max_harmonics + 1):
        h_freq = f0 * h
        if h_freq >= CROP_BINS * bin_res:
            break
        bin_f = h_freq / bin_res
        amp = 1.0 / (h ** harmonic_decay)
        bin_start = max(0, int(bin_f - 4 * sigma))
        bin_end = min(CROP_BINS - 1, int(bin_f + 4 * sigma))
        for k in range(bin_start, bin_end + 1):
            d = k - bin_f
            spectrum[k] += amp * np.exp(-0.5 * d * d / (sigma * sigma))

    norm = np.linalg.norm(spectrum)
    if norm > 1e-10:
        spectrum /= norm
    return spectrum


def fill_missing_notes(templates: dict[int, np.ndarray],
                       use_synthetic: bool = True) -> dict[int, np.ndarray]:
    """
    Fill gaps in the note range.  For missing notes, interpolate from neighbors
    or fall back to synthetic templates.
    """
    filled = dict(templates)

    for midi in range(MIN_MIDI, MAX_MIDI + 1):
        if midi in filled:
            continue

        # Try interpolation from nearest neighbors
        lower = upper = None
        for offset in range(1, 13):
            if lower is None and (midi - offset) in filled:
                lower = midi - offset
            if upper is None and (midi + offset) in filled:
                upper = midi + offset
            if lower is not None and upper is not None:
                break

        if lower is not None and upper is not None:
            # Linear interpolation in spectral domain
            alpha = (midi - lower) / (upper - lower)
            interp = (1 - alpha) * filled[lower] + alpha * filled[upper]
            norm = np.linalg.norm(interp)
            if norm > 1e-10:
                interp /= norm
            filled[midi] = interp
        elif lower is not None:
            filled[midi] = filled[lower].copy()
        elif upper is not None:
            filled[midi] = filled[upper].copy()
        elif use_synthetic:
            filled[midi] = generate_synthetic_template(midi)

    return filled


# ── C++ header generation ─────────────────────────────────────────────────────

def write_cpp_header(instruments: dict[str, dict[int, np.ndarray]],
                     output_path: str):
    """
    Write all instrument dictionaries to a C++ header file.
    Format: static constexpr float arrays indexed by (midi_note - MIN_MIDI).
    """
    with open(output_path, 'w') as f:
        f.write("// Auto-generated by build_dictionaries.py — DO NOT EDIT\n")
        f.write("// Spectral dictionaries for SparsePitchDetector\n")
        f.write("//\n")
        f.write(f"// FFT size: {FFT_SIZE}, crop bins: {CROP_BINS}, "
                f"sample rate: {TARGET_SR}\n")
        f.write(f"// MIDI range: {MIN_MIDI} (E2) to {MAX_MIDI} (E6)\n")
        f.write(f"// Instruments: {', '.join(instruments.keys())}\n")
        f.write("//\n")
        f.write(f"// Total size: ~{len(instruments) * NUM_NOTES * CROP_BINS * 4 / 1024:.0f} KB\n\n")
        f.write("#pragma once\n\n")
        f.write("#include <array>\n\n")
        f.write("namespace DSP {\n")
        f.write("namespace Core {\n")
        f.write("namespace DictData {\n\n")
        f.write(f"static constexpr int dictMinNote = {MIN_MIDI};\n")
        f.write(f"static constexpr int dictMaxNote = {MAX_MIDI};\n")
        f.write(f"static constexpr int dictSize = {NUM_NOTES};\n")
        f.write(f"static constexpr int cropBins = {CROP_BINS};\n\n")

        # Instrument enum
        f.write("enum class Instrument {\n")
        for i, name in enumerate(instruments.keys()):
            cpp_name = name.replace(' ', '_').replace('-', '_').title()
            f.write(f"    {cpp_name} = {i},\n")
        f.write(f"    Synthetic = {len(instruments)},\n")
        f.write(f"    NumInstruments = {len(instruments) + 1}\n")
        f.write("};\n\n")

        # Per-instrument arrays
        for inst_name, templates in instruments.items():
            cpp_name = inst_name.replace(' ', '_').replace('-', '_').lower()
            coverage = sum(1 for m in range(MIN_MIDI, MAX_MIDI + 1)
                          if m in templates)

            f.write(f"// {inst_name}: {coverage}/{NUM_NOTES} notes from real data\n")
            f.write(f"// clang-format off\n")
            f.write(f"static constexpr float {cpp_name}Dict[{NUM_NOTES}][{CROP_BINS}] = {{\n")

            for midi in range(MIN_MIDI, MAX_MIDI + 1):
                idx = midi - MIN_MIDI
                if midi in templates:
                    data = templates[midi]
                else:
                    # Should not happen if fill_missing_notes was called
                    data = np.zeros(CROP_BINS, dtype=np.float32)

                f.write(f"    {{ // MIDI {midi} (note {idx})\n")
                # Write in rows of 8 values for readability
                for row_start in range(0, CROP_BINS, 8):
                    row_end = min(row_start + 8, CROP_BINS)
                    vals = ", ".join(f"{data[k]:.8f}f" for k in range(row_start, row_end))
                    comma = "," if row_end < CROP_BINS else ""
                    f.write(f"        {vals}{comma}\n")
                comma = "," if midi < MAX_MIDI else ""
                f.write(f"    }}{comma}\n")

            f.write(f"}};\n")
            f.write(f"// clang-format on\n\n")

        # Accessor function
        f.write("// Returns a pointer to the dictionary array for the given instrument.\n")
        f.write(f"// Each dictionary is float[{NUM_NOTES}][{CROP_BINS}].\n")
        f.write(f"inline const float (*getDictionary(Instrument inst))[{CROP_BINS}]\n")
        f.write("{\n")
        f.write("    switch (inst)\n")
        f.write("    {\n")
        for inst_name in instruments.keys():
            cpp_name = inst_name.replace(' ', '_').replace('-', '_').lower()
            enum_name = inst_name.replace(' ', '_').replace('-', '_').title()
            f.write(f"        case Instrument::{enum_name}: return {cpp_name}Dict;\n")
        f.write(f"        default: return nullptr;\n")
        f.write("    }\n")
        f.write("}\n\n")

        f.write("} // namespace DictData\n")
        f.write("} // namespace Core\n")
        f.write("} // namespace DSP\n")

    file_size = os.path.getsize(output_path)
    print(f"\nWrote {output_path} ({file_size / 1024 / 1024:.1f} MB)")


# ── Main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Build spectral dictionaries for Electrica's SparsePitchDetector")
    parser.add_argument("--datasets-dir", default=os.path.join(os.path.dirname(__file__), "datasets"),
                        help="Root directory containing downloaded datasets")
    parser.add_argument("--output", default=None,
                        help="Output C++ header path (default: Electrica/source/DSP/Core/GeneratedDictionaries.h)")
    parser.add_argument("--skip-nsynth", action="store_true",
                        help="Skip NSynth processing")
    parser.add_argument("--skip-iowa", action="store_true",
                        help="Skip Iowa MIS processing")
    parser.add_argument("--skip-vocalset", action="store_true",
                        help="Skip VocalSet processing")
    parser.add_argument("--synthetic-only", action="store_true",
                        help="Generate only synthetic dictionaries (no datasets needed)")
    args = parser.parse_args()

    if args.output is None:
        # Default: write to Electrica source tree
        script_dir = Path(__file__).resolve().parent
        repo_root = script_dir.parent.parent
        args.output = str(repo_root / "Electrica" / "source" / "DSP" / "Core" / "GeneratedDictionaries.h")

    instruments: dict[str, dict[int, np.ndarray]] = {}

    if args.synthetic_only:
        print("Generating synthetic-only dictionaries...")
        synth_dict = {}
        for midi in range(MIN_MIDI, MAX_MIDI + 1):
            synth_dict[midi] = generate_synthetic_template(midi)
        instruments["guitar"] = dict(synth_dict)
        instruments["keyboard"] = dict(synth_dict)
        instruments["piano"] = dict(synth_dict)
        instruments["voice"] = dict(synth_dict)
    else:
        # NSynth: guitar + keyboard
        if not args.skip_nsynth:
            nsynth_dir = os.path.join(args.datasets_dir, "nsynth")
            if os.path.isdir(nsynth_dir):
                print("\n=== Processing NSynth ===")
                nsynth_results = process_nsynth(nsynth_dir, ["guitar", "keyboard"])
                for family, templates in nsynth_results.items():
                    if templates:
                        instruments[family] = fill_missing_notes(templates)
            else:
                print(f"NSynth not found at {nsynth_dir}, skipping.")

        # Iowa MIS: piano
        if not args.skip_iowa:
            iowa_dir = os.path.join(args.datasets_dir, "iowa_mis")
            if os.path.isdir(iowa_dir):
                print("\n=== Processing Iowa MIS Piano ===")
                piano_templates = process_iowa(iowa_dir)
                if piano_templates:
                    instruments["piano"] = fill_missing_notes(piano_templates)
            else:
                print(f"Iowa MIS not found at {iowa_dir}, skipping.")

        # VocalSet: voice
        if not args.skip_vocalset:
            vocalset_dir = os.path.join(args.datasets_dir, "vocalset")
            if os.path.isdir(vocalset_dir):
                print("\n=== Processing VocalSet ===")
                vocal_templates = process_vocalset(vocalset_dir)
                if vocal_templates:
                    instruments["voice"] = fill_missing_notes(vocal_templates)
            else:
                print(f"VocalSet not found at {vocalset_dir}, skipping.")

    if not instruments:
        print("\nERROR: No datasets processed. Run fetch_datasets.sh first,")
        print("or use --synthetic-only to generate placeholder dictionaries.")
        sys.exit(1)

    # Summary
    print(f"\n=== Dictionary Summary ===")
    for name, templates in instruments.items():
        real_count = len(templates)
        print(f"  {name}: {real_count} notes, "
              f"~{real_count * CROP_BINS * 4 / 1024:.0f} KB")

    # Write header
    os.makedirs(os.path.dirname(args.output), exist_ok=True)
    write_cpp_header(instruments, args.output)
    print("Done!")


if __name__ == "__main__":
    main()
