#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : DirektBaseEditor (p, p.apvts, p.getPresetManager(),
                        "Electrica", DirektDSP::Colours::accentBlue,
                        16.0f / 10.0f, 900, createDescriptors()),
      apvtsRef (p.apvts)
{
    buildMidiPopup();
    buildAdvancedPopup();

    midiSettingsBtn.setColour (juce::TextButton::buttonColourId, DirektDSP::Colours::bgSection);
    midiSettingsBtn.setColour (juce::TextButton::textColourOffId, DirektDSP::Colours::accentBlue);
    midiSettingsBtn.onClick = [this] {
        showPopup ("MIDI Settings", midiPopupContent.get(), 600, 450);
    };
    addAndMakeVisible (midiSettingsBtn);

    advancedBtn.setColour (juce::TextButton::buttonColourId, DirektDSP::Colours::bgSection);
    advancedBtn.setColour (juce::TextButton::textColourOffId, DirektDSP::Colours::accentBlue);
    advancedBtn.onClick = [this] {
        showPopup ("Advanced Tracking", advancedPopupContent.get(), 400, 200);
    };
    addAndMakeVisible (advancedBtn);

    debugBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF8B0000));
    debugBtn.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    debugBtn.onClick = [this, &p] {
        if (debugOverlay == nullptr)
        {
            auto& dsp = p.getDSPProcessor();
            debugOverlay = std::make_unique<DebugOverlay> (
                dsp.debugData, dsp.midiActivityNote,
                dsp.midiActivityChannel, dsp.midiActivityVelocity);
        }
        showPopup ("Debug Overlay", debugOverlay.get(), 500, 400);
    };
    addAndMakeVisible (debugBtn);

    setSize (900, 563);
}

void PluginEditor::layoutCustomSections (juce::Rectangle<int> mainArea)
{
    auto area = mainArea.reduced (8);

    auto* oscSection      = findSection ("Oscillator");
    auto* envSection      = findSection ("Envelope");
    auto* filterSection   = findSection ("Filter");
    auto* distSection     = findSection ("Distortion");
    auto* compSection     = findSection ("Compressor");
    auto* trackSection    = findSection ("Tracking");
    auto* commonSection   = findSection ("Common");

    if (oscSection == nullptr || commonSection == nullptr)
    {
        DirektBaseEditor::layoutCustomSections (mainArea);
        return;
    }

    // Row 1: Oscillator (full width, ~25%)
    auto row1 = area.removeFromTop (static_cast<int>(static_cast<float>(area.getHeight()) * 0.25f));
    if (oscSection) oscSection->setBounds (row1.reduced (0, 2));

    // Row 2: Envelope | Filter (~25%)
    auto row2 = area.removeFromTop (static_cast<int>(static_cast<float>(area.getHeight()) * 0.33f));
    auto row2Left = row2.removeFromLeft (row2.getWidth() / 2);
    if (envSection) envSection->setBounds (row2Left.reduced (2));
    if (filterSection) filterSection->setBounds (row2.reduced (2));

    // Row 3: Distortion | Compressor (~25%)
    auto row3 = area.removeFromTop (static_cast<int>(static_cast<float>(area.getHeight()) * 0.50f));
    auto row3Left = row3.removeFromLeft (row3.getWidth() / 2);
    if (distSection) distSection->setBounds (row3Left.reduced (2));
    if (compSection) compSection->setBounds (row3.reduced (2));

    // Row 4: Tracking | Common | Buttons (rest)
    auto row4 = area;
    auto trackW = row4.getWidth() * 2 / 5;
    auto commonW = row4.getWidth() * 2 / 5;
    auto btnW = row4.getWidth() - trackW - commonW;

    if (trackSection) trackSection->setBounds (row4.removeFromLeft (trackW).reduced (2));
    if (commonSection) commonSection->setBounds (row4.removeFromLeft (commonW).reduced (2));

    // Buttons in remaining space
    auto btnArea = row4.reduced (4);
    int btnH = btnArea.getHeight() / 3;
    midiSettingsBtn.setBounds (btnArea.removeFromTop (btnH).reduced (2));
    advancedBtn.setBounds (btnArea.removeFromTop (btnH).reduced (2));
    debugBtn.setBounds (btnArea.reduced (2));
}

void PluginEditor::buildMidiPopup()
{
    using CT = DirektDSP::ControlType;

    std::vector<DirektDSP::SectionDescriptor> midiDescriptors = {
        { "Enable & Channel", {
            { "MIDI_OUT",          "MIDI Output",   CT::Toggle, "Enable MIDI note output from pitch tracking" },
            { "MIDI_CHANNEL",      "Channel",       CT::Knob,   "MIDI output channel (1-16)" },
            { "MIDI_POLY_SPREAD",  "Poly Spread",   CT::Toggle, "Spread polyphonic voices across consecutive MIDI channels" },
        }, 3 },

        { "Velocity", {
            { "MIDI_VEL_OVERRIDE", "Override",  CT::Toggle,   "Override detected velocity with a fixed value" },
            { "MIDI_VEL_VALUE",    "Value",     CT::Knob,     "Fixed velocity value when override is enabled" },
            { "MIDI_VEL_CURVE",    "Curve",     CT::ComboBox, "Velocity response curve shape" },
        }, 3 },

        { "Scale Lock", {
            { "MIDI_SCALE_LOCK", "Lock",  CT::Toggle,   "Quantize detected notes to the selected scale" },
            { "MIDI_SCALE_ROOT", "Root",  CT::ComboBox, "Root note of the scale" },
            { "MIDI_SCALE_TYPE", "Scale", CT::ComboBox, "Scale type (Major, Minor, Pentatonic, etc.)" },
        }, 3 },

        { "Gate & Timing", {
            { "MIDI_GATE",         "Gate",           CT::Knob, "Input level threshold for triggering MIDI notes" },
            { "MIDI_NOTEOFF_DELAY","Note-Off Delay", CT::Knob, "Delay before sending note-off after signal drops" },
            { "MIDI_MIN_NOTE_DUR", "Min Duration",   CT::Knob, "Minimum note duration — prevents rapid retriggers" },
        }, 3 },

        { "Transpose & Range", {
            { "MIDI_TRANSPOSE",   "Transpose", CT::Knob,   "Transpose MIDI output in semitones" },
            { "MIDI_NOTE_MIN",    "Note Min",  CT::Knob,   "Lowest allowed MIDI note number" },
            { "MIDI_NOTE_MAX",    "Note Max",  CT::Knob,   "Highest allowed MIDI note number" },
            { "MIDI_OCTAVE_LOCK", "Oct Lock",  CT::Toggle, "Lock output to a single octave range" },
        }, 4 },

        { "Articulation", {
            { "MIDI_RETRIGGER",      "Retrigger",      CT::ComboBox, "When to retrigger notes (on new pitch, on transient, etc.)" },
            { "MIDI_NOTE_HOLD",      "Note Hold",      CT::Toggle,   "Hold notes until a new pitch is detected" },
            { "MIDI_TRANSIENT_SENS", "Transient Sens",  CT::Knob,    "Sensitivity of the transient detector for retriggers" },
            { "MIDI_TRANSIENT_HOLD", "Transient Hold",  CT::Knob,    "Hold time after a transient before allowing retrigger" },
        }, 4 },

        { "Pitch Bend", {
            { "MIDI_PITCHBEND",       "Enable",  CT::Toggle,   "Send pitch bend messages for smooth pitch transitions" },
            { "MIDI_PITCHBEND_RANGE", "Range",   CT::ComboBox, "Pitch bend range in semitones (must match synth setting)" },
        }, 2 },

        { "CC Output", {
            { "MIDI_CC_ENABLE", "Enable",    CT::Toggle, "Send input envelope as a MIDI CC message" },
            { "MIDI_CC_NUMBER", "CC #",      CT::Knob,   "MIDI CC number to send envelope data on" },
            { "MIDI_CC_SMOOTH", "Smoothing", CT::Knob,   "Smoothing applied to CC output to reduce jitter" },
        }, 3 },
    };

    midiSections = DirektDSP::DirektAutoLayout::buildSections (apvtsRef, midiDescriptors);

    midiPopupContent = std::make_unique<PopupContentPanel>();
    midiPopupContent->setColumns (2);
    for (auto& bs : midiSections)
        midiPopupContent->addSection (bs.section.get());
}

void PluginEditor::buildAdvancedPopup()
{
    using CT = DirektDSP::ControlType;

    std::vector<DirektDSP::SectionDescriptor> advDescriptors = {
        { "Advanced Tracking", {
            { "YIN_WINDOW",       "Det. Window",     CT::Knob,     "Detection window size in ms — larger = more accurate but higher latency" },
            { "YIN_THRESHOLD",    "Tolerance",       CT::Knob,     "Pitch detection tolerance — lower = stricter, fewer false notes" },
            { "CONFIDENCE_GATE",  "Confidence Gate", CT::Knob,     "Minimum confidence to accept a pitch — raise to reject uncertain detections" },
            { "POLY_PEAK_GATE",   "Poly Peak Gate",  CT::Knob,     "Threshold for spectral peaks in polyphonic mode — raise to reduce ghost notes" },
            { "POLY_INSTRUMENT",  "Poly Instrument", CT::ComboBox, "Spectral dictionary for polyphonic detection (match to your instrument)" },
        }, 5 },
    };

    advancedSections = DirektDSP::DirektAutoLayout::buildSections (apvtsRef, advDescriptors);

    advancedPopupContent = std::make_unique<PopupContentPanel>();
    advancedPopupContent->setColumns (1);
    for (auto& bs : advancedSections)
        advancedPopupContent->addSection (bs.section.get());
}

std::vector<DirektDSP::SectionDescriptor> PluginEditor::createDescriptors()
{
    using CT = DirektDSP::ControlType;
    return {
        { "Oscillator", {
            { "OSC_WAVE",   "Wave",   CT::ComboBox, "Synth oscillator waveform (Sine, Saw, Square, Triangle)" },
            { "OSC_OCTAVE", "Octave", CT::Knob,     "Oscillator octave offset from detected pitch" },
            { "OSC_DETUNE", "Detune", CT::Knob,     "Fine detune in cents — adds thickness" },
            { "OSC_LEVEL",  "Level",  CT::Knob,     "Synth oscillator volume level" },
        }, 4 },

        { "Envelope", {
            { "ENV_ATTACK",      "Attack",      CT::Knob, "Synth amplitude attack time" },
            { "ENV_RELEASE",     "Release",     CT::Knob, "Synth amplitude release time" },
            { "ENV_SENSITIVITY", "Sensitivity", CT::Knob, "How closely the synth envelope follows input dynamics" },
        }, 3 },

        { "Filter", {
            { "FILTER_TYPE", "Type", CT::ComboBox, "Filter type (Low-pass, High-pass, Band-pass)" },
            { "FILTER_FREQ", "Freq", CT::Knob,     "Filter cutoff frequency" },
            { "FILTER_RESO", "Reso", CT::Knob,     "Filter resonance — adds emphasis at the cutoff" },
        }, 3 },

        { "Distortion", {
            { "DIST_DRIVE", "Drive", CT::Knob,     "Distortion drive amount applied to the synth signal" },
            { "DIST_TYPE",  "Type",  CT::ComboBox, "Distortion waveshaper type" },
        }, 2 },

        { "Compressor", {
            { "COMP_AMOUNT", "Amount", CT::Knob,     "Compression amount — evens out synth dynamics" },
            { "COMP_SPEED",  "Speed",  CT::ComboBox, "Compressor response speed (Fast, Medium, Slow)" },
        }, 2 },

        { "Tracking", {
            { "GLIDE",        "Glide",     CT::Knob,     "Portamento time between detected pitches" },
            { "TRACKING",     "Mode",      CT::ComboBox, "Tracking mode — Mono (single voice) or Poly (up to 6 voices)" },
            { "SNAP_TO_NOTE", "Snap",      CT::Toggle,   "Quantize tracked pitch to nearest semitone" },
            { "PITCH_ALGO",   "Algo",      CT::ComboBox, "Mono pitch detection algorithm (MPM or Cycfi Q)" },
            { "POLY_ALGO",    "Poly Algo", CT::ComboBox, "Polyphonic detection algorithm" },
            { "INPUT_MODE",   "Input",     CT::ComboBox, "Input routing (Mono L, Mono R, Stereo sum)" },
        }, 6 },

        { "Common", {
            { "INPUT_GAIN",  "Input",  CT::Knob, "Input gain before processing" },
            { "OUTPUT_GAIN", "Output", CT::Knob, "Output gain after processing" },
            { "MIX",         "Mix",    CT::Knob, "Dry/wet mix — 0% = clean signal, 100% = synth only" },
        }, 3 },
    };
}
