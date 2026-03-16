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
    midiSettingsBtn.setBounds (btnArea.removeFromTop (btnArea.getHeight() / 2).reduced (2));
    advancedBtn.setBounds (btnArea.reduced (2));
}

void PluginEditor::buildMidiPopup()
{
    using CT = DirektDSP::ControlType;

    std::vector<DirektDSP::SectionDescriptor> midiDescriptors = {
        { "Enable & Channel", {
            { "MIDI_OUT",          "MIDI Output",   CT::Toggle },
            { "MIDI_CHANNEL",      "Channel",       CT::Knob },
            { "MIDI_POLY_SPREAD",  "Poly Spread",   CT::Toggle },
        }, 3 },

        { "Velocity", {
            { "MIDI_VEL_OVERRIDE", "Override",  CT::Toggle },
            { "MIDI_VEL_VALUE",    "Value",     CT::Knob },
            { "MIDI_VEL_CURVE",    "Curve",     CT::ComboBox },
        }, 3 },

        { "Scale Lock", {
            { "MIDI_SCALE_LOCK", "Lock",  CT::Toggle },
            { "MIDI_SCALE_ROOT", "Root",  CT::ComboBox },
            { "MIDI_SCALE_TYPE", "Scale", CT::ComboBox },
        }, 3 },

        { "Gate & Timing", {
            { "MIDI_GATE",         "Gate",           CT::Knob },
            { "MIDI_NOTEOFF_DELAY","Note-Off Delay", CT::Knob },
            { "MIDI_MIN_NOTE_DUR", "Min Duration",   CT::Knob },
        }, 3 },

        { "Transpose & Range", {
            { "MIDI_TRANSPOSE",   "Transpose", CT::Knob },
            { "MIDI_NOTE_MIN",    "Note Min",  CT::Knob },
            { "MIDI_NOTE_MAX",    "Note Max",  CT::Knob },
            { "MIDI_OCTAVE_LOCK", "Oct Lock",  CT::Toggle },
        }, 4 },

        { "Articulation", {
            { "MIDI_RETRIGGER",      "Retrigger",      CT::ComboBox },
            { "MIDI_NOTE_HOLD",      "Note Hold",      CT::Toggle },
            { "MIDI_TRANSIENT_SENS", "Transient Sens",  CT::Knob },
            { "MIDI_TRANSIENT_HOLD", "Transient Hold",  CT::Knob },
        }, 4 },

        { "Pitch Bend", {
            { "MIDI_PITCHBEND",       "Enable",  CT::Toggle },
            { "MIDI_PITCHBEND_RANGE", "Range",   CT::ComboBox },
        }, 2 },

        { "CC Output", {
            { "MIDI_CC_ENABLE", "Enable",    CT::Toggle },
            { "MIDI_CC_NUMBER", "CC #",      CT::Knob },
            { "MIDI_CC_SMOOTH", "Smoothing", CT::Knob },
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
            { "YIN_WINDOW",      "Det. Window",    CT::Knob },
            { "YIN_THRESHOLD",   "Tolerance",      CT::Knob },
            { "CONFIDENCE_GATE", "Confidence Gate", CT::Knob },
            { "POLY_PEAK_GATE",  "Poly Peak Gate", CT::Knob },
        }, 4 },
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
            { "OSC_WAVE",   "Wave",   CT::ComboBox },
            { "OSC_OCTAVE", "Octave", CT::Knob },
            { "OSC_DETUNE", "Detune", CT::Knob },
            { "OSC_LEVEL",  "Level",  CT::Knob },
        }, 4 },

        { "Envelope", {
            { "ENV_ATTACK",      "Attack",      CT::Knob },
            { "ENV_RELEASE",     "Release",     CT::Knob },
            { "ENV_SENSITIVITY", "Sensitivity", CT::Knob },
        }, 3 },

        { "Filter", {
            { "FILTER_TYPE", "Type", CT::ComboBox },
            { "FILTER_FREQ", "Freq", CT::Knob },
            { "FILTER_RESO", "Reso", CT::Knob },
        }, 3 },

        { "Distortion", {
            { "DIST_DRIVE", "Drive", CT::Knob },
            { "DIST_TYPE",  "Type",  CT::ComboBox },
        }, 2 },

        { "Compressor", {
            { "COMP_AMOUNT", "Amount", CT::Knob },
            { "COMP_SPEED",  "Speed",  CT::ComboBox },
        }, 2 },

        { "Tracking", {
            { "GLIDE",        "Glide",  CT::Knob },
            { "TRACKING",     "Mode",   CT::ComboBox },
            { "SNAP_TO_NOTE", "Snap",   CT::Toggle },
            { "PITCH_ALGO",   "Algo",   CT::ComboBox },
            { "INPUT_MODE",   "Input",  CT::ComboBox },
        }, 5 },

        { "Common", {
            { "INPUT_GAIN",  "Input",  CT::Knob },
            { "OUTPUT_GAIN", "Output", CT::Knob },
            { "MIX",         "Mix",    CT::Knob },
        }, 3 },
    };
}
