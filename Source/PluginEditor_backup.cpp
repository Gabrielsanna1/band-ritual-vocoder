#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================

ColorVocoderAudioProcessorEditor::ColorVocoderAudioProcessorEditor (ColorVocoderAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      presetManager (p.apvts),
      bandDisplay (p)
{
    // ===== PRESET UI =====
    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("Select Preset...");
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.setWantsKeyboardFocus (false);  // Don't steal focus from DAW
    presetBox.setMouseClickGrabsKeyboardFocus (false);
    refreshPresetList();

    presetBox.onChange = [this]()
    {
        loadSelectedPreset();
    };

    addAndMakeVisible (savePresetButton);
    savePresetButton.setWantsKeyboardFocus (false);  // Don't steal focus from DAW
    savePresetButton.setMouseClickGrabsKeyboardFocus (false);
    savePresetButton.onClick = [this]()
    {
        savePresetWithDialog();
    };

    addAndMakeVisible (deletePresetButton);
    deletePresetButton.setColour (juce::TextButton::buttonColourId, juce::Colours::darkred.darker());
    deletePresetButton.setWantsKeyboardFocus (false);  // Don't steal focus from DAW
    deletePresetButton.setMouseClickGrabsKeyboardFocus (false);
    deletePresetButton.onClick = [this]()
    {
        deleteSelectedPreset();
    };

    // Bypass
    addAndMakeVisible (bypassButton);
    bypassButton.setButtonText ("Bypass");
    bypassButton.setWantsKeyboardFocus (false);  // Don't steal focus from DAW
    bypassButton.setMouseClickGrabsKeyboardFocus (false);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "bypass", bypassButton);

    // Band visual
    addAndMakeVisible (bandDisplay);

    // Group boxes
    addAndMakeVisible (globalGroup);
    addAndMakeVisible (scaleGroup);
    addAndMakeVisible (harmonicGroup);
    addAndMakeVisible (bandsGroup);
    addAndMakeVisible (noiseGroup);
    addAndMakeVisible (carrierGroup);  // NEW

    // Global
    setupKnob (mixSlider,   mixLabel,   "Mix");
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "mix", mixSlider);

    setupKnob (depthSlider, depthLabel, "Depth");
    depthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "depth", depthSlider);

    // Scale & Formant
    setupComboWithLabel (keyBox, keyLabel, "Key");
    keyBox.addItemList (juce::StringArray { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }, 1);
    keyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, "key", keyBox);

    setupComboWithLabel (scaleBox, scaleLabel, "Scale");
    scaleBox.addItemList (juce::StringArray { "Major","Minor" }, 1);
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, "scaleType", scaleBox);

    setupKnob (scaleAmtSlider, scaleAmtLabel, "Scale Amt");
    scaleAmtAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "scaleAmount", scaleAmtSlider);

    setupKnob (formantSlider, formantLabel, "Formant");
    formantAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "formant", formantSlider);

    // ===== CHORD CONTROLS =====
    setupComboWithLabel (chordDegreeBox, chordDegreeLabel, "Chord");
    chordDegreeBox.addItemList (juce::StringArray { "I", "ii", "iii", "IV", "V", "vi", "vii" }, 1);
    chordDegreeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, "chordDegree", chordDegreeBox);

    setupComboWithLabel (chordVariationBox, chordVariationLabel, "Variation");
    chordVariationBox.addItemList (juce::StringArray { "Triad", "Power", "7th", "9th", "sus2", "sus4", "add9" }, 1);
    chordVariationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, "chordVariation", chordVariationBox);

    setupKnob (chordAmountSlider, chordAmountLabel, "Chord Amt");
    chordAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "chordAmount", chordAmountSlider);

    // Harmonics (only Shift)
    setupKnob (harmonicShiftSlider, harmonicShiftLabel, "Harm Shift");
    harmonicShiftSlider.setRange (-12.0, 12.0, 1.0);
    harmonicShiftAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "harmonicShift", harmonicShiftSlider);

    // Bands
    setupKnob (bandCountSlider, bandCountLabel, "Bands");
    bandCountSlider.setRange (2, 64, 1);
    bandCountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "bandCount", bandCountSlider);

    setupKnob (bandWidthSlider, bandWidthLabel, "Width");
    bandWidthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "bandWidth", bandWidthSlider);

    setupHzKnob (lowFreqSlider, lowFreqLabel, "Low Freq");
    lowFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "lowFreq", lowFreqSlider);

    setupHzKnob (highFreqSlider, highFreqLabel, "High Freq");
    highFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "highFreq", highFreqSlider);

    setupMsKnob (attackSlider, attackLabel, "Attack");
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "attackMs", attackSlider);

    setupMsKnob (releaseSlider, releaseLabel, "Release");
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "releaseMs", releaseSlider);

    // ===== NOISE / UNVOICED (Ableton-style: single Unvoiced control) =====
    setupKnob (noiseColorSlider, noiseColorLabel, "Noise Color");
    noiseColorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "noiseColor", noiseColorSlider);

    setupKnob (hpBlendSlider, hpBlendLabel, "HP Blend");
    hpBlendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "hpBlend", hpBlendSlider);

    // HP Blend Slope combo box (below HP Blend knob)
    setupComboWithLabel (hpBlendSlopeBox, hpBlendSlopeLabel, "Slope");
    hpBlendSlopeBox.addItemList (juce::StringArray { "12dB", "24dB", "48dB" }, 1);
    hpBlendSlopeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, "hpBlendSlope", hpBlendSlopeBox);

    setupKnob (unvoicedSlider, unvoicedLabel, "Unvoiced");
    unvoicedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "unvoiced", unvoicedSlider);

    // Gate (now in Bands section conceptually, but still set up here)
    setupKnob (gateSlider, gateLabel, "Gate");
    gateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "gate", gateSlider);

    // ===== CARRIER controls =====
    setupComboWithLabel (carrierModeBox, carrierModeLabel, "Mode");
    carrierModeBox.addItemList (juce::StringArray { "Self", "External", "Noise", "Saw", "Square", "Pitch Track" }, 1);
    carrierModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, "carrierMode", carrierModeBox);

    setupHzKnob (carrierPitchSlider, carrierPitchLabel, "Pitch");
    carrierPitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "carrierPitch", carrierPitchSlider);

    // ===== INPUT FILTER SECTION (Visual Display with Spectrum Analyzer) =====
    inputFilterDisplay = std::make_unique<InputFilterDisplay> (processor.apvts, processor);
    addAndMakeVisible (*inputFilterDisplay);

    // When user clicks a different band, update knob value
    // HP/LP: controls Q width (0.5-10), Bell: controls Q (0.5-10)
    // All use same 0.5-10 range now!
    // Selection: 1=HP, 2=LP, 3=Bell1, 4=Bell2, 5=CompBell1, 6=CompBell2
    inputFilterDisplay->onBandSelected = [this](int selected)
    {
        if (selected == 1 || selected == 2)
        {
            // HP or LP selected - show Q width value (controls resonance peak width)
            const char* paramId = (selected == 1) ? "inputHPQWidth" : "inputLPQWidth";
            auto* param = processor.apvts.getParameter (paramId);
            if (param != nullptr)
            {
                float qValue = param->convertFrom0to1 (param->getValue());
                bellQSlider.setValue (qValue, juce::dontSendNotification);
            }
        }
        else if (selected == 5 || selected == 6)
        {
            // Complement Bell selected - show compBell Q value
            const char* paramId = (selected == 5) ? "compBell1Q" : "compBell2Q";
            auto* param = processor.apvts.getParameter (paramId);
            if (param != nullptr)
            {
                float qValue = param->convertFrom0to1 (param->getValue());
                bellQSlider.setValue (qValue, juce::dontSendNotification);
            }
        }
        else
        {
            // Normal Bell selected - show bell Q value
            const char* paramId = (selected == 3) ? "bell1Q" : "bell2Q";
            auto* param = processor.apvts.getParameter (paramId);
            if (param != nullptr)
            {
                float qValue = param->convertFrom0to1 (param->getValue());
                bellQSlider.setValue (qValue, juce::dontSendNotification);
            }
        }
    };

    // ===== EQ KNOB (HP/LP: Q width, Bell: Q) - all control peak width =====
    setupKnob (bellQSlider, bellQLabel, "Width");
    // Unified range 0.5-10 for all bands, with skew 0.5 to match parameters
    bellQSlider.setRange (0.5, 10.0, 0.01);
    bellQSlider.setSkewFactor (0.5);  // Match parameter skew - more resolution at low end
    // Start with HP Q width
    auto* initQPtr = processor.apvts.getRawParameterValue ("inputHPQWidth");
    bellQSlider.setValue (initQPtr ? initQPtr->load() : 1.25, juce::dontSendNotification);

    // Dynamic control - all bands use Q (0.5-10) for peak width
    // Selection: 1=HP, 2=LP, 3=Bell1, 4=Bell2, 5=CompBell1, 6=CompBell2
    bellQSlider.onValueChange = [this]()
    {
        float value = (float) bellQSlider.getValue();
        int selected = inputFilterDisplay ? inputFilterDisplay->getSelectedBand() : 1;

        if (selected == 1 || selected == 2)
        {
            // HP/LP - knob controls Q width (resonance peak width)
            const char* paramId = (selected == 1) ? "inputHPQWidth" : "inputLPQWidth";
            float q = juce::jlimit (0.5f, 10.0f, value);
            auto* param = processor.apvts.getParameter (paramId);
            if (param != nullptr)
                param->setValueNotifyingHost (param->convertTo0to1 (q));
        }
        else if (selected == 5 || selected == 6)
        {
            // Complement Bell - knob controls Q directly
            const char* paramId = (selected == 5) ? "compBell1Q" : "compBell2Q";
            float q = juce::jlimit (0.5f, 10.0f, value);
            auto* param = processor.apvts.getParameter (paramId);
            if (param != nullptr)
                param->setValueNotifyingHost (param->convertTo0to1 (q));
        }
        else
        {
            // Normal Bell - knob controls Q directly (0.5-10 range)
            const char* paramId = (selected == 3) ? "bell1Q" : "bell2Q";
            float q = juce::jlimit (0.5f, 10.0f, value);
            auto* param = processor.apvts.getParameter (paramId);
            if (param != nullptr)
                param->setValueNotifyingHost (param->convertTo0to1 (q));
        }
    };

    // ===== Licensing UI =====
    addAndMakeVisible (licenseEditor);
    licenseEditor.setTextToShowWhenEmpty ("Enter Gumroad license key", juce::Colours::grey);
    licenseEditor.setInputRestrictions (64);

    addAndMakeVisible (activateButton);
    activateButton.setButtonText ("Activate");

    addAndMakeVisible (licenseStatusLabel);
    licenseStatusLabel.setJustificationType (juce::Justification::centredLeft);
    licenseStatusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);

    // Initialise visibility + text depending on whether there's already a license on disk
    updateLicenseUI();

    activateButton.onClick = [this]()
    {
        auto key = licenseEditor.getText().trim();
        bool ok  = processor.getLicenseManager().tryActivate (key);

        if (ok)
        {
            licenseEditor.clear();
            updateLicenseUI(); // hide the controls once activated
        }
        else
        {
            licenseStatusLabel.setText (processor.getLicenseManager().getLastError(),
                                        juce::dontSendNotification);
        }
    };

    // Set size at END so resized() sees all components
    setSize (820, 620);  // Taller window for 200px EQ display
    setResizable (false, false);

    // IMPORTANT: Don't steal keyboard focus from the DAW
    // This prevents the issue where clicking a knob stops keyboard/MIDI input to Ableton
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
}


//==============================================================

ColorVocoderAudioProcessorEditor::~ColorVocoderAudioProcessorEditor()
{
    // Remove look and feel from all sliders before destruction
    mixSlider.setLookAndFeel (nullptr);
    depthSlider.setLookAndFeel (nullptr);
    scaleAmtSlider.setLookAndFeel (nullptr);
    formantSlider.setLookAndFeel (nullptr);
    harmonicShiftSlider.setLookAndFeel (nullptr);
    bandCountSlider.setLookAndFeel (nullptr);
    bandWidthSlider.setLookAndFeel (nullptr);
    lowFreqSlider.setLookAndFeel (nullptr);
    highFreqSlider.setLookAndFeel (nullptr);
    attackSlider.setLookAndFeel (nullptr);
    releaseSlider.setLookAndFeel (nullptr);
    gateSlider.setLookAndFeel (nullptr);
    noiseColorSlider.setLookAndFeel (nullptr);
    hpBlendSlider.setLookAndFeel (nullptr);
    unvoicedSlider.setLookAndFeel (nullptr);
    carrierPitchSlider.setLookAndFeel (nullptr);
    bellQSlider.setLookAndFeel (nullptr);
    chordAmountSlider.setLookAndFeel (nullptr);
}

void ColorVocoderAudioProcessorEditor::setupKnob (Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);  // No text box to avoid crash
    slider.setMouseDragSensitivity (150);
    slider.setWantsKeyboardFocus (false);  // Don't steal focus from DAW
    slider.setMouseClickGrabsKeyboardFocus (false);

    addAndMakeVisible (slider);

    addAndMakeVisible (label);
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white);
    label.setFont (11.0f);
}

void ColorVocoderAudioProcessorEditor::setupHzKnob (Slider& slider, juce::Label& label, const juce::String& text)
{
    setupKnob (slider, label, text);
    slider.setTextValueSuffix (" Hz");
}

void ColorVocoderAudioProcessorEditor::setupMsKnob (Slider& slider, juce::Label& label, const juce::String& text)
{
    setupKnob (slider, label, text);
    slider.setTextValueSuffix (" ms");
}

void ColorVocoderAudioProcessorEditor::setupComboWithLabel (Combo& box, juce::Label& label, const juce::String& text)
{
    addAndMakeVisible (box);
    box.setJustificationType (juce::Justification::centred);
    box.setColour (Combo::backgroundColourId, juce::Colours::darkgrey.darker());
    box.setWantsKeyboardFocus (false);  // Don't steal focus from DAW
    box.setMouseClickGrabsKeyboardFocus (false);

    addAndMakeVisible (label);
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colours::white);
    label.setFont (11.0f);
}

//==============================================================

void ColorVocoderAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (15, 15, 20)); // dark bg

    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    g.drawFittedText ("BAND RITUAL",
                      getLocalBounds().removeFromTop (24),
                      juce::Justification::centred, 1);
}

//==============================================================

void ColorVocoderAudioProcessorEditor::resized()
{
    // DENSE LAYOUT - BIG KNOBS, CLOSE TOGETHER
    const int W = getWidth();   // 820
    const int H = getHeight();  // 520

    const int margin = 4;
    const int gap = 2;          // Minimal gap between groups
    const int titleH = 22;
    const int labelH = 14;      // Slightly bigger for readability
    const int comboH = 22;
    const int groupTitleH = 16;
    const int eqDisplayH = 150;  // Much taller EQ display to fill empty space
    const int licenseH = 14;

    // BIG KNOBS - 70px for main knobs (slightly smaller to fit better)
    const int knobSize = 70;
    const int smallKnob = 52;   // For areas with combos

    bypassButton.setBounds (4, 3, 45, 16);

    // ===== PRESET CONTROLS (in title bar area) =====
    const int presetBoxW = 180;
    const int presetBtnW = 45;
    const int presetDelW = 22;
    const int presetH = 18;
    const int presetY = 2;

    // Position preset controls to the right of title
    int presetStartX = W - presetBoxW - presetBtnW - presetDelW - margin - 8;
    presetBox.setBounds (presetStartX, presetY, presetBoxW, presetH);
    savePresetButton.setBounds (presetStartX + presetBoxW + 4, presetY, presetBtnW, presetH);
    deletePresetButton.setBounds (presetStartX + presetBoxW + presetBtnW + 6, presetY, presetDelW, presetH);

    // Band display at top (colorful bands visualizer)
    const int bandDisplayH = 30;
    bandDisplay.setBounds (margin, titleH, W - margin*2, bandDisplayH);

    int yPos = titleH + bandDisplayH + gap;

    // Row heights - TALLER lower row to fit 2 rows of knobs + labels
    const int upperRowH = 95;
    const int lowerRowH = 195;  // Was 185, need more for labels

    // Upper row: 3 groups (Scale merged with Harmonics for chord controls)
    int col1W = (W - margin*2 - gap*2) / 4;          // Global: 1/4
    int col2W = (W - margin*2 - gap*2) / 2;          // Scale & Chord: 2/4 (merged)
    int col3W = (W - margin*2 - gap*2) / 4;          // Carrier: 1/4
    globalGroup.setBounds    (margin,                       yPos, col1W, upperRowH);
    scaleGroup.setBounds     (margin + col1W + gap,         yPos, col2W, upperRowH);
    // harmonicGroup hidden - merged into scaleGroup
    harmonicGroup.setVisible (false);
    carrierGroup.setBounds   (margin + col1W + gap + col2W + gap, yPos, col3W, upperRowH);

    yPos += upperRowH + gap;

    // Lower row: 2 groups
    int halfW = (W - margin*2 - gap) / 2;
    bandsGroup.setBounds (margin,                  yPos, halfW, lowerRowH);
    noiseGroup.setBounds (margin + halfW + gap,    yPos, halfW, lowerRowH);

    yPos += lowerRowH + gap;

    // EQ Display + Q Knob - FILL ALL REMAINING SPACE
    const int qKnobW = 70;
    const int eqHeight = 200;  // Fixed large height

    if (inputFilterDisplay != nullptr)
        inputFilterDisplay->setBounds (margin, yPos, W - margin*2 - qKnobW - gap, eqHeight);

    // Q knob to the right of EQ display, vertically centered
    int qKnobY = yPos + (eqHeight - qKnobW) / 2;
    bellQSlider.setBounds (W - margin - qKnobW, qKnobY, qKnobW, qKnobW);
    bellQLabel.setBounds (W - margin - qKnobW, qKnobY + qKnobW, qKnobW, 14);

    // License at very bottom
    licenseStatusLabel.setBounds (margin, H - licenseH - margin, W - margin*2, licenseH);
    licenseEditor.setVisible (false);
    activateButton.setVisible (false);

    // === KNOB LAYOUTS - TIGHT, BIG ===

    // Global: 2 BIG knobs touching each other
    {
        auto b = globalGroup.getBounds();
        int innerY = b.getY() + groupTitleH;
        int centerX = b.getCentreX();
        int spacing = 4;  // Tiny gap between knobs

        mixSlider.setBounds (centerX - knobSize - spacing/2, innerY, knobSize, knobSize);
        mixLabel.setBounds (centerX - knobSize - spacing/2, innerY + knobSize, knobSize, labelH);

        depthSlider.setBounds (centerX + spacing/2, innerY, knobSize, knobSize);
        depthLabel.setBounds (centerX + spacing/2, innerY + knobSize, knobSize, labelH);
    }

    // Scale & Chord: 4 combos on top, 4 knobs below (merged with Harmonics)
    {
        auto b = scaleGroup.getBounds();
        int innerX = b.getX() + 4;
        int innerY = b.getY() + groupTitleH - 4;
        int innerW = b.getWidth() - 8;
        int cw = innerW / 4;  // 4 combos across

        // Row 1: 4 combo boxes (Key, Scale, Chord, Variation)
        keyLabel.setBounds (innerX, innerY, cw - 2, labelH);
        keyBox.setBounds (innerX, innerY + labelH, cw - 4, comboH);

        scaleLabel.setBounds (innerX + cw, innerY, cw - 2, labelH);
        scaleBox.setBounds (innerX + cw, innerY + labelH, cw - 4, comboH);

        chordDegreeLabel.setBounds (innerX + cw*2, innerY, cw - 2, labelH);
        chordDegreeBox.setBounds (innerX + cw*2, innerY + labelH, cw - 4, comboH);

        chordVariationLabel.setBounds (innerX + cw*3, innerY, cw - 2, labelH);
        chordVariationBox.setBounds (innerX + cw*3, innerY + labelH, cw - 4, comboH);

        // Row 2: 4 knobs (Scale Amt, Formant, Chord Amt, Harm Shift)
        int knobY = innerY + labelH + comboH + 4;
        int tinyKnob = 42;  // Smaller to fit 4
        int knobSpacing = (innerW - tinyKnob * 4) / 3;

        scaleAmtSlider.setBounds (innerX, knobY, tinyKnob, tinyKnob);
        scaleAmtLabel.setBounds (innerX - 4, knobY + tinyKnob, tinyKnob + 8, labelH);

        formantSlider.setBounds (innerX + tinyKnob + knobSpacing, knobY, tinyKnob, tinyKnob);
        formantLabel.setBounds (innerX + tinyKnob + knobSpacing - 4, knobY + tinyKnob, tinyKnob + 8, labelH);

        chordAmountSlider.setBounds (innerX + (tinyKnob + knobSpacing) * 2, knobY, tinyKnob, tinyKnob);
        chordAmountLabel.setBounds (innerX + (tinyKnob + knobSpacing) * 2 - 4, knobY + tinyKnob, tinyKnob + 8, labelH);

        harmonicShiftSlider.setBounds (innerX + (tinyKnob + knobSpacing) * 3, knobY, tinyKnob, tinyKnob);
        harmonicShiftLabel.setBounds (innerX + (tinyKnob + knobSpacing) * 3 - 4, knobY + tinyKnob, tinyKnob + 8, labelH);
    }

    // Carrier: combo + knob below
    {
        auto b = carrierGroup.getBounds();
        int innerX = b.getX() + 4;
        int innerY = b.getY() + groupTitleH - 2;
        int innerW = b.getWidth() - 8;

        carrierModeLabel.setBounds (innerX, innerY, innerW, labelH);
        carrierModeBox.setBounds (innerX, innerY + labelH, innerW, comboH);

        int knobY = innerY + labelH + comboH + 2;
        int cx = b.getCentreX() - smallKnob/2;
        carrierPitchSlider.setBounds (cx, knobY, smallKnob, smallKnob);
        carrierPitchLabel.setBounds (innerX, knobY + smallKnob, innerW, labelH);
    }

    // Bands: 4 knobs top, 3 knobs bottom - TIGHT GRID
    {
        auto b = bandsGroup.getBounds();
        int innerX = b.getX() + 6;
        int innerY = b.getY() + groupTitleH;
        int innerW = b.getWidth() - 12;

        // 4 knobs in top row - calculate spacing to fit tightly
        int row1Spacing = (innerW - knobSize * 4) / 3;
        if (row1Spacing < 2) row1Spacing = 2;

        // Labels go BELOW the text box (which is part of knobSize)
        bandCountSlider.setBounds (innerX, innerY, knobSize, knobSize);
        bandCountLabel.setBounds  (innerX, innerY + knobSize, knobSize, labelH);

        bandWidthSlider.setBounds (innerX + knobSize + row1Spacing, innerY, knobSize, knobSize);
        bandWidthLabel.setBounds  (innerX + knobSize + row1Spacing, innerY + knobSize, knobSize, labelH);

        lowFreqSlider.setBounds   (innerX + (knobSize + row1Spacing)*2, innerY, knobSize, knobSize);
        lowFreqLabel.setBounds    (innerX + (knobSize + row1Spacing)*2, innerY + knobSize, knobSize, labelH);

        highFreqSlider.setBounds  (innerX + (knobSize + row1Spacing)*3, innerY, knobSize, knobSize);
        highFreqLabel.setBounds   (innerX + (knobSize + row1Spacing)*3, innerY + knobSize, knobSize, labelH);

        // 3 knobs in bottom row - centered
        int row2Y = innerY + knobSize + labelH + 4;
        int row2TotalW = knobSize * 3 + row1Spacing * 2;
        int row2StartX = innerX + (innerW - row2TotalW) / 2;

        attackSlider.setBounds  (row2StartX, row2Y, knobSize, knobSize);
        attackLabel.setBounds   (row2StartX, row2Y + knobSize, knobSize, labelH);

        releaseSlider.setBounds (row2StartX + knobSize + row1Spacing, row2Y, knobSize, knobSize);
        releaseLabel.setBounds  (row2StartX + knobSize + row1Spacing, row2Y + knobSize, knobSize, labelH);

        gateSlider.setBounds    (row2StartX + (knobSize + row1Spacing)*2, row2Y, knobSize, knobSize);
        gateLabel.setBounds     (row2StartX + (knobSize + row1Spacing)*2, row2Y + knobSize, knobSize, labelH);
    }

    // Noise/Unvoiced: 3 knobs top, 2 knobs bottom - TIGHT GRID
    {
        auto b = noiseGroup.getBounds();
        int innerX = b.getX() + 6;
        int innerY = b.getY() + groupTitleH;
        int innerW = b.getWidth() - 12;

        // 2 knobs in top row: Unvoiced, HP Blend
        int row1Spacing = (innerW - knobSize * 2) / 3;
        if (row1Spacing < 2) row1Spacing = 2;

        unvoicedSlider.setBounds    (innerX + row1Spacing, innerY, knobSize, knobSize);
        unvoicedLabel.setBounds     (innerX + row1Spacing, innerY + knobSize, knobSize, labelH);

        hpBlendSlider.setBounds     (innerX + row1Spacing * 2 + knobSize, innerY, knobSize, knobSize);
        hpBlendLabel.setBounds      (innerX + row1Spacing * 2 + knobSize, innerY + knobSize, knobSize, labelH);

        // Row 2: Noise Color (left) and HP Slope combo (right)
        int row2Y = innerY + knobSize + labelH + 4;
        int comboW = 60;
        int comboH = 20;

        noiseColorSlider.setBounds (innerX + row1Spacing, row2Y, knobSize, knobSize);
        noiseColorLabel.setBounds  (innerX + row1Spacing, row2Y + knobSize, knobSize, labelH);

        // HP Slope combo below HP Blend
        int hpSlopeX = innerX + row1Spacing * 2 + knobSize + (knobSize - comboW) / 2;
        hpBlendSlopeBox.setBounds   (hpSlopeX, row2Y + 10, comboW, comboH);
        hpBlendSlopeLabel.setBounds (hpSlopeX, row2Y + 10 + comboH, comboW, labelH);
    }

    // Update license visibility
    updateLicenseUI();
}

//==============================================================
// License UI helper
//==============================================================

void ColorVocoderAudioProcessorEditor::updateLicenseUI()
{
    const bool licensed = processor.getLicenseManager().isLicensed();

    if (licensed)
    {
        // Hide the input + button once the license is valid
        licenseEditor.setVisible (false);
        activateButton.setVisible (false);

        licenseStatusLabel.setText ("Licensed via Gumroad. Thank you for supporting Band Ritual.",
                                    juce::dontSendNotification);
    }
    else
    {
        // Show them if not licensed
        licenseEditor.setVisible (true);
        activateButton.setVisible (true);

        licenseStatusLabel.setText ("Unlicensed – paste the Gumroad key from your receipt/email.",
                                    juce::dontSendNotification);
    }
}

//==============================================================
// Preset Management
//==============================================================

void ColorVocoderAudioProcessorEditor::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);

    auto presets = presetManager.getPresetList();

    int id = 1;
    for (const auto& preset : presets)
    {
        presetBox.addItem (preset, id++);
    }

    // Select current preset if it exists in the list
    auto currentName = presetManager.getCurrentPresetName();
    int idx = presets.indexOf (currentName);
    if (idx >= 0)
        presetBox.setSelectedId (idx + 1, juce::dontSendNotification);
}

void ColorVocoderAudioProcessorEditor::savePresetWithDialog()
{
    // Create a simple dialog to get preset name
    auto* dialog = new juce::AlertWindow ("Save Preset",
                                          "Enter a name for your preset:",
                                          juce::MessageBoxIconType::NoIcon);

    dialog->addTextEditor ("presetName", presetManager.getCurrentPresetName(), "Preset Name:");
    dialog->addButton ("Save", 1);
    dialog->addButton ("Cancel", 0);

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dialog] (int result)
        {
            if (result == 1)
            {
                auto name = dialog->getTextEditorContents ("presetName").trim();
                if (name.isNotEmpty())
                {
                    if (presetManager.savePreset (name))
                    {
                        refreshPresetList();
                        // Select the newly saved preset
                        auto presets = presetManager.getPresetList();
                        int idx = presets.indexOf (name);
                        if (idx >= 0)
                            presetBox.setSelectedId (idx + 1, juce::dontSendNotification);
                    }
                }
            }
            delete dialog;
        }), true);
}

void ColorVocoderAudioProcessorEditor::loadSelectedPreset()
{
    int selectedId = presetBox.getSelectedId();
    if (selectedId <= 0)
        return;

    auto presetName = presetBox.getItemText (presetBox.getSelectedItemIndex());
    presetManager.loadPreset (presetName);
}

void ColorVocoderAudioProcessorEditor::deleteSelectedPreset()
{
    int selectedId = presetBox.getSelectedId();
    if (selectedId <= 0)
        return;

    auto presetName = presetBox.getItemText (presetBox.getSelectedItemIndex());

    // Confirm deletion using NativeMessageBox
    juce::NativeMessageBox::showOkCancelBox (
        juce::MessageBoxIconType::WarningIcon,
        "Delete Preset",
        "Are you sure you want to delete \"" + presetName + "\"?",
        nullptr,  // parent component
        juce::ModalCallbackFunction::create ([this, presetName] (int result)
        {
            if (result == 1)
            {
                if (presetManager.deletePreset (presetName))
                {
                    refreshPresetList();
                }
            }
        }));
}
