#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PresetManager.h"

//==============================================================
// Custom LookAndFeel for bigger knobs
//==============================================================
class BigKnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BigKnobLookAndFeel()
    {
        setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xFF64D2FF));
        setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xFF2A2A35));
        setColour (juce::Slider::thumbColourId, juce::Colours::white);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        // Use the FULL width/height for the knob (no padding)
        auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();

        // Account for text box if present
        if (slider.getTextBoxPosition() != juce::Slider::NoTextBox)
        {
            int textBoxHeight = slider.getTextBoxHeight();
            bounds = bounds.withTrimmedBottom ((float) textBoxHeight + 2);
        }

        // Make knob as big as possible in the available space
        float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto knobBounds = bounds.withSizeKeepingCentre (size, size);

        float radius = size / 2.0f - 4.0f;
        float centreX = knobBounds.getCentreX();
        float centreY = knobBounds.getCentreY();

        // Background circle
        g.setColour (juce::Colour (0xFF1A1A24));
        g.fillEllipse (centreX - radius, centreY - radius, radius * 2, radius * 2);

        // Outer ring
        g.setColour (juce::Colour (0xFF2A2A35));
        g.drawEllipse (centreX - radius, centreY - radius, radius * 2, radius * 2, 2.0f);

        // Arc track (background)
        juce::Path arcBg;
        float arcRadius = radius - 6.0f;
        arcBg.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (0xFF3A3A45));
        g.strokePath (arcBg, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));

        // Arc track (filled portion)
        float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        juce::Path arcFill;
        arcFill.addCentredArc (centreX, centreY, arcRadius, arcRadius, 0.0f,
                               rotaryStartAngle, angle, true);
        g.setColour (juce::Colour (0xFF64D2FF));
        g.strokePath (arcFill, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));

        // Pointer/thumb
        float pointerLength = radius - 10.0f;
        float pointerThickness = 3.0f;
        juce::Path pointer;
        pointer.addRoundedRectangle (-pointerThickness / 2, -pointerLength,
                                      pointerThickness, pointerLength * 0.6f, 1.5f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle)
                                    .translated (centreX, centreY));
        g.setColour (juce::Colours::white);
        g.fillPath (pointer);

        // Center dot
        g.setColour (juce::Colour (0xFF64D2FF));
        g.fillEllipse (centreX - 4, centreY - 4, 8, 8);
    }
};

//==============================================================
// BandDisplay – visualiser for bands
//==============================================================
class BandDisplay : public juce::Component,
                    private juce::Timer
{
public:
    BandDisplay (ColorVocoderAudioProcessor& p)
        : processor (p)
    {
        // Don't steal keyboard focus from DAW
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);
    }

    void visibilityChanged() override
    {
        if (isVisible() && !isTimerRunning())
            startTimerHz (30);
        else if (!isVisible())
            stopTimer();
    }

    ~BandDisplay() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();

        // background
        g.setColour (juce::Colour::fromRGB (10, 10, 15));
        g.fillRect (area);

        // Safety: check for empty OR size mismatch (thread safety during updates)
        if (freqs.empty() || envs.empty() || freqs.size() != envs.size())
        {
            g.setColour (juce::Colours::grey);
            g.setFont (juce::Font (12.0f));
            g.drawFittedText ("Loading...", getLocalBounds(),
                              juce::Justification::centred, 1);
            return;
        }

        // faint horizontal center line
        g.setColour (juce::Colour::fromRGBA (255, 255, 255, 25));
        g.drawLine (area.getX(), area.getCentreY(),
                    area.getRight(), area.getCentreY(), 1.0f);

        // find max env for scaling
        float maxEnv = 0.0f;
        for (auto e : envs)
            maxEnv = std::max (maxEnv, e);

        if (maxEnv < 1.0e-6f)
            maxEnv = 1.0e-6f;

        const int n = (int) freqs.size();

        // thickness based on band count
        float idealWidth = area.getWidth() / (float) juce::jmax (n, 1);
        float bandWidth  = juce::jlimit (3.0f, 20.0f, idealWidth * 0.7f);

        float logLow  = std::log (std::max (lowHz, 20.0f));
        float logHigh = std::log (std::max (highHz, lowHz + 1.0f));

        for (int i = 0; i < n; ++i)
        {
            float f   = freqs[(size_t) i];
            float env = envs [(size_t) i];

            float logF = std::log (juce::jlimit (lowHz, highHz, f));
            float norm = (logF - logLow) / (logHigh - logLow);
            norm = juce::jlimit (0.0f, 1.0f, norm);

            float xCenter = area.getX() + norm * area.getWidth();
            float barHeightNorm = juce::jlimit (0.0f, 1.0f, env / maxEnv);
            float barHeight = barHeightNorm * area.getHeight();

            float x = xCenter - bandWidth * 0.5f;
            float y = area.getCentreY() - barHeight * 0.5f;

            // Colour gradient: low -> high = teal -> magenta/pink
            float colourPos = norm;
            float hueStart = 0.52f;
            float hueEnd   = 0.92f;
            float hue      = hueStart + (hueEnd - hueStart) * colourPos;

            juce::Colour bandColour = juce::Colour::fromHSV (hue, 0.85f, 0.95f, 1.0f);

            float alpha = 0.25f + 0.75f * barHeightNorm;
            bandColour = bandColour.withAlpha (alpha);

            g.setColour (bandColour);
            g.fillRoundedRectangle (juce::Rectangle<float> (x, y, bandWidth, barHeight), 2.0f);
        }
    }

private:
    void timerCallback() override
    {
        processor.getBandSnapshot (freqs, envs, lowHz, highHz);
        repaint();
    }

    ColorVocoderAudioProcessor& processor;
    std::vector<float> freqs;
    std::vector<float> envs;
    float lowHz  = 80.0f;
    float highHz = 20000.0f;
};

//==============================================================
// InputFilterDisplay – EQ display with spectrum analyzer
//==============================================================
class InputFilterDisplay : public juce::Component,
                           private juce::Timer
{
public:
    InputFilterDisplay (juce::AudioProcessorValueTreeState& apvts, ColorVocoderAudioProcessor& proc)
        : processor (proc),
          apvtsRef (apvts),
          hpFreqParam (apvts.getParameter ("inputHPFreq")),
          lpFreqParam (apvts.getParameter ("inputLPFreq")),
          hpSlopeParam (apvts.getParameter ("inputHPSlope")),
          lpSlopeParam (apvts.getParameter ("inputLPSlope")),
          enabledParam (apvts.getParameter ("inputFilterEnabled")),
          // Bell 1 params (Normal EQ)
          bell1EnabledParam (apvts.getParameter ("bell1Enabled")),
          bell1FreqParam (apvts.getParameter ("bell1Freq")),
          bell1GainParam (apvts.getParameter ("bell1Gain")),
          bell1QParam (apvts.getParameter ("bell1Q")),
          // Bell 2 params (Normal EQ)
          bell2EnabledParam (apvts.getParameter ("bell2Enabled")),
          bell2FreqParam (apvts.getParameter ("bell2Freq")),
          bell2GainParam (apvts.getParameter ("bell2Gain")),
          bell2QParam (apvts.getParameter ("bell2Q")),
          // HP/LP boost params (Y-axis)
          hpQParam (apvts.getParameter ("inputHPQ")),
          lpQParam (apvts.getParameter ("inputLPQ")),
          // HP/LP Q width params (knob - controls peak width)
          hpQWidthParam (apvts.getParameter ("inputHPQWidth")),
          lpQWidthParam (apvts.getParameter ("inputLPQWidth")),
          // Complement mode
          complementParam (apvts.getParameter ("filterComplement")),
          // Complement Bell 1 params (wet/dry ratio control)
          compBell1EnabledParam (apvts.getParameter ("compBell1Enabled")),
          compBell1FreqParam (apvts.getParameter ("compBell1Freq")),
          compBell1GainParam (apvts.getParameter ("compBell1Gain")),
          compBell1QParam (apvts.getParameter ("compBell1Q")),
          // Complement Bell 2 params
          compBell2EnabledParam (apvts.getParameter ("compBell2Enabled")),
          compBell2FreqParam (apvts.getParameter ("compBell2Freq")),
          compBell2GainParam (apvts.getParameter ("compBell2Gain")),
          compBell2QParam (apvts.getParameter ("compBell2Q")),
          // Sub/High complement levels
          subComplementLevelParam (apvts.getParameter ("subComplementLevel")),
          highComplementLevelParam (apvts.getParameter ("highComplementLevel"))
    {
        // Don't steal keyboard focus from DAW
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);
    }

    void visibilityChanged() override
    {
        if (isVisible() && !isTimerRunning())
            startTimerHz (30);
        else if (!isVisible())
            stopTimer();
    }

    ~InputFilterDisplay() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        try
        {
            paintInternal (g);
        }
        catch (...)
        {
            // Prevent crash - draw fallback
            g.fillAll (juce::Colour (0xFF1A1A24));
            g.setColour (juce::Colours::red);
            g.drawText ("EQ Error", getLocalBounds(), juce::Justification::centred);
        }
    }

    void paintInternal (juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // Check if filter is enabled
        bool enabled = (enabledParam != nullptr) ? (enabledParam->getValue() > 0.5f) : true;

        // === GRADIENT BACKGROUND ===
        juce::ColourGradient bgGradient (
            juce::Colour (0xFF0D0D12), 0, bounds.getY(),
            juce::Colour (0xFF1A1A24), 0, bounds.getBottom(), false);
        g.setGradientFill (bgGradient);
        g.fillRoundedRectangle (bounds, 6.0f);

        // Border
        g.setColour (enabled ? juce::Colour (0xFF3A3A45) : juce::Colour (0xFF1A1A1A));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

        // Reserve space for toggle button
        auto toggleArea = bounds.removeFromLeft (50).reduced (8, 8);
        auto plotArea = bounds.reduced (8, 12);  // Less vertical reduction for small heights

        // === DRAW TOGGLE BUTTON ===
        drawToggleButton (g, toggleArea, enabled);

        // Safety check for valid plot area
        if (plotArea.getWidth() < 50 || plotArea.getHeight() < 20)
        {
            g.setColour (juce::Colours::grey);
            g.drawText ("Master EQ", getLocalBounds(), juce::Justification::centred);
            return;
        }

        // Safety check for parameters
        if (hpFreqParam == nullptr || lpFreqParam == nullptr ||
            hpSlopeParam == nullptr || lpSlopeParam == nullptr)
        {
            g.setColour (juce::Colours::grey);
            g.drawText ("Master EQ", bounds, juce::Justification::centred);
            return;
        }

        // === GRID LINES ===
        drawGrid (g, plotArea, enabled);

        // === SPECTRUM ANALYZER ===
        drawSpectrum (g, plotArea, enabled ? 0.5f : 0.15f);

        // Get parameter values
        float hpFreq = hpFreqParam->convertFrom0to1 (hpFreqParam->getValue());
        float lpFreq = lpFreqParam->convertFrom0to1 (lpFreqParam->getValue());
        int hpSlope = (int) (hpSlopeParam->convertFrom0to1 (hpSlopeParam->getValue()) + 0.5f);
        int lpSlope = (int) (lpSlopeParam->convertFrom0to1 (lpSlopeParam->getValue()) + 0.5f);

        // Dim everything if disabled
        float alpha = enabled ? 1.0f : 0.3f;

        // === RESPONSE CURVE (clipped to plot area) ===
        g.saveState();
        g.reduceClipRegion (plotArea.toNearestInt());

        // Check if we're in Complement view (clicking COMPLEMENT toggles the view)
        bool complementOn = (complementParam != nullptr) && (complementParam->getValue() > 0.5f);

        // Choose curve color based on view mode
        juce::Colour curveColour = complementOn ? juce::Colour (0xFFFFD700) : juce::Colour (0xFF64D2FF);  // Gold vs Blue
        juce::Colour fillColourTop = complementOn ? juce::Colour (0x50FFD700) : juce::Colour (0x5064D2FF);
        juce::Colour fillColourBot = complementOn ? juce::Colour (0x10FFD700) : juce::Colour (0x1064D2FF);

        // Get HP/LP gain and Q width for complement curve
        float hpGainDb = (hpQParam != nullptr) ? hpQParam->convertFrom0to1 (hpQParam->getValue()) : 0.0f;
        float lpGainDb = (lpQParam != nullptr) ? lpQParam->convertFrom0to1 (lpQParam->getValue()) : 0.0f;
        float hpQWidth = (hpQWidthParam != nullptr) ? hpQWidthParam->convertFrom0to1 (hpQWidthParam->getValue()) : 1.25f;
        float lpQWidth = (lpQWidthParam != nullptr) ? lpQWidthParam->convertFrom0to1 (lpQWidthParam->getValue()) : 1.25f;

        // Show HP/LP filter curve, with bells depending on mode:
        // - Normal mode: regular bells (1/2)
        // - Complement mode: complement bells (W1/W2) + HP/LP gain/Q
        juce::Path responsePath = complementOn
            ? createComplementCurve (plotArea, hpFreq, lpFreq, hpSlope, lpSlope, hpGainDb, lpGainDb, hpQWidth, lpQWidth)
            : createResponsePath (plotArea, hpFreq, lpFreq, hpSlope, lpSlope, 0.0f, 0.0f);

        // Gradient fill under curve - closes at bottom of plot area
        juce::Path fillPath = responsePath;
        fillPath.lineTo (plotArea.getRight(), plotArea.getBottom() + 50);  // Go below
        fillPath.lineTo (plotArea.getX(), plotArea.getBottom() + 50);
        fillPath.closeSubPath();

        juce::ColourGradient fillGradient (
            fillColourTop.withMultipliedAlpha (alpha), 0, plotArea.getY(),
            fillColourBot.withMultipliedAlpha (alpha), 0, plotArea.getBottom(), false);
        g.setGradientFill (fillGradient);
        g.fillPath (fillPath);

        // Curve stroke with glow effect
        for (int i = 3; i > 0; --i)
        {
            g.setColour (curveColour.withAlpha (0.15f * alpha / (float) i));
            g.strokePath (responsePath, juce::PathStrokeType ((float) i * 2.5f));
        }
        g.setColour (curveColour.withAlpha (alpha));
        g.strokePath (responsePath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));

        g.restoreState();  // Remove clip region

        // === DRAGGABLE NODES ===
        float hpX = freqToX (hpFreq, plotArea);
        float lpX = freqToX (lpFreq, plotArea);
        float centerY = plotArea.getCentreY();

        // HP/LP Y position based on resonance BOOST in dB (0-12dB)
        // Uses SAME formula as Bell nodes so same Y = same dB boost
        float hpBoostDb = (hpQParam != nullptr) ? hpQParam->convertFrom0to1 (hpQParam->getValue()) : 0.0f;
        float lpBoostDb = (lpQParam != nullptr) ? lpQParam->convertFrom0to1 (lpQParam->getValue()) : 0.0f;

        // Same formula as Bell: Y = centerY - (dB / 18) * (height * 0.4)
        // This ensures +12dB resonance shows at same Y as +12dB bell gain
        float hpY = centerY - (hpBoostDb / 24.0f) * (plotArea.getHeight() * 0.4f);
        float lpY = centerY - (lpBoostDb / 24.0f) * (plotArea.getHeight() * 0.4f);

        // Node colors: HP/LP = blue, Bell = orange/yellow
        juce::Colour hpLpColour (0xFF64D2FF);

        // HP Node - clamp Y for handle bounds
        float hpYClamped = juce::jlimit (plotArea.getY() + 12.0f, plotArea.getBottom() - 12.0f, hpY);
        hpHandleBounds = juce::Rectangle<float> (hpX - 12, hpYClamped - 12, 24, 24);
        // Show slope and dB boost like Bell nodes do
        juce::String hpLabel = getSlopeLabel (hpSlope) + " HP";
        if (hpBoostDb > 0.1f)
            hpLabel = formatGain (hpBoostDb);
        drawNode (g, hpX, hpYClamped, hpLpColour.withAlpha (alpha), draggingHP,
                  formatFreq (hpFreq), hpLabel, true, plotArea);

        // LP Node - clamp Y for handle bounds
        float lpYClamped = juce::jlimit (plotArea.getY() + 12.0f, plotArea.getBottom() - 12.0f, lpY);
        lpHandleBounds = juce::Rectangle<float> (lpX - 12, lpYClamped - 12, 24, 24);
        juce::String lpLabel = getSlopeLabel (lpSlope) + " LP";
        if (lpBoostDb > 0.1f)
            lpLabel = formatGain (lpBoostDb);
        drawNode (g, lpX, lpYClamped, hpLpColour.withAlpha (alpha), draggingLP,
                  formatFreq (lpFreq), lpLabel, false, plotArea);

        // === BELL EQ NODES (different depending on view mode) ===
        if (complementOn)
        {
            // === COMPLEMENT VIEW: Show complement bells (gold/yellow, controls wet/dry) ===
            juce::Colour compBell1Colour (0xFFFFD700);  // Gold
            juce::Colour compBell2Colour (0xFFFFAA00);  // Darker gold
            bool cb1On = (compBell1EnabledParam != nullptr) && (compBell1EnabledParam->getValue() > 0.5f);
            bool cb2On = (compBell2EnabledParam != nullptr) && (compBell2EnabledParam->getValue() > 0.5f);

            // Complement Bell 1 Node
            if (compBell1FreqParam != nullptr && compBell1GainParam != nullptr)
            {
                float cb1Freq = compBell1FreqParam->convertFrom0to1 (compBell1FreqParam->getValue());
                float cb1Gain = compBell1GainParam->convertFrom0to1 (compBell1GainParam->getValue());
                float cb1X = freqToX (cb1Freq, plotArea);
                float cb1Y = centerY - (cb1Gain / 18.0f) * (plotArea.getHeight() * 0.4f);

                compBell1HandleBounds = juce::Rectangle<float> (cb1X - 12, cb1Y - 12, 24, 24);

                juce::Colour cb1Col = cb1On ? compBell1Colour : compBell1Colour.withAlpha (0.3f);
                // Show "W1" for Wet/dry control bell 1
                drawBellNode (g, cb1X, cb1Y, cb1Col.withAlpha (alpha), draggingCompBell1 || cb1On,
                             formatFreq (cb1Freq), formatGain (cb1Gain), plotArea, "W1");
            }

            // Complement Bell 2 Node
            if (compBell2FreqParam != nullptr && compBell2GainParam != nullptr)
            {
                float cb2Freq = compBell2FreqParam->convertFrom0to1 (compBell2FreqParam->getValue());
                float cb2Gain = compBell2GainParam->convertFrom0to1 (compBell2GainParam->getValue());
                float cb2X = freqToX (cb2Freq, plotArea);
                float cb2Y = centerY - (cb2Gain / 18.0f) * (plotArea.getHeight() * 0.4f);

                compBell2HandleBounds = juce::Rectangle<float> (cb2X - 12, cb2Y - 12, 24, 24);

                juce::Colour cb2Col = cb2On ? compBell2Colour : compBell2Colour.withAlpha (0.3f);
                drawBellNode (g, cb2X, cb2Y, cb2Col.withAlpha (alpha), draggingCompBell2 || cb2On,
                             formatFreq (cb2Freq), formatGain (cb2Gain), plotArea, "W2");
            }

            // === SUB LOCKED INDICATOR ===
            // Show "LOCKED" indicator for sub band (below HP frequency) when complement is on
            float hpX = freqToX (hpFreq, plotArea);
            if (hpFreq > 25.0f && hpX > plotArea.getX() + 30)
            {
                g.setColour (juce::Colour (0xFF00FF88).withAlpha (alpha * 0.8f));  // Green
                g.setFont (9.0f);
                float lockX = plotArea.getX() + 4;
                float lockY = plotArea.getY() + 4;
                g.drawText ("SUB", (int) lockX, (int) lockY, 30, 12, juce::Justification::left);
                // Lock icon (simplified)
                g.fillRoundedRectangle (lockX + 32, lockY + 1, 10, 10, 2.0f);
                g.setColour (juce::Colour (0xFF0D0D12));
                g.fillRoundedRectangle (lockX + 34, lockY + 5, 6, 5, 1.0f);
            }
        }
        else
        {
            // === NORMAL VIEW: Show normal bells (orange, controls gain) ===
            juce::Colour bell1Colour (0xFFFF9F40);  // Orange
            juce::Colour bell2Colour (0xFFFFD700);  // Gold
            bool bell1On = (bell1EnabledParam != nullptr) && (bell1EnabledParam->getValue() > 0.5f);
            bool bell2On = (bell2EnabledParam != nullptr) && (bell2EnabledParam->getValue() > 0.5f);

            // Bell 1 Node
            if (bell1FreqParam != nullptr && bell1GainParam != nullptr)
            {
                float b1Freq = bell1FreqParam->convertFrom0to1 (bell1FreqParam->getValue());
                float b1Gain = bell1GainParam->convertFrom0to1 (bell1GainParam->getValue());
                float b1X = freqToX (b1Freq, plotArea);
                // Y position based on gain: 0dB = center, +18dB = top, -18dB = bottom
                float b1Y = centerY - (b1Gain / 24.0f) * (plotArea.getHeight() * 0.4f);

                bell1HandleBounds = juce::Rectangle<float> (b1X - 12, b1Y - 12, 24, 24);

                juce::Colour b1Col = bell1On ? bell1Colour : bell1Colour.withAlpha (0.3f);
                drawBellNode (g, b1X, b1Y, b1Col.withAlpha (alpha), draggingBell1 || bell1On,
                             formatFreq (b1Freq), formatGain (b1Gain), plotArea, "1");
            }

            // Bell 2 Node
            if (bell2FreqParam != nullptr && bell2GainParam != nullptr)
            {
                float b2Freq = bell2FreqParam->convertFrom0to1 (bell2FreqParam->getValue());
                float b2Gain = bell2GainParam->convertFrom0to1 (bell2GainParam->getValue());
                float b2X = freqToX (b2Freq, plotArea);
                float b2Y = centerY - (b2Gain / 24.0f) * (plotArea.getHeight() * 0.4f);

                bell2HandleBounds = juce::Rectangle<float> (b2X - 12, b2Y - 12, 24, 24);

                juce::Colour b2Col = bell2On ? bell2Colour : bell2Colour.withAlpha (0.3f);
                drawBellNode (g, b2X, b2Y, b2Col.withAlpha (alpha), draggingBell2 || bell2On,
                             formatFreq (b2Freq), formatGain (b2Gain), plotArea, "2");
            }
        }

        // === TITLE (shows current mode) ===
        g.setColour (complementOn ? juce::Colour (0xFFFFD700).withAlpha (alpha) : juce::Colour (0x80FFFFFF).withAlpha (alpha));
        g.setFont (10.0f);
        juce::String titleText = complementOn ? "COMPLEMENT" : "MASTER EQ";
        g.drawText (titleText, bounds.withTrimmedLeft(10).removeFromTop (18), juce::Justification::centredLeft);

        // === COMPLEMENT BUTTON (right side) ===
        // Use original bounds (before toggle area was removed) for button positioning
        auto fullBounds = getLocalBounds().toFloat();
        drawComplementButton (g, fullBounds, complementOn, enabled);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        try
        {
            mouseDownInternal (e);
        }
        catch (...) {}
    }

    void mouseDownInternal (const juce::MouseEvent& e)
    {
        // Check toggle button click
        auto toggleArea = getLocalBounds().toFloat().removeFromLeft (50).reduced (8, 8);
        if (toggleArea.contains (e.position) && enabledParam != nullptr)
        {
            bool current = enabledParam->getValue() > 0.5f;
            enabledParam->setValueNotifyingHost (current ? 0.0f : 1.0f);
            repaint();
            return;
        }

        // Check complement button click (only if bounds are valid)
        if (!complementButtonBounds.isEmpty() &&
            complementButtonBounds.contains (e.position) &&
            complementParam != nullptr)
        {
            bool current = complementParam->getValue() > 0.5f;
            complementParam->setValueNotifyingHost (current ? 0.0f : 1.0f);
            repaint();
            return;
        }

        // LEFT-CLICK: Start dragging
        // (Double-click cycles slope for HP/LP, toggles enable for Bell - see mouseDoubleClick)
        int newSelection = selectedBand;

        // Check which view mode we're in
        bool complementView = (complementParam != nullptr) && (complementParam->getValue() > 0.5f);

        if (hpHandleBounds.expanded (6).contains (e.position))
        {
            draggingHP = true;
            newSelection = 1;  // HP selected
        }
        else if (lpHandleBounds.expanded (6).contains (e.position))
        {
            draggingLP = true;
            newSelection = 2;  // LP selected
        }
        else if (complementView)
        {
            // In complement view, drag complement bells (only if params exist and bounds are valid)
            if (!compBell1HandleBounds.isEmpty() && compBell1HandleBounds.expanded (6).contains (e.position))
            {
                draggingCompBell1 = true;
                newSelection = 5;  // Comp Bell 1 selected
                if (compBell1EnabledParam != nullptr)
                    compBell1EnabledParam->setValueNotifyingHost (1.0f);
            }
            else if (!compBell2HandleBounds.isEmpty() && compBell2HandleBounds.expanded (6).contains (e.position))
            {
                draggingCompBell2 = true;
                newSelection = 6;  // Comp Bell 2 selected
                if (compBell2EnabledParam != nullptr)
                    compBell2EnabledParam->setValueNotifyingHost (1.0f);
            }
        }
        else
        {
            // In normal view, drag normal bells
            if (bell1HandleBounds.expanded (6).contains (e.position))
            {
                draggingBell1 = true;
                newSelection = 3;  // Bell 1 selected
                if (bell1EnabledParam != nullptr)
                    bell1EnabledParam->setValueNotifyingHost (1.0f);
            }
            else if (bell2HandleBounds.expanded (6).contains (e.position))
            {
                draggingBell2 = true;
                newSelection = 4;  // Bell 2 selected
                if (bell2EnabledParam != nullptr)
                    bell2EnabledParam->setValueNotifyingHost (1.0f);
            }
        }

        // Update selection and notify editor to update Q knob
        if (newSelection != selectedBand)
        {
            selectedBand = newSelection;
            if (onBandSelected)
                onBandSelected (selectedBand);
        }

        repaint();
    }

    // Get which band is currently selected for Q knob (1=HP, 2=LP, 3=Bell1, 4=Bell2)
    int getSelectedBand() const { return selectedBand; }

    // Callback when selection changes - editor uses this to update Q knob
    std::function<void(int)> onBandSelected;

    void mouseDrag (const juce::MouseEvent& e) override
    {
        try
        {
            mouseDragInternal (e);
        }
        catch (...) {}
    }

    void mouseDragInternal (const juce::MouseEvent& e)
    {
        auto bounds = getLocalBounds().toFloat();
        bounds.removeFromLeft (50);  // Skip toggle area
        auto plotArea = bounds.reduced (8, 12);  // Match paint() calculation
        float centerY = plotArea.getCentreY();

        if (draggingHP && hpFreqParam != nullptr)
        {
            // X controls frequency
            float freq = xToFreq (e.position.x, plotArea);
            freq = juce::jlimit (20.0f, 500.0f, freq);
            hpFreqParam->setValueNotifyingHost (hpFreqParam->convertTo0to1 (freq));

            // Y controls resonance BOOST in dB - SAME formula as Bell nodes!
            // This ensures same Y position = same dB boost
            if (hpQParam != nullptr)
            {
                float yNorm = (centerY - e.position.y) / (plotArea.getHeight() * 0.4f);
                float boostDb = juce::jlimit (-24.0f, 24.0f, yNorm * 24.0f);  // -24 to +24dB range
                hpQParam->setValueNotifyingHost (hpQParam->convertTo0to1 (boostDb));
            }
        }
        else if (draggingLP && lpFreqParam != nullptr)
        {
            // X controls frequency
            float freq = xToFreq (e.position.x, plotArea);
            freq = juce::jlimit (1000.0f, 20000.0f, freq);
            lpFreqParam->setValueNotifyingHost (lpFreqParam->convertTo0to1 (freq));

            // Y controls resonance BOOST in dB - SAME formula as Bell nodes!
            if (lpQParam != nullptr)
            {
                float yNorm = (centerY - e.position.y) / (plotArea.getHeight() * 0.4f);
                float boostDb = juce::jlimit (-24.0f, 24.0f, yNorm * 24.0f);  // -24 to +24dB range
                lpQParam->setValueNotifyingHost (lpQParam->convertTo0to1 (boostDb));
            }
        }
        else if (draggingBell1 && bell1FreqParam != nullptr && bell1GainParam != nullptr)
        {
            // X controls frequency
            float freq = xToFreq (e.position.x, plotArea);
            freq = juce::jlimit (20.0f, 20000.0f, freq);
            bell1FreqParam->setValueNotifyingHost (bell1FreqParam->convertTo0to1 (freq));

            // Y controls gain: top = +18dB, center = 0dB, bottom = -18dB
            float yNorm = (centerY - e.position.y) / (plotArea.getHeight() * 0.4f);
            float gain = juce::jlimit (-24.0f, 24.0f, yNorm * 24.0f);
            bell1GainParam->setValueNotifyingHost (bell1GainParam->convertTo0to1 (gain));
        }
        else if (draggingBell2 && bell2FreqParam != nullptr && bell2GainParam != nullptr)
        {
            // X controls frequency
            float freq = xToFreq (e.position.x, plotArea);
            freq = juce::jlimit (20.0f, 20000.0f, freq);
            bell2FreqParam->setValueNotifyingHost (bell2FreqParam->convertTo0to1 (freq));

            // Y controls gain
            float yNorm = (centerY - e.position.y) / (plotArea.getHeight() * 0.4f);
            float gain = juce::jlimit (-24.0f, 24.0f, yNorm * 24.0f);
            bell2GainParam->setValueNotifyingHost (bell2GainParam->convertTo0to1 (gain));
        }
        // Complement Bell 1 dragging
        else if (draggingCompBell1 && compBell1FreqParam != nullptr && compBell1GainParam != nullptr)
        {
            float freq = xToFreq (e.position.x, plotArea);
            freq = juce::jlimit (20.0f, 20000.0f, freq);
            compBell1FreqParam->setValueNotifyingHost (compBell1FreqParam->convertTo0to1 (freq));

            float yNorm = (centerY - e.position.y) / (plotArea.getHeight() * 0.4f);
            float gain = juce::jlimit (-18.0f, 18.0f, yNorm * 18.0f);  // Complement uses -18 to +18 range
            compBell1GainParam->setValueNotifyingHost (compBell1GainParam->convertTo0to1 (gain));
        }
        // Complement Bell 2 dragging
        else if (draggingCompBell2 && compBell2FreqParam != nullptr && compBell2GainParam != nullptr)
        {
            float freq = xToFreq (e.position.x, plotArea);
            freq = juce::jlimit (20.0f, 20000.0f, freq);
            compBell2FreqParam->setValueNotifyingHost (compBell2FreqParam->convertTo0to1 (freq));

            float yNorm = (centerY - e.position.y) / (plotArea.getHeight() * 0.4f);
            float gain = juce::jlimit (-18.0f, 18.0f, yNorm * 18.0f);  // Complement uses -18 to +18 range
            compBell2GainParam->setValueNotifyingHost (compBell2GainParam->convertTo0to1 (gain));
        }

        // Immediate visual feedback
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        draggingHP = false;
        draggingLP = false;
        draggingBell1 = false;
        draggingBell2 = false;
        draggingCompBell1 = false;
        draggingCompBell2 = false;
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        // Scroll wheel on HP/LP nodes changes slope (6 options: 6/12/24/48/72/96 dB/oct)
        if (hpHandleBounds.expanded (10).contains (e.position) && hpSlopeParam != nullptr)
        {
            int slope = (int) (hpSlopeParam->convertFrom0to1 (hpSlopeParam->getValue()) + 0.5f);
            int delta = (wheel.deltaY > 0) ? 1 : -1;
            int newSlope = juce::jlimit (0, 5, slope + delta);
            hpSlopeParam->setValueNotifyingHost (hpSlopeParam->convertTo0to1 ((float) newSlope));
            repaint();
        }
        else if (lpHandleBounds.expanded (10).contains (e.position) && lpSlopeParam != nullptr)
        {
            int slope = (int) (lpSlopeParam->convertFrom0to1 (lpSlopeParam->getValue()) + 0.5f);
            int delta = (wheel.deltaY > 0) ? 1 : -1;
            int newSlope = juce::jlimit (0, 5, slope + delta);
            lpSlopeParam->setValueNotifyingHost (lpSlopeParam->convertTo0to1 ((float) newSlope));
            repaint();
        }
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        try
        {
            mouseDoubleClickInternal (e);
        }
        catch (...) {}
    }

    void mouseDoubleClickInternal (const juce::MouseEvent& e)
    {
        // Check which view mode we're in
        bool complementView = (complementParam != nullptr) && (complementParam->getValue() > 0.5f);

        // Double-click to cycle slope for HP/LP nodes (6 options: 6/12/24/48/72/96 dB/oct)
        if (hpHandleBounds.expanded (6).contains (e.position) && hpSlopeParam != nullptr)
        {
            int slope = (int) (hpSlopeParam->convertFrom0to1 (hpSlopeParam->getValue()) + 0.5f);
            hpSlopeParam->setValueNotifyingHost (hpSlopeParam->convertTo0to1 ((float) ((slope + 1) % 6)));
        }
        else if (lpHandleBounds.expanded (6).contains (e.position) && lpSlopeParam != nullptr)
        {
            int slope = (int) (lpSlopeParam->convertFrom0to1 (lpSlopeParam->getValue()) + 0.5f);
            lpSlopeParam->setValueNotifyingHost (lpSlopeParam->convertTo0to1 ((float) ((slope + 1) % 6)));
        }
        else if (complementView)
        {
            // Double-click to toggle complement bells on/off
            if (compBell1HandleBounds.expanded (6).contains (e.position) && compBell1EnabledParam != nullptr)
            {
                bool current = compBell1EnabledParam->getValue() > 0.5f;
                compBell1EnabledParam->setValueNotifyingHost (current ? 0.0f : 1.0f);
            }
            else if (compBell2HandleBounds.expanded (6).contains (e.position) && compBell2EnabledParam != nullptr)
            {
                bool current = compBell2EnabledParam->getValue() > 0.5f;
                compBell2EnabledParam->setValueNotifyingHost (current ? 0.0f : 1.0f);
            }
        }
        else
        {
            // Double-click to toggle normal bell bands on/off
            if (bell1HandleBounds.expanded (6).contains (e.position) && bell1EnabledParam != nullptr)
            {
                bool current = bell1EnabledParam->getValue() > 0.5f;
                bell1EnabledParam->setValueNotifyingHost (current ? 0.0f : 1.0f);
            }
            else if (bell2HandleBounds.expanded (6).contains (e.position) && bell2EnabledParam != nullptr)
            {
                bool current = bell2EnabledParam->getValue() > 0.5f;
                bell2EnabledParam->setValueNotifyingHost (current ? 0.0f : 1.0f);
            }
        }
        repaint();
    }

private:
    void timerCallback() override
    {
        repaint();  // Refresh display for spectrum analyzer
    }

    void drawToggleButton (juce::Graphics& g, juce::Rectangle<float> area, bool enabled) const
    {
        // Draw power button style toggle
        auto center = area.getCentre();
        float size = juce::jmin (area.getWidth(), area.getHeight()) * 0.8f;
        auto buttonBounds = juce::Rectangle<float> (size, size).withCentre (center);

        // Background circle
        g.setColour (enabled ? juce::Colour (0xFF64D2FF).withAlpha (0.3f) : juce::Colour (0x40FFFFFF));
        g.fillEllipse (buttonBounds);

        // Border
        g.setColour (enabled ? juce::Colour (0xFF64D2FF) : juce::Colour (0x80FFFFFF));
        g.drawEllipse (buttonBounds.reduced (1), 2.0f);

        // Power icon
        g.setColour (enabled ? juce::Colour (0xFF64D2FF) : juce::Colour (0x80FFFFFF));
        float iconSize = size * 0.35f;

        // Arc
        juce::Path arc;
        arc.addCentredArc (center.x, center.y, iconSize, iconSize, 0,
                           juce::MathConstants<float>::pi * 0.3f,
                           juce::MathConstants<float>::pi * 1.7f, true);
        g.strokePath (arc, juce::PathStrokeType (2.0f));

        // Top line
        g.drawLine (center.x, center.y - iconSize * 0.3f, center.x, center.y - iconSize * 1.1f, 2.0f);
    }

    void drawComplementButton (juce::Graphics& g, juce::Rectangle<float> bounds, bool complementOn, bool filterEnabled) const
    {
        // Position on the right side of the display
        float buttonWidth = 70.0f;
        float buttonHeight = 22.0f;
        complementButtonBounds = juce::Rectangle<float> (
            bounds.getRight() - buttonWidth - 10,
            bounds.getY() + 6,
            buttonWidth,
            buttonHeight);

        float alpha = filterEnabled ? 1.0f : 0.4f;

        // Background - green/magenta when on, dark when off
        juce::Colour bgColour = complementOn ? juce::Colour (0xFF9B59B6) : juce::Colour (0xFF2A2A35);
        g.setColour (bgColour.withAlpha (alpha));
        g.fillRoundedRectangle (complementButtonBounds, 4.0f);

        // Border
        juce::Colour borderColour = complementOn ? juce::Colour (0xFFAA66CC) : juce::Colour (0xFF4A4A55);
        g.setColour (borderColour.withAlpha (alpha));
        g.drawRoundedRectangle (complementButtonBounds.reduced (0.5f), 4.0f, 1.0f);

        // Text
        g.setColour ((complementOn ? juce::Colours::white : juce::Colour (0xAAFFFFFF)).withAlpha (alpha));
        g.setFont (10.0f);
        g.drawText ("COMPLEMENT", complementButtonBounds, juce::Justification::centred);
    }

    void drawGrid (juce::Graphics& g, juce::Rectangle<float> area, bool enabled) const
    {
        float alpha = enabled ? 1.0f : 0.3f;

        // Vertical frequency lines
        g.setColour (juce::Colour (0x15FFFFFF).withMultipliedAlpha (alpha));
        const float freqs[] = { 30, 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
        for (float f : freqs)
        {
            float x = freqToX (f, area);
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        }

        // Horizontal 0dB line (brighter)
        g.setColour (juce::Colour (0x25FFFFFF).withMultipliedAlpha (alpha));
        g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

        // Frequency labels
        g.setColour (juce::Colour (0x60FFFFFF).withMultipliedAlpha (alpha));
        g.setFont (9.0f);
        const float labelFreqs[] = { 100, 1000, 10000 };
        const char* labelTexts[] = { "100", "1k", "10k" };
        for (int i = 0; i < 3; ++i)
        {
            float x = freqToX (labelFreqs[i], area);
            g.drawText (labelTexts[i], (int) x - 15, (int) area.getBottom() - 12, 30, 12, juce::Justification::centred);
        }
    }

    juce::Path createResponsePath (juce::Rectangle<float> area, float hpFreq, float lpFreq,
                                   int hpSlope, int lpSlope, float /*hpYOff*/, float /*lpYOff*/) const
    {
        juce::Path path;
        const float zeroDbY = area.getCentreY();
        const float dbRange = 48.0f;  // +/-24dB range
        const float pixelsPerDb = area.getHeight() * 0.45f / (dbRange / 2.0f);

        // Clamp frequencies to safe ranges
        hpFreq = juce::jlimit (20.0f, 20000.0f, hpFreq);
        lpFreq = juce::jlimit (20.0f, 20000.0f, lpFreq);

        const int hpOrder = getFilterOrder (hpSlope);
        const int lpOrder = getFilterOrder (lpSlope);

        // Get HP/LP resonance BOOST in dB (0-12dB range) - Y axis control
        float hpBoostDb = (hpQParam != nullptr) ? hpQParam->convertFrom0to1 (hpQParam->getValue()) : 0.0f;
        float lpBoostDb = (lpQParam != nullptr) ? lpQParam->convertFrom0to1 (lpQParam->getValue()) : 0.0f;

        // Get HP/LP Q width (0.5-10) - knob control for peak width
        float hpQWidth = (hpQWidthParam != nullptr) ? hpQWidthParam->convertFrom0to1 (hpQWidthParam->getValue()) : 1.0f;
        float lpQWidth = (lpQWidthParam != nullptr) ? lpQWidthParam->convertFrom0to1 (lpQWidthParam->getValue()) : 1.0f;

        // Get bell parameters
        bool bell1On = (bell1EnabledParam != nullptr) && (bell1EnabledParam->getValue() > 0.5f);
        bool bell2On = (bell2EnabledParam != nullptr) && (bell2EnabledParam->getValue() > 0.5f);

        float b1Freq = 500.0f, b1Gain = 0.0f, b1Q = 1.0f;
        float b2Freq = 2000.0f, b2Gain = 0.0f, b2Q = 1.0f;

        if (bell1FreqParam != nullptr) b1Freq = bell1FreqParam->convertFrom0to1 (bell1FreqParam->getValue());
        if (bell1GainParam != nullptr) b1Gain = bell1GainParam->convertFrom0to1 (bell1GainParam->getValue());
        if (bell1QParam != nullptr) b1Q = bell1QParam->convertFrom0to1 (bell1QParam->getValue());

        if (bell2FreqParam != nullptr) b2Freq = bell2FreqParam->convertFrom0to1 (bell2FreqParam->getValue());
        if (bell2GainParam != nullptr) b2Gain = bell2GainParam->convertFrom0to1 (bell2GainParam->getValue());
        if (bell2QParam != nullptr) b2Q = bell2QParam->convertFrom0to1 (bell2QParam->getValue());

        bool started = false;
        for (float x = area.getX(); x <= area.getRight(); x += 1.0f)
        {
            float freq = xToFreq (x, area);
            float dbTotal = 0.0f;

            // HP response with resonance/cut (simple approximation)
            if (hpFreq > 21.0f)
            {
                float ratio = hpFreq / freq;
                float magnitude = 1.0f / std::sqrt (1.0f + std::pow (ratio, 2.0f * hpOrder));
                dbTotal += 20.0f * std::log10 (std::max (magnitude, 0.00001f));

                // Add resonance peak or cut - width controlled by Q knob, height by Y axis
                // Positive dB = boost/peak, Negative dB = cut/dip
                if (std::abs (hpBoostDb) > 0.1f)
                {
                    float octavesFromCutoff = std::log2 (freq / hpFreq);
                    // Q width controls peak narrowness (higher Q = narrower)
                    float peakWidth = 0.5f / juce::jlimit (0.5f, 10.0f, hpQWidth);
                    float peakShape = std::exp (-octavesFromCutoff * octavesFromCutoff / (2.0f * peakWidth * peakWidth));
                    dbTotal += hpBoostDb * peakShape;  // Works for both + and - values
                }
            }

            // LP response with resonance/cut (simple approximation)
            if (lpFreq < 19500.0f)
            {
                float ratio = freq / lpFreq;
                float magnitude = 1.0f / std::sqrt (1.0f + std::pow (ratio, 2.0f * lpOrder));
                dbTotal += 20.0f * std::log10 (std::max (magnitude, 0.00001f));

                // Add resonance peak or cut - width controlled by Q knob, height by Y axis
                if (std::abs (lpBoostDb) > 0.1f)
                {
                    float octavesFromCutoff = std::log2 (freq / lpFreq);
                    float peakWidth = 0.5f / juce::jlimit (0.5f, 10.0f, lpQWidth);
                    float peakShape = std::exp (-octavesFromCutoff * octavesFromCutoff / (2.0f * peakWidth * peakWidth));
                    dbTotal += lpBoostDb * peakShape;  // Works for both + and - values
                }
            }

            // Bell 1 response (parametric EQ - 2nd order response like Ableton)
            if (bell1On && std::abs (b1Gain) > 0.1f)
            {
                float w = freq / b1Freq;
                float dw = w - 1.0f / w;  // Frequency deviation term
                float bellShape = 1.0f / (1.0f + b1Q * b1Q * dw * dw);
                dbTotal += b1Gain * bellShape;
            }

            // Bell 2 response (parametric EQ - 2nd order response like Ableton)
            if (bell2On && std::abs (b2Gain) > 0.1f)
            {
                float w = freq / b2Freq;
                float dw = w - 1.0f / w;
                float bellShape = 1.0f / (1.0f + b2Q * b2Q * dw * dw);
                dbTotal += b2Gain * bellShape;
            }

            float y = zeroDbY - dbTotal * pixelsPerDb;

            if (!started) { path.startNewSubPath (x, y); started = true; }
            else path.lineTo (x, y);
        }
        return path;
    }

    // Create response path for Complement mode - shows HP/LP + complement bells (W1/W2) + HP/LP gain/Q
    juce::Path createComplementCurve (juce::Rectangle<float> area, float hpFreq, float lpFreq,
                                      int hpSlope, int lpSlope,
                                      float hpGainDb, float lpGainDb, float hpQWidth, float lpQWidth) const
    {
        juce::Path path;
        const float zeroDbY = area.getCentreY();
        const float dbRange = 48.0f;
        const float pixelsPerDb = area.getHeight() * 0.45f / (dbRange / 2.0f);

        hpFreq = juce::jlimit (20.0f, 20000.0f, hpFreq);
        lpFreq = juce::jlimit (20.0f, 20000.0f, lpFreq);

        int hpOrder = getFilterOrder (hpSlope);
        int lpOrder = getFilterOrder (lpSlope);

        // Get complement bell parameters (W1/W2)
        bool cb1On = (compBell1EnabledParam != nullptr) && (compBell1EnabledParam->getValue() > 0.5f);
        bool cb2On = (compBell2EnabledParam != nullptr) && (compBell2EnabledParam->getValue() > 0.5f);

        float cb1Freq = 1000.0f, cb1Gain = 0.0f, cb1Q = 1.0f;
        float cb2Freq = 4000.0f, cb2Gain = 0.0f, cb2Q = 1.0f;

        if (compBell1FreqParam != nullptr) cb1Freq = compBell1FreqParam->convertFrom0to1 (compBell1FreqParam->getValue());
        if (compBell1GainParam != nullptr) cb1Gain = compBell1GainParam->convertFrom0to1 (compBell1GainParam->getValue());
        if (compBell1QParam != nullptr) cb1Q = compBell1QParam->convertFrom0to1 (compBell1QParam->getValue());

        if (compBell2FreqParam != nullptr) cb2Freq = compBell2FreqParam->convertFrom0to1 (compBell2FreqParam->getValue());
        if (compBell2GainParam != nullptr) cb2Gain = compBell2GainParam->convertFrom0to1 (compBell2GainParam->getValue());
        if (compBell2QParam != nullptr) cb2Q = compBell2QParam->convertFrom0to1 (compBell2QParam->getValue());

        bool started = false;
        for (float x = area.getX(); x <= area.getRight(); x += 1.0f)
        {
            float freq = xToFreq (x, area);
            float dbTotal = 0.0f;

            // HP response with resonance
            if (hpFreq > 21.0f)
            {
                float ratio = hpFreq / freq;
                float magnitude = 1.0f / std::sqrt (1.0f + std::pow (ratio, 2.0f * hpOrder));
                dbTotal += 20.0f * std::log10 (std::max (magnitude, 0.00001f));

                // Add resonance peak at HP cutoff (2nd order parametric response)
                if (std::abs (hpGainDb) > 0.1f)
                {
                    float w = freq / hpFreq;
                    float dw = w - 1.0f / w;
                    float resonanceShape = 1.0f / (1.0f + hpQWidth * hpQWidth * dw * dw);
                    dbTotal += hpGainDb * resonanceShape;
                }
            }

            // LP response with resonance
            if (lpFreq < 19500.0f)
            {
                float ratio = freq / lpFreq;
                float magnitude = 1.0f / std::sqrt (1.0f + std::pow (ratio, 2.0f * lpOrder));
                dbTotal += 20.0f * std::log10 (std::max (magnitude, 0.00001f));

                // Add resonance peak at LP cutoff (2nd order parametric response)
                if (std::abs (lpGainDb) > 0.1f)
                {
                    float w = freq / lpFreq;
                    float dw = w - 1.0f / w;
                    float resonanceShape = 1.0f / (1.0f + lpQWidth * lpQWidth * dw * dw);
                    dbTotal += lpGainDb * resonanceShape;
                }
            }

            // Complement Bell 1 (W1) - affects wet/dry ratio curve (2nd order parametric)
            if (cb1On && std::abs (cb1Gain) > 0.1f)
            {
                float w = freq / cb1Freq;
                float dw = w - 1.0f / w;
                float bellShape = 1.0f / (1.0f + cb1Q * cb1Q * dw * dw);
                dbTotal += cb1Gain * bellShape;
            }

            // Complement Bell 2 (W2) - 2nd order parametric
            if (cb2On && std::abs (cb2Gain) > 0.1f)
            {
                float w = freq / cb2Freq;
                float dw = w - 1.0f / w;
                float bellShape = 1.0f / (1.0f + cb2Q * cb2Q * dw * dw);
                dbTotal += cb2Gain * bellShape;
            }

            float y = zeroDbY - dbTotal * pixelsPerDb;

            if (!started) { path.startNewSubPath (x, y); started = true; }
            else path.lineTo (x, y);
        }
        return path;
    }

    int getFilterOrder (int slopeIdx) const
    {
        // Convert slope index to filter order (number of poles)
        // 0=6dB (1 pole), 1=12dB (2 poles), 2=24dB (4 poles), 3=48dB (8 poles), 4=72dB (12 poles), 5=96dB (16 poles)
        const int orders[] = { 1, 2, 4, 8, 12, 16 };
        return orders[juce::jlimit (0, 5, slopeIdx)];
    }

    // Create response path for Complement view (shows wet/dry ratio curve)
    juce::Path createComplementResponsePath (juce::Rectangle<float> area, float hpFreq, float lpFreq,
                                             int hpSlope, int lpSlope) const
    {
        juce::Path path;
        const float zeroDbY = area.getCentreY();
        const float dbRange = 48.0f;
        const float pixelsPerDb = area.getHeight() * 0.45f / (dbRange / 2.0f);

        hpFreq = juce::jlimit (20.0f, 20000.0f, hpFreq);
        lpFreq = juce::jlimit (20.0f, 20000.0f, lpFreq);

        // Get complement bell parameters
        bool cb1On = (compBell1EnabledParam != nullptr) && (compBell1EnabledParam->getValue() > 0.5f);
        bool cb2On = (compBell2EnabledParam != nullptr) && (compBell2EnabledParam->getValue() > 0.5f);

        float cb1Freq = 500.0f, cb1Gain = 0.0f, cb1Q = 1.0f;
        float cb2Freq = 2000.0f, cb2Gain = 0.0f, cb2Q = 1.0f;

        if (compBell1FreqParam != nullptr) cb1Freq = compBell1FreqParam->convertFrom0to1 (compBell1FreqParam->getValue());
        if (compBell1GainParam != nullptr) cb1Gain = compBell1GainParam->convertFrom0to1 (compBell1GainParam->getValue());
        if (compBell1QParam != nullptr) cb1Q = compBell1QParam->convertFrom0to1 (compBell1QParam->getValue());

        if (compBell2FreqParam != nullptr) cb2Freq = compBell2FreqParam->convertFrom0to1 (compBell2FreqParam->getValue());
        if (compBell2GainParam != nullptr) cb2Gain = compBell2GainParam->convertFrom0to1 (compBell2GainParam->getValue());
        if (compBell2QParam != nullptr) cb2Q = compBell2QParam->convertFrom0to1 (compBell2QParam->getValue());

        bool started = false;
        for (float x = area.getX(); x <= area.getRight(); x += 1.0f)
        {
            float freq = xToFreq (x, area);
            float dbTotal = 0.0f;

            // Complement curve shows wet/dry ratio:
            // - Above 0dB = more WET signal at that frequency
            // - Below 0dB = more DRY signal (protected)
            // Base: flat line at 0dB (normal wet signal)

            // Sub zone (below HP) shows fixed dry level
            if (freq < hpFreq)
            {
                // Sub complement: show fixed level (protected)
                float subLevel = (subComplementLevelParam != nullptr)
                    ? subComplementLevelParam->convertFrom0to1 (subComplementLevelParam->getValue()) : 0.0f;
                // Invert for display: sub is DRY so show below center
                dbTotal = -6.0f + subLevel;  // Offset to show it's "protected"
            }
            // High zone (above LP) shows high dry level
            else if (freq > lpFreq)
            {
                float highLevel = (highComplementLevelParam != nullptr)
                    ? highComplementLevelParam->convertFrom0to1 (highComplementLevelParam->getValue()) : 0.0f;
                dbTotal = highLevel;
            }
            // Mid zone: controlled by complement bells
            else
            {
                // Complement Bell 1 (boost = more wet at that frequency) - 2nd order parametric
                if (cb1On && std::abs (cb1Gain) > 0.1f)
                {
                    float w = freq / cb1Freq;
                    float dw = w - 1.0f / w;
                    float bellShape = 1.0f / (1.0f + cb1Q * cb1Q * dw * dw);
                    dbTotal += cb1Gain * bellShape;
                }

                // Complement Bell 2 - 2nd order parametric
                if (cb2On && std::abs (cb2Gain) > 0.1f)
                {
                    float w = freq / cb2Freq;
                    float dw = w - 1.0f / w;
                    float bellShape = 1.0f / (1.0f + cb2Q * cb2Q * dw * dw);
                    dbTotal += cb2Gain * bellShape;
                }
            }

            float y = zeroDbY - dbTotal * pixelsPerDb;

            if (!started) { path.startNewSubPath (x, y); started = true; }
            else path.lineTo (x, y);
        }
        return path;
    }

    void drawNode (juce::Graphics& g, float x, float y, juce::Colour /*colour*/, bool active,
                   const juce::String& freqText, const juce::String& slopeText, bool /*isHP*/,
                   juce::Rectangle<float> plotArea) const
    {
        // Glacial blue/white color scheme
        juce::Colour nodeColour = juce::Colour (0xFFE8F4FC);  // Light glacial blue-white
        const float radius = active ? 10.0f : 8.0f;
        const float strokeWidth = 2.5f;  // 12% thicker ring

        // Clamp node Y position to stay within plot area for drawing
        float clampedY = juce::jlimit (plotArea.getY() + radius + 5, plotArea.getBottom() - radius - 5, y);

        // Subtle glow for active
        if (active)
        {
            for (int i = 3; i > 0; --i)
            {
                float r = radius + i * 3.0f;
                g.setColour (nodeColour.withAlpha (0.15f / (float) i));
                g.fillEllipse (x - r, clampedY - r, r * 2, r * 2);
            }
        }

        // Ring style: outline only when inactive, filled when active
        if (active)
        {
            g.setColour (nodeColour);
            g.fillEllipse (x - radius, clampedY - radius, radius * 2, radius * 2);
        }
        else
        {
            g.setColour (nodeColour.withAlpha (0.9f));
            g.drawEllipse (x - radius, clampedY - radius, radius * 2, radius * 2, strokeWidth);
        }

        // Labels below node - clean white text
        float labelY = clampedY + radius + 6;
        if (labelY + 24 > plotArea.getBottom() - 10)
            labelY = clampedY - radius - 28;  // Put above if no room below

        g.setColour (nodeColour);
        g.setFont (11.0f);
        g.drawText (freqText, (int) x - 30, (int) labelY, 60, 13, juce::Justification::centred);
        g.setFont (9.0f);
        g.setColour (nodeColour.withAlpha (0.7f));
        g.drawText (slopeText, (int) x - 30, (int) labelY + 12, 60, 11, juce::Justification::centred);
    }

    float freqToX (float freq, juce::Rectangle<float> area) const
    {
        float norm = (std::log10 (juce::jlimit (20.0f, 20000.0f, freq)) - std::log10 (20.0f))
                   / (std::log10 (20000.0f) - std::log10 (20.0f));
        return area.getX() + norm * area.getWidth();
    }

    float xToFreq (float x, juce::Rectangle<float> area) const
    {
        float norm = juce::jlimit (0.0f, 1.0f, (x - area.getX()) / area.getWidth());
        return 20.0f * std::pow (1000.0f, norm);
    }

    float getSlopeDb (int idx) const { const float s[] = {6,12,24,48}; return s[juce::jlimit(0,3,idx)]; }
    juce::String getSlopeLabel (int idx) const { const char* s[] = {"6dB","12dB","24dB","48dB","72dB","96dB"}; return s[juce::jlimit(0,5,idx)]; }
    juce::String formatFreq (float f) const { return f >= 1000 ? juce::String(f/1000,1)+"k" : juce::String((int)f)+"Hz"; }
    juce::String formatGain (float g) const { return (g >= 0 ? "+" : "") + juce::String(g, 1) + "dB"; }

    void drawBellNode (juce::Graphics& g, float x, float y, juce::Colour /*colour*/, bool active,
                       const juce::String& freqText, const juce::String& gainText,
                       juce::Rectangle<float> plotArea, const juce::String& label) const
    {
        // Same glacial white as HP/LP nodes for consistency
        juce::Colour nodeColour = juce::Colour (0xFFE8F4FC);  // Light glacial blue-white
        const float radius = active ? 10.0f : 8.0f;
        const float strokeWidth = 2.5f;  // 12% thicker ring

        // Clamp node Y position to stay within plot area
        float clampedY = juce::jlimit (plotArea.getY() + radius + 5, plotArea.getBottom() - radius - 5, y);

        // Subtle glow for active
        if (active)
        {
            for (int i = 3; i > 0; --i)
            {
                float r = radius + i * 3.0f;
                g.setColour (nodeColour.withAlpha (0.15f / (float) i));
                g.fillEllipse (x - r, clampedY - r, r * 2, r * 2);
            }
        }

        // Ring style: outline only (same as HP/LP), filled when active
        if (active)
        {
            g.setColour (nodeColour);
            g.fillEllipse (x - radius, clampedY - radius, radius * 2, radius * 2);
            // Label in center when filled
            g.setColour (juce::Colour (0xFF1A1A2E));  // Dark text on light fill
            g.setFont (10.0f);
            g.drawText (label, (int)(x - radius), (int)(clampedY - radius),
                       (int)(radius * 2), (int)(radius * 2), juce::Justification::centred);
        }
        else
        {
            // Empty ring style - same as HP/LP
            g.setColour (nodeColour.withAlpha (0.9f));
            g.drawEllipse (x - radius, clampedY - radius, radius * 2, radius * 2, strokeWidth);
            // Label in center of ring
            g.setFont (9.0f);
            g.drawText (label, (int)(x - radius - 1), (int)(clampedY - radius),
                       (int)(radius * 2 + 2), (int)(radius * 2), juce::Justification::centred);
        }

        // Freq and gain labels below - clean readable text
        float labelY = clampedY + radius + 6;
        if (labelY + 26 > plotArea.getBottom() - 10)
            labelY = clampedY - radius - 30;

        g.setColour (nodeColour);
        g.setFont (11.0f);
        g.drawText (freqText, (int) x - 30, (int) labelY, 60, 13, juce::Justification::centred);
        g.setFont (9.0f);
        g.setColour (nodeColour.withAlpha (0.7f));
        g.drawText (gainText, (int) x - 30, (int) labelY + 12, 60, 11, juce::Justification::centred);
    }

    void drawSpectrum (juce::Graphics& g, juce::Rectangle<float> area, float alpha) const
    {
        // ===== COMPLEMENT ZONE VISUALIZATION =====
        // When Complement mode is ON, show the frequencies that complement provides
        // (inverse of what the main EQ cuts - the "clean passthrough zone")
        bool complementOn = complementParam ? (complementParam->getValue() > 0.5f) : false;

        if (complementOn)
        {
            drawComplementZone (g, area, alpha);
        }

        // Get spectrum data from processor
        std::array<float, ColorVocoderAudioProcessor::fftSize / 2> spectrum;
        processor.getSpectrumData (spectrum);

        const float minFreq = 20.0f;
        const float maxFreq = 20000.0f;
        // Use actual sample rate from processor instead of hardcoded value
        const float sampleRate = (float) processor.getCurrentSampleRate();
        const int numBins = ColorVocoderAudioProcessor::fftSize / 2;

        // Check if we have any data (avoid drawing empty spectrum)
        float maxMag = 0.0f;
        for (int i = 1; i < numBins; ++i)
            maxMag = std::max (maxMag, spectrum[(size_t) i]);

        if (maxMag < 0.0001f)
            return;  // No audio data yet

        juce::Path spectrumPath;
        bool pathStarted = false;

        // Store peak info for note labels
        struct PeakInfo { float freq; float x; float y; float mag; };
        std::vector<PeakInfo> peaks;

        // Draw spectrum with boosted visibility
        float prevMag = 0.0f;
        float prevPrevMag = 0.0f;

        for (int i = 1; i < numBins; ++i)
        {
            float freq = (float) i * sampleRate / (float) ColorVocoderAudioProcessor::fftSize;
            if (freq < minFreq || freq > maxFreq)
                continue;

            float x = freqToX (freq, area);
            float magnitude = spectrum[(size_t) i];

            // Boost magnitude for visibility (gentler curve)
            float boosted = std::pow (magnitude, 0.5f) * 1.2f;  // Gentler boost
            boosted = juce::jlimit (0.0f, 1.0f, boosted);

            float y = area.getBottom() - boosted * area.getHeight() * 0.7f;

            if (!pathStarted)
            {
                spectrumPath.startNewSubPath (x, area.getBottom());
                spectrumPath.lineTo (x, y);
                pathStarted = true;
            }
            else
            {
                spectrumPath.lineTo (x, y);
            }

            // Detect peaks: current bin is higher than neighbors AND above threshold
            // Threshold: boosted > 0.35 means it's a significant peak (above ~-9dB visual)
            if (i > 2 && prevMag > prevPrevMag && prevMag > magnitude && prevMag > 0.35f)
            {
                float peakFreq = (float) (i - 1) * sampleRate / (float) ColorVocoderAudioProcessor::fftSize;
                float peakX = freqToX (peakFreq, area);
                float peakY = area.getBottom() - prevMag * area.getHeight() * 0.9f;
                peaks.push_back ({ peakFreq, peakX, peakY, prevMag });
            }

            prevPrevMag = prevMag;
            prevMag = boosted;
        }

        if (pathStarted)
        {
            spectrumPath.lineTo (area.getRight(), area.getBottom());
            spectrumPath.closeSubPath();

            // Bright cyan/blue gradient - very visible
            juce::ColourGradient specGradient (
                juce::Colour (0xFF00DDFF).withAlpha (alpha * 0.7f), 0, area.getY(),
                juce::Colour (0xFF0066FF).withAlpha (alpha * 0.15f), 0, area.getBottom(), false);
            g.setGradientFill (specGradient);
            g.fillPath (spectrumPath);

            // Bright outline
            g.setColour (juce::Colour (0xFF00FFFF).withAlpha (alpha * 0.6f));
            g.strokePath (spectrumPath, juce::PathStrokeType (1.5f));
        }

        // Draw note labels on peaks (max 6, with minimum spacing)
        if (!peaks.empty())
        {
            // Sort by magnitude (strongest first)
            std::sort (peaks.begin(), peaks.end(),
                [](const PeakInfo& a, const PeakInfo& b) { return a.mag > b.mag; });

            // Keep only top peaks, ensuring no overlap
            std::vector<PeakInfo> displayPeaks;
            const float minLabelSpacing = 28.0f;  // Minimum pixels between labels

            for (const auto& peak : peaks)
            {
                if (displayPeaks.size() >= 6)
                    break;

                // Check if this peak is far enough from existing labels
                bool tooClose = false;
                for (const auto& existing : displayPeaks)
                {
                    if (std::abs (peak.x - existing.x) < minLabelSpacing)
                    {
                        tooClose = true;
                        break;
                    }
                }

                if (!tooClose)
                    displayPeaks.push_back (peak);
            }

            // Draw note labels
            g.setFont (9.0f);

            for (const auto& peak : displayPeaks)
            {
                juce::String noteName = freqToNoteName (peak.freq);

                // Position label above peak (with some padding)
                float labelY = peak.y - 12.0f;

                // Keep label within bounds
                if (labelY < area.getY() + 2)
                    labelY = area.getY() + 2;

                // Semi-transparent background for readability
                float textWidth = 22.0f;
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.fillRoundedRectangle (peak.x - textWidth / 2, labelY - 1, textWidth, 11, 3.0f);

                // Note text - bright color
                g.setColour (juce::Colour (0xFFFFFF00).withAlpha (alpha * 0.95f));  // Yellow
                g.drawText (noteName, (int) (peak.x - textWidth / 2), (int) labelY,
                           (int) textWidth, 10, juce::Justification::centred);
            }
        }
    }

    // Convert frequency to musical note name
    static juce::String freqToNoteName (float freq)
    {
        static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

        // Calculate MIDI note number from frequency (A4 = 440Hz = MIDI 69)
        float midiNote = 12.0f * std::log2 (freq / 440.0f) + 69.0f;
        int noteNum = (int) std::round (midiNote);

        if (noteNum < 0 || noteNum > 127)
            return "";

        int octave = (noteNum / 12) - 1;
        int noteIndex = noteNum % 12;

        return juce::String (noteNames[noteIndex]) + juce::String (octave);
    }

    // Draw complement zone showing frequencies passed through by complement buffer
    // When HP cuts lows from wet, complement provides clean lows (and vice versa for LP)
    void drawComplementZone (juce::Graphics& g, juce::Rectangle<float> area, float alpha) const
    {
        // Get HP and LP frequencies
        float hpFreq = hpFreqParam ? hpFreqParam->convertFrom0to1 (hpFreqParam->getValue()) : 20.0f;
        float lpFreq = lpFreqParam ? lpFreqParam->convertFrom0to1 (lpFreqParam->getValue()) : 20000.0f;

        // Only draw if HP/LP are actually cutting something
        bool hpActive = hpFreq > 25.0f;   // HP is cutting lows
        bool lpActive = lpFreq < 19000.0f; // LP is cutting highs

        if (!hpActive && !lpActive)
            return;

        // Complement color: green/teal to distinguish from blue spectrum
        juce::Colour complementColor = juce::Colour (0xFF00FF88);  // Green-cyan

        // Draw HP complement zone (low frequencies that complement provides)
        if (hpActive)
        {
            float leftX = area.getX();
            float rightX = freqToX (hpFreq, area);

            // Create gradient from left to HP cutoff
            juce::ColourGradient gradient (
                complementColor.withAlpha (alpha * 0.25f), leftX, area.getY(),
                complementColor.withAlpha (alpha * 0.05f), rightX, area.getY(), false);

            // Fill the zone
            g.setGradientFill (gradient);
            g.fillRect (leftX, area.getY(), rightX - leftX, area.getHeight());

            // Subtle vertical line at HP cutoff
            g.setColour (complementColor.withAlpha (alpha * 0.4f));
            g.drawVerticalLine ((int) rightX, area.getY(), area.getBottom());

            // Label: "CLEAN LOWS" near the zone
            if (rightX - leftX > 40)  // Only if there's room
            {
                g.setFont (8.0f);
                g.setColour (complementColor.withAlpha (alpha * 0.7f));
                float labelX = leftX + 4;
                float labelY = area.getBottom() - 14;
                g.drawText ("CLEAN", (int) labelX, (int) labelY, 36, 10, juce::Justification::left);
            }
        }

        // LP complement zone removed - highs now follow mix knob like everything else
        // Only SUB (below HP) is at fixed level, so only SUB gets the green zone
        (void) lpActive;  // Suppress unused warning
    }

    ColorVocoderAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvtsRef;

    // HP/LP filter params
    juce::RangedAudioParameter* hpFreqParam = nullptr;
    juce::RangedAudioParameter* lpFreqParam = nullptr;
    juce::RangedAudioParameter* hpSlopeParam = nullptr;
    juce::RangedAudioParameter* lpSlopeParam = nullptr;
    juce::RangedAudioParameter* enabledParam = nullptr;

    // Bell EQ params
    juce::RangedAudioParameter* bell1EnabledParam = nullptr;
    juce::RangedAudioParameter* bell1FreqParam = nullptr;
    juce::RangedAudioParameter* bell1GainParam = nullptr;
    juce::RangedAudioParameter* bell1QParam = nullptr;
    juce::RangedAudioParameter* bell2EnabledParam = nullptr;
    juce::RangedAudioParameter* bell2FreqParam = nullptr;
    juce::RangedAudioParameter* bell2GainParam = nullptr;
    juce::RangedAudioParameter* bell2QParam = nullptr;

    // Handle bounds for dragging
    mutable juce::Rectangle<float> hpHandleBounds, lpHandleBounds;
    mutable juce::Rectangle<float> bell1HandleBounds, bell2HandleBounds;

    // Dragging states
    bool draggingHP = false, draggingLP = false;
    bool draggingBell1 = false, draggingBell2 = false;

    // Which band is selected for Q knob (1=HP, 2=LP, 3=Bell1, 4=Bell2)
    int selectedBand = 1;

    // HP/LP Q parameters (boost - Y axis)
    juce::RangedAudioParameter* hpQParam = nullptr;
    juce::RangedAudioParameter* lpQParam = nullptr;

    // HP/LP Q width parameters (peak width - knob control)
    juce::RangedAudioParameter* hpQWidthParam = nullptr;
    juce::RangedAudioParameter* lpQWidthParam = nullptr;

    // Complement mode
    juce::RangedAudioParameter* complementParam = nullptr;
    mutable juce::Rectangle<float> complementButtonBounds { 0, 0, 0, 0 };  // Initialize to empty

    // Complement Bell params (for wet/dry ratio control)
    juce::RangedAudioParameter* compBell1EnabledParam = nullptr;
    juce::RangedAudioParameter* compBell1FreqParam = nullptr;
    juce::RangedAudioParameter* compBell1GainParam = nullptr;
    juce::RangedAudioParameter* compBell1QParam = nullptr;
    juce::RangedAudioParameter* compBell2EnabledParam = nullptr;
    juce::RangedAudioParameter* compBell2FreqParam = nullptr;
    juce::RangedAudioParameter* compBell2GainParam = nullptr;
    juce::RangedAudioParameter* compBell2QParam = nullptr;

    // Sub/High complement level params
    juce::RangedAudioParameter* subComplementLevelParam = nullptr;
    juce::RangedAudioParameter* highComplementLevelParam = nullptr;

    // Complement bell handle bounds for dragging
    mutable juce::Rectangle<float> compBell1HandleBounds, compBell2HandleBounds;
    bool draggingCompBell1 = false, draggingCompBell2 = false;
};

//==============================================================
// Main editor
//==============================================================

class ColorVocoderAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    ColorVocoderAudioProcessorEditor (ColorVocoderAudioProcessor&);
    ~ColorVocoderAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ColorVocoderAudioProcessor& processor;

    // ===== PRESET MANAGEMENT =====
    PresetManager presetManager;
    juce::ComboBox presetBox;
    juce::TextButton savePresetButton { "Save" };
    juce::TextButton deletePresetButton { "X" };

    void refreshPresetList();
    void savePresetWithDialog();
    void loadSelectedPreset();
    void deleteSelectedPreset();

    // Custom look and feel for bigger knobs
    BigKnobLookAndFeel bigKnobLookAndFeel;

    using Slider = juce::Slider;
    using Combo  = juce::ComboBox;
    using Button = juce::ToggleButton;

    // Band visualiser
    BandDisplay bandDisplay;

    // Controls
    Button bypassButton;

    // Global
    Slider mixSlider, depthSlider;
    juce::Label mixLabel, depthLabel;

    // Scale & Formant
    Combo  keyBox, scaleBox;
    Slider scaleAmtSlider, formantSlider;
    juce::Label keyLabel, scaleLabel, scaleAmtLabel, formantLabel;

    // Chord controls (added to Scale & Formant section)
    Combo  chordDegreeBox, chordVariationBox;
    Slider chordAmountSlider;
    juce::Label chordDegreeLabel, chordVariationLabel, chordAmountLabel;

    // Harmonics
    Slider harmonicShiftSlider;
    juce::Label harmonicShiftLabel;

    // Bands
    Slider bandCountSlider, bandWidthSlider;
    Slider lowFreqSlider, highFreqSlider;
    Slider attackSlider, releaseSlider;
    juce::Label bandCountLabel, bandWidthLabel;
    juce::Label lowFreqLabel, highFreqLabel;
    juce::Label attackLabel, releaseLabel;

    // Noise / Unvoiced (Ableton-style: single Unvoiced control)
    Slider noiseColorSlider, hpBlendSlider;
    Slider unvoicedSlider;
    Combo  hpBlendSlopeBox;
    juce::Label noiseColorLabel, hpBlendLabel;
    juce::Label unvoicedLabel, hpBlendSlopeLabel;

    // Gate (moved to Bands section)
    Slider gateSlider;
    juce::Label gateLabel;

    // ===== PHASE 1: Carrier controls =====
    Combo carrierModeBox;
    Slider carrierPitchSlider;
    juce::Label carrierModeLabel, carrierPitchLabel;

    // ===== INPUT FILTER SECTION (Visual Display) =====
    std::unique_ptr<InputFilterDisplay> inputFilterDisplay;

    // ===== BELL EQ Q KNOB =====
    Slider bellQSlider;
    juce::Label bellQLabel;

    // Group boxes
    juce::GroupComponent globalGroup    { "globalGroup",    "Global" };
    juce::GroupComponent scaleGroup     { "scaleGroup",     "Scale & Chord" };  // Merged with Harmonics
    juce::GroupComponent harmonicGroup  { "harmonicGroup",  "Harmonics" };      // Hidden, merged into scaleGroup
    juce::GroupComponent bandsGroup     { "bandsGroup",     "Bands" };
    juce::GroupComponent noiseGroup     { "noiseGroup",     "Noise / Unvoiced" };
    juce::GroupComponent carrierGroup   { "carrierGroup",   "Carrier" };

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>      mixAttachment, depthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>    keyAttachment, scaleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>      scaleAmtAttachment, formantAttachment;

    // Chord attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>   chordDegreeAttachment, chordVariationAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>     chordAmountAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>      harmonicShiftAttachment;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>      bandCountAttachment, bandWidthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>      lowFreqAttachment, highFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>      attackAttachment, releaseAttachment;

    // Noise / Unvoiced attachments (Ableton-style)
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>     noiseColorAttachment, hpBlendAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>     unvoicedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>   hpBlendSlopeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>     gateAttachment;

    // ===== PHASE 1: Carrier attachments =====
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>   carrierModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>     carrierPitchAttachment;

    // Bell EQ Q knob - no attachment, uses onValueChange callback for dynamic selection

    // INPUT FILTER attachments not needed - InputFilterDisplay handles params directly

    // Licensing UI
    juce::TextEditor licenseEditor;
    juce::TextButton activateButton { "Activate" };
    juce::Label      licenseStatusLabel;

    // Helpers
    void setupKnob (Slider& slider, juce::Label& label, const juce::String& text);
    void setupHzKnob (Slider& slider, juce::Label& label, const juce::String& text);
    void setupMsKnob (Slider& slider, juce::Label& label, const juce::String& text);
    void setupComboWithLabel (Combo& box, juce::Label& label, const juce::String& text);

    // Show/hide license controls based on state
    void updateLicenseUI();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ColorVocoderAudioProcessorEditor)
};
