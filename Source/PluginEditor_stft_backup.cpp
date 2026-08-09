#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
// InputFilterDisplay Implementation
// (Methods moved from header to cpp for better compilation)
//==============================================================================

void InputFilterDisplay::paint (juce::Graphics& g)
{
    try { paintInternal (g); }
    catch (...) {
        g.fillAll (juce::Colour (0xFF1A1A24));
        g.setColour (juce::Colours::red);
        g.drawText ("EQ Error", getLocalBounds(), juce::Justification::centred);
    }
}

void InputFilterDisplay::paintInternal (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    bool enabled = (enabledParam != nullptr) ? (enabledParam->getValue() > 0.5f) : true;

    // Gradient background
    juce::ColourGradient bgGradient (
        juce::Colour (0xFF0D0D12), 0, bounds.getY(),
        juce::Colour (0xFF1A1A24), 0, bounds.getBottom(), false);
    g.setGradientFill (bgGradient);
    g.fillRoundedRectangle (bounds, 6.0f);

    // Border
    g.setColour (enabled ? juce::Colour (0xFF3A3A45) : juce::Colour (0xFF1A1A1A));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    auto toggleArea = bounds.removeFromLeft (50).reduced (8, 8);
    auto plotArea = bounds.reduced (8, 12);

    drawToggleButton (g, toggleArea, enabled);

    if (plotArea.getWidth() < 50 || plotArea.getHeight() < 20)
    {
        g.setColour (juce::Colours::grey);
        g.drawText ("Master EQ", getLocalBounds(), juce::Justification::centred);
        return;
    }

    if (hpFreqParam == nullptr || lpFreqParam == nullptr)
    {
        g.setColour (juce::Colours::grey);
        g.drawText ("Master EQ", bounds, juce::Justification::centred);
        return;
    }

    float alpha = enabled ? 1.0f : 0.3f;

    drawGrid (g, plotArea, enabled);
    drawSpectrum (g, plotArea, enabled ? 0.5f : 0.15f);

    float hpFreq = hpFreqParam->convertFrom0to1 (hpFreqParam->getValue());
    float lpFreq = lpFreqParam->convertFrom0to1 (lpFreqParam->getValue());
    int hpSlope = (int) (hpSlopeParam->convertFrom0to1 (hpSlopeParam->getValue()) + 0.5f);
    int lpSlope = (int) (lpSlopeParam->convertFrom0to1 (lpSlopeParam->getValue()) + 0.5f);

    g.saveState();
    g.reduceClipRegion (plotArea.toNearestInt());

    bool complementOn = (complementParam != nullptr) && (complementParam->getValue() > 0.5f);

    juce::Colour curveColour = complementOn ? juce::Colour (0xFFFFD700) : juce::Colour (0xFF64D2FF);
    juce::Colour fillColourTop = complementOn ? juce::Colour (0x50FFD700) : juce::Colour (0x5064D2FF);
    juce::Colour fillColourBot = complementOn ? juce::Colour (0x10FFD700) : juce::Colour (0x1064D2FF);

    float hpGainDb = (hpQParam != nullptr) ? hpQParam->convertFrom0to1 (hpQParam->getValue()) : 0.0f;
    float lpGainDb = (lpQParam != nullptr) ? lpQParam->convertFrom0to1 (lpQParam->getValue()) : 0.0f;
    float hpQWidth = (hpQWidthParam != nullptr) ? hpQWidthParam->convertFrom0to1 (hpQWidthParam->getValue()) : 1.25f;
    float lpQWidth = (lpQWidthParam != nullptr) ? lpQWidthParam->convertFrom0to1 (lpQWidthParam->getValue()) : 1.25f;

    if (complementOn)
    {
        // === DUAL CURVE VISUALIZATION FOR COMPLEMENT MODE ===
        // WET curve (gold) shows what happens to wet signal
        // DRY curve (cyan) shows the INVERSE - what happens to dry signal

        juce::Path wetPath = createComplementCurve (plotArea, hpFreq, lpFreq, hpSlope, lpSlope, hpGainDb, lpGainDb, hpQWidth, lpQWidth);
        juce::Path dryPath = createDryInverseCurve (plotArea, hpFreq, lpFreq, hpSlope, lpSlope, hpGainDb, lpGainDb, hpQWidth, lpQWidth);

        // Colors for dual curves
        juce::Colour wetColour (0xFFFFD700);  // Gold for WET
        juce::Colour dryColour (0xFF64D2FF);  // Cyan for DRY

        // Draw fill between the two curves to show separation zone
        // This creates a visual representation of the wet/dry separation
        juce::Path separationFill;

        // Build path: wet curve forward, then dry curve backward
        bool firstPoint = true;
        for (float x = plotArea.getX(); x <= plotArea.getRight(); x += 1.0f)
        {
            float freq = xToFreq (x, plotArea);

            // Calculate wet Y position
            float wetDb = 0.0f;
            float dryDb = 0.0f;

            const float zeroDbY = plotArea.getCentreY();
            const float dbRange = 36.0f;  // ±18dB so all points can reach top
            const float pixelsPerDb = plotArea.getHeight() * 0.45f / (dbRange / 2.0f);

            // Get bell parameters
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

            // Only bells contribute to separation (HP/LP are same for both)
            if (cb1On && std::abs (cb1Gain) > 0.1f)
            {
                float w = freq / cb1Freq;
                float dw = w - 1.0f / w;
                float bellShape = 1.0f / (1.0f + cb1Q * cb1Q * dw * dw);
                wetDb += cb1Gain * bellShape;
                dryDb += (-cb1Gain) * bellShape;
            }
            if (cb2On && std::abs (cb2Gain) > 0.1f)
            {
                float w = freq / cb2Freq;
                float dw = w - 1.0f / w;
                float bellShape = 1.0f / (1.0f + cb2Q * cb2Q * dw * dw);
                wetDb += cb2Gain * bellShape;
                dryDb += (-cb2Gain) * bellShape;
            }

            float wetY = zeroDbY - wetDb * pixelsPerDb;
            float dryY = zeroDbY - dryDb * pixelsPerDb;

            if (firstPoint) { separationFill.startNewSubPath (x, wetY); firstPoint = false; }
            else separationFill.lineTo (x, wetY);
        }

        // Now go back along dry curve
        for (float x = plotArea.getRight(); x >= plotArea.getX(); x -= 1.0f)
        {
            float freq = xToFreq (x, plotArea);

            const float zeroDbY = plotArea.getCentreY();
            const float dbRange = 36.0f;  // ±18dB so all points can reach top
            const float pixelsPerDb = plotArea.getHeight() * 0.45f / (dbRange / 2.0f);

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

            float dryDb = 0.0f;
            if (cb1On && std::abs (cb1Gain) > 0.1f)
            {
                float w = freq / cb1Freq;
                float dw = w - 1.0f / w;
                float bellShape = 1.0f / (1.0f + cb1Q * cb1Q * dw * dw);
                dryDb += (-cb1Gain) * bellShape;
            }
            if (cb2On && std::abs (cb2Gain) > 0.1f)
            {
                float w = freq / cb2Freq;
                float dw = w - 1.0f / w;
                float bellShape = 1.0f / (1.0f + cb2Q * cb2Q * dw * dw);
                dryDb += (-cb2Gain) * bellShape;
            }

            float dryY = zeroDbY - dryDb * pixelsPerDb;
            separationFill.lineTo (x, dryY);
        }
        separationFill.closeSubPath();

        // Fill the separation zone with gradient (gold to cyan blend) - dry at 50%
        juce::ColourGradient sepGradient (
            wetColour.withAlpha (0.25f * alpha), 0, plotArea.getY(),
            dryColour.withAlpha (0.125f * alpha), 0, plotArea.getBottom(), false);
        g.setGradientFill (sepGradient);
        g.fillPath (separationFill);

        // Draw WET curve fill (above 0dB = gold zone)
        juce::Path wetFillPath = wetPath;
        wetFillPath.lineTo (plotArea.getRight(), plotArea.getCentreY());
        wetFillPath.lineTo (plotArea.getX(), plotArea.getCentreY());
        wetFillPath.closeSubPath();

        juce::ColourGradient wetFillGradient (
            wetColour.withAlpha (0.3f * alpha), 0, plotArea.getY(),
            wetColour.withAlpha (0.05f * alpha), 0, plotArea.getCentreY(), false);
        g.setGradientFill (wetFillGradient);
        g.fillPath (wetFillPath);

        // Draw DRY curve fill (below 0dB = cyan zone) - 50% opacity to be secondary
        juce::Path dryFillPath = dryPath;
        dryFillPath.lineTo (plotArea.getRight(), plotArea.getCentreY());
        dryFillPath.lineTo (plotArea.getX(), plotArea.getCentreY());
        dryFillPath.closeSubPath();

        juce::ColourGradient dryFillGradient (
            dryColour.withAlpha (0.025f * alpha), 0, plotArea.getCentreY(),
            dryColour.withAlpha (0.15f * alpha), 0, plotArea.getBottom(), false);
        g.setGradientFill (dryFillGradient);
        g.fillPath (dryFillPath);

        // Draw 0dB reference line
        g.setColour (juce::Colour (0x40FFFFFF));
        g.drawHorizontalLine ((int) plotArea.getCentreY(), plotArea.getX(), plotArea.getRight());

        // Draw DRY curve with glow (cyan) - 50% opacity to be secondary to main curve
        for (int i = 3; i > 0; --i)
        {
            g.setColour (dryColour.withAlpha (0.06f * alpha / (float) i));
            g.strokePath (dryPath, juce::PathStrokeType ((float) i * 2.0f));
        }
        g.setColour (dryColour.withAlpha (0.5f * alpha));
        g.strokePath (dryPath, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved));

        // Draw WET curve with glow (gold) - on top
        for (int i = 3; i > 0; --i)
        {
            g.setColour (wetColour.withAlpha (0.15f * alpha / (float) i));
            g.strokePath (wetPath, juce::PathStrokeType ((float) i * 2.5f));
        }
        g.setColour (wetColour.withAlpha (alpha));
        g.strokePath (wetPath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));

        // Draw small labels for WET/DRY curves (DRY at 50% to be secondary)
        g.setFont (8.0f);
        g.setColour (wetColour.withAlpha (0.8f * alpha));
        g.drawText ("WET", plotArea.getRight() - 28, (int) plotArea.getY() + 2, 26, 10, juce::Justification::right);
        g.setColour (dryColour.withAlpha (0.4f * alpha));
        g.drawText ("DRY", plotArea.getRight() - 28, (int) plotArea.getBottom() - 12, 26, 10, juce::Justification::right);
    }
    else
    {
        // === NORMAL MODE (non-complement) ===
        juce::Path responsePath = createResponsePath (plotArea, hpFreq, lpFreq, hpSlope, lpSlope, 0.0f, 0.0f);

        juce::Path fillPath = responsePath;
        fillPath.lineTo (plotArea.getRight(), plotArea.getBottom() + 50);
        fillPath.lineTo (plotArea.getX(), plotArea.getBottom() + 50);
        fillPath.closeSubPath();

        juce::ColourGradient fillGradient (
            fillColourTop.withMultipliedAlpha (alpha), 0, plotArea.getY(),
            fillColourBot.withMultipliedAlpha (alpha), 0, plotArea.getBottom(), false);
        g.setGradientFill (fillGradient);
        g.fillPath (fillPath);

        for (int i = 3; i > 0; --i)
        {
            g.setColour (curveColour.withAlpha (0.15f * alpha / (float) i));
            g.strokePath (responsePath, juce::PathStrokeType ((float) i * 2.5f));
        }
        g.setColour (curveColour.withAlpha (alpha));
        g.strokePath (responsePath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
    }

    g.restoreState();

    // Draw nodes
    float hpX = freqToX (hpFreq, plotArea);
    float lpX = freqToX (lpFreq, plotArea);
    float centerY = plotArea.getCentreY();

    float hpBoostDb = (hpQParam != nullptr) ? hpQParam->convertFrom0to1 (hpQParam->getValue()) : 0.0f;
    float lpBoostDb = (lpQParam != nullptr) ? lpQParam->convertFrom0to1 (lpQParam->getValue()) : 0.0f;

    float hpY = centerY - (hpBoostDb / 18.0f) * (plotArea.getHeight() * 0.45f);
    float lpY = centerY - (lpBoostDb / 18.0f) * (plotArea.getHeight() * 0.45f);

    juce::Colour hpLpColour (0xFF64D2FF);

    float hpYClamped = juce::jlimit (plotArea.getY() + 12.0f, plotArea.getBottom() - 12.0f, hpY);
    hpHandleBounds = juce::Rectangle<float> (hpX - 12, hpYClamped - 12, 24, 24);
    juce::String hpLabel = getSlopeLabel (hpSlope) + " HP";
    if (hpBoostDb > 0.1f) hpLabel = formatGain (hpBoostDb);
    drawNode (g, hpX, hpYClamped, hpLpColour.withAlpha (alpha), draggingHP,
              formatFreq (hpFreq), hpLabel, true, plotArea);

    float lpYClamped = juce::jlimit (plotArea.getY() + 12.0f, plotArea.getBottom() - 12.0f, lpY);
    lpHandleBounds = juce::Rectangle<float> (lpX - 12, lpYClamped - 12, 24, 24);
    juce::String lpLabel = getSlopeLabel (lpSlope) + " LP";
    if (lpBoostDb > 0.1f) lpLabel = formatGain (lpBoostDb);
    drawNode (g, lpX, lpYClamped, hpLpColour.withAlpha (alpha), draggingLP,
              formatFreq (lpFreq), lpLabel, false, plotArea);

    // Bell nodes
    if (complementOn)
    {
        juce::Colour compBell1Colour (0xFFFFD700);
        juce::Colour compBell2Colour (0xFFFFAA00);
        bool cb1On = (compBell1EnabledParam != nullptr) && (compBell1EnabledParam->getValue() > 0.5f);
        bool cb2On = (compBell2EnabledParam != nullptr) && (compBell2EnabledParam->getValue() > 0.5f);

        if (compBell1FreqParam != nullptr && compBell1GainParam != nullptr)
        {
            float cb1Freq = compBell1FreqParam->convertFrom0to1 (compBell1FreqParam->getValue());
            float cb1Gain = compBell1GainParam->convertFrom0to1 (compBell1GainParam->getValue());
            float cb1X = freqToX (cb1Freq, plotArea);
            float cb1Y = centerY - (cb1Gain / 18.0f) * (plotArea.getHeight() * 0.45f);
            compBell1HandleBounds = juce::Rectangle<float> (cb1X - 12, cb1Y - 12, 24, 24);
            juce::Colour cb1Col = cb1On ? compBell1Colour : compBell1Colour.withAlpha (0.3f);
            drawBellNode (g, cb1X, cb1Y, cb1Col.withAlpha (alpha), draggingCompBell1,
                         formatFreq (cb1Freq), formatGain (cb1Gain), plotArea, "W1");
        }

        if (compBell2FreqParam != nullptr && compBell2GainParam != nullptr)
        {
            float cb2Freq = compBell2FreqParam->convertFrom0to1 (compBell2FreqParam->getValue());
            float cb2Gain = compBell2GainParam->convertFrom0to1 (compBell2GainParam->getValue());
            float cb2X = freqToX (cb2Freq, plotArea);
            float cb2Y = centerY - (cb2Gain / 18.0f) * (plotArea.getHeight() * 0.45f);
            compBell2HandleBounds = juce::Rectangle<float> (cb2X - 12, cb2Y - 12, 24, 24);
            juce::Colour cb2Col = cb2On ? compBell2Colour : compBell2Colour.withAlpha (0.3f);
            drawBellNode (g, cb2X, cb2Y, cb2Col.withAlpha (alpha), draggingCompBell2,
                         formatFreq (cb2Freq), formatGain (cb2Gain), plotArea, "W2");
        }

        // Sub locked indicator
        if (hpFreq > 25.0f && hpX > plotArea.getX() + 30)
        {
            g.setColour (juce::Colour (0xFF00FF88).withAlpha (alpha * 0.8f));
            g.setFont (9.0f);
            g.drawText ("SUB", (int) (plotArea.getX() + 4), (int) (plotArea.getY() + 4), 30, 12, juce::Justification::left);
        }
    }
    else
    {
        juce::Colour bell1Colour (0xFFFF9F40);
        juce::Colour bell2Colour (0xFFFFD700);
        bool bell1On = (bell1EnabledParam != nullptr) && (bell1EnabledParam->getValue() > 0.5f);
        bool bell2On = (bell2EnabledParam != nullptr) && (bell2EnabledParam->getValue() > 0.5f);

        if (bell1FreqParam != nullptr && bell1GainParam != nullptr)
        {
            float b1Freq = bell1FreqParam->convertFrom0to1 (bell1FreqParam->getValue());
            float b1Gain = bell1GainParam->convertFrom0to1 (bell1GainParam->getValue());
            float b1X = freqToX (b1Freq, plotArea);
            float b1Y = centerY - (b1Gain / 18.0f) * (plotArea.getHeight() * 0.45f);
            bell1HandleBounds = juce::Rectangle<float> (b1X - 12, b1Y - 12, 24, 24);
            juce::Colour b1Col = bell1On ? bell1Colour : bell1Colour.withAlpha (0.3f);
            drawBellNode (g, b1X, b1Y, b1Col.withAlpha (alpha), draggingBell1,
                         formatFreq (b1Freq), formatGain (b1Gain), plotArea, "1");
        }

        if (bell2FreqParam != nullptr && bell2GainParam != nullptr)
        {
            float b2Freq = bell2FreqParam->convertFrom0to1 (bell2FreqParam->getValue());
            float b2Gain = bell2GainParam->convertFrom0to1 (bell2GainParam->getValue());
            float b2X = freqToX (b2Freq, plotArea);
            float b2Y = centerY - (b2Gain / 18.0f) * (plotArea.getHeight() * 0.45f);
            bell2HandleBounds = juce::Rectangle<float> (b2X - 12, b2Y - 12, 24, 24);
            juce::Colour b2Col = bell2On ? bell2Colour : bell2Colour.withAlpha (0.3f);
            drawBellNode (g, b2X, b2Y, b2Col.withAlpha (alpha), draggingBell2,
                         formatFreq (b2Freq), formatGain (b2Gain), plotArea, "2");
        }
    }

    // Title - only show when complement is ON (no "MASTER EQ" text needed)
    if (complementOn)
    {
        g.setColour (juce::Colour (0xFFFFD700).withAlpha (alpha));
        g.setFont (10.0f);
        g.drawText ("COMPLEMENT", getLocalBounds().toFloat().withTrimmedLeft (60).removeFromTop (18), juce::Justification::centredLeft);
    }

    // Complement button
    drawComplementButton (g, getLocalBounds().toFloat(), complementOn, enabled);
}

void InputFilterDisplay::mouseDown (const juce::MouseEvent& e)
{
    try
    {
        auto toggleArea = getLocalBounds().toFloat().removeFromLeft (50).reduced (8, 8);
        if (toggleArea.contains (e.position) && enabledParam != nullptr)
        {
            bool current = enabledParam->getValue() > 0.5f;
            enabledParam->setValueNotifyingHost (current ? 0.0f : 1.0f);
            repaint();
            return;
        }

        if (!complementButtonBounds.isEmpty() &&
            complementButtonBounds.contains (e.position) &&
            complementParam != nullptr)
        {
            bool current = complementParam->getValue() > 0.5f;
            complementParam->setValueNotifyingHost (current ? 0.0f : 1.0f);
            repaint();
            return;
        }

        int newSelection = selectedBand;
        bool complementView = (complementParam != nullptr) && (complementParam->getValue() > 0.5f);

        if (hpHandleBounds.expanded (6).contains (e.position))
        {
            draggingHP = true;
            newSelection = 1;
        }
        else if (lpHandleBounds.expanded (6).contains (e.position))
        {
            draggingLP = true;
            newSelection = 2;
        }
        else if (complementView)
        {
            if (!compBell1HandleBounds.isEmpty() && compBell1HandleBounds.expanded (6).contains (e.position))
            {
                draggingCompBell1 = true;
                newSelection = 5;
                if (compBell1EnabledParam != nullptr)
                    compBell1EnabledParam->setValueNotifyingHost (1.0f);
            }
            else if (!compBell2HandleBounds.isEmpty() && compBell2HandleBounds.expanded (6).contains (e.position))
            {
                draggingCompBell2 = true;
                newSelection = 6;
                if (compBell2EnabledParam != nullptr)
                    compBell2EnabledParam->setValueNotifyingHost (1.0f);
            }
        }
        else
        {
            if (bell1HandleBounds.expanded (6).contains (e.position))
            {
                draggingBell1 = true;
                newSelection = 3;
                if (bell1EnabledParam != nullptr)
                    bell1EnabledParam->setValueNotifyingHost (1.0f);
            }
            else if (bell2HandleBounds.expanded (6).contains (e.position))
            {
                draggingBell2 = true;
                newSelection = 4;
                if (bell2EnabledParam != nullptr)
                    bell2EnabledParam->setValueNotifyingHost (1.0f);
            }
        }

        if (newSelection != selectedBand)
        {
            selectedBand = newSelection;
            if (onBandSelected) onBandSelected (selectedBand);
        }

        repaint();
    }
    catch (...) {}
}

void InputFilterDisplay::mouseDrag (const juce::MouseEvent& e)
{
    try
    {
        auto bounds = getLocalBounds().toFloat();
        bounds.removeFromLeft (50);
        auto plotArea = bounds.reduced (8, 12);
        float centerY = plotArea.getCentreY();

        if (draggingHP && hpFreqParam != nullptr)
        {
            float freq = xToFreq (e.position.x, plotArea);
            freq = juce::jlimit (20.0f, 500.0f, freq);
            hpFreqParam->setValueNotifyingHost (hpFreqParam->convertTo0to1 (freq));

            if (hpQParam != nullptr)
            {
                float yNorm = (centerY - e.position.y) / (plotArea.getHeight() * 0.45f);
                float boostDb = juce::jlimit (-18.0f, 18.0f, yNorm * 18.0f);  // 18dB scale
                hpQParam->setValueNotifyingHost (hpQParam->convertTo0to1 (boostDb));
            }
        }
        else if (draggingLP && lpFreqParam != nullptr)
        {
            float freq = xToFreq (e.position.x, plotArea);
            freq = juce::jlimit (1000.0f, 20000.0f, freq);
            lpFreqParam->setValueNotifyingHost (lpFreqParam->convertTo0to1 (freq));

            if (lpQParam != nullptr)
            {
                float yNorm = (centerY - e.position.y) / (plotArea.getHeight() * 0.45f);
                float boostDb = juce::jlimit (-18.0f, 18.0f, yNorm * 18.0f);  // 18dB scale
                lpQParam->setValueNotifyingHost (lpQParam->convertTo0to1 (boostDb));
            }
        }
        else if (draggingBell1 && bell1FreqParam != nullptr && bell1GainParam != nullptr)
        {
            float freq = xToFreq (e.position.x, plotArea);
            freq = juce::jlimit (20.0f, 20000.0f, freq);
            bell1FreqParam->setValueNotifyingHost (bell1FreqParam->convertTo0to1 (freq));

            float yNorm = (centerY - e.position.y) / (plotArea.getHeight() * 0.45f);
            float gain = juce::jlimit (-18.0f, 18.0f, yNorm * 18.0f);  // 18dB scale
            bell1GainParam->setValueNotifyingHost (bell1GainParam->convertTo0to1 (gain));
        }
        else if (draggingBell2 && bell2FreqParam != nullptr && bell2GainParam != nullptr)
        {
            float freq = xToFreq (e.position.x, plotArea);
            freq = juce::jlimit (20.0f, 20000.0f, freq);
            bell2FreqParam->setValueNotifyingHost (bell2FreqParam->convertTo0to1 (freq));

            float yNorm = (centerY - e.position.y) / (plotArea.getHeight() * 0.45f);
            float gain = juce::jlimit (-18.0f, 18.0f, yNorm * 18.0f);  // 18dB scale
            bell2GainParam->setValueNotifyingHost (bell2GainParam->convertTo0to1 (gain));
        }
        else if (draggingCompBell1 && compBell1FreqParam != nullptr && compBell1GainParam != nullptr)
        {
            float freq = xToFreq (e.position.x, plotArea);
            freq = juce::jlimit (20.0f, 20000.0f, freq);
            compBell1FreqParam->setValueNotifyingHost (compBell1FreqParam->convertTo0to1 (freq));

            float yNorm = (centerY - e.position.y) / (plotArea.getHeight() * 0.45f);
            float gain = juce::jlimit (-18.0f, 18.0f, yNorm * 18.0f);  // 18dB scale
            compBell1GainParam->setValueNotifyingHost (compBell1GainParam->convertTo0to1 (gain));
        }
        else if (draggingCompBell2 && compBell2FreqParam != nullptr && compBell2GainParam != nullptr)
        {
            float freq = xToFreq (e.position.x, plotArea);
            freq = juce::jlimit (20.0f, 20000.0f, freq);
            compBell2FreqParam->setValueNotifyingHost (compBell2FreqParam->convertTo0to1 (freq));

            float yNorm = (centerY - e.position.y) / (plotArea.getHeight() * 0.45f);
            float gain = juce::jlimit (-18.0f, 18.0f, yNorm * 18.0f);  // 18dB scale
            compBell2GainParam->setValueNotifyingHost (compBell2GainParam->convertTo0to1 (gain));
        }

        // Update the right-side knobs in real time during drag
        if (onBandSelected)
            onBandSelected (selectedBand);

        repaint();
    }
    catch (...) {}
}

void InputFilterDisplay::mouseUp (const juce::MouseEvent&)
{
    draggingHP = false;
    draggingLP = false;
    draggingBell1 = false;
    draggingBell2 = false;
    draggingCompBell1 = false;
    draggingCompBell2 = false;
    repaint();
}

void InputFilterDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
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

void InputFilterDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    try
    {
        bool complementView = (complementParam != nullptr) && (complementParam->getValue() > 0.5f);

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
            // Double-click on complement bell = reset gain to 0dB (keep frequency)
            if (compBell1HandleBounds.expanded (6).contains (e.position) && compBell1GainParam != nullptr)
            {
                // Reset gain to 0dB (0.5 normalized = 0dB in -24 to +24 range)
                compBell1GainParam->setValueNotifyingHost (0.5f);
            }
            else if (compBell2HandleBounds.expanded (6).contains (e.position) && compBell2GainParam != nullptr)
            {
                compBell2GainParam->setValueNotifyingHost (0.5f);
            }
        }
        else
        {
            // Double-click on bell = reset gain to 0dB (keep frequency)
            if (bell1HandleBounds.expanded (6).contains (e.position) && bell1GainParam != nullptr)
            {
                // Reset gain to 0dB (0.5 normalized = 0dB in -24 to +24 range)
                bell1GainParam->setValueNotifyingHost (0.5f);
            }
            else if (bell2HandleBounds.expanded (6).contains (e.position) && bell2GainParam != nullptr)
            {
                bell2GainParam->setValueNotifyingHost (0.5f);
            }
        }
        repaint();
    }
    catch (...) {}
}

// Helper drawing methods
void InputFilterDisplay::drawToggleButton (juce::Graphics& g, juce::Rectangle<float> area, bool enabled) const
{
    auto center = area.getCentre();
    float size = juce::jmin (area.getWidth(), area.getHeight()) * 0.8f;
    auto buttonBounds = juce::Rectangle<float> (size, size).withCentre (center);

    g.setColour (enabled ? juce::Colour (0xFF64D2FF).withAlpha (0.3f) : juce::Colour (0x40FFFFFF));
    g.fillEllipse (buttonBounds);

    g.setColour (enabled ? juce::Colour (0xFF64D2FF) : juce::Colour (0x80FFFFFF));
    g.drawEllipse (buttonBounds.reduced (1), 2.0f);

    g.setColour (enabled ? juce::Colour (0xFF64D2FF) : juce::Colour (0x80FFFFFF));
    float iconSize = size * 0.35f;

    juce::Path arc;
    arc.addCentredArc (center.x, center.y, iconSize, iconSize, 0,
                       juce::MathConstants<float>::pi * 0.3f,
                       juce::MathConstants<float>::pi * 1.7f, true);
    g.strokePath (arc, juce::PathStrokeType (2.0f));

    g.drawLine (center.x, center.y - iconSize * 0.3f, center.x, center.y - iconSize * 1.1f, 2.0f);
}

void InputFilterDisplay::drawComplementButton (juce::Graphics& g, juce::Rectangle<float> bounds, bool complementOn, bool filterEnabled) const
{
    float buttonWidth = 70.0f;
    float buttonHeight = 22.0f;
    complementButtonBounds = juce::Rectangle<float> (
        bounds.getRight() - buttonWidth - 10,
        bounds.getY() + 6,
        buttonWidth,
        buttonHeight);

    float alpha = filterEnabled ? 1.0f : 0.4f;

    juce::Colour bgColour = complementOn ? juce::Colour (0xFF9B59B6) : juce::Colour (0xFF2A2A35);
    g.setColour (bgColour.withAlpha (alpha));
    g.fillRoundedRectangle (complementButtonBounds, 4.0f);

    juce::Colour borderColour = complementOn ? juce::Colour (0xFFAA66CC) : juce::Colour (0xFF4A4A55);
    g.setColour (borderColour.withAlpha (alpha));
    g.drawRoundedRectangle (complementButtonBounds.reduced (0.5f), 4.0f, 1.0f);

    g.setColour ((complementOn ? juce::Colours::white : juce::Colour (0xAAFFFFFF)).withAlpha (alpha));
    g.setFont (10.0f);
    g.drawText ("COMPLEMENT", complementButtonBounds, juce::Justification::centred);
}

void InputFilterDisplay::drawGrid (juce::Graphics& g, juce::Rectangle<float> area, bool enabled) const
{
    float alpha = enabled ? 1.0f : 0.3f;

    g.setColour (juce::Colour (0x15FFFFFF).withMultipliedAlpha (alpha));
    const float freqs[] = { 30, 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    for (float f : freqs)
    {
        float x = freqToX (f, area);
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
    }

    g.setColour (juce::Colour (0x25FFFFFF).withMultipliedAlpha (alpha));
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

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

void InputFilterDisplay::drawSpectrum (juce::Graphics& g, juce::Rectangle<float> area, float alpha) const
{
    bool complementOn = complementParam ? (complementParam->getValue() > 0.5f) : false;

    if (complementOn)
        drawComplementZone (g, area, alpha);

    const float minFreq = 20.0f;
    const float maxFreq = 20000.0f;
    const float sampleRate = (float) processor.getCurrentSampleRate();
    const int numBins = ColorVocoderAudioProcessor::fftSize / 2;

    // Helper lambda to build spectrum path
    auto buildSpectrumPath = [&](const std::array<float, ColorVocoderAudioProcessor::fftSize / 2>& spectrum,
                                  juce::Path& outPath, float& outMaxMag) -> bool
    {
        outMaxMag = 0.0f;
        for (int i = 1; i < numBins; ++i)
            outMaxMag = std::max (outMaxMag, spectrum[(size_t) i]);

        if (outMaxMag < 0.0001f)
            return false;

        bool pathStarted = false;
        for (int i = 1; i < numBins; ++i)
        {
            float freq = (float) i * sampleRate / (float) ColorVocoderAudioProcessor::fftSize;
            if (freq < minFreq || freq > maxFreq)
                continue;

            float x = freqToX (freq, area);
            float magnitude = spectrum[(size_t) i];
            float boosted = std::pow (magnitude, 0.5f) * 1.2f;
            boosted = juce::jlimit (0.0f, 1.0f, boosted);
            float y = area.getBottom() - boosted * area.getHeight() * 0.7f;

            if (!pathStarted)
            {
                outPath.startNewSubPath (x, area.getBottom());
                outPath.lineTo (x, y);
                pathStarted = true;
            }
            else
            {
                outPath.lineTo (x, y);
            }
        }

        if (pathStarted)
        {
            outPath.lineTo (area.getRight(), area.getBottom());
            outPath.closeSubPath();
        }
        return pathStarted;
    };

    if (complementOn)
    {
        // === DUAL SPECTRUM DISPLAY FOR COMPLEMENT MODE ===
        // WET spectrum in gold, DRY spectrum in cyan at 50% opacity
        std::array<float, ColorVocoderAudioProcessor::fftSize / 2> wetSpectrum, drySpectrum;
        processor.getDualSpectrumData (wetSpectrum, drySpectrum);

        juce::Path wetPath, dryPath;
        float wetMaxMag = 0.0f, dryMaxMag = 0.0f;
        bool wetValid = buildSpectrumPath (wetSpectrum, wetPath, wetMaxMag);
        bool dryValid = buildSpectrumPath (drySpectrum, dryPath, dryMaxMag);

        // Draw DRY spectrum first (behind) at 50% opacity - cyan
        if (dryValid)
        {
            juce::ColourGradient dryGradient (
                juce::Colour (0xFF64D2FF).withAlpha (alpha * 0.35f), 0, area.getY(),
                juce::Colour (0xFF0066FF).withAlpha (alpha * 0.075f), 0, area.getBottom(), false);
            g.setGradientFill (dryGradient);
            g.fillPath (dryPath);

            g.setColour (juce::Colour (0xFF64D2FF).withAlpha (alpha * 0.3f));
            g.strokePath (dryPath, juce::PathStrokeType (1.0f));
        }

        // Draw WET spectrum on top - gold
        if (wetValid)
        {
            juce::ColourGradient wetGradient (
                juce::Colour (0xFFFFD700).withAlpha (alpha * 0.7f), 0, area.getY(),
                juce::Colour (0xFFFF8800).withAlpha (alpha * 0.15f), 0, area.getBottom(), false);
            g.setGradientFill (wetGradient);
            g.fillPath (wetPath);

            g.setColour (juce::Colour (0xFFFFD700).withAlpha (alpha * 0.6f));
            g.strokePath (wetPath, juce::PathStrokeType (1.5f));
        }
    }
    else
    {
        // === NORMAL MODE - Single spectrum ===
        std::array<float, ColorVocoderAudioProcessor::fftSize / 2> spectrum;
        processor.getSpectrumData (spectrum);

        juce::Path spectrumPath;
        float maxMag = 0.0f;
        bool pathValid = buildSpectrumPath (spectrum, spectrumPath, maxMag);

        if (pathValid)
        {
            juce::ColourGradient specGradient (
                juce::Colour (0xFF00DDFF).withAlpha (alpha * 0.7f), 0, area.getY(),
                juce::Colour (0xFF0066FF).withAlpha (alpha * 0.15f), 0, area.getBottom(), false);
            g.setGradientFill (specGradient);
            g.fillPath (spectrumPath);

            g.setColour (juce::Colour (0xFF00FFFF).withAlpha (alpha * 0.6f));
            g.strokePath (spectrumPath, juce::PathStrokeType (1.5f));
        }
    }

    // Peak labels with note names (using combined spectrum for complement mode)
    std::array<float, ColorVocoderAudioProcessor::fftSize / 2> peakSpectrum;
    processor.getSpectrumData (peakSpectrum);

    struct PeakInfo { float freq; float x; float y; float mag; };
    std::vector<PeakInfo> peaks;

    float prevMag = 0.0f;
    float prevPrevMag = 0.0f;

    for (int i = 1; i < numBins; ++i)
    {
        float freq = (float) i * sampleRate / (float) ColorVocoderAudioProcessor::fftSize;
        if (freq < minFreq || freq > maxFreq)
            continue;

        float magnitude = peakSpectrum[(size_t) i];
        float boosted = std::pow (magnitude, 0.5f) * 1.2f;
        boosted = juce::jlimit (0.0f, 1.0f, boosted);

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

    if (!peaks.empty())
    {
        std::sort (peaks.begin(), peaks.end(),
            [](const PeakInfo& a, const PeakInfo& b) { return a.mag > b.mag; });

        std::vector<PeakInfo> displayPeaks;
        const float minLabelSpacing = 28.0f;

        for (const auto& peak : peaks)
        {
            if (displayPeaks.size() >= 6)
                break;

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

        g.setFont (9.0f);

        for (const auto& peak : displayPeaks)
        {
            juce::String noteName = freqToNoteName (peak.freq);

            float labelY = peak.y - 12.0f;
            if (labelY < area.getY() + 2)
                labelY = area.getY() + 2;

            float textWidth = 22.0f;
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRoundedRectangle (peak.x - textWidth / 2, labelY - 1, textWidth, 11, 3.0f);

            g.setColour (juce::Colour (0xFFFFFF00).withAlpha (alpha * 0.95f));
            g.drawText (noteName, (int) (peak.x - textWidth / 2), (int) labelY,
                       (int) textWidth, 10, juce::Justification::centred);
        }
    }
}

void InputFilterDisplay::drawComplementZone (juce::Graphics& g, juce::Rectangle<float> area, float alpha) const
{
    float hpFreq = hpFreqParam ? hpFreqParam->convertFrom0to1 (hpFreqParam->getValue()) : 20.0f;
    float lpFreq = lpFreqParam ? lpFreqParam->convertFrom0to1 (lpFreqParam->getValue()) : 20000.0f;

    bool hpActive = hpFreq > 25.0f;
    (void) lpFreq;

    if (!hpActive)
        return;

    juce::Colour complementColor = juce::Colour (0xFF00FF88);

    float leftX = area.getX();
    float rightX = freqToX (hpFreq, area);

    juce::ColourGradient gradient (
        complementColor.withAlpha (alpha * 0.25f), leftX, area.getY(),
        complementColor.withAlpha (alpha * 0.05f), rightX, area.getY(), false);

    g.setGradientFill (gradient);
    g.fillRect (leftX, area.getY(), rightX - leftX, area.getHeight());

    g.setColour (complementColor.withAlpha (alpha * 0.4f));
    g.drawVerticalLine ((int) rightX, area.getY(), area.getBottom());

    if (rightX - leftX > 40)
    {
        g.setFont (8.0f);
        g.setColour (complementColor.withAlpha (alpha * 0.7f));
        g.drawText ("CLEAN", (int) (leftX + 4), (int) (area.getBottom() - 14), 36, 10, juce::Justification::left);
    }
}

void InputFilterDisplay::drawNode (juce::Graphics& g, float x, float y, juce::Colour, bool active,
                                   const juce::String& freqText, const juce::String& slopeText, bool,
                                   juce::Rectangle<float> plotArea) const
{
    juce::Colour nodeColour = juce::Colour (0xFFE8F4FC);
    const float radius = active ? 7.7f : 6.0f;  // 23% smaller
    const float strokeWidth = 2.0f;

    float clampedY = juce::jlimit (plotArea.getY() + radius + 5, plotArea.getBottom() - radius - 5, y);

    if (active)
    {
        for (int i = 3; i > 0; --i)
        {
            float r = radius + i * 3.0f;
            g.setColour (nodeColour.withAlpha (0.15f / (float) i));
            g.fillEllipse (x - r, clampedY - r, r * 2, r * 2);
        }
    }

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

    float labelY = clampedY + radius + 6;
    if (labelY + 24 > plotArea.getBottom() - 10)
        labelY = clampedY - radius - 28;

    g.setColour (nodeColour);
    g.setFont (11.0f);
    g.drawText (freqText, (int) x - 30, (int) labelY, 60, 13, juce::Justification::centred);
    g.setFont (9.0f);
    g.setColour (nodeColour.withAlpha (0.7f));
    g.drawText (slopeText, (int) x - 30, (int) labelY + 12, 60, 11, juce::Justification::centred);
}

void InputFilterDisplay::drawBellNode (juce::Graphics& g, float x, float y, juce::Colour, bool active,
                                       const juce::String& freqText, const juce::String& gainText,
                                       juce::Rectangle<float> plotArea, const juce::String& label) const
{
    juce::Colour nodeColour = juce::Colour (0xFFE8F4FC);
    const float radius = active ? 7.7f : 6.0f;  // 23% smaller
    const float strokeWidth = 2.0f;

    float clampedY = juce::jlimit (plotArea.getY() + radius + 5, plotArea.getBottom() - radius - 5, y);

    if (active)
    {
        for (int i = 3; i > 0; --i)
        {
            float r = radius + i * 3.0f;
            g.setColour (nodeColour.withAlpha (0.15f / (float) i));
            g.fillEllipse (x - r, clampedY - r, r * 2, r * 2);
        }
    }

    if (active)
    {
        g.setColour (nodeColour);
        g.fillEllipse (x - radius, clampedY - radius, radius * 2, radius * 2);
        g.setColour (juce::Colour (0xFF1A1A2E));
        g.setFont (8.0f);  // Smaller font for smaller node
        g.drawText (label, (int)(x - radius), (int)(clampedY - radius),
                   (int)(radius * 2), (int)(radius * 2), juce::Justification::centred);
    }
    else
    {
        g.setColour (nodeColour.withAlpha (0.9f));
        g.drawEllipse (x - radius, clampedY - radius, radius * 2, radius * 2, strokeWidth);
        g.setFont (7.0f);  // Smaller font for smaller node
        g.drawText (label, (int)(x - radius - 1), (int)(clampedY - radius),
                   (int)(radius * 2 + 2), (int)(radius * 2), juce::Justification::centred);
    }

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

juce::Path InputFilterDisplay::createResponsePath (juce::Rectangle<float> area, float hpFreq, float lpFreq,
                                                   int hpSlope, int lpSlope, float, float) const
{
    juce::Path path;
    const float zeroDbY = area.getCentreY();
    const float dbRange = 36.0f;  // ±18dB so all points can reach top
    const float pixelsPerDb = area.getHeight() * 0.45f / (dbRange / 2.0f);

    hpFreq = juce::jlimit (20.0f, 20000.0f, hpFreq);
    lpFreq = juce::jlimit (20.0f, 20000.0f, lpFreq);

    const int hpOrder = getFilterOrder (hpSlope);
    const int lpOrder = getFilterOrder (lpSlope);

    float hpBoostDb = (hpQParam != nullptr) ? hpQParam->convertFrom0to1 (hpQParam->getValue()) : 0.0f;
    float lpBoostDb = (lpQParam != nullptr) ? lpQParam->convertFrom0to1 (lpQParam->getValue()) : 0.0f;
    float hpQWidth = (hpQWidthParam != nullptr) ? hpQWidthParam->convertFrom0to1 (hpQWidthParam->getValue()) : 1.0f;
    float lpQWidth = (lpQWidthParam != nullptr) ? lpQWidthParam->convertFrom0to1 (lpQWidthParam->getValue()) : 1.0f;

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

        if (hpFreq > 21.0f)
        {
            float ratio = hpFreq / freq;
            float magnitude = 1.0f / std::sqrt (1.0f + std::pow (ratio, 2.0f * hpOrder));
            dbTotal += 20.0f * std::log10 (std::max (magnitude, 0.00001f));

            if (std::abs (hpBoostDb) > 0.1f)
            {
                float octavesFromCutoff = std::log2 (freq / hpFreq);
                float peakWidth = 0.5f / juce::jlimit (0.5f, 10.0f, hpQWidth);
                float peakShape = std::exp (-octavesFromCutoff * octavesFromCutoff / (2.0f * peakWidth * peakWidth));
                dbTotal += hpBoostDb * peakShape;
            }
        }

        if (lpFreq < 19500.0f)
        {
            float ratio = freq / lpFreq;
            float magnitude = 1.0f / std::sqrt (1.0f + std::pow (ratio, 2.0f * lpOrder));
            dbTotal += 20.0f * std::log10 (std::max (magnitude, 0.00001f));

            if (std::abs (lpBoostDb) > 0.1f)
            {
                float octavesFromCutoff = std::log2 (freq / lpFreq);
                float peakWidth = 0.5f / juce::jlimit (0.5f, 10.0f, lpQWidth);
                float peakShape = std::exp (-octavesFromCutoff * octavesFromCutoff / (2.0f * peakWidth * peakWidth));
                dbTotal += lpBoostDb * peakShape;
            }
        }

        if (bell1On && std::abs (b1Gain) > 0.1f)
        {
            float w = freq / b1Freq;
            float dw = w - 1.0f / w;
            float bellShape = 1.0f / (1.0f + b1Q * b1Q * dw * dw);
            dbTotal += b1Gain * bellShape;
        }

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

juce::Path InputFilterDisplay::createComplementCurve (juce::Rectangle<float> area, float hpFreq, float lpFreq,
                                                      int hpSlope, int lpSlope,
                                                      float hpGainDb, float lpGainDb, float hpQWidth, float lpQWidth) const
{
    juce::Path path;
    const float zeroDbY = area.getCentreY();
    const float dbRange = 36.0f;  // ±18dB so all points can reach top
    const float pixelsPerDb = area.getHeight() * 0.45f / (dbRange / 2.0f);

    hpFreq = juce::jlimit (20.0f, 20000.0f, hpFreq);
    lpFreq = juce::jlimit (20.0f, 20000.0f, lpFreq);

    int hpOrder = getFilterOrder (hpSlope);
    int lpOrder = getFilterOrder (lpSlope);

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

        if (hpFreq > 21.0f)
        {
            float ratio = hpFreq / freq;
            float magnitude = 1.0f / std::sqrt (1.0f + std::pow (ratio, 2.0f * hpOrder));
            dbTotal += 20.0f * std::log10 (std::max (magnitude, 0.00001f));

            if (std::abs (hpGainDb) > 0.1f)
            {
                float w = freq / hpFreq;
                float dw = w - 1.0f / w;
                float resonanceShape = 1.0f / (1.0f + hpQWidth * hpQWidth * dw * dw);
                dbTotal += hpGainDb * resonanceShape;
            }
        }

        if (lpFreq < 19500.0f)
        {
            float ratio = freq / lpFreq;
            float magnitude = 1.0f / std::sqrt (1.0f + std::pow (ratio, 2.0f * lpOrder));
            dbTotal += 20.0f * std::log10 (std::max (magnitude, 0.00001f));

            if (std::abs (lpGainDb) > 0.1f)
            {
                float w = freq / lpFreq;
                float dw = w - 1.0f / w;
                float resonanceShape = 1.0f / (1.0f + lpQWidth * lpQWidth * dw * dw);
                dbTotal += lpGainDb * resonanceShape;
            }
        }

        if (cb1On && std::abs (cb1Gain) > 0.1f)
        {
            float w = freq / cb1Freq;
            float dw = w - 1.0f / w;
            float bellShape = 1.0f / (1.0f + cb1Q * cb1Q * dw * dw);
            dbTotal += cb1Gain * bellShape;
        }

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

juce::Path InputFilterDisplay::createDryInverseCurve (juce::Rectangle<float> area, float hpFreq, float lpFreq,
                                                       int hpSlope, int lpSlope,
                                                       float hpGainDb, float lpGainDb, float hpQWidth, float lpQWidth) const
{
    // DRY curve: Same HP/LP filters, but INVERTED bell gains
    // When WET gets +6dB, DRY gets -6dB (and vice versa)
    juce::Path path;
    const float zeroDbY = area.getCentreY();
    const float dbRange = 36.0f;  // ±18dB so all points can reach top
    const float pixelsPerDb = area.getHeight() * 0.45f / (dbRange / 2.0f);

    hpFreq = juce::jlimit (20.0f, 20000.0f, hpFreq);
    lpFreq = juce::jlimit (20.0f, 20000.0f, lpFreq);

    int hpOrder = getFilterOrder (hpSlope);
    int lpOrder = getFilterOrder (lpSlope);

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

        // HP/LP filters apply the same to DRY
        if (hpFreq > 21.0f)
        {
            float ratio = hpFreq / freq;
            float magnitude = 1.0f / std::sqrt (1.0f + std::pow (ratio, 2.0f * hpOrder));
            dbTotal += 20.0f * std::log10 (std::max (magnitude, 0.00001f));

            if (std::abs (hpGainDb) > 0.1f)
            {
                float w = freq / hpFreq;
                float dw = w - 1.0f / w;
                float resonanceShape = 1.0f / (1.0f + hpQWidth * hpQWidth * dw * dw);
                dbTotal += hpGainDb * resonanceShape;
            }
        }

        if (lpFreq < 19500.0f)
        {
            float ratio = freq / lpFreq;
            float magnitude = 1.0f / std::sqrt (1.0f + std::pow (ratio, 2.0f * lpOrder));
            dbTotal += 20.0f * std::log10 (std::max (magnitude, 0.00001f));

            if (std::abs (lpGainDb) > 0.1f)
            {
                float w = freq / lpFreq;
                float dw = w - 1.0f / w;
                float resonanceShape = 1.0f / (1.0f + lpQWidth * lpQWidth * dw * dw);
                dbTotal += lpGainDb * resonanceShape;
            }
        }

        // INVERTED bell gains for DRY signal
        if (cb1On && std::abs (cb1Gain) > 0.1f)
        {
            float w = freq / cb1Freq;
            float dw = w - 1.0f / w;
            float bellShape = 1.0f / (1.0f + cb1Q * cb1Q * dw * dw);
            dbTotal += (-cb1Gain) * bellShape;  // INVERTED: negative of wet gain
        }

        if (cb2On && std::abs (cb2Gain) > 0.1f)
        {
            float w = freq / cb2Freq;
            float dw = w - 1.0f / w;
            float bellShape = 1.0f / (1.0f + cb2Q * cb2Q * dw * dw);
            dbTotal += (-cb2Gain) * bellShape;  // INVERTED: negative of wet gain
        }

        float y = zeroDbY - dbTotal * pixelsPerDb;

        if (!started) { path.startNewSubPath (x, y); started = true; }
        else path.lineTo (x, y);
    }
    return path;
}

float InputFilterDisplay::freqToX (float freq, juce::Rectangle<float> area) const
{
    float norm = (std::log10 (juce::jlimit (20.0f, 20000.0f, freq)) - std::log10 (20.0f))
               / (std::log10 (20000.0f) - std::log10 (20.0f));
    return area.getX() + norm * area.getWidth();
}

float InputFilterDisplay::xToFreq (float x, juce::Rectangle<float> area) const
{
    float norm = juce::jlimit (0.0f, 1.0f, (x - area.getX()) / area.getWidth());
    return 20.0f * std::pow (1000.0f, norm);
}

int InputFilterDisplay::getFilterOrder (int slopeIdx) const
{
    const int orders[] = { 1, 2, 4, 8, 12, 16 };
    return orders[juce::jlimit (0, 5, slopeIdx)];
}

juce::String InputFilterDisplay::getSlopeLabel (int idx) const
{
    const char* s[] = {"6dB","12dB","24dB","48dB","72dB","96dB"};
    return s[juce::jlimit(0,5,idx)];
}

juce::String InputFilterDisplay::formatFreq (float f) const
{
    return f >= 1000 ? juce::String(f/1000,1)+"k" : juce::String((int)f)+"Hz";
}

juce::String InputFilterDisplay::formatGain (float g) const
{
    return (g >= 0 ? "+" : "") + juce::String(g, 1) + "dB";
}

juce::String InputFilterDisplay::freqToNoteName (float freq)
{
    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    float midiNote = 12.0f * std::log2 (freq / 440.0f) + 69.0f;
    int noteNum = (int) std::round (midiNote);

    if (noteNum < 0 || noteNum > 127)
        return "";

    int octave = (noteNum / 12) - 1;
    int noteIndex = noteNum % 12;

    return juce::String (noteNames[noteIndex]) + juce::String (octave);
}

//==============================================================================
// ScaleChordBox Implementation
//==============================================================================

//==============================================================================
// MicrotuningPanel Implementation
//==============================================================================

MicrotuningPanel::MicrotuningPanel (juce::AudioProcessorValueTreeState& apvts)
    : apvtsRef (apvts)
{
    // Make panel always on top and opaque
    setAlwaysOnTop (true);
    setOpaque (true);

    // Reference Pitch slider - Serum-style draggable number
    refPitchSlider.setSliderStyle (juce::Slider::LinearBarVertical);
    refPitchSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    refPitchSlider.setRange (400.0, 480.0, 0.1);
    refPitchSlider.setNumDecimalPlacesToDisplay (1);
    refPitchSlider.setTextValueSuffix (" Hz");
    refPitchSlider.setColour (juce::Slider::trackColourId, juce::Colour (0xFF3A3A45));
    refPitchSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xFFE0E0E0));
    refPitchSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xFF252530));
    refPitchSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xFF3A3A45));
    addAndMakeVisible (refPitchSlider);

    refPitchLabel.setFont (juce::Font (12.0f));
    refPitchLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFB0B0B0));
    addAndMakeVisible (refPitchLabel);

    // 12 cent offset sliders
    for (int i = 0; i < 12; ++i)
    {
        auto& slider = centSliders[i];
        slider.setSliderStyle (juce::Slider::LinearVertical);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setRange (-100.0, 100.0, 1.0);
        slider.setDoubleClickReturnValue (true, 0.0);
        slider.setColour (juce::Slider::trackColourId, juce::Colour (0xFF3A3A45));
        slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xFF64D2FF));
        addAndMakeVisible (slider);

        noteLabels[i].setText (noteNames[i], juce::dontSendNotification);
        noteLabels[i].setFont (juce::Font (10.0f));
        noteLabels[i].setColour (juce::Label::textColourId, juce::Colour (0xFFB0B0B0));
        noteLabels[i].setJustificationType (juce::Justification::centred);
        addAndMakeVisible (noteLabels[i]);
    }

    // Load button
    loadButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF3A3A45));
    loadButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFE0E0E0));
    loadButton.onClick = [this]() { loadTuningFile(); };
    addAndMakeVisible (loadButton);

    // Reset button
    resetButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF3A3A45));
    resetButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFE0E0E0));
    resetButton.onClick = [this]() {
        refPitchSlider.setValue (440.0, juce::sendNotificationSync);
        for (auto& slider : centSliders)
            slider.setValue (0.0, juce::sendNotificationSync);
    };
    addAndMakeVisible (resetButton);
}

void MicrotuningPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark background with rounded corners
    g.setColour (juce::Colour (0xF0151520));
    g.fillRoundedRectangle (bounds, 8.0f);

    // Border
    g.setColour (juce::Colour (0xFF3A3A45));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

    // Title
    g.setColour (juce::Colour (0xFFE0E0E0));
    g.setFont (juce::Font (14.0f, juce::Font::bold));
    g.drawText ("Microtuning", bounds.removeFromTop (24).reduced (8, 4), juce::Justification::centredLeft);

    // Cent offset label
    auto centArea = bounds.reduced (8, 0);
    centArea.removeFromTop (50);  // Skip reference pitch area
    g.setFont (juce::Font (10.0f));
    g.setColour (juce::Colour (0xFF808080));
    g.drawText ("Cents  \u00B1100", centArea.removeFromTop (12), juce::Justification::centred);
}

void MicrotuningPanel::resized()
{
    auto bounds = getLocalBounds().reduced (8);
    bounds.removeFromTop (24);  // Title

    // Reference pitch row
    auto refRow = bounds.removeFromTop (50);
    refPitchLabel.setBounds (refRow.removeFromLeft (35).reduced (0, 12));
    refPitchSlider.setBounds (refRow.removeFromLeft (70).reduced (0, 4));

    // Spacing
    bounds.removeFromTop (20);

    // 12 cent sliders in a row
    auto sliderRow = bounds.removeFromTop (80);
    int sliderW = sliderRow.getWidth() / 12;
    for (int i = 0; i < 12; ++i)
    {
        auto col = sliderRow.removeFromLeft (sliderW);
        noteLabels[i].setBounds (col.removeFromBottom (14));
        centSliders[i].setBounds (col.reduced (2, 2));
    }

    bounds.removeFromTop (8);

    // Buttons row
    auto buttonRow = bounds.removeFromTop (24);
    loadButton.setBounds (buttonRow.removeFromLeft (100).reduced (2, 0));
    buttonRow.removeFromLeft (8);
    resetButton.setBounds (buttonRow.removeFromLeft (60).reduced (2, 0));
}

void MicrotuningPanel::updateTrialState()
{
    bool allowed = isLoadingAllowed();
    if (allowed)
    {
        loadButton.setButtonText ("Load .scl/.tun");
        loadButton.setTooltip ("");
        loadButton.setEnabled (true);
    }
    else
    {
        loadButton.setButtonText (juce::CharPointer_UTF8 ("\xf0\x9f\x94\x92 Load .scl/.tun"));  // 🔒
        loadButton.setTooltip ("Available with full license");
        loadButton.setEnabled (false);
    }
}

void MicrotuningPanel::loadTuningFile()
{
    // Check if loading is allowed (blocked in trial mode)
    if (!isLoadingAllowed())
        return;

    auto chooser = std::make_shared<juce::FileChooser> (
        "Select a tuning file...",
        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
        "*.scl;*.tun");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser] (const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                if (file.hasFileExtension (".scl"))
                    parseScalaFile (file);
                else if (file.hasFileExtension (".tun"))
                    parseTunFile (file);
            }
        });
}

bool MicrotuningPanel::parseScalaFile (const juce::File& file)
{
    // Parse Scala .scl format
    // Format: lines starting with ! are comments
    // First non-comment line is description
    // Second is number of notes
    // Following lines are cents or ratios

    juce::StringArray lines;
    file.readLines (lines);

    std::vector<double> cents;
    bool foundDescription = false;
    bool foundCount = false;
    int expectedCount = 0;

    for (const auto& line : lines)
    {
        auto trimmed = line.trim();
        if (trimmed.isEmpty() || trimmed.startsWithChar ('!'))
            continue;

        if (!foundDescription)
        {
            foundDescription = true;
            continue;
        }

        if (!foundCount)
        {
            expectedCount = trimmed.getIntValue();
            foundCount = true;
            continue;
        }

        // Parse cents or ratio
        double c = 0.0;
        if (trimmed.containsChar ('/'))
        {
            // It's a ratio like "3/2"
            auto parts = juce::StringArray::fromTokens (trimmed, "/", "");
            if (parts.size() == 2)
            {
                double num = parts[0].getDoubleValue();
                double den = parts[1].getDoubleValue();
                if (den > 0.0)
                    c = 1200.0 * std::log2 (num / den);
            }
        }
        else
        {
            // It's cents
            c = trimmed.getDoubleValue();
        }

        cents.push_back (c);
        if ((int) cents.size() >= expectedCount)
            break;
    }

    // Convert to per-note cent offsets
    // Standard Scala: 12-note scales, where interval[i] is from 1/1 to note i+1
    if (cents.size() >= 11)
    {
        // First note (C) stays at 0, each subsequent note gets deviation from equal temperament
        for (int i = 0; i < 12; ++i)
        {
            double equalTempCents = i * 100.0;
            double scalaCents = (i == 0) ? 0.0 : cents[i - 1];

            // Handle octave (the last interval should be 1200)
            if (i == 0 && cents.size() >= 12)
                scalaCents = 0.0;

            double offset = scalaCents - equalTempCents;
            // Wrap to -100..+100 range
            while (offset > 100.0) offset -= 100.0;
            while (offset < -100.0) offset += 100.0;

            centSliders[i].setValue (offset, juce::sendNotificationSync);
        }
        return true;
    }

    return false;
}

bool MicrotuningPanel::parseTunFile (const juce::File& file)
{
    // Parse .tun format (MIDI Tuning Standard)
    // Format varies, but commonly has [Tuning] section with note=cents

    juce::StringArray lines;
    file.readLines (lines);

    bool inTuningSection = false;
    std::array<double, 128> noteTunings;
    noteTunings.fill (0.0);

    for (const auto& line : lines)
    {
        auto trimmed = line.trim();

        if (trimmed.startsWithIgnoreCase ("[Tuning]"))
        {
            inTuningSection = true;
            continue;
        }

        if (trimmed.startsWithChar ('['))
        {
            inTuningSection = false;
            continue;
        }

        if (inTuningSection && trimmed.contains ("="))
        {
            auto parts = juce::StringArray::fromTokens (trimmed, "=", "");
            if (parts.size() == 2)
            {
                int noteNum = parts[0].trim().getIntValue();
                double cents = parts[1].trim().getDoubleValue();
                if (noteNum >= 0 && noteNum < 128)
                    noteTunings[noteNum] = cents;
            }
        }
    }

    // Extract octave 4 (middle octave) as the reference
    // MIDI note 60 = C4
    for (int i = 0; i < 12; ++i)
    {
        int midiNote = 60 + i;
        double cents = noteTunings[midiNote];
        double equalTemp = midiNote * 100.0;
        double offset = cents - equalTemp;

        // Wrap to -100..+100 range
        while (offset > 100.0) offset -= 100.0;
        while (offset < -100.0) offset += 100.0;

        centSliders[i].setValue (offset, juce::sendNotificationSync);
    }

    return true;
}

//==============================================================================
// ScaleChordBox Implementation
//==============================================================================

ScaleChordBox::ScaleChordBox()
{
    addAndMakeVisible (keyBox);
    addAndMakeVisible (scaleBox);
    addAndMakeVisible (chordBox);
    addAndMakeVisible (varsBox);

    // Fill bars (visible) - drag to adjust amount
    addAndMakeVisible (scaleAmntBar);
    addAndMakeVisible (chordAmntBar);

    // Knobs (hidden) - used for parameter attachment
    addChildComponent (scaleAmntKnob);  // hidden
    addChildComponent (chordAmntKnob);  // hidden

    keyBox.setWantsKeyboardFocus (false);
    scaleBox.setWantsKeyboardFocus (false);
    chordBox.setWantsKeyboardFocus (false);
    varsBox.setWantsKeyboardFocus (false);

    scaleAmntKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    scaleAmntKnob.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    chordAmntKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    chordAmntKnob.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);

    // Connect fill bars to hidden knobs
    scaleAmntBar.onValueChanged = [this](float v) {
        scaleAmntKnob.setValue (v, juce::sendNotificationSync);
    };
    chordAmntBar.onValueChanged = [this](float v) {
        chordAmntKnob.setValue (v, juce::sendNotificationSync);
    };

    // Sync fill bars when knobs change (from parameter load)
    scaleAmntKnob.onValueChange = [this]() {
        scaleAmntBar.setValue ((float) scaleAmntKnob.getValue());
    };
    chordAmntKnob.onValueChange = [this]() {
        chordAmntBar.setValue ((float) chordAmntKnob.getValue());
    };

    startTimerHz (15);  // 15fps polling for MIDI chord trigger sync
}

void ScaleChordBox::resized()
{
    auto bounds = getLocalBounds().reduced (1);
    const int rowH = 20;  // Compact row height
    const int barWidth = 12;   // Vertical fill bar width
    const int barMargin = 3;
    const int rowGap = 2;
    const int boxGap = 2;  // Gap between dropdowns

    auto row1 = bounds.removeFromTop (rowH);
    bounds.removeFromTop (rowGap);
    auto row2 = bounds.removeFromTop (rowH);

    // Row 1: Key dropdown, Scale dropdown, scale fill bar
    auto barArea1 = row1.removeFromRight (barWidth + barMargin);
    scaleAmntBar.setBounds (barArea1.getX() + barMargin / 2, row1.getY(), barWidth, rowH);
    int halfW = (row1.getWidth() - boxGap) / 2;  // Equal width for all dropdowns
    keyBox.setBounds (row1.getX(), row1.getY(), halfW, rowH);
    scaleBox.setBounds (row1.getX() + halfW + boxGap, row1.getY(), halfW, rowH);

    // Row 2: MIDI toggle, chord dropdown, vars dropdown, chord fill bar
    auto barArea2 = row2.removeFromRight (barWidth + barMargin);
    chordAmntBar.setBounds (barArea2.getX() + barMargin / 2, row2.getY(), barWidth, rowH);

    // Reserve space for MIDI chord toggle on the left of row 2
    const int toggleW = 14;
    const int toggleGap = 2;
    midiChordToggleBounds = juce::Rectangle<float> (
        (float) row2.getX(), (float) row2.getY() + 3.0f,
        (float) toggleW, (float) rowH - 6.0f);

    int row2ContentX = row2.getX() + toggleW + toggleGap;
    int row2ContentW = row2.getWidth() - toggleW - toggleGap;
    int chordHalfW = (row2ContentW - boxGap) / 2;

    chordBox.setBounds (row2ContentX, row2.getY(), chordHalfW, rowH);
    varsBox.setBounds (row2ContentX + chordHalfW + boxGap, row2.getY(), chordHalfW, rowH);

    // Hidden knobs (off-screen, just for parameter attachment)
    scaleAmntKnob.setBounds (-100, -100, 1, 1);
    chordAmntKnob.setBounds (-100, -100, 1, 1);
}

void ScaleChordBox::paint (juce::Graphics& g)
{
    // Draw MIDI chord trigger toggle
    if (midiChordToggleBounds.isEmpty())
        return;

    bool active = getMidiChordState ? getMidiChordState() : false;

    // Toggle background
    g.setColour (active ? juce::Colour (0xFFFF8C00) : juce::Colour (0xFF3A3A45));
    g.fillRoundedRectangle (midiChordToggleBounds, 2.0f);

    // "M" label
    g.setColour (active ? juce::Colours::black : juce::Colour (0xFF888888));
    g.setFont (9.0f);
    g.drawText ("M", midiChordToggleBounds, juce::Justification::centred);
}

void ScaleChordBox::mouseDown (const juce::MouseEvent& e)
{
    if (midiChordToggleBounds.contains (e.position))
    {
        if (toggleMidiChordState)
            toggleMidiChordState();
        repaint();
    }
}

void ScaleChordBox::timerCallback()
{
    // Sync chordBox display when MIDI Chord Trigger changes the degree
    if (getCurrentChordDegree)
    {
        int degree = getCurrentChordDegree();
        if (degree != lastSyncedDegree)
        {
            lastSyncedDegree = degree;
            chordBox.setSelectedItemIndex (degree, juce::dontSendNotification);
        }
    }

    // Repaint toggle state
    repaint (midiChordToggleBounds.toNearestInt());
}

//==============================================================================
// SlopeClickBox Implementation
//==============================================================================

SlopeClickBox::SlopeClickBox()
{
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
}

void SlopeClickBox::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background
    g.setColour (juce::Colour (0xFF252530));
    g.fillRoundedRectangle (bounds, 3.0f);

    // Border
    g.setColour (juce::Colour (0xFF3A3A45));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);

    // Text
    g.setColour (juce::Colour (0xFFE0E0E0));
    g.setFont (10.0f);
    g.drawText (slopeLabels[slopeIndex], bounds, juce::Justification::centred);
}

void SlopeClickBox::mouseDown (const juce::MouseEvent&)
{
    slopeIndex = (slopeIndex + 1) % slopeLabels.size();
    repaint();

    if (onSlopeChanged)
        onSlopeChanged (slopeIndex);
}

void SlopeClickBox::setSlopeIndex (int idx)
{
    slopeIndex = juce::jlimit (0, slopeLabels.size() - 1, idx);
    repaint();
}

//==============================================================================
// Main Editor Implementation
//==============================================================================

ColorVocoderAudioProcessorEditor::ColorVocoderAudioProcessorEditor (ColorVocoderAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      presetManager (p.apvts),
      bandDisplay (p),
      bandDistEditor (p),
      bandOffsetEditor (p),
      perBandQCarousel (p)
{
    // ===== SCALED CONTENT WRAPPER =====
    // All UI components are added to this wrapper, which is scaled with AffineTransform
    // This gives us Serum-style proportional scaling when the window is resized
    scaledContent = std::make_unique<juce::Component>();
    scaledContent->setBounds (0, 0, NATIVE_WIDTH, NATIVE_HEIGHT);
    addAndMakeVisible (scaledContent.get());

    // ===== PRESET UI =====
    scaledContent->addAndMakeVisible (presetButton);
    presetButton.setWantsKeyboardFocus (false);
    presetButton.setLookAndFeel (&thinPresetLAF);
    presetButton.getProperties().set ("isPresetButton", true);
    presetButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2A35));
    presetButton.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xFF4A4A55));
    presetButton.onClick = [this]() { showPresetMenu(); };
    updatePresetButtonText();

    scaledContent->addAndMakeVisible (savePresetButton);
    savePresetButton.setWantsKeyboardFocus (false);
    savePresetButton.setLookAndFeel (&thinPresetLAF);
    savePresetButton.getProperties().set ("isSaveButton", true);
    savePresetButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2A35));
    savePresetButton.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xFF4A4A55));
    savePresetButton.onClick = [this]() { savePresetWithDialog(); };

    // ===== VISUALIZERS =====
    scaledContent->addAndMakeVisible (bandDisplay);
    scaledContent->addChildComponent (bandDistEditor);  // Hidden by default, shown when BAND DIST clicked

    // Wire up distribution editor to processor
    bandDistEditor.onDistributionChanged = [this] (const std::vector<float>& curve) {
        processor.setBandDistribution (curve);
    };

    // Load current curve from processor (persists across editor open/close)
    const auto& savedCurve = processor.getBandDistribution();
    if (!savedCurve.empty())
        bandDistEditor.setDistributionCurve (savedCurve);

    inputFilterDisplay = std::make_unique<InputFilterDisplay> (processor.apvts, processor);
    scaledContent->addAndMakeVisible (*inputFilterDisplay);

    // Piano keyboard for pitch class exclusion (below EQ display)
    pianoKeyboard = std::make_unique<PianoKeyboard> (processor);
    scaledContent->addAndMakeVisible (*pianoKeyboard);

    // ===== CUSTOM CHORD EDITOR (hidden by default) =====
    customChordEditor = std::make_unique<CustomChordEditor> (processor);
    scaledContent->addChildComponent (*customChordEditor);  // Hidden initially

    // EDIT button in title bar
    scaledContent->addAndMakeVisible (editButton);
    editButton.setWantsKeyboardFocus (false);
    editButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2A35));
    editButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    editButton.onClick = [this]() { toggleEditMode(); };

    // ===== LEFT PANEL =====
    scaledContent->addAndMakeVisible (bypassButton);
    bypassButton.setButtonText ("Bypass");
    bypassButton.setWantsKeyboardFocus (false);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "bypass", bypassButton);

    scaledContent->addAndMakeVisible (carrierModeBox);
    carrierModeBox.addItemList (juce::StringArray { "Self", "External", "Noise", "Saw", "Square", "Pitch Track" }, 1);
    carrierModeBox.setWantsKeyboardFocus (false);
    carrierModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, "carrierMode", carrierModeBox);
    scaledContent->addAndMakeVisible (carrierModeLabel);

    // ===== ALGORITHM MODE SECTION =====
    scaledContent->addAndMakeVisible (algorithmModeBox);
    algorithmModeBox.addItemList (juce::StringArray { "Ritual", "Linked", "Split" }, 1);
    algorithmModeBox.setWantsKeyboardFocus (false);
    algorithmModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, "algorithmMode", algorithmModeBox);
    algorithmModeBox.onChange = [this]() { updateQKnobVisibility(); };
    scaledContent->addAndMakeVisible (algorithmModeLabel);
    algorithmModeLabel.setFont (juce::Font (9.0f));
    algorithmModeLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));

    // Carrier Q knob (shown in all modes - controls carrier/global Q)
    setupKnob (carrierQKnob, carrierQLabel, smallKnobLAF);
    carrierQKnob.setRange (1.0, 60.0, 0.1);
    carrierQAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "bandWidth", carrierQKnob);  // Links to bandWidth which controls Q
    // Note: bandWidth 0.01-2.0 maps to Q 200-0.5 (inverse, Ableton-calibrated)
    carrierQKnob.setRange (0.01, 2.0, 0.01);

    // Modulator Q knob (only visible in SPLIT mode)
    setupKnob (modulatorQKnob, modulatorQLabel, smallKnobLAF);
    modulatorQKnob.setRange (1.0, 60.0, 0.1);
    modulatorQAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "modulatorQ", modulatorQKnob);
    modulatorQLabel.setFont (juce::Font (9.0f));


    // Initialize Q knob visibility based on current algorithm mode
    updateQKnobVisibility();

    setupKnob (mixSlider, mixLabel, bigKnobLAF);  // Big knob filmstrip for MIX
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "mix", mixSlider);

    // Mod Level vertical bar (next to Mix)
    scaledContent->addAndMakeVisible (modLevelBar);
    scaledContent->addAndMakeVisible (modLevelLabel);
    modLevelLabel.setJustificationType (juce::Justification::centred);
    modLevelLabel.setFont (juce::Font (9.0f));
    modLevelLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.8f));

    // Hidden slider for parameter attachment
    modLevelSlider.setRange (0.0, 2.0, 0.01);
    modLevelSlider.setValue (1.0);
    modLevelSlider.setSliderStyle (juce::Slider::LinearVertical);
    modLevelSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    modLevelSlider.setVisible (false);

    modLevelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "modLevel", modLevelSlider);

    // Sync bar with slider
    modLevelBar.setValue ((float) modLevelSlider.getValue() / 2.0f);  // 0-2 -> 0-1
    modLevelBar.onValueChanged = [this](float barValue) {
        modLevelSlider.setValue (barValue * 2.0);  // 0-1 -> 0-2
    };
    modLevelSlider.onValueChange = [this]() {
        modLevelBar.setValue ((float) modLevelSlider.getValue() / 2.0f);
    };

    scaledContent->addAndMakeVisible (scaleChordBox);

    // ===== MICROTUNING BUTTON (panel added at end for z-order) =====
    tuningButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF252530));
    tuningButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFB0B0B0));
    tuningButton.onClick = [this]() { toggleMicrotuningPanel(); };
    scaledContent->addAndMakeVisible (tuningButton);

    // ===== LICENSE BUTTON (in title bar) =====
    // Only show if not licensed - once licensed, button is hidden
    auto& lm = processor.getLicenseManager();
    if (!lm.isLicensed())
    {
        if (lm.isTrialExpired())
            licenseButton.setButtonText ("EXPIRED");
        else if (lm.isInTrialMode())
            licenseButton.setButtonText ("Trial: " + juce::String (lm.getTrialDaysRemaining()) + "d");
        else
            licenseButton.setButtonText ("LICENSE");

        licenseButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF3A2A1A));
        licenseButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFFFD700));
        licenseButton.onClick = [this]() { toggleLicensePanel(); };
        scaledContent->addAndMakeVisible (licenseButton);
    }

    // ===== ABOUT BUTTON (clean circular "i" icon) =====
    aboutButton.onClick = [this]() { toggleAboutPanel(); };
    scaledContent->addAndMakeVisible (aboutButton);

    // ===== FEEDBACK BUTTON ("+" in top bar) =====
    feedbackButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0x00000000));
    feedbackButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFF888888));
    feedbackButton.onClick = [this]() {
        showFeedbackMessage = !showFeedbackMessage;
        repaint();
    };
    scaledContent->addAndMakeVisible (feedbackButton);

    // ===== INPUT / OUTPUT GAIN KNOBS =====
    setupKnob (inputGainKnob, inputGainLabel, smallKnobLAF);
    inputGainLabel.setText ("IN", juce::dontSendNotification);
    inputGainLabel.setFont (8.0f);
    inputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "inputGain", inputGainKnob);

    setupKnob (outputGainKnob, outputGainLabel, smallKnobLAF);
    outputGainLabel.setText ("OUT", juce::dontSendNotification);
    outputGainLabel.setFont (8.0f);
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, "outputGain", outputGainKnob);

    // Level meters (self-repainting at 30fps)
    inputMeter.getLevel = [this]() { return processor.getInputRmsLevel(); };
    outputMeter.getLevel = [this]() { return processor.getOutputRmsLevel(); };
    scaledContent->addAndMakeVisible (inputMeter);
    scaledContent->addAndMakeVisible (outputMeter);

    // Scale/Chord attachments
    // keyBox uses StarComboBox with manual parameter sync (not ComboBoxAttachment)
    scaleChordBox.keyBox.setItems (juce::StringArray { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" });
    if (auto* keyParam = processor.apvts.getRawParameterValue ("key"))
        scaleChordBox.keyBox.setSelectedItemIndex ((int) keyParam->load(), juce::dontSendNotification);
    scaleChordBox.keyBox.onChange = [this]()
    {
        if (auto* param = processor.apvts.getParameter ("key"))
            param->setValueNotifyingHost ((float) scaleChordBox.keyBox.getSelectedItemIndex() / 11.0f);
    };

    // Scale box items - show lock icons for trial-restricted scales
    {
        auto& lm = processor.getLicenseManager();
        bool isTrial = !lm.isLicensed();
        juce::String lock = juce::CharPointer_UTF8 ("\xf0\x9f\x94\x92 ");  // 🔒

        scaleChordBox.scaleBox.addItemList (juce::StringArray {
            "Major",                                           // 0 - Ionian (allowed)
            "Minor",                                           // 1 - Aeolian (allowed)
            isTrial ? lock + "Dorian" : "Dorian",              // 2
            isTrial ? lock + "Phrygian" : "Phrygian",          // 3
            isTrial ? lock + "Lydian" : "Lydian",              // 4
            isTrial ? lock + "Mixolydian" : "Mixolydian",      // 5
            isTrial ? lock + "Locrian" : "Locrian",            // 6
            "Major Penta",                                     // 7 (allowed)
            "Minor Penta",                                     // 8 (allowed)
            isTrial ? lock + "Blues" : "Blues",                // 9
            isTrial ? lock + "Harm Minor" : "Harm Minor",      // 10
            isTrial ? lock + "Mel Minor" : "Mel Minor",        // 11
            isTrial ? lock + "Whole Tone" : "Whole Tone",      // 12
            isTrial ? lock + "Diminished" : "Diminished",      // 13
            isTrial ? lock + "Chromatic" : "Chromatic"         // 14
        }, 1);

        // Set tooltip for locked scales
        if (isTrial)
            scaleChordBox.scaleBox.setTooltip ("Some scales require full license");
    }
    scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, "scaleType", scaleChordBox.scaleBox);

    // Trial mode: restrict to Major (0), Minor (1), Major Penta (7), Minor Penta (8)
    scaleChordBox.scaleBox.onChange = [this]()
    {
        auto& lm = processor.getLicenseManager();
        if (!lm.isLicensed())
        {
            int idx = scaleChordBox.scaleBox.getSelectedItemIndex();
            // Allowed in trial: 0=Major, 1=Minor, 7=Major Penta, 8=Minor Penta
            if (idx != 0 && idx != 1 && idx != 7 && idx != 8)
            {
                // Revert to Major
                scaleChordBox.scaleBox.setSelectedItemIndex (0, juce::sendNotificationAsync);
            }
        }
    };

    // chordBox is now StarComboBox - use setItems() and manual parameter sync
    scaleChordBox.chordBox.setItems (juce::StringArray { "I", "ii", "iii", "IV", "V", "vi", "vii" });
    if (auto* chordParam = processor.apvts.getRawParameterValue ("chordDegree"))
        scaleChordBox.chordBox.setSelectedItemIndex ((int) chordParam->load(), juce::dontSendNotification);
    scaleChordBox.chordBox.onChange = [this]()
    {
        if (auto* param = processor.apvts.getParameter ("chordDegree"))
            param->setValueNotifyingHost ((float) scaleChordBox.chordBox.getSelectedItemIndex() / 6.0f);
    };
    // Note: chordDegreeAttachment is no longer used (StarComboBox handles it manually)

    scaleChordBox.varsBox.addItemList (juce::StringArray { "Triad", "Power", "7th", "9th", "sus2", "sus4", "add9", "6th", "dim", "Custom" }, 1);
    chordVariationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, "chordVariation", scaleChordBox.varsBox);

    scaleChordBox.scaleAmntKnob.setLookAndFeel (&smallKnobLAF);
    scaleAmtAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "scaleAmount", scaleChordBox.scaleAmntKnob);

    scaleChordBox.chordAmntKnob.setLookAndFeel (&smallKnobLAF);
    chordAmountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "chordAmount", scaleChordBox.chordAmntKnob);

    // MIDI Chord Trigger callbacks
    scaleChordBox.getMidiChordState = [this]() -> bool {
        if (auto* p = processor.apvts.getRawParameterValue ("midiChordTrigger"))
            return p->load() > 0.5f;
        return false;
    };
    scaleChordBox.toggleMidiChordState = [this]() {
        if (auto* param = processor.apvts.getParameter ("midiChordTrigger"))
        {
            bool current = param->getValue() > 0.5f;
            param->setValueNotifyingHost (current ? 0.0f : 1.0f);

            // When turning M off, reset chord amount to 0%
            if (current)
            {
                if (auto* chordParam = processor.apvts.getParameter ("chordAmount"))
                    chordParam->setValueNotifyingHost (0.0f);
            }
        }
    };
    scaleChordBox.getCurrentChordDegree = [this]() -> int {
        if (auto* p = processor.apvts.getRawParameterValue ("chordDegree"))
            return (int) p->load();
        return 0;
    };

    // ===== CENTER PANEL - Vocoder Knobs (SMALL) =====
    // Band Count - draggable number (1-100)
    bandCountSlider.setSliderStyle (juce::Slider::LinearBarVertical);
    bandCountSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 50, 20);
    bandCountSlider.setRange (1.0, 100.0, 1.0);
    bandCountSlider.setNumDecimalPlacesToDisplay (0);
    bandCountSlider.setTextValueSuffix ("");
    bandCountSlider.setColour (juce::Slider::trackColourId, juce::Colour (0xFF3A3A45));
    bandCountSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colour (0xFFE0E0E0));
    bandCountSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xFF252530));
    bandCountSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0xFF3A3A45));
    scaledContent->addAndMakeVisible (bandCountSlider);
    scaledContent->addAndMakeVisible (bandCountLabel);
    bandCountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "bandCount", bandCountSlider);

    // Stacked Mode button - short "STACK" text, styled as clickable rectangle
    additiveModeToggle.setButtonText ("STACK");
    additiveModeToggle.setTooltip ("Stack: bands at same frequency stack (fuller) vs deduplicate (consistent)");
    additiveModeToggle.setLookAndFeel (&noTickToggleLAF);  // No tick box, text only
    additiveModeToggle.onClick = [this]() {
        bool isOn = additiveModeToggle.getToggleState();
        additiveModeToggle.setColour (juce::ToggleButton::textColourId,
            isOn ? juce::Colour (0xFF00DDFF) : juce::Colour (0xFF888888));

        if (isOn && preCapBandCountValue > 0)
        {
            // STACK ON: restore pre-cap value
            bandCountSlider.setValue ((double) preCapBandCountValue, juce::sendNotificationSync);
            preCapBandCountValue = -1;
        }
        else if (! isOn)
        {
            // STACK OFF: store current value before capping happens
            if (preCapBandCountValue < 0)
                preCapBandCountValue = (int) bandCountSlider.getValue();
        }

        repaint();  // Update background rectangle color
    };
    scaledContent->addAndMakeVisible (additiveModeToggle);
    additiveModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "additiveMode", additiveModeToggle);
    // Initialize color based on current parameter state
    {
        bool isOn = processor.apvts.getRawParameterValue ("additiveMode")->load() > 0.5f;
        additiveModeToggle.setColour (juce::ToggleButton::textColourId,
            isOn ? juce::Colour (0xFF00DDFF) : juce::Colour (0xFF888888));
    }

    // Adaptive Envelope toggle (OFF = zappy Ableton-style, ON = per-band adaptive)
    adaptiveEnvToggle.setButtonText ("ADAPT");
    adaptiveEnvToggle.setTooltip ("Adaptive Envelope: OFF = flat timing (zappy), ON = per-band adaptive (smoother)");
    adaptiveEnvToggle.setLookAndFeel (&noTickToggleLAF);
    adaptiveEnvToggle.onClick = [this]() {
        bool isOn = adaptiveEnvToggle.getToggleState();
        adaptiveEnvToggle.setColour (juce::ToggleButton::textColourId,
            isOn ? juce::Colour (0xFF00DDFF) : juce::Colour (0xFF888888));
        repaint();
    };
    scaledContent->addAndMakeVisible (adaptiveEnvToggle);
    adaptiveEnvAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, "adaptiveEnv", adaptiveEnvToggle);
    {
        bool isOn = processor.apvts.getRawParameterValue ("adaptiveEnv")->load() > 0.5f;
        adaptiveEnvToggle.setColour (juce::ToggleButton::textColourId,
            isOn ? juce::Colour (0xFF00DDFF) : juce::Colour (0xFF888888));
    }

    // Band Distribution button
    bandDistButton.setButtonText ("DIST");
    bandDistButton.setTooltip ("Band Distribution - edit frequency spacing");
    bandDistButton.setLookAndFeel (&twoLineButtonLAF);
    bandDistButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFF888888));
    bandDistButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xFF00DDFF));
    bandDistButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    bandDistButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    bandDistButton.onClick = [this]() {
        if (currentBandView == BandViewMode::Distribution)
            currentBandView = BandViewMode::Normal;
        else
            currentBandView = BandViewMode::Distribution;
        updateBandViewVisibility();
    };
    bandDistButton.setClickingTogglesState (true);
    scaledContent->addAndMakeVisible (bandDistButton);

    // Band Offset button
    bandOffsetButton.setButtonText ("OFFSET");
    bandOffsetButton.setTooltip ("Band Offset - edit modulator-to-carrier band mapping");
    bandOffsetButton.setLookAndFeel (&twoLineButtonLAF);
    bandOffsetButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFF888888));
    bandOffsetButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xFF00DDFF));
    bandOffsetButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    bandOffsetButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    bandOffsetButton.onClick = [this]() {
        if (currentBandView == BandViewMode::Offset)
            currentBandView = BandViewMode::Normal;
        else
            currentBandView = BandViewMode::Offset;
        updateBandViewVisibility();
    };
    bandOffsetButton.setClickingTogglesState (true);
    scaledContent->addAndMakeVisible (bandOffsetButton);

    // Per-Band Q button
    perBandQButton.setButtonText ("BANDS");
    perBandQButton.setTooltip ("Per-Band Q - view and edit individual band Q values");
    perBandQButton.setLookAndFeel (&twoLineButtonLAF);
    perBandQButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFF888888));
    perBandQButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xFF00DDFF));
    perBandQButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    perBandQButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    perBandQButton.onClick = [this]() {
        if (currentBandView == BandViewMode::PerBandQ)
            currentBandView = BandViewMode::Normal;
        else
            currentBandView = BandViewMode::PerBandQ;
        updateBandViewVisibility();
    };
    perBandQButton.setClickingTogglesState (true);
    scaledContent->addAndMakeVisible (perBandQButton);

    // Initialize band view visibility — use addChildComponent so they start hidden
    // Only bandDisplay (Normal view) should be visible on startup
    scaledContent->addChildComponent (bandOffsetEditor);
    scaledContent->addChildComponent (perBandQCarousel);

    // Wire up offset curve callback
    bandOffsetEditor.onOffsetChanged = [this] (const std::vector<float>& curve) {
        processor.setBandOffsetCurve (curve);
    };

    setupKnob (bandWidthSlider, bandWidthLabel, smallKnobLAF);
    bandWidthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "bandWidth", bandWidthSlider);

    setupKnob (attackSlider, attackLabel, smallKnobLAF);
    attackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "attackMs", attackSlider);

    setupKnob (releaseSlider, releaseLabel, smallKnobLAF);
    releaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "releaseMs", releaseSlider);

    setupKnob (gateSlider, gateLabel, smallKnobLAF);
    gateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "gate", gateSlider);

    setupKnob (depthSlider, depthLabel, smallKnobLAF);
    depthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "depth", depthSlider);

    // ===== RIGHT PANEL (all small knobs for compact layout) =====
    setupKnob (formantSlider, formantLabel, smallKnobLAF);
    formantAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "formant", formantSlider);

    setupKnob (harmShiftSlider, harmShiftLabel, smallKnobLAF);
    harmShiftAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "harmonicShift", harmShiftSlider);

    setupKnob (unvoicedSlider, unvoicedLabel, smallKnobLAF);
    unvoicedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "unvoiced", unvoicedSlider);

    // Noise Mode selector (Auto/Vocal/Synth)
    scaledContent->addAndMakeVisible (noiseModeCombo);
    noiseModeCombo.addItemList (juce::StringArray { "Auto", "Voc", "Syn" }, 1);
    noiseModeCombo.setWantsKeyboardFocus (false);
    noiseModeCombo.setJustificationType (juce::Justification::centred);
    noiseModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, "noiseMode", noiseModeCombo);

    setupKnob (hpBlendSlider, hpBlendLabel, smallKnobLAF);
    hpBlendAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "hpBlend", hpBlendSlider);

    scaledContent->addAndMakeVisible (hpBlendSlopeBox);
    hpBlendSlopeBox.onSlopeChanged = [this](int slopeIdx) {
        auto* param = processor.apvts.getParameter ("hpBlendSlope");
        if (param != nullptr)
            param->setValueNotifyingHost (param->convertTo0to1 ((float) slopeIdx));
    };

    setupKnob (noiseColorSlider, noiseColorLabel, smallKnobLAF);
    noiseColorAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "noiseColor", noiseColorSlider);

    // ===== SPECTRAL X/Y PAD (disabled - not impactful enough) =====
    // spectralPad = std::make_unique<SpectralXYPad> (processor.apvts);
    // scaledContent->addAndMakeVisible (*spectralPad);

    // ===== BOTTOM PANEL - Tone Sculpt =====
    scaledContent->addAndMakeVisible (toneSculptLabel);
    toneSculptLabel.setFont (juce::Font (14.0f, juce::Font::bold));
    toneSculptLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFE0E0E0));

    scaledContent->addAndMakeVisible (complementToggle);
    complementToggle.setWantsKeyboardFocus (false);
    complementAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "filterComplement", complementToggle);

    setupKnob (freqKnob, freqLabel, smallKnobLAF);
    setupKnob (gainKnob, gainLabel, smallKnobLAF);
    setupKnob (qKnob, qLabel, smallKnobLAF);

    // Helper lambdas to get param IDs for selected band
    auto getFreqParamId = [](int selected) -> const char* {
        switch (selected) {
            case 1: return "inputHPFreq";
            case 2: return "inputLPFreq";
            case 3: return "bell1Freq";
            case 4: return "bell2Freq";
            case 5: return "compBell1Freq";
            case 6: return "compBell2Freq";
            default: return nullptr;
        }
    };

    auto getGainParamId = [](int selected) -> const char* {
        switch (selected) {
            case 1: return "inputHPQ";      // HP uses Q param for resonance boost
            case 2: return "inputLPQ";      // LP uses Q param for resonance boost
            case 3: return "bell1Gain";
            case 4: return "bell2Gain";
            case 5: return "compBell1Gain";
            case 6: return "compBell2Gain";
            default: return nullptr;
        }
    };

    auto getQParamId = [](int selected) -> const char* {
        switch (selected) {
            case 1: return "inputHPQWidth";
            case 2: return "inputLPQWidth";
            case 3: return "bell1Q";
            case 4: return "bell2Q";
            case 5: return "compBell1Q";
            case 6: return "compBell2Q";
            default: return nullptr;
        }
    };

    // When a band is selected, update all three knobs
    inputFilterDisplay->onBandSelected = [this, getFreqParamId, getGainParamId, getQParamId](int selected)
    {
        // Update freq knob
        if (auto* freqParam = processor.apvts.getParameter (getFreqParamId (selected)))
        {
            float freqValue = freqParam->convertFrom0to1 (freqParam->getValue());
            freqKnob.setValue (freqValue, juce::dontSendNotification);
        }

        // Update gain knob
        if (auto* gainParam = processor.apvts.getParameter (getGainParamId (selected)))
        {
            float gainValue = gainParam->convertFrom0to1 (gainParam->getValue());
            gainKnob.setValue (gainValue, juce::dontSendNotification);
        }

        // Update Q knob
        if (auto* qParam = processor.apvts.getParameter (getQParamId (selected)))
        {
            float qValue = qParam->convertFrom0to1 (qParam->getValue());
            qKnob.setValue (qValue, juce::dontSendNotification);
        }
    };

    // Default frequencies for each band type (for double-click reset)
    auto getDefaultFreq = [](int selected) -> double {
        switch (selected) {
            case 1: return 180.0;    // HP: 180 Hz
            case 2: return 20000.0;  // LP: 20 kHz
            case 3: return 500.0;    // Bell 1: 500 Hz
            case 4: return 3500.0;   // Bell 2: 3.5 kHz (peak hearing sensitivity)
            case 5: return 500.0;    // Comp Bell 1: 500 Hz
            case 6: return 3500.0;   // Comp Bell 2: 3.5 kHz
            default: return 1000.0;
        }
    };

    // Freq knob setup
    freqKnob.setRange (20.0, 20000.0, 1.0);
    freqKnob.setSkewFactorFromMidPoint (1000.0);
    freqKnob.setDoubleClickReturnValue (false, 1000.0);  // Disabled - we handle it manually
    freqKnob.onValueChange = [this, getFreqParamId]()
    {
        float value = (float) freqKnob.getValue();
        int selected = inputFilterDisplay ? inputFilterDisplay->getSelectedBand() : 1;
        if (auto* param = processor.apvts.getParameter (getFreqParamId (selected)))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    // Custom double-click handler for freq knob (per-band defaults)
    freqKnobDoubleClickHandler = std::make_unique<FreqKnobDoubleClickHandler> (freqKnob);
    freqKnobDoubleClickHandler->getDefaultValue = [this, getDefaultFreq]() {
        int selected = inputFilterDisplay ? inputFilterDisplay->getSelectedBand() : 1;
        return getDefaultFreq (selected);
    };
    freqKnob.addMouseListener (freqKnobDoubleClickHandler.get(), false);

    // Gain knob setup
    gainKnob.setRange (-18.0, 18.0, 0.1);
    gainKnob.setDoubleClickReturnValue (true, 0.0);  // Double-click resets to 0dB
    gainKnob.onValueChange = [this, getGainParamId]()
    {
        float value = (float) gainKnob.getValue();
        int selected = inputFilterDisplay ? inputFilterDisplay->getSelectedBand() : 1;
        if (auto* param = processor.apvts.getParameter (getGainParamId (selected)))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    // Q knob setup
    qKnob.setRange (0.5, 10.0, 0.01);
    qKnob.setSkewFactor (0.5);
    qKnob.setDoubleClickReturnValue (true, 1.25);  // Double-click resets to 1.25 Q
    qKnob.onValueChange = [this, getQParamId]()
    {
        float value = (float) qKnob.getValue();
        int selected = inputFilterDisplay ? inputFilterDisplay->getSelectedBand() : 1;
        if (auto* param = processor.apvts.getParameter (getQParamId (selected)))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    // ===== HIDDEN SLIDERS (for low/high freq controlled by band display) =====
    lowFreqSlider.setRange (20, 2000, 1);
    lowFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "lowFreq", lowFreqSlider);

    highFreqSlider.setRange (1000, 20000, 1);
    highFreqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "highFreq", highFreqSlider);

    // Connect band display dragging to frequency sliders
    bandDisplay.onLowFreqChanged = [this](float freq) {
        lowFreqSlider.setValue (freq, juce::sendNotificationSync);
    };
    bandDisplay.onHighFreqChanged = [this](float freq) {
        highFreqSlider.setValue (freq, juce::sendNotificationSync);
    };
    bandDisplay.onUniqueBandCountChanged = [this](int count) {
        // Show unique band count - arrow indicates "results in X"
        // Compact format fits the narrow label area
        bandCountLabel.setText (juce::String::charToString (0x2192) + juce::String (count), juce::dontSendNotification);
    };

    bandDisplay.onMaxUsefulBandsChanged = [this](int maxUseful) {
        // Dynamically limit slider range when STACK is OFF and scale limits useful bands
        // This prevents users from increasing bands above what the scale provides
        bandCountSlider.setRange (1.0, (double) maxUseful, 1.0);

        // Also cap current value if it exceeds new max
        int currentValue = (int) bandCountSlider.getValue();
        if (currentValue > maxUseful)
        {
            // Store pre-cap value only if not already stored
            if (preCapBandCountValue < 0)
                preCapBandCountValue = currentValue;
            bandCountSlider.setValue ((double) maxUseful, juce::sendNotificationSync);
        }
    };

    // ===== AUTO-MAX BAND COUNT ON SCALE/CHORD CHANGE =====
    // When scale, key, chord, or additive mode changes, auto-set bandCount to max useful value
    autoMaxBandListener.editor = this;
    processor.apvts.addParameterListener ("scaleType", &autoMaxBandListener);
    processor.apvts.addParameterListener ("key", &autoMaxBandListener);
    processor.apvts.addParameterListener ("scaleAmount", &autoMaxBandListener);
    processor.apvts.addParameterListener ("chordDegree", &autoMaxBandListener);
    processor.apvts.addParameterListener ("chordVariation", &autoMaxBandListener);
    processor.apvts.addParameterListener ("chordAmount", &autoMaxBandListener);
    processor.apvts.addParameterListener ("additiveMode", &autoMaxBandListener);

    carrierPitchSlider.setRange (50, 2000, 1);
    carrierPitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "carrierPitch", carrierPitchSlider);

    // ===== REGISTER KNOBS FOR POPUP DISPLAY =====
    registerKnobForPopup (mixSlider, "Mix", ValueFormat::Percent);
    // bandCountCombo is a dropdown, not a knob - no popup needed
    registerKnobForPopup (bandWidthSlider, "Width", ValueFormat::Number);
    registerKnobForPopup (attackSlider, "Attack", ValueFormat::Ms);
    registerKnobForPopup (releaseSlider, "Release", ValueFormat::Ms);
    registerKnobForPopup (gateSlider, "Gate", ValueFormat::Int);  // 0-100 (already percentage)
    registerKnobForPopup (depthSlider, "Depth", ValueFormat::Percent);
    registerKnobForPopup (formantSlider, "Formant", ValueFormat::Semitones);
    registerKnobForPopup (harmShiftSlider, "Shift", ValueFormat::Semitones);
    registerKnobForPopup (unvoicedSlider, "Noise", ValueFormat::Percent);
    registerKnobForPopup (hpBlendSlider, "HP Blend", ValueFormat::Percent);
    registerKnobForPopup (noiseColorSlider, "Color", ValueFormat::NoiseColor);
    registerKnobForPopup (freqKnob, "Freq", ValueFormat::Frequency);
    registerKnobForPopup (gainKnob, "Gain", ValueFormat::Db);
    registerKnobForPopup (qKnob, "Q", ValueFormat::Number, 2);

    // ===== REGISTER KNOBS FOR RIGHT-CLICK CONTEXT MENU =====
    // Each knob gets: Reset to Default, Save as Default, Enter Value, Copy, Paste, MIDI Learn, Lock
    registerKnobContextMenu (mixSlider, "mix");
    registerKnobContextMenu (bandCountSlider, "bandCount");
    registerKnobContextMenu (bandWidthSlider, "bandWidth");
    registerKnobContextMenu (attackSlider, "attackMs");
    registerKnobContextMenu (releaseSlider, "releaseMs");
    registerKnobContextMenu (gateSlider, "gate");
    registerKnobContextMenu (depthSlider, "depth");
    registerKnobContextMenu (formantSlider, "formant");
    registerKnobContextMenu (harmShiftSlider, "harmonicShift");
    registerKnobContextMenu (unvoicedSlider, "unvoiced");
    registerKnobContextMenu (hpBlendSlider, "hpBlend");
    registerKnobContextMenu (noiseColorSlider, "noiseColor");
    registerKnobContextMenu (freqKnob, "inputHPFreq");  // Default, changes with selection
    registerKnobContextMenu (gainKnob, "inputHPQ");     // Default, changes with selection
    registerKnobContextMenu (qKnob, "inputHPQWidth");   // Default, changes with selection

    // ===== MICROTUNING PANEL (desktop window popup) =====
    microtuningPanel = std::make_unique<MicrotuningPanel> (processor.apvts);
    // NOT added as child - it's shown as a desktop window when toggled

    // Trial mode: block microtuning file import
    microtuningPanel->isLoadingAllowed = [this]()
    {
        return processor.getLicenseManager().isLicensed();
    };
    microtuningPanel->updateTrialState();

    // Create attachments for microtuning parameters
    microtuningPanel->refPitchAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "referencePitch", microtuningPanel->refPitchSlider);

    for (int i = 0; i < 12; ++i)
    {
        juce::String paramId = "centOffset" + juce::String (i);
        microtuningPanel->centAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, paramId, microtuningPanel->centSliders[i]);
    }

    // ===== WINDOW SETUP =====
    setSize (NATIVE_WIDTH, NATIVE_HEIGHT);  // Start at native size
    setResizable (true, true);
    // Allow resize from 40% to 250% of native size (proportional scaling both ways)
    setResizeLimits (NATIVE_WIDTH * 2 / 5, NATIVE_HEIGHT * 2 / 5,   // Min: 40% (288x192)
                     NATIVE_WIDTH * 5 / 2, NATIVE_HEIGHT * 5 / 2);  // Max: 250% (1800x1202)
    getConstrainer()->setFixedAspectRatio ((double) NATIVE_WIDTH / (double) NATIVE_HEIGHT);

    setWantsKeyboardFocus (true);  // Enable keyboard shortcuts (Cmd+Shift+E for screenshot)
    setMouseClickGrabsKeyboardFocus (true);

    // Layout all components at native resolution (only done once)
    layoutComponents();

    // Set band count to max useful on initial load
    juce::MessageManager::callAsync ([this]() {
        updateBandCountToMax();
    });

    // Show welcome/activation panel if needed (first launch or trial expired)
    showWelcomePanelIfNeeded();
}

ColorVocoderAudioProcessorEditor::~ColorVocoderAudioProcessorEditor()
{
    // Remove auto-max band count listeners
    processor.apvts.removeParameterListener ("scaleType", &autoMaxBandListener);
    processor.apvts.removeParameterListener ("key", &autoMaxBandListener);
    processor.apvts.removeParameterListener ("scaleAmount", &autoMaxBandListener);
    processor.apvts.removeParameterListener ("chordDegree", &autoMaxBandListener);
    processor.apvts.removeParameterListener ("chordVariation", &autoMaxBandListener);
    processor.apvts.removeParameterListener ("chordAmount", &autoMaxBandListener);
    processor.apvts.removeParameterListener ("additiveMode", &autoMaxBandListener);

    // Remove microtuning panel from desktop if showing
    if (microtuningPanel != nullptr && microtuningPanel->isOnDesktop())
        microtuningPanel->removeFromDesktop();

    // Remove look and feel from all sliders
    mixSlider.setLookAndFeel (nullptr);
    // bandCountCombo is a dropdown - no look and feel cleanup needed
    bandWidthSlider.setLookAndFeel (nullptr);
    attackSlider.setLookAndFeel (nullptr);
    releaseSlider.setLookAndFeel (nullptr);
    gateSlider.setLookAndFeel (nullptr);
    depthSlider.setLookAndFeel (nullptr);
    formantSlider.setLookAndFeel (nullptr);
    harmShiftSlider.setLookAndFeel (nullptr);
    unvoicedSlider.setLookAndFeel (nullptr);
    hpBlendSlider.setLookAndFeel (nullptr);
    noiseColorSlider.setLookAndFeel (nullptr);
    freqKnob.setLookAndFeel (nullptr);
    gainKnob.setLookAndFeel (nullptr);
    qKnob.setLookAndFeel (nullptr);
    scaleChordBox.scaleAmntKnob.setLookAndFeel (nullptr);
    scaleChordBox.chordAmntKnob.setLookAndFeel (nullptr);
    additiveModeToggle.setLookAndFeel (nullptr);
    adaptiveEnvToggle.setLookAndFeel (nullptr);
    bandDistButton.setLookAndFeel (nullptr);
    presetButton.setLookAndFeel (nullptr);
    savePresetButton.setLookAndFeel (nullptr);
}

void ColorVocoderAudioProcessorEditor::updateBandCountToMax()
{
    // Calculate max useful bands for current scale/chord configuration
    int maxBands = processor.calculateMaxUsefulBands();

    // Update slider range FIRST to prevent SliderAttachment from clamping
    // the new value to an outdated range maximum
    bandCountSlider.setRange (1.0, (double) maxBands, 1.0);

    // Set bandCount parameter to max useful value
    if (auto* param = processor.apvts.getParameter ("bandCount"))
    {
        // bandCount range is 1-100, so normalize accordingly
        float normalized = (float)(maxBands - 1) / 99.0f;
        normalized = juce::jlimit (0.0f, 1.0f, normalized);
        param->setValueNotifyingHost (normalized);
    }
}

void ColorVocoderAudioProcessorEditor::toggleMicrotuningPanel()
{
    // Close other popups first (only one popup at a time)
    if (licensePanel && licensePanel->isVisible())
    {
        if (licensePanel->isOnDesktop())
            licensePanel->removeFromDesktop();
        licensePanel->setVisible (false);
    }
    if (aboutPanel && aboutPanel->isVisible())
    {
        removeChildComponent (aboutPanel.get());
        aboutPanel->setVisible (false);
        aboutPanel.reset();
    }

    auto* panel = microtuningPanel.get();
    bool isCurrentlyVisible = panel->isVisible() && panel->isOnDesktop();

    if (isCurrentlyVisible)
    {
        // Hide it
        panel->removeFromDesktop();
        panel->setVisible (false);
        tuningButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF252530));
        tuningButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFB0B0B0));
    }
    else
    {
        // Show as floating desktop window
        panel->setSize (320, 220);
        panel->addToDesktop (juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowHasDropShadow);

        // Position near the button
        auto screenPos = tuningButton.localPointToGlobal (juce::Point<int> (0, tuningButton.getHeight() + 4));
        panel->setTopLeftPosition (screenPos.x, screenPos.y);

        panel->setVisible (true);
        panel->toFront (true);

        tuningButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF3A3A50));
        tuningButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFF64D2FF));
    }
}

void ColorVocoderAudioProcessorEditor::toggleLicensePanel()
{
    // Close other popups first (only one popup at a time)
    if (microtuningPanel && microtuningPanel->isVisible())
    {
        if (microtuningPanel->isOnDesktop())
            microtuningPanel->removeFromDesktop();
        microtuningPanel->setVisible (false);
        tuningButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF252530));
        tuningButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFB0B0B0));
    }
    if (aboutPanel && aboutPanel->isVisible())
    {
        removeChildComponent (aboutPanel.get());
        aboutPanel->setVisible (false);
        aboutPanel.reset();
    }

    auto& lm = processor.getLicenseManager();

    // If already licensed, don't show popup - button just stays green
    if (lm.isLicensed())
        return;

    // Create panel if needed
    if (licensePanel == nullptr)
    {
        licensePanel = std::make_unique<LicensePanel> (processor);
        // Set callback to refresh UI when license is activated
        licensePanel->onActivationSuccess = [this]() {
            refreshTrialRestrictedUI();
        };
    }

    auto* panel = licensePanel.get();
    bool isCurrentlyVisible = panel->isVisible() && panel->isOnDesktop();

    if (isCurrentlyVisible)
    {
        // Hide it
        panel->removeFromDesktop();
        panel->setVisible (false);
    }
    else
    {
        // Show as floating desktop window - always on top so it's not hidden
        panel->setSize (320, 160);
        panel->addToDesktop (juce::ComponentPeer::windowIsTemporary
                           | juce::ComponentPeer::windowHasDropShadow
                           | juce::ComponentPeer::windowIsSemiTransparent);
        panel->setAlwaysOnTop (true);

        // Position near the button - account for scaling transform
        auto editorScreenPos = getScreenPosition();
        float scale = (float) getWidth() / (float) NATIVE_WIDTH;
        int btnX = (int) (licenseButton.getX() * scale);
        int btnY = (int) ((licenseButton.getY() + licenseButton.getHeight() + 4) * scale);
        panel->setTopLeftPosition (editorScreenPos.x + btnX, editorScreenPos.y + btnY);

        panel->setVisible (true);
        panel->toFront (true);
    }
}

void ColorVocoderAudioProcessorEditor::toggleAboutPanel()
{
    // Close other popups first (only one popup at a time)
    if (microtuningPanel && microtuningPanel->isVisible())
    {
        if (microtuningPanel->isOnDesktop())
            microtuningPanel->removeFromDesktop();
        microtuningPanel->setVisible (false);
        tuningButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF252530));
        tuningButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFB0B0B0));
    }
    if (licensePanel && licensePanel->isVisible())
    {
        if (licensePanel->isOnDesktop())
            licensePanel->removeFromDesktop();
        licensePanel->setVisible (false);
    }

    // If panel exists and visible, hide it
    if (aboutPanel && aboutPanel->isVisible())
    {
        removeChildComponent (aboutPanel.get());
        aboutPanel->setVisible (false);
        aboutPanel.reset();
        return;
    }

    // Create panel as full-screen overlay (child of editor, not scaledContent)
    aboutPanel = std::make_unique<AboutPanel> (processor);
    aboutPanel->onDismiss = [this]() { toggleAboutPanel(); };

    addAndMakeVisible (aboutPanel.get());

    // Size to fill entire editor
    aboutPanel->setBounds (getLocalBounds());
    aboutPanel->toFront (true);
}

void ColorVocoderAudioProcessorEditor::showWelcomePanelIfNeeded()
{
    auto& lm = processor.getLicenseManager();

    // Show welcome panel if first launch OR trial expired (and not licensed)
    if (!lm.needsActivationPrompt())
        return;

    // Create welcome panel as overlay (child of editor, not scaledContent)
    welcomePanel = std::make_unique<WelcomePanel> (processor);
    welcomePanel->onComplete = [this]() {
        hideWelcomePanel();

        auto& lm = processor.getLicenseManager();

        if (lm.isLicensed())
        {
            // Full license activated - unlock all features (scales, microtuning)
            refreshTrialRestrictedUI();
        }
        else if (lm.isInTrialMode())
        {
            // Trial started - update button text, keep restrictions
            licenseButton.setButtonText ("Trial: " + juce::String (lm.getTrialDaysRemaining()) + "d");
            licenseButton.setVisible (true);
        }

        repaint();
    };

    // Add as child of editor (on top of everything)
    addAndMakeVisible (welcomePanel.get());

    // Size to fill entire editor
    welcomePanel->setBounds (getLocalBounds());
    welcomePanel->toFront (true);
}

void ColorVocoderAudioProcessorEditor::hideWelcomePanel()
{
    if (welcomePanel)
    {
        removeChildComponent (welcomePanel.get());
        welcomePanel.reset();
    }
}

void ColorVocoderAudioProcessorEditor::refreshTrialRestrictedUI()
{
    // Called after license activation to remove lock icons and enable features
    // without requiring plugin reload

    auto& lm = processor.getLicenseManager();
    bool isLicensed = lm.isLicensed();

    // 1. Rebuild scale dropdown without lock icons
    {
        // Remember current selection
        int currentIdx = scaleChordBox.scaleBox.getSelectedItemIndex();

        // Detach parameter attachment temporarily
        scaleAttachment.reset();

        // Clear and rebuild items without locks (since now licensed)
        scaleChordBox.scaleBox.clear (juce::dontSendNotification);
        scaleChordBox.scaleBox.addItemList (juce::StringArray {
            "Major",           // 0 - Ionian
            "Minor",           // 1 - Aeolian
            "Dorian",          // 2
            "Phrygian",        // 3
            "Lydian",          // 4
            "Mixolydian",      // 5
            "Locrian",         // 6
            "Major Penta",     // 7
            "Minor Penta",     // 8
            "Blues",           // 9
            "Harmonic Minor",  // 10
            "Melodic Minor",   // 11
            "Whole Tone",      // 12
            "Diminished",      // 13
            "Chromatic"        // 14
        }, 1);

        // Remove tooltip
        scaleChordBox.scaleBox.setTooltip ("");

        // Restore selection
        scaleChordBox.scaleBox.setSelectedItemIndex (currentIdx, juce::dontSendNotification);

        // Reattach parameter
        scaleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            processor.apvts, "scaleType", scaleChordBox.scaleBox);
    }

    // 2. Update microtuning panel (enable import button)
    if (microtuningPanel)
        microtuningPanel->updateTrialState();

    // 3. Hide license button (user is now licensed)
    if (isLicensed)
        licenseButton.setVisible (false);

    // 4. Refresh display
    repaint();
}

void ColorVocoderAudioProcessorEditor::setupKnob (Slider& slider, juce::Label& label, FilmstripLookAndFeel& laf)
{
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
    slider.setMouseDragSensitivity (150);
    slider.setWantsKeyboardFocus (false);
    slider.setMouseClickGrabsKeyboardFocus (false);
    slider.setLookAndFeel (&laf);

    scaledContent->addAndMakeVisible (slider);

    label.setJustificationType (juce::Justification::centred);
    label.setColour (juce::Label::textColourId, juce::Colour (0xFFE0E0E0));
    label.setFont (11.0f);
    scaledContent->addAndMakeVisible (label);
}

void ColorVocoderAudioProcessorEditor::setupCombo (Combo& box)
{
    box.setJustificationType (juce::Justification::centred);
    box.setWantsKeyboardFocus (false);
    box.setMouseClickGrabsKeyboardFocus (false);
    scaledContent->addAndMakeVisible (box);
}

void ColorVocoderAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Dark background (fills entire editor at current size)
    g.fillAll (juce::Colour (0xFF1A1A1F));

    // Calculate scale factor to match scaledContent transform
    float scale = (float) getWidth() / (float) NATIVE_WIDTH;

    // Apply the same scale transform used by scaledContent
    // This ensures all painting aligns with the scaled child components
    g.addTransform (juce::AffineTransform::scale (scale));

    // Now draw everything in native (unscaled) coordinates

    // Title (positioned in native coordinates - top-left per gui-layout-5)
    g.setColour (juce::Colour (0xFFE0E0E0));
    g.setFont (juce::Font (14.0f, juce::Font::bold));
    g.drawText ("BAND RITUAL", juce::Rectangle<int> (1, 0, 93, 27), juce::Justification::centredLeft);

    // Draw border around vocoder knobs area (like the band display has)
    if (!vocoderKnobsArea.isEmpty())
    {
        auto knobBounds = vocoderKnobsArea.toFloat();
        g.setColour (juce::Colour (0xFF252530));  // Slightly lighter background
        g.fillRoundedRectangle (knobBounds, 6.0f);
        g.setColour (juce::Colour (0xFF3A3A45));  // Border color (same as band display)
        g.drawRoundedRectangle (knobBounds.reduced (0.5f), 6.0f, 1.0f);
    }

    // Draw border around harmonic section (formant + harm shift)
    if (!harmonicKnobsArea.isEmpty())
    {
        auto harmonicBounds = harmonicKnobsArea.toFloat();
        g.setColour (juce::Colour (0xFF252530));  // Same background as vocoder knobs
        g.fillRoundedRectangle (harmonicBounds, 6.0f);
        g.setColour (juce::Colour (0xFF3A3A45));  // Same border color
        g.drawRoundedRectangle (harmonicBounds.reduced (0.5f), 6.0f, 1.0f);
    }

    // Draw border around noise section
    if (!noiseKnobsArea.isEmpty())
    {
        auto noiseBounds = noiseKnobsArea.toFloat();
        g.setColour (juce::Colour (0xFF252530));  // Same background as vocoder knobs
        g.fillRoundedRectangle (noiseBounds, 6.0f);
        g.setColour (juce::Colour (0xFF3A3A45));  // Same border color
        g.drawRoundedRectangle (noiseBounds.reduced (0.5f), 6.0f, 1.0f);
    }

    // Draw rectangle background for STACK toggle (makes it look clickable)
    {
        auto stackBounds = additiveModeToggle.getBounds().toFloat();
        bool isOn = additiveModeToggle.getToggleState();

        // Background - darker when off, subtle highlight when on
        g.setColour (isOn ? juce::Colour (0xFF2A3A40) : juce::Colour (0xFF252530));
        g.fillRoundedRectangle (stackBounds, 3.0f);

        // Border
        g.setColour (isOn ? juce::Colour (0xFF00AACC) : juce::Colour (0xFF3A3A45));
        g.drawRoundedRectangle (stackBounds.reduced (0.5f), 3.0f, 1.0f);
    }

    // Draw rectangle backgrounds for view toggle buttons
    auto drawViewButton = [&g] (juce::Rectangle<int> bounds, bool isOn) {
        auto boundsF = bounds.toFloat();
        g.setColour (isOn ? juce::Colour (0xFF2A3A40) : juce::Colour (0xFF252530));
        g.fillRoundedRectangle (boundsF, 3.0f);
        g.setColour (isOn ? juce::Colour (0xFF00AACC) : juce::Colour (0xFF3A3A45));
        g.drawRoundedRectangle (boundsF.reduced (0.5f), 3.0f, 1.0f);
    };

    drawViewButton (bandDistButton.getBounds(), currentBandView == BandViewMode::Distribution);
    drawViewButton (bandOffsetButton.getBounds(), currentBandView == BandViewMode::Offset);
    drawViewButton (perBandQButton.getBounds(), currentBandView == BandViewMode::PerBandQ);

    // Level meters are now self-repainting LevelMeterBar components (inputMeter, outputMeter)

    // Feedback message overlay (shown when "+" button toggled)
    if (showFeedbackMessage && currentBandView == BandViewMode::Normal)
    {
        juce::Rectangle<int> vizArea (188, 29, 423, 157);
        g.setColour (juce::Colour (0xDD1A1A1F));
        g.fillRect (vizArea);
        g.setColour (juce::Colour (0xFFAAAAAA));
        g.setFont (juce::Font (13.0f));
        g.drawText ("What else? Share ideas \xe2\x86\x92 www.zyxyzyx.com",
                     vizArea, juce::Justification::centred);
    }
}

void ColorVocoderAudioProcessorEditor::resized()
{
    // Calculate scale factor based on current window size vs native design size
    float scale = (float) getWidth() / (float) NATIVE_WIDTH;

    // Apply proportional transform to the content wrapper
    // This scales ALL child components uniformly - Serum-style scaling
    scaledContent->setTransform (juce::AffineTransform::scale (scale));

    // Keep scaledContent at native size - the transform handles visual scaling
    scaledContent->setBounds (0, 0, NATIVE_WIDTH, NATIVE_HEIGHT);

    // Welcome panel overlay (if visible) - fills entire editor
    if (welcomePanel)
        welcomePanel->setBounds (getLocalBounds());

    // About panel overlay (if visible) - fills entire editor
    if (aboutPanel)
        aboutPanel->setBounds (getLocalBounds());

    // Layout is only done once (in constructor via layoutComponents)
    // No need to recalculate positions on resize - the transform handles it
}

void ColorVocoderAudioProcessorEditor::layoutComponents()
{
    // =========================================================================
    // FIXED PIXEL LAYOUT at NATIVE resolution (720 x 481)
    // Based on GUI-Frame-Tool exported layout.json
    // This is called once from the constructor. The resized() method handles
    // scaling via AffineTransform for proportional GUI scaling.
    // =========================================================================

    // ===================== TITLE BAR (y=0, h=29) =====================
    // Layout per gui-layout-4: [BAND RITUAL] [LICENSE/i] [DIST|OFFSET|BANDS] [+] [SAVE|PRESET] [empty]

    // Bypass button removed per user request - hide it
    bypassButton.setBounds (0, 0, 0, 0);
    bypassButton.setVisible (false);

    // License button (30% shorter, centered vertically)
    if (licenseButton.isVisible())
        licenseButton.setBounds (94, 4, 50, 19);

    // About button (circular "i" icon) - 30% shorter, centered vertically
    aboutButton.setBounds (162, 4, 19, 19);

    // EDIT button for custom chord editor
    editButton.setBounds (310, 3, 22, 22);

    // Preset controls (per gui-layout-5)
    savePresetButton.setBounds (336, 3, 22, 22);  // Square floppy icon
    presetButton.setBounds (360, 3, 176, 22);     // Sleeker height

    // ===================== MAIN LAYOUT =====================
    // Based on exported layout.json coordinates (720x481)
    const int labelH = 12;

    // Column definitions per gui-layout-5 (722x485)
    const int leftPanelX = 0;
    const int leftPanelW = 186;
    const int centerPanelX = 187;
    const int centerPanelW = 427;
    const int rightPanelX = 613;
    const int rightPanelW = 110;

    // ==================== LEFT PANEL (x:0, w:186) ====================
    // Per gui-layout-5 frame positions

    // Row 1 (y:27): ALGORITHM dropdown + Q knobs
    algorithmModeLabel.setVisible (false);  // Hide label per frame instructions
    algorithmModeBox.setBounds (7, 28, 113, 18);

    // Q knobs to the right of algorithm
    const int qKnobSize = 26;
    carrierQKnob.setBounds (125, 27, qKnobSize, qKnobSize);
    carrierQLabel.setBounds (125, 27 + qKnobSize - 6, qKnobSize, 10);
    modulatorQKnob.setBounds (151, 27, qKnobSize, qKnobSize);
    modulatorQLabel.setBounds (151, 27 + qKnobSize - 6, qKnobSize + 6, 10);


    // Row 2 (y:47): MODE dropdown - NO LABEL, just dropdown
    carrierModeLabel.setVisible (false);  // REMOVE THE WHITE TEXT MODE
    carrierModeBox.setBounds (7, 48, 113, 18);

    // MODE AND MIX section (y:69, h:101)
    const int mixSectionY = 69;
    const int mixKnobSize = 70;
    const int modBarW = 14;
    const int modBarH = 55;
    int mixX = leftPanelX + (leftPanelW - mixKnobSize - 6 - modBarW) / 2;

    mixSlider.setBounds (mixX, mixSectionY + 4, mixKnobSize, mixKnobSize);
    mixLabel.setBounds (mixX, mixSectionY + 4 + mixKnobSize, mixKnobSize, labelH);

    // Mod Level bar to the right of Mix knob
    int modBarX = mixX + mixKnobSize + 6;
    int modBarY = mixSectionY + 4 + (mixKnobSize - modBarH) / 2;
    modLevelBar.setBounds (modBarX, modBarY, modBarW, modBarH);
    modLevelLabel.setBounds (modBarX - 4, modBarY + modBarH + 2, modBarW + 8, labelH);

    // Section 2: KEY SCALE CHORD (y:171, h:50) per gui-layout-5
    const int scaleChordY = 171;
    const int scaleChordH = 50;
    scaleChordBox.setBounds (1, scaleChordY, 186, scaleChordH);

    // TUNING button (y:221) per gui-layout-5 — slightly thinner and shorter
    tuningButton.setBounds (3, 221, 182, 20);

    // Microtuning panel is a desktop window, positioned dynamically when shown

    // ==================== CENTER PANEL (x:187, w:427) ====================
    // Visualizations section: x:187, y:27, w:427, h:159 per gui-layout-5
    // Bands controls: x:187, y:186, w:427, h:61 per gui-layout-5

    const int bandDisplayY = 27;
    const int bandDisplayH = 159;
    const int vocoderRowY = 186;
    const int vocoderRowH = 61;

    // Band display and editors (same bounds, toggle visibility)
    juce::Rectangle<int> bandArea (centerPanelX, bandDisplayY, centerPanelW, bandDisplayH);
    bandDisplay.setBounds (bandArea);
    bandDistEditor.setBounds (bandArea);
    bandOffsetEditor.setBounds (bandArea);
    perBandQCarousel.setBounds (bandArea);

    // Vocoder knobs row (6 knobs evenly spaced)
    const int smallVocoderKnob = 36;
    juce::Rectangle<int> knobRow (centerPanelX + 4, vocoderRowY + 4, centerPanelW - 8, vocoderRowH - 8);

    int numKnobs = 6;
    int extraButtonSpace = 44;  // Space for STACK/DIST buttons between bandCount and width knobs
    int knobSpacing = (knobRow.getWidth() - (smallVocoderKnob * numKnobs) - extraButtonSpace) / (numKnobs + 1);
    knobSpacing = juce::jmax (4, knobSpacing);

    int kx = knobRow.getX() + knobSpacing;
    int ky = knobRow.getY();

    // Store bounds for painting border (with some padding)
    vocoderKnobsArea = knobRow.expanded (4, 2);

    auto placeSmallKnob = [&](juce::Slider& slider, juce::Label& label) {
        slider.setBounds (kx, ky, smallVocoderKnob, smallVocoderKnob);
        label.setBounds (kx - 4, ky + smallVocoderKnob, smallVocoderKnob + 8, labelH);
        kx += smallVocoderKnob + knobSpacing;
    };

    // Band count slider (draggable number display)
    bandCountSlider.setBounds (kx, ky, smallVocoderKnob, smallVocoderKnob);
    bandCountLabel.setBounds (kx - 4, ky + smallVocoderKnob, smallVocoderKnob + 8, labelH);

    // DIST/OFFSET/BANDS buttons moved to HEADER ROW per gui-layout-5
    bandDistButton.setBounds (187, 0, 39, 27);
    bandOffsetButton.setBounds (226, 0, 37, 27);
    perBandQButton.setBounds (263, 0, 38, 27);

    // Feedback button - HIDDEN per gui-layout-5
    feedbackButton.setBounds (0, 0, 0, 0);
    feedbackButton.setVisible (false);

    // Input/Output gain knobs in top bar (per gui-layout-5: x:653, w:71)
    inputGainKnob.setBounds (655, 0, 26, 26);
    inputGainLabel.setBounds (651, 20, 34, 10);
    outputGainKnob.setBounds (689, 0, 26, 26);
    outputGainLabel.setBounds (685, 20, 34, 10);
    // Level meters: thin bars beside knobs
    inputMeter.setBounds (651, 3, 3, 20);
    outputMeter.setBounds (716, 3, 3, 20);

    // STACK button stays next to band count slider
    {
        int btnX1 = kx + smallVocoderKnob + 2;
        additiveModeToggle.setBounds (btnX1, ky + 8, 50, 20);
    }

    kx += smallVocoderKnob + knobSpacing + 54;  // Space for STACK button

    placeSmallKnob (bandWidthSlider, bandWidthLabel);
    placeSmallKnob (attackSlider, attackLabel);
    placeSmallKnob (releaseSlider, releaseLabel);

    // ADAPT button next to release knob (same style as STACK button)
    {
        int btnX1 = kx - knobSpacing + 2;  // Right after release knob
        adaptiveEnvToggle.setBounds (btnX1, ky + 8, 50, 20);
        kx += 54;  // Space for ADAPT button
    }

    placeSmallKnob (gateSlider, gateLabel);
    placeSmallKnob (depthSlider, depthLabel);

    // ==================== RIGHT PANEL (x:613, w:110) ====================
    // Per gui-layout-5:
    // FORMANT, HARM SHIFT: (y:48, h:68)
    // NOISE, HP BLEND, NOISE COL: (y:117, h:130)

    const int smallKnob = 38;
    int halfW = rightPanelW / 2;

    // Harmonic section (y:48, h:68) per gui-layout-5
    const int harmonicY = 48;
    const int harmonicH = 68;
    int ry = harmonicY + 8;

    // Row 1: formant | harm shift (both small knobs side by side)
    formantSlider.setBounds (rightPanelX + 8, ry, smallKnob, smallKnob);
    formantLabel.setBounds (rightPanelX + 4, ry + smallKnob, smallKnob + 8, labelH);

    harmShiftSlider.setBounds (rightPanelX + halfW + 8, ry, smallKnob, smallKnob);
    harmShiftLabel.setBounds (rightPanelX + halfW + 2, ry + smallKnob, smallKnob + 12, labelH);

    // Store harmonic section bounds (formant + harm shift)
    harmonicKnobsArea = juce::Rectangle<int> (rightPanelX, harmonicY, rightPanelW, harmonicH);

    // Noise section (y:117, h:130) per gui-layout-5
    const int noiseY = 117;
    const int noiseH = 130;
    int noiseStartY = noiseY;
    ry = noiseY + 8;

    // Row 1 in noise section: noise | hp-blend (small knobs side by side)
    unvoicedSlider.setBounds (rightPanelX + 8, ry, smallKnob, smallKnob);
    unvoicedLabel.setBounds (rightPanelX + 4, ry + smallKnob, smallKnob + 8, labelH);

    hpBlendSlider.setBounds (rightPanelX + halfW + 8, ry, smallKnob, smallKnob);
    hpBlendLabel.setBounds (rightPanelX + halfW + 4, ry + smallKnob, smallKnob + 8, labelH);

    // Slope box at bottom-right of hp-blend knob
    hpBlendSlopeBox.setBounds (rightPanelX + halfW + smallKnob - 16, ry + smallKnob - 12, 24, 12);

    ry += smallKnob + labelH + 2;

    // Row 2: Noise Mode dropdown (short labels fit under knob)
    int dropdownW = smallKnob + 2;  // Match knob width
    noiseModeCombo.setBounds (rightPanelX + 8, ry, dropdownW, 16);

    ry += 22;

    // Row 3: noise color (left side) + spectral pad (right side)
    noiseColorSlider.setBounds (rightPanelX + 8, ry, smallKnob, smallKnob);
    noiseColorLabel.setBounds (rightPanelX + 4, ry + smallKnob, smallKnob + 8, labelH);

    // Spectral X/Y pad - 60x60px to fit in right panel
    const int spectralPadSize = 60;
    if (spectralPad != nullptr)
    {
        int padX = rightPanelX + smallKnob + 12;
        int padY = ry - 4;
        spectralPad->setBounds (padX, padY, spectralPadSize, spectralPadSize);
    }

    ry += smallKnob + labelH + 4;

    // Store noise section bounds for background painting
    noiseKnobsArea = juce::Rectangle<int> (rightPanelX, noiseY, rightPanelW, noiseH);

    // Complement toggle - HIDDEN (already in EQ display below)
    complementToggle.setBounds (0, 0, 0, 0);
    complementToggle.setVisible (false);

    // ==================== BOTTOM SECTION ====================
    // EQUALIZER (SPECTRUM) & COMPLEMENT MODE: (x:0, y:246, w:669, h:164)
    // EQ FREQ, GAIN, Q: (x:669, y:246, w:50, h:164)
    // MIDI KEYBOARD: (x:0, y:410, w:726, h:71)

    // Tone sculpt label - hidden
    toneSculptLabel.setBounds (0, 0, 0, 0);
    toneSculptLabel.setVisible (false);

    // EQ section (per gui-layout-5: spectrum x:1 y:247 w:668 h:169, knobs x:668 w:55)
    const int eqY = 247;
    const int eqH = 169;
    const int eqSpectrumW = 668;
    const int eqKnobsX = 668;
    const int eqKnobsW = 55;

    // EQ knobs column (3 small knobs stacked)
    const int eqKnobSz = 32;
    int totalKnobHeight = (eqKnobSz + labelH) * 3;
    int eqSpacing = (eqH - totalKnobHeight) / 4;
    eqSpacing = juce::jmax (1, eqSpacing);

    int eqKnobY = eqY + eqSpacing;

    freqKnob.setBounds (eqKnobsX + 8, eqKnobY, eqKnobSz, eqKnobSz);
    freqLabel.setBounds (eqKnobsX + 2, eqKnobY + eqKnobSz, 46, labelH);
    eqKnobY += eqKnobSz + labelH + eqSpacing;

    gainKnob.setBounds (eqKnobsX + 8, eqKnobY, eqKnobSz, eqKnobSz);
    gainLabel.setBounds (eqKnobsX + 2, eqKnobY + eqKnobSz, 46, labelH);
    eqKnobY += eqKnobSz + labelH + eqSpacing;

    qKnob.setBounds (eqKnobsX + 8, eqKnobY, eqKnobSz, eqKnobSz);
    qLabel.setBounds (eqKnobsX + 2, eqKnobY + eqKnobSz, 46, labelH);

    // Input filter display (spectrum visualization, per gui-layout-5)
    if (inputFilterDisplay != nullptr)
        inputFilterDisplay->setBounds (1, eqY, eqSpectrumW, eqH);

    // Piano keyboard (full width at bottom, per gui-layout-5: x:1 y:415 w:721 h:74)
    const int keyboardY = 415;
    const int keyboardH = 74;
    if (pianoKeyboard != nullptr)
        pianoKeyboard->setBounds (1, keyboardY, 721, keyboardH);

    // Custom chord editor (overlays the bottom section: EQ + piano area)
    if (customChordEditor != nullptr)
        customChordEditor->setBounds (1, eqY, NATIVE_WIDTH - 2, NATIVE_HEIGHT - eqY - 1);
}

//==============================================================================
// Preset Management
//==============================================================================

void ColorVocoderAudioProcessorEditor::updatePresetButtonText()
{
    auto name = presetManager.getCurrentPresetName();
    auto folder = presetManager.getCurrentPresetFolder();

    if (folder.isNotEmpty())
        presetButton.setButtonText (folder + " / " + name);
    else
        presetButton.setButtonText (name);
}

void ColorVocoderAudioProcessorEditor::showPresetMenu()
{
    auto organized = presetManager.getFolderOrganizedPresets();
    auto currentName = presetManager.getCurrentPresetName();
    auto currentFolder = presetManager.getCurrentPresetFolder();

    juce::PopupMenu menu;
    int itemId = 1;

    // "Init" option at the very top — resets all parameters to default
    bool isInit = (currentName == "Init" || currentName.isEmpty()) && currentFolder.isEmpty();
    menu.addItem (-4, "Init", true, isInit);
    menu.addSeparator();

    // Root presets at top (IDs 1-999)
    if (organized.count (""))
    {
        for (const auto& preset : organized[""])
        {
            bool isCurrent = (preset == currentName && currentFolder.isEmpty());
            menu.addItem (itemId, preset, true, isCurrent);
            itemId++;
        }
    }

    // Folder submenus (IDs: (folderIdx+1)*1000 + presetIdx+1)
    int folderIdx = 0;
    std::vector<std::pair<juce::String, juce::StringArray>> folders;

    for (const auto& [folder, presets] : organized)
    {
        if (folder.isEmpty()) continue;  // Skip root
        folders.push_back ({ folder, presets });
    }

    if (!folders.empty() && itemId > 1)
        menu.addSeparator();

    for (const auto& [folder, presets] : folders)
    {
        if (presets.isEmpty())
        {
            folderIdx++;
            continue;  // Skip empty folders in menu
        }

        juce::PopupMenu submenu;
        int presetIdx = 0;

        for (const auto& preset : presets)
        {
            int id = (folderIdx + 1) * 1000 + presetIdx + 1;
            bool isCurrent = (preset == currentName && folder == currentFolder);
            submenu.addItem (id, preset, true, isCurrent);
            presetIdx++;
        }

        menu.addSubMenu (folder, submenu, true, nullptr, false);
        folderIdx++;
    }

    // Separator + management options
    menu.addSeparator();
    menu.addItem (-1, "New Folder...");

    // Delete current preset option (if one is loaded and isn't "Init")
    if (currentName != "Init" && currentName.isNotEmpty())
    {
        auto deleteLabel = "Delete \"" + currentName + "\"";
        menu.addItem (-2, deleteLabel);
    }

    menu.addSeparator();
    menu.addItem (-3, "Open Preset Folder...");

    // Build lookup tables to decode IDs back to names
    // We need to capture this data for the async callback
    struct PresetLookup
    {
        std::map<int, std::pair<juce::String, juce::String>> idToPreset;  // id -> {name, folder}
    };
    auto lookup = std::make_shared<PresetLookup>();

    // Re-iterate to build lookup
    itemId = 1;
    if (organized.count (""))
    {
        for (const auto& preset : organized[""])
        {
            lookup->idToPreset[itemId] = { preset, "" };
            itemId++;
        }
    }

    folderIdx = 0;
    for (const auto& [folder, presets] : folders)
    {
        if (presets.isEmpty())
        {
            folderIdx++;
            continue;
        }

        int presetIdx = 0;
        for (const auto& preset : presets)
        {
            int id = (folderIdx + 1) * 1000 + presetIdx + 1;
            lookup->idToPreset[id] = { preset, folder };
            presetIdx++;
        }
        folderIdx++;
    }

    auto options = juce::PopupMenu::Options()
                      .withTargetComponent (&presetButton)
                      .withMinimumWidth (presetButton.getWidth());

    menu.showMenuAsync (options, [this, lookup] (int result)
    {
        if (result == 0) return;  // Dismissed

        if (result == -4)
        {
            // Init — reset all APVTS parameters to their defaults
            auto& params = processor.apvts;
            for (auto* param : params.processor.getParameters())
            {
                if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
                    p->setValueNotifyingHost (p->getDefaultValue());
            }
            // Reset non-APVTS state
            processor.resetAllBandGains();
            processor.resetAllPerBandQLocks();
            processor.resetBandDistribution();
            processor.resetBandOffsetCurve();
            processor.setGlobalBandOffset (0.0f);
            processor.resetAllNoteExclusions();
            processor.lastVariationPerDegree.fill (0);
            // Update preset name to Init
            presetManager.setCurrentPresetName ("Init");
            presetManager.setCurrentPresetFolder ("");
            presetManager.markAsSaved();
            updatePresetButtonText();
            return;
        }

        if (result == -1)
        {
            createNewFolderDialog();
            return;
        }

        if (result == -2)
        {
            deleteCurrentPreset();
            return;
        }

        if (result == -3)
        {
            presetManager.getPresetDirectory().revealToUser();
            return;
        }

        auto it = lookup->idToPreset.find (result);
        if (it != lookup->idToPreset.end())
        {
            auto& [name, folder] = it->second;
            if (folder.isEmpty())
                presetManager.loadPreset (name);
            else
                presetManager.loadPreset (name, folder);

            presetManager.setCurrentPresetFolder (folder);
            updatePresetButtonText();
        }
    });
}

void ColorVocoderAudioProcessorEditor::createNewFolderDialog (std::function<void (const juce::String&)> onCreated)
{
    auto* dialog = new juce::AlertWindow ("New Folder",
                                          "Enter a name for the new folder:",
                                          juce::MessageBoxIconType::NoIcon);

    dialog->addTextEditor ("folderName", "", "Folder Name:");
    dialog->addButton ("Create", 1);
    dialog->addButton ("Cancel", 0);

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dialog, onCreated] (int result)
        {
            if (result == 1)
            {
                auto name = dialog->getTextEditorContents ("folderName").trim();

                // Sanitize invalid chars
                name = name.replaceCharacters ("/\\:*?\"<>|", "_________");

                if (name.isNotEmpty())
                {
                    presetManager.createFolder (name);
                    if (onCreated)
                        onCreated (name);
                }
            }
            delete dialog;
        }), true);
}

void ColorVocoderAudioProcessorEditor::savePresetWithDialog()
{
    auto* dialog = new juce::AlertWindow ("Save Preset",
                                          "Enter a name and choose a folder:",
                                          juce::MessageBoxIconType::NoIcon);

    dialog->addTextEditor ("presetName", presetManager.getCurrentPresetName(), "Preset Name:");

    // Folder selection combo
    dialog->addComboBox ("folderSelect", {}, "Save In:");
    auto* folderCombo = dialog->getComboBoxComponent ("folderSelect");

    folderCombo->addItem ("(Root)", 1);

    auto folders = presetManager.getFolderList();
    for (int i = 0; i < folders.size(); i++)
        folderCombo->addItem (folders[i], i + 2);

    folderCombo->addSeparator();
    folderCombo->addItem ("+ New Folder...", 9999);

    // Pre-select current folder
    auto currentFolder = presetManager.getCurrentPresetFolder();
    if (currentFolder.isEmpty())
    {
        folderCombo->setSelectedId (1, juce::dontSendNotification);
    }
    else
    {
        int idx = folders.indexOf (currentFolder);
        if (idx >= 0)
            folderCombo->setSelectedId (idx + 2, juce::dontSendNotification);
        else
            folderCombo->setSelectedId (1, juce::dontSendNotification);
    }

    // Handle "New Folder..." selection
    folderCombo->onChange = [this, folderCombo]()
    {
        if (folderCombo->getSelectedId() == 9999)
        {
            createNewFolderDialog ([this, folderCombo] (const juce::String& newFolder)
            {
                if (newFolder.isNotEmpty())
                {
                    // Refresh the combo with the new folder
                    folderCombo->clear (juce::dontSendNotification);
                    folderCombo->addItem ("(Root)", 1);

                    auto updatedFolders = presetManager.getFolderList();
                    for (int i = 0; i < updatedFolders.size(); i++)
                        folderCombo->addItem (updatedFolders[i], i + 2);

                    folderCombo->addSeparator();
                    folderCombo->addItem ("+ New Folder...", 9999);

                    // Select the newly created folder
                    int idx = updatedFolders.indexOf (newFolder);
                    if (idx >= 0)
                        folderCombo->setSelectedId (idx + 2, juce::dontSendNotification);
                }
            });
        }
    };

    dialog->addButton ("Save", 1);
    dialog->addButton ("Cancel", 0);

    dialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, dialog] (int result)
        {
            if (result == 1)
            {
                auto name = dialog->getTextEditorContents ("presetName").trim();
                auto* fc = dialog->getComboBoxComponent ("folderSelect");

                juce::String folder;
                int selectedId = fc->getSelectedId();
                if (selectedId > 1 && selectedId != 9999)
                    folder = fc->getText();

                if (name.isNotEmpty())
                {
                    if (presetManager.savePreset (name, folder))
                        updatePresetButtonText();
                }
            }
            delete dialog;
        }), true);
}

void ColorVocoderAudioProcessorEditor::deleteCurrentPreset()
{
    auto currentName = presetManager.getCurrentPresetName();
    auto currentFolder = presetManager.getCurrentPresetFolder();

    if (currentName.isEmpty() || currentName == "Init")
        return;

    auto displayName = currentFolder.isNotEmpty()
                         ? currentFolder + " / " + currentName
                         : currentName;

    juce::NativeMessageBox::showOkCancelBox (
        juce::MessageBoxIconType::WarningIcon,
        "Delete Preset",
        "Are you sure you want to delete \"" + displayName + "\"?",
        nullptr,
        juce::ModalCallbackFunction::create ([this, currentName, currentFolder] (int result)
        {
            if (result == 1)
            {
                if (presetManager.deletePreset (currentName, currentFolder))
                {
                    presetManager.setCurrentPresetName ("Init");
                    presetManager.setCurrentPresetFolder ("");
                    updatePresetButtonText();
                }
            }
        }));
}

//==============================================================================
// Knob Value Popup System
//==============================================================================

void ColorVocoderAudioProcessorEditor::registerKnobForPopup (juce::Slider& slider, const juce::String& name, ValueFormat format, int decimals)
{
    knobInfoMap[&slider] = { name, format, decimals };
    slider.addListener (this);
}

void ColorVocoderAudioProcessorEditor::registerKnobContextMenu (juce::Slider& slider, const juce::String& paramId)
{
    knobContextMenus.push_back (std::make_unique<KnobContextMenu> (slider, processor.apvts, paramId));
}

juce::String ColorVocoderAudioProcessorEditor::formatKnobValue (juce::Slider* slider)
{
    auto it = knobInfoMap.find (slider);
    if (it == knobInfoMap.end())
        return juce::String (slider->getValue(), 1);

    const auto& info = it->second;
    float value = (float) slider->getValue();

    switch (info.format)
    {
        case ValueFormat::Percent:
            return KnobValuePopup::formatPercent (value);

        case ValueFormat::SignedPercent:
            return KnobValuePopup::formatSignedPercent (value);

        case ValueFormat::Ms:
            return KnobValuePopup::formatMs (value);

        case ValueFormat::Semitones:
            return KnobValuePopup::formatSemitones (value);

        case ValueFormat::Frequency:
            return KnobValuePopup::formatFrequency (value);

        case ValueFormat::Db:
            return KnobValuePopup::formatDb (value);

        case ValueFormat::Int:
            return KnobValuePopup::formatInt ((int) value);

        case ValueFormat::NoiseColor:
            return KnobValuePopup::formatNoiseColor (value);

        case ValueFormat::Number:
        default:
            return KnobValuePopup::formatNumber (value, info.decimals);
    }
}

void ColorVocoderAudioProcessorEditor::sliderDragStarted (juce::Slider* slider)
{
    auto it = knobInfoMap.find (slider);
    if (it != knobInfoMap.end())
    {
        knobPopup.showForSlider (slider, it->second.name, formatKnobValue (slider));
    }
}

void ColorVocoderAudioProcessorEditor::sliderValueChanged (juce::Slider* slider)
{
    if (knobPopup.isVisible())
    {
        auto it = knobInfoMap.find (slider);
        if (it != knobInfoMap.end())
        {
            knobPopup.showForSlider (slider, it->second.name, formatKnobValue (slider));
        }
    }
}

void ColorVocoderAudioProcessorEditor::sliderDragEnded (juce::Slider*)
{
    knobPopup.hide();
}

//==============================================================================
// Band View Toggle System
//==============================================================================

void ColorVocoderAudioProcessorEditor::updateBandViewVisibility()
{
    // Update button toggle states
    bandDistButton.setToggleState (currentBandView == BandViewMode::Distribution, juce::dontSendNotification);
    bandOffsetButton.setToggleState (currentBandView == BandViewMode::Offset, juce::dontSendNotification);
    perBandQButton.setToggleState (currentBandView == BandViewMode::PerBandQ, juce::dontSendNotification);

    // Update visibility
    bandDisplay.setVisible (currentBandView == BandViewMode::Normal);
    bandDistEditor.setVisible (currentBandView == BandViewMode::Distribution);
    bandOffsetEditor.setVisible (currentBandView == BandViewMode::Offset);
    perBandQCarousel.setVisible (currentBandView == BandViewMode::PerBandQ);

    repaint();
}

void ColorVocoderAudioProcessorEditor::updateQKnobVisibility()
{
    // Get current algorithm mode: 0=RITUAL, 1=LINKED, 2=SPLIT
    int mode = algorithmModeBox.getSelectedItemIndex();

    // Modulator Q knob only visible in SPLIT mode (mode == 2)
    bool showModQ = (mode == 2);
    modulatorQKnob.setVisible (showModQ);
    modulatorQLabel.setVisible (showModQ);

    // Update carrier Q label: "Q" in RITUAL/LINKED, "CAR Q" in SPLIT
    carrierQLabel.setText (showModQ ? "CAR Q" : "Q", juce::dontSendNotification);

    // Trigger layout update if needed
    if (scaledContent)
        scaledContent->repaint();
}

//==============================================================================
// BandDisplay - MIDI Drag Export Implementation
//==============================================================================

juce::File BandDisplay::createMidiFileFromBands()
{
    // Get band frequencies and gains from processor
    const auto& bandGains = processor.getAllBandGains();
    int numBands = (int) displayFreqs.size();

    if (numBands == 0)
        return {};

    // Deduplicate: map MIDI note -> highest velocity
    // Multiple bands may map to the same MIDI note (especially with many bands)
    std::map<int, int> noteVelocityMap;

    for (int i = 0; i < numBands; ++i)
    {
        float freq = displayFreqs[(size_t) i];

        // Skip bands outside the active range
        if (freq < lowHz || freq > highHz)
            continue;

        // Skip notes that are excluded by user (e.g., clicking on keyboard to mute a note)
        int pitchClass = processor.frequencyToPitchClass (freq);
        if (processor.getNoteExclusion (pitchClass))
            continue;

        int midiNote = freqToMidiNote (freq);
        if (midiNote < 0 || midiNote > 127)
            continue;

        // Velocity from band gain (0-1 -> 1-127)
        float gain = (i < ColorVocoderAudioProcessor::maxBands) ? bandGains[(size_t) i] : 1.0f;
        int velocity = juce::jlimit (1, 127, juce::roundToInt (gain * 127.0f));

        // Keep highest velocity for each note
        auto it = noteVelocityMap.find (midiNote);
        if (it == noteVelocityMap.end() || velocity > it->second)
            noteVelocityMap[midiNote] = velocity;
    }

    if (noteVelocityMap.empty())
        return {};

    // Create MIDI file (Type 0 - single track, most compatible)
    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote (960);  // Higher resolution PPQ

    juce::MidiMessageSequence track;

    // Add tempo meta event (120 BPM = 500000 microseconds per quarter note)
    track.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);

    // Add time signature (4/4)
    track.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);

    // Add all notes - 1 bar duration (4 beats = 3840 ticks at 960 PPQ)
    double noteDuration = 960.0 * 4.0;  // 1 bar

    for (const auto& [note, velocity] : noteVelocityMap)
    {
        track.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) velocity), 0.0);
        track.addEvent (juce::MidiMessage::noteOff (1, note, (juce::uint8) 0), noteDuration);
    }

    // End of track marker
    track.addEvent (juce::MidiMessage::endOfTrack(), noteDuration + 1.0);

    track.updateMatchedPairs();
    midiFile.addTrack (track);

    // Write to Desktop folder for reliability (temp files can be problematic with drag-drop)
    auto desktopDir = juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
    auto exportDir = desktopDir.getChildFile ("BandRitual_MIDI");
    exportDir.createDirectory();

    auto midiFilePath = exportDir.getChildFile ("BandRitual_export.mid");

    // Delete old file if exists
    midiFilePath.deleteFile();

    // Write MIDI file
    juce::FileOutputStream outputStream (midiFilePath);
    if (outputStream.openedOk())
    {
        bool success = midiFile.writeTo (outputStream);
        outputStream.flush();

        if (success)
        {
            DBG ("MIDI file written to: " << midiFilePath.getFullPathName());
            DBG ("File size: " << midiFilePath.getSize() << " bytes, " << (int) noteVelocityMap.size() << " notes");
            return midiFilePath;
        }
    }

    DBG ("Failed to write MIDI file!");
    return {};
}

void BandDisplay::startMidiDrag (const juce::MouseEvent&)
{
    // Only start drag once (prevent multiple file creations)
    if (!midiDragStarted)
        return;

    midiDragStarted = false;  // Prevent re-entry

    // Create the MIDI file
    currentMidiFile = createMidiFileFromBands();

    if (!currentMidiFile.existsAsFile())
    {
        DBG ("Failed to create MIDI file for drag export");
        return;
    }

    DBG ("Starting MIDI drag with file: " << currentMidiFile.getFullPathName());

    // Perform the drag operation
    // File is kept on Desktop/BandRitual_MIDI/ for user reference
    juce::StringArray files;
    files.add (currentMidiFile.getFullPathName());

    juce::DragAndDropContainer::performExternalDragDropOfFiles (
        files,
        false,  // canMoveFiles = false (copy, don't move)
        this,
        nullptr  // No cleanup - file stays in BandRitual_MIDI folder
    );
}

// ==================== SCREENSHOT EXPORT (Cmd+Shift+E) ====================

void ColorVocoderAudioProcessorEditor::toggleEditMode()
{
    editModeActive = !editModeActive;
    processor.editModeActive.store (editModeActive);

    // Toggle visibility
    if (customChordEditor != nullptr)
        customChordEditor->setVisible (editModeActive);
    if (inputFilterDisplay != nullptr)
        inputFilterDisplay->setVisible (!editModeActive);
    if (pianoKeyboard != nullptr)
        pianoKeyboard->setVisible (!editModeActive);

    // Also hide EQ knobs in edit mode
    freqKnob.setVisible (!editModeActive);
    freqLabel.setVisible (!editModeActive);
    gainKnob.setVisible (!editModeActive);
    gainLabel.setVisible (!editModeActive);
    qKnob.setVisible (!editModeActive);
    qLabel.setVisible (!editModeActive);

    // Update EDIT button appearance
    editButton.setColour (juce::TextButton::buttonColourId,
                           editModeActive ? juce::Colour (0xFF2266AA) : juce::Colour (0xFF2A2A35));
    editButton.setColour (juce::TextButton::textColourOffId,
                           editModeActive ? juce::Colour (0xFF64D2FF) : juce::Colours::white);

    repaint();
}

bool ColorVocoderAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    // Cmd+Shift+E (Mac) or Ctrl+Shift+E (Windows) = Export screenshot
    auto mods = key.getModifiers();
    if (key.getKeyCode() == 'E' && mods.isCommandDown() && mods.isShiftDown())
    {
        exportScreenshot();
        return true;
    }

    // No-modifier keys
    if (!mods.isCommandDown() && !mods.isCtrlDown() && !mods.isAltDown())
    {
        int kc = key.getKeyCode();

        // Esc exits edit mode
        if (kc == juce::KeyPress::escapeKey && editModeActive)
        {
            toggleEditMode();
            return true;
        }

        // ===== EDIT MODE: letter keys select trigger keys =====
        if (editModeActive && customChordEditor != nullptr)
        {
            char upperKc = (char) juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) kc);
            const char* validKeys = "QWERTYUASDFGHJZXCVBNM";
            if (juce::String (validKeys).containsChar (upperKc))
            {
                processor.lastTriggeredLetterKey.store (upperKc);
                customChordEditor->repaint();
                return true;
            }
            return false;
        }

        // ===== QWERTY CHORD TRIGGERING (only when orange M toggle is active) =====
        // M toggle itself is click-only (no keyboard shortcut)
        auto* midiChordParam = processor.apvts.getRawParameterValue ("midiChordTrigger");
        bool midiChordActive = midiChordParam ? (midiChordParam->load() > 0.5f) : false;

        if (midiChordActive)
        {
            // Home row A S D F G H J = chord degrees I-VII (0-6)
            int chordDegree = -1;
            switch (kc)
            {
                case 'A': case 'a': chordDegree = 0; break;
                case 'S': case 's': chordDegree = 1; break;
                case 'D': case 'd': chordDegree = 2; break;
                case 'F': case 'f': chordDegree = 3; break;
                case 'G': case 'g': chordDegree = 4; break;
                case 'H': case 'h': chordDegree = 5; break;
                case 'J': case 'j': chordDegree = 6; break;
            }

            if (chordDegree >= 0)
            {
                char upperKc = (char) juce::CharacterFunctions::toUpperCase ((juce::juce_wchar) kc);
                processor.lastTriggeredLetterKey.store (upperKc);

                if (auto* curDegreeParam = processor.apvts.getRawParameterValue ("chordDegree"))
                {
                    int oldDegree = (int) curDegreeParam->load();
                    if (auto* curVarParam = processor.apvts.getRawParameterValue ("chordVariation"))
                        processor.lastVariationPerDegree[(size_t) oldDegree] = (int) curVarParam->load();
                }

                if (auto* param = processor.apvts.getParameter ("chordDegree"))
                    param->setValueNotifyingHost ((float) chordDegree / 6.0f);

                if (auto* param = processor.apvts.getParameter ("chordVariation"))
                    param->setValueNotifyingHost ((float) processor.lastVariationPerDegree[(size_t) chordDegree] / 9.0f);

                if (processor.midiChordFirstPress)
                {
                    if (auto* param = processor.apvts.getParameter ("chordAmount"))
                        param->setValueNotifyingHost (1.0f);
                    processor.midiChordFirstPress = false;
                }

                return true;
            }

            // Top row Q W E R T Y U = chord variations 0-6
            int variation = -1;
            switch (kc)
            {
                case 'Q': case 'q': variation = 0; break;
                case 'W': case 'w': variation = 1; break;
                case 'E': case 'e': variation = 2; break;
                case 'R': case 'r': variation = 3; break;
                case 'T': case 't': variation = 4; break;
                case 'Y': case 'y': variation = 5; break;
                case 'U': case 'u': variation = 6; break;
            }

            if (variation >= 0)
            {
                if (auto* param = processor.apvts.getParameter ("chordVariation"))
                    param->setValueNotifyingHost ((float) variation / 9.0f);

                if (auto* curDegreeParam = processor.apvts.getRawParameterValue ("chordDegree"))
                    processor.lastVariationPerDegree[(size_t) (int) curDegreeParam->load()] = variation;

                return true;
            }

            // Number keys 0-9 = variation selection
            int numKeyVar = -1;
            if (kc >= '1' && kc <= '9')
                numKeyVar = kc - '1';
            else if (kc == '0')
                numKeyVar = 9;

            if (numKeyVar >= 0)
            {
                if (auto* param = processor.apvts.getParameter ("chordVariation"))
                    param->setValueNotifyingHost ((float) numKeyVar / 9.0f);

                if (auto* curDegreeParam = processor.apvts.getRawParameterValue ("chordDegree"))
                    processor.lastVariationPerDegree[(size_t) (int) curDegreeParam->load()] = numKeyVar;

                return true;
            }
        }

        // D/O/B band view shortcuts REMOVED — use click-only to avoid
        // conflicts when plugin is on a MIDI track in a DAW.
        // M toggle shortcut also REMOVED — click the orange M button instead.

        // Esc = Close any open section (return to Normal)
        if (kc == juce::KeyPress::escapeKey)
        {
            if (currentBandView != BandViewMode::Normal)
            {
                currentBandView = BandViewMode::Normal;
                updateBandViewVisibility();
                repaint();
                return true;
            }
        }
    }

    return false;
}

void ColorVocoderAudioProcessorEditor::exportScreenshot()
{
    // Create image at NATIVE resolution (unscaled)
    juce::Image screenshot (juce::Image::ARGB, NATIVE_WIDTH, NATIVE_HEIGHT, true);
    juce::Graphics g (screenshot);

    // Render the scaledContent (which contains all the UI at native resolution)
    if (scaledContent != nullptr)
    {
        scaledContent->paintEntireComponent (g, false);
    }

    // Save to Desktop
    auto desktop = juce::File::getSpecialLocation (juce::File::userDesktopDirectory);
    auto outputFile = desktop.getChildFile ("BandRitual_GUI_Screenshot.png");

    // Delete old file if exists
    if (outputFile.existsAsFile())
        outputFile.deleteFile();

    // Write PNG
    juce::FileOutputStream stream (outputFile);
    if (stream.openedOk())
    {
        juce::PNGImageFormat pngFormat;
        pngFormat.writeImageToStream (screenshot, stream);
        DBG ("Screenshot saved to: " << outputFile.getFullPathName());

        // Show confirmation (brief flash or just open the file)
        outputFile.revealToUser();
    }
}
