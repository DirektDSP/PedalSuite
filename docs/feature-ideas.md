# PedalSuite Feature Ideas

## Cross-Plugin Features
- **Preset system** — Save/recall/browse presets per plugin (PresetManager exists but no UI yet)
- **MIDI learn** — Map any parameter to a MIDI CC
- **A/B comparison** — Toggle between two parameter snapshots
- **Undo/redo** — Parameter change history
- **Parameter locking** — Lock specific params when switching presets

## Hydraulica
- **Sidechain input** — External sidechain source for the compressor
- **Parallel compression blend** — Dedicated parallel path separate from mix
- **Multiband DOOM** — Split into 2-3 bands, each with its own DOOM amount
- **Envelope shape** — Logarithmic/linear/exponential attack/release curves
- **Visual GR meter** — Real-time gain reduction metering in the UI (data already exists in DebugData)

## Pneumatica
- **Mid/side processing** — Apply width/crunch to mid vs side independently
- **Modulated shimmer** — LFO on the shimmer pitch/rate
- **Vinyl mode** — Combined noise + filtering preset that emulates vinyl degradation
- **Dynamic crunch** — Input-level-dependent saturation amount
- **Multi-band noise** — Different noise types per frequency band

## Mechanica
- **Cab/IR loader** — Load impulse responses after the distortion chain
- **Waveshaper editor** — Custom transfer function curve drawing
- **Envelope follower → filter** — Dynamic filtering driven by input level
- **Parallel distortion paths** — Two waveshapers blended together
- **LFO modulation** — Modulate drive, bias, or filter params with an LFO
- **More waveshapers** — Bit crush, sine fold, Chebyshev polynomial, asymmetric tube

## Electrica
- **Multi-oscillator** — Add a second oscillator with independent waveform/octave/detune
- **LFO → filter** — Modulate filter cutoff with synth-style LFO
- **Arpeggiator** — Arpeggiate the detected note for MIDI output
- **Chord mode** — Output chord intervals from a single detected note (e.g. +3rd, +5th)
- **Envelope → filter** — Envelope follower controlling filter cutoff (auto-wah style)
- **Sub-oscillator** — Dedicated -1/-2 octave sine/square under the main osc
- **Formant filter** — Vowel-shaped filter bank for vocal-like synth tones
- **Poly voice limit** — User-selectable max polyphony (1-6)
- **MIDI MPE output** — Per-note pitch bend and pressure via MPE
- **Wavetable oscillator** — Load/scan wavetables instead of basic waveforms

## UI/UX
- **Tooltips** — Hover descriptions for each parameter
- **Value readout** — Click a knob to type an exact value
- **Randomize** — Randomize all params (or per-section) for sound design
- **Init preset** — One-click reset to defaults
- **Theme switching** — Light/dark or per-plugin color customization
- **Tuner display** — Show detected pitch as a tuner (Electrica)
- **Spectrum analyzer** — Input/output spectrum visualization
