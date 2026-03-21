# PedalSuite Plugin Parameters

## Hydraulica — Doom Compressor
Mick Gordon-style extreme compressor. Takes signal from -200dB to 0dB instantly. For crushing, slamming, and doom-style dynamics.

| Parameter | Range | Default | Description |
|---|---|---|---|
| DOOM | 0–100% | 0% | Extreme compression amount |
| THRESHOLD | -60 to 0 dB | -20 dB | Compressor threshold |
| RATIO | 1:1 to 100:1 | 4:1 | Compression ratio |
| ATTACK | 0.01–100 ms | 5 ms | Compressor attack |
| RELEASE | 1–2000 ms | 100 ms | Compressor release |
| KNEE | 0–30 dB | 6 dB | Compressor knee width |
| MAKEUP | 0–60 dB | 0 dB | Makeup gain |
| SC_HPF | 20–500 Hz | 20 Hz | Sidechain high-pass filter |
| LIMITER_ON | On/Off | On | Output limiter enable |
| LIMITER_CEILING | -6 to 0 dB | -0.3 dB | Limiter ceiling |
| INPUT_GAIN | -48 to 24 dB | 0 dB | Input gain |
| OUTPUT_GAIN | -48 to 24 dB | 0 dB | Output gain |
| MIX | 0–100% | 50% | Dry/wet mix |

---

## Pneumatica — High-End Tailoring Pedal
Adds width, crunch, noise, and shimmer to the high frequencies. For polish, air, and analog character.

| Parameter | Range | Default | Description |
|---|---|---|---|
| CROSSOVER | 500–16000 Hz | 3000 Hz | Crossover frequency (splits low/high) |
| WIDTH | 0–200% | 100% | Stereo width of high band |
| SHIMMER | 0–100% | 0% | Shimmer effect amount |
| CRUNCH | 0–100% | 0% | Saturation amount |
| CRUNCH_TYPE | Soft, Warm, Crispy | Soft | Saturation character |
| NOISE_LEVEL | -80 to -20 dB | -80 dB | Added noise level |
| NOISE_TYPE | Analog Hiss, Tape Hiss, Digital Crackle | Analog Hiss | Noise character |
| NOISE_FILTER | 500–20000 Hz | 8000 Hz | Noise filter cutoff |
| AIR_FREQ | 4000–20000 Hz | 12000 Hz | Air EQ center frequency |
| AIR_GAIN | -12 to 12 dB | 0 dB | Air EQ gain |
| INPUT_GAIN | -48 to 24 dB | 0 dB | Input gain |
| OUTPUT_GAIN | -48 to 24 dB | 0 dB | Output gain |
| MIX | 0–100% | 50% | Dry/wet mix |

---

## Mechanica — Nerdy Distortion
Deep-tweakable distortion with multiple waveshapers, pre/post EQ, oversampling, feedback loop, and noise gate.

| Parameter | Range | Default | Description |
|---|---|---|---|
| DRIVE | 0–60 dB | 12 dB | Distortion drive |
| WAVESHAPER | Tanh, Atan, Foldback, Hard Clip, Tube, Rectify | Tanh | Waveshaper algorithm |
| BIAS | -1 to 1 | 0 | DC bias into waveshaper |
| ASYMMETRY | 0–100% | 0% | Asymmetric clipping amount |
| STAGES | 1–3 | 1 | Cascaded distortion stages |
| OVERSAMPLING | Off, 2x, 4x, 8x | 2x | Oversampling rate |
| PRE_LOW | -12 to 12 dB | 0 dB | Pre-EQ low shelf |
| PRE_MID | -12 to 12 dB | 0 dB | Pre-EQ mid peak |
| PRE_MID_FREQ | 200–5000 Hz | 800 Hz | Pre-EQ mid frequency |
| PRE_MID_Q | 0.1–10 | 1.0 | Pre-EQ mid Q |
| PRE_HIGH | -12 to 12 dB | 0 dB | Pre-EQ high shelf |
| POST_LOW | -12 to 12 dB | 0 dB | Post-EQ low shelf |
| POST_MID | -12 to 12 dB | 0 dB | Post-EQ mid peak |
| POST_MID_FREQ | 200–5000 Hz | 800 Hz | Post-EQ mid frequency |
| POST_MID_Q | 0.1–10 | 1.0 | Post-EQ mid Q |
| POST_HIGH | -12 to 12 dB | 0 dB | Post-EQ high shelf |
| FEEDBACK | 0–80% | 0% | Feedback loop amount |
| FEEDBACK_TONE | 200–8000 Hz | 2000 Hz | Feedback loop filter |
| GATE_THRESHOLD | -80 to 0 dB | -80 dB | Noise gate threshold |
| GATE_RELEASE | 10–500 ms | 50 ms | Noise gate release |
| INPUT_GAIN | -48 to 24 dB | 0 dB | Input gain |
| OUTPUT_GAIN | -48 to 24 dB | 0 dB | Output gain |
| MIX | 0–100% | 50% | Dry/wet mix |

---

## Electrica — Synth Pedal
Pitch-tracks input (guitar/vocal), layers a synth oscillator on top, then distorts and compresses. Full MIDI output with scale lock.

### Oscillator
| Parameter | Range | Default | Description |
|---|---|---|---|
| OSC_WAVE | Saw, Square, Sine, Triangle | Saw | Oscillator waveform |
| OSC_OCTAVE | -2 to +2 | 0 | Octave shift |
| OSC_DETUNE | -100 to 100 cents | 0 | Fine detune |
| OSC_LEVEL | -48 to 12 dB | 0 dB | Oscillator level |

### Envelope
| Parameter | Range | Default | Description |
|---|---|---|---|
| ENV_ATTACK | 1–500 ms | 10 ms | Synth envelope attack |
| ENV_RELEASE | 10–2000 ms | 200 ms | Synth envelope release |
| ENV_SENSITIVITY | -60 to -10 dB | -40 dB | Envelope sensitivity threshold |

### Filter
| Parameter | Range | Default | Description |
|---|---|---|---|
| FILTER_TYPE | LPF, HPF, BPF | LPF | Filter type |
| FILTER_FREQ | 50–16000 Hz | 2000 Hz | Filter cutoff |
| FILTER_RESO | 0.1–10 Q | 0.707 | Filter resonance |

### Distortion & Compression
| Parameter | Range | Default | Description |
|---|---|---|---|
| DIST_DRIVE | 0–40 dB | 0 dB | Distortion drive |
| DIST_TYPE | Soft, Hard, Fold | Soft | Distortion type |
| COMP_AMOUNT | 0–100% | 0% | Compressor amount |
| COMP_SPEED | Fast, Medium, Slow | Medium | Compressor speed |

### Tracking & Input
| Parameter | Range | Default | Description |
|---|---|---|---|
| INPUT_MODE | Guitar, Vocal | Guitar | Input type preset |
| TRACKING | Mono, Poly | Mono | Mono or polyphonic tracking |
| GLIDE | 0–500 ms | 20 ms | Pitch glide/portamento |
| SNAP_TO_NOTE | On/Off | Off | Snap to nearest semitone |
| PITCH_ALGO | MPM, Cycfi Q | MPM | Pitch detection algorithm |
| POLY_PEAK_GATE | -80 to -10 dB | -40 dB | Poly mode peak gate threshold |

### Advanced Pitch Detection
| Parameter | Range | Default | Description |
|---|---|---|---|
| YIN_WINDOW | 5–50 ms | 20 ms | Detection window size |
| YIN_THRESHOLD | 0.05–0.50 | 0.20 | Detection confidence threshold |
| CONFIDENCE_GATE | 0.05–0.95 | 0.30 | Minimum confidence to track |

### MIDI Output
| Parameter | Range | Default | Description |
|---|---|---|---|
| MIDI_OUT | On/Off | Off | Enable MIDI output |
| MIDI_CHANNEL | 1–16 | 1 | MIDI output channel |
| MIDI_POLY_SPREAD | On/Off | Off | Spread poly voices across channels |
| MIDI_VEL_OVERRIDE | On/Off | Off | Override velocity with fixed value |
| MIDI_VEL_VALUE | 1–127 | 100 | Fixed velocity value |
| MIDI_VEL_CURVE | Linear, Soft, Hard, S-Curve | Linear | Velocity response curve |
| MIDI_SCALE_LOCK | On/Off | Off | Quantize to scale |
| MIDI_SCALE_ROOT | C through B | C | Scale root note |
| MIDI_SCALE_TYPE | Chromatic, Major, Minor, Harmonic Min, Dorian, Phrygian, Lydian, Mixolydian, Whole Tone, Blues, Penta Maj, Penta Min | Major | Scale type |
| MIDI_GATE | -80 to -10 dB | -60 dB | MIDI note-on gate threshold |
| MIDI_NOTEOFF_DELAY | 0–200 ms | 0 ms | Delay before note-off |
| MIDI_TRANSPOSE | -24 to +24 semi | 0 | MIDI transpose |
| MIDI_NOTE_MIN | 0–127 | 0 | Minimum MIDI note |
| MIDI_NOTE_MAX | 0–127 | 127 | Maximum MIDI note |
| MIDI_OCTAVE_LOCK | On/Off | Off | Lock output to single octave |
| MIDI_RETRIGGER | Retrigger, Legato | Retrigger | Note articulation mode |
| MIDI_NOTE_HOLD | On/Off | Off | Hold last note on silence |
| MIDI_TRANSIENT_SENS | 0–100% | 50% | Transient detection sensitivity |
| MIDI_TRANSIENT_HOLD | 20–500 ms | 50 ms | Transient hold time |
| MIDI_MIN_NOTE_DUR | 20–500 ms | 50 ms | Minimum note duration |
| MIDI_PITCHBEND | On/Off | Off | Send pitch bend |
| MIDI_PITCHBEND_RANGE | 2 semi, 12 semi, 24 semi | 2 semi | Pitch bend range |
| MIDI_CC_ENABLE | On/Off | Off | Send CC from envelope |
| MIDI_CC_NUMBER | 0–127 | 1 | CC number to send |
| MIDI_CC_SMOOTH | 1–100 ms | 10 ms | CC smoothing time |

### Common
| Parameter | Range | Default | Description |
|---|---|---|---|
| INPUT_GAIN | -48 to 24 dB | 0 dB | Input gain |
| OUTPUT_GAIN | -48 to 24 dB | 0 dB | Output gain |
| MIX | 0–100% | 50% | Dry/wet mix |
