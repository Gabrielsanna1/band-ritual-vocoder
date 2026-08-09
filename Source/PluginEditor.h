#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PresetManager.h"
#include "FilmstripLookAndFeel.h"
#include "BinaryData.h"

//==============================================================================
// KnobValuePopup – Shows parameter name and value when dragging knobs
//==============================================================================
class KnobValuePopup : public juce::Component
{
public:
    KnobValuePopup()
    {
        setInterceptsMouseClicks (false, false);
        setAlwaysOnTop (true);

        // Load popup frame image
        popupImage = juce::ImageCache::getFromMemory (
            BinaryData::popup_frame_trimmed_png,
            BinaryData::popup_frame_trimmed_pngSize);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Draw popup frame image stretched to fit
        g.drawImage (popupImage, bounds);

        // Draw text centered
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText (text, bounds.reduced (8, 0), juce::Justification::centred);
    }

    void showForSlider (juce::Slider* slider, const juce::String& name, const juce::String& value)
    {
        text = name + ": " + value;

        // Calculate popup size based on text
        juce::Font font (13.0f, juce::Font::bold);
        int textWidth = font.getStringWidth (text) + 24;
        int popupWidth = juce::jmax (80, textWidth);
        int popupHeight = 28;

        // Position centered below the slider
        if (auto* parent = slider->getParentComponent())
        {
            auto sliderBounds = slider->getBoundsInParent();
            int popupX = sliderBounds.getCentreX() - popupWidth / 2;
            int popupY = sliderBounds.getBottom() + 4;

            // Make sure popup stays within parent bounds
            if (popupX < 0) popupX = 0;
            if (popupX + popupWidth > parent->getWidth())
                popupX = parent->getWidth() - popupWidth;

            setBounds (popupX, popupY, popupWidth, popupHeight);

            if (getParentComponent() != parent)
            {
                if (getParentComponent() != nullptr)
                    getParentComponent()->removeChildComponent (this);
                parent->addAndMakeVisible (this);
            }

            setVisible (true);
            toFront (false);
            repaint();  // Force redraw with updated text value
        }
    }

    void hide()
    {
        setVisible (false);
    }

    // Value formatting helpers
    static juce::String formatPercent (float value)
    {
        return juce::String (juce::roundToInt (value * 100.0f)) + "%";
    }

    static juce::String formatSignedPercent (float value)
    {
        int pct = juce::roundToInt (value * 100.0f);
        if (pct > 0) return "+" + juce::String (pct) + "%";
        return juce::String (pct) + "%";
    }

    static juce::String formatMs (float ms)
    {
        if (ms >= 1000.0f)
            return juce::String (ms / 1000.0f, 1) + "s";
        return juce::String (juce::roundToInt (ms)) + "ms";
    }

    static juce::String formatSemitones (float st)
    {
        int semis = juce::roundToInt (st);
        if (semis > 0) return "+" + juce::String (semis) + "st";
        if (semis < 0) return juce::String (semis) + "st";
        return "0st";
    }

    static juce::String formatFrequency (float hz)
    {
        if (hz >= 1000.0f)
            return juce::String (hz / 1000.0f, 1) + "k";
        return juce::String (juce::roundToInt (hz)) + "Hz";
    }

    static juce::String formatDb (float db)
    {
        if (db > 0) return "+" + juce::String (db, 1) + "dB";
        return juce::String (db, 1) + "dB";
    }

    static juce::String formatNumber (float value, int decimals = 1)
    {
        return juce::String (value, decimals);
    }

    static juce::String formatInt (int value)
    {
        return juce::String (value);
    }

    static juce::String formatNoiseColor (float value)
    {
        if (value < -0.3f) return "Pink";
        if (value > 0.3f) return "White";
        return "Neutral";
    }

private:
    juce::String text;
    juce::Image popupImage;
};

//==============================================================================
// CircleInfoButton – Clean circular info button with "i" icon
//==============================================================================
class CircleInfoButton : public juce::Component
{
public:
    CircleInfoButton()
    {
        setInterceptsMouseClicks (true, false);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.0f);
        float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
        auto centre = bounds.getCentre();

        // Circle background - subtle dark with slight transparency
        g.setColour (isMouseOver ? juce::Colour (0xFF3A3A45) : juce::Colour (0xFF2A2A35));
        g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

        // Circle border
        g.setColour (juce::Colour (0xFF4A4A55));
        g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);

        // Draw "i" icon
        g.setColour (isMouseOver ? juce::Colours::white : juce::Colour (0xFF999999));
        g.setFont (juce::Font (radius * 1.3f, juce::Font::bold));
        g.drawText ("i", bounds, juce::Justification::centred);
    }

    void mouseEnter (const juce::MouseEvent&) override
    {
        isMouseOver = true;
        repaint();
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        isMouseOver = false;
        repaint();
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (onClick)
            onClick();
    }

    std::function<void()> onClick;

private:
    bool isMouseOver = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CircleInfoButton)
};

//==============================================================================
// BandDisplay – visualiser for vocoder bands with Ableton-style draggable gains
//==============================================================================
class BandDisplay : public juce::Component,
                    private juce::Timer
{
public:
    BandDisplay (ColorVocoderAudioProcessor& p)
        : processor (p)
    {
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);
        // Force cursor to persist - needed for DAW compatibility (Ableton, Cubase, etc.)
        cursorFixer = std::make_unique<CursorFixer>();
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

        // Dark background
        g.setColour (juce::Colour (0xFF1A1A1F));
        g.fillRoundedRectangle (area, 6.0f);

        // Border
        g.setColour (juce::Colour (0xFF3A3A45));
        g.drawRoundedRectangle (area.reduced (0.5f), 6.0f, 1.0f);

        // Ableton-style zoom/scroll bar at top
        zoomBarBounds = area.removeFromTop (18.0f);
        auto barInner = zoomBarBounds.reduced (6, 4);
        // Shorten zoom track so it doesn't extend behind MIDI button (per gui-layout-5)
        barInner.setWidth (std::min (barInner.getWidth(), 358.0f));

        // Background track
        g.setColour (juce::Colour (0xFF1A1A1F));
        g.fillRoundedRectangle (barInner, 3.0f);

        // Calculate thumb position from zoom state
        // viewStart and viewEnd are 0.0-1.0 representing position in full 20Hz-20kHz range
        float viewStart = zoomCenter - 0.5f / zoomLevel;
        float viewEnd = zoomCenter + 0.5f / zoomLevel;
        viewStart = juce::jlimit (0.0f, 1.0f, viewStart);
        viewEnd = juce::jlimit (0.0f, 1.0f, viewEnd);

        // Thumb bounds (the draggable visible portion indicator)
        float thumbLeft = barInner.getX() + viewStart * barInner.getWidth();
        float thumbRight = barInner.getX() + viewEnd * barInner.getWidth();
        float thumbWidth = std::max (20.0f, thumbRight - thumbLeft);  // Min width for clickability
        zoomThumbBounds = juce::Rectangle<float> (thumbLeft, barInner.getY(), thumbWidth, barInner.getHeight());

        // Draw thumb background
        g.setColour (juce::Colour (0xFF3A3A45));
        g.fillRoundedRectangle (zoomThumbBounds, 3.0f);

        // Left and right bracket handles
        float handleWidth = 8.0f;
        zoomLeftHandle = juce::Rectangle<float> (zoomThumbBounds.getX(), zoomThumbBounds.getY(),
                                                  handleWidth, zoomThumbBounds.getHeight());
        zoomRightHandle = juce::Rectangle<float> (zoomThumbBounds.getRight() - handleWidth, zoomThumbBounds.getY(),
                                                   handleWidth, zoomThumbBounds.getHeight());

        // Draw bracket handles [ and ]
        g.setColour (juce::Colour (0xFF5A5A65));
        g.fillRoundedRectangle (zoomLeftHandle, 2.0f);
        g.fillRoundedRectangle (zoomRightHandle, 2.0f);

        // Draw bracket symbols
        g.setColour (juce::Colour (0xFFAAAAAA));
        g.setFont (10.0f);
        g.drawText ("[", zoomLeftHandle, juce::Justification::centred);
        g.drawText ("]", zoomRightHandle, juce::Justification::centred);

        // ===== MIDI FOLLOW TOGGLE (top-right corner) =====
        {
            // Get current MIDI Follow state from processor
            auto* midiFollowParam = processor.apvts.getRawParameterValue ("midiFollow");
            bool midiFollowOn = midiFollowParam ? (midiFollowParam->load() > 0.5f) : false;

            // Position in top-right corner of zoom bar area
            float toggleW = 50.0f;
            float toggleH = 14.0f;
            midiFollowBounds = juce::Rectangle<float> (
                getLocalBounds().getRight() - toggleW - 8.0f,
                zoomBarBounds.getCentreY() - toggleH / 2.0f,
                toggleW, toggleH);

            // Draw toggle background
            g.setColour (midiFollowOn ? juce::Colour (0xFF00A0FF) : juce::Colour (0xFF3A3A45));
            g.fillRoundedRectangle (midiFollowBounds, 3.0f);

            // Draw border
            g.setColour (midiFollowOn ? juce::Colour (0xFF40C0FF) : juce::Colour (0xFF5A5A65));
            g.drawRoundedRectangle (midiFollowBounds, 3.0f, 1.0f);

            // Draw text
            g.setColour (midiFollowOn ? juce::Colours::white : juce::Colour (0xFFAAAAAA));
            g.setFont (9.0f);
            g.drawText ("MIDI", midiFollowBounds, juce::Justification::centred);
        }

        // Safety check
        if (displayFreqs.empty() || displayEnvs.empty() || displayFreqs.size() != displayEnvs.size())
        {
            g.setColour (juce::Colours::grey);
            g.setFont (12.0f);
            g.drawFittedText ("No signal", getLocalBounds().reduced (0, 20),
                              juce::Justification::centred, 1);
            return;
        }

        // Padding for internal drawing
        auto plotArea = area.reduced (10, 8);

        // Calculate visible frequency range based on zoom
        float fullLogLow = std::log (20.0f);
        float fullLogHigh = std::log (20000.0f);
        float logRange = (fullLogHigh - fullLogLow) / zoomLevel;
        float logCenter = fullLogLow + zoomCenter * (fullLogHigh - fullLogLow);
        float viewLogLow = logCenter - logRange * 0.5f;
        float viewLogHigh = logCenter + logRange * 0.5f;

        // Clamp to valid range
        if (viewLogLow < fullLogLow) { viewLogLow = fullLogLow; viewLogHigh = viewLogLow + logRange; }
        if (viewLogHigh > fullLogHigh) { viewLogHigh = fullLogHigh; viewLogLow = viewLogHigh - logRange; }

        float viewLowHz = std::exp (viewLogLow);
        float viewHighHz = std::exp (viewLogHigh);

        // Fixed reference level: 1.0 = 0 dB (full scale)
        // Bars show ABSOLUTE levels, not normalized/relative
        // This allows seeing Mod Level changes and actual signal levels
        const float referenceLevel = 0.30f;  // ~-10.5 dBFS = full bar. Was 0.08 (~-22 dBFS) which pegged the
                                              // meter for normal input (looked maxed vs Ableton). Display only.

        const int n = (int) displayFreqs.size();
        bandRects.resize ((size_t) n);

        // Get band gains from processor
        const auto& bandGains = processor.getAllBandGains();

        // Calculate X positions of the low/high frequency boundary lines
        float lowFreqX, highFreqX;
        {
            float lowNorm = (std::log (std::max (lowHz, 20.0f)) - viewLogLow) / (viewLogHigh - viewLogLow);
            lowNorm = juce::jlimit (0.0f, 1.0f, lowNorm);
            lowFreqX = plotArea.getX() + lowNorm * plotArea.getWidth();

            float highNorm = (std::log (std::max (highHz, 21.0f)) - viewLogLow) / (viewLogHigh - viewLogLow);
            highNorm = juce::jlimit (0.0f, 1.0f, highNorm);
            highFreqX = plotArea.getX() + highNorm * plotArea.getWidth();
        }

        // First pass: collect all visible band info (index, xCenter, gain)
        struct VisibleBand { int idx; float xCenter; float gain; float env; };
        std::vector<VisibleBand> visibleBands;
        visibleBands.reserve ((size_t) n);

        for (int i = 0; i < n; ++i)
        {
            float f = displayFreqs[(size_t) i];  // Use smoothed display values

            // Skip bands outside the low/high frequency range
            if (f < lowHz || f > highHz)
            {
                bandRects[(size_t) i] = juce::Rectangle<float>();
                continue;
            }

            // Check if band is in visible zoom range
            float logF = std::log (juce::jlimit (20.0f, 20000.0f, f));
            if (logF < viewLogLow || logF > viewLogHigh)
            {
                bandRects[(size_t) i] = juce::Rectangle<float>();
                continue;
            }

            float norm = (logF - viewLogLow) / (viewLogHigh - viewLogLow);
            norm = juce::jlimit (0.0f, 1.0f, norm);
            float xCenter = plotArea.getX() + norm * plotArea.getWidth();
            int abi = actualBandIdx (i);
            float gain = (abi < ColorVocoderAudioProcessor::maxBands) ? bandGains[(size_t) abi] : 1.0f;
            float env = displayEnvs[(size_t) i];  // Use smoothed display values

            visibleBands.push_back ({ i, xCenter, gain, env });
        }

        // Sort bands by frequency (xCenter) to ensure correct left-to-right order
        std::sort (visibleBands.begin(), visibleBands.end(),
                   [](const VisibleBand& a, const VisibleBand& b) { return a.xCenter < b.xCenter; });

        // Calculate line dimensions - use INTEGER MATH to guarantee no overlap
        int iLowFreqX = (int) std::floor (lowFreqX);
        int iHighFreqX = (int) std::floor (highFreqX);
        int availableWidth = iHighFreqX - iLowFreqX;
        int numBands = (int) visibleBands.size();
        if (numBands == 0) return;

        // ============================================================
        // PERFECT NO-OVERLAP ALGORITHM
        // Each band gets an EXCLUSIVE slot - mathematically impossible to overlap
        // Uses INTEGER-BASED slot assignment to guarantee unique positions
        // ============================================================

        // Pre-calculate ALL positions using exclusive slots
        std::vector<int> slotLeftsInt (numBands);
        std::vector<int> slotCentersInt (numBands);
        std::vector<int> lineLeftsInt (numBands);
        std::vector<int> lineRightsInt (numBands);
        std::vector<float> lineLefts (numBands);
        std::vector<float> lineRights (numBands);
        std::vector<float> slotLefts (numBands);
        std::vector<float> slotRights (numBands);

        // First, compute UNIQUE slot positions using integer arithmetic
        // This guarantees each band gets a distinct, non-overlapping slot
        for (int vi = 0; vi < numBands; ++vi)
        {
            // Use Bresenham-style integer division to distribute slots evenly
            // slot[vi] starts at: (vi * availableWidth) / numBands
            // This guarantees slot[vi+1].left > slot[vi].left when possible
            int slotLeft = iLowFreqX + (vi * availableWidth) / numBands;
            int slotRight = iLowFreqX + ((vi + 1) * availableWidth) / numBands;

            // Ensure last slot reaches boundary
            if (vi == numBands - 1)
                slotRight = iHighFreqX;

            int actualSlotWidth = slotRight - slotLeft;
            int slotCenter = slotLeft + actualSlotWidth / 2;

            // Calculate gap and line width based on this slot's actual width
            // Bands ALWAYS fill their slots (Ableton-style) - visual width is independent of bandwidth param
            int gapWidth, lineWidthInt;
            if (actualSlotWidth >= 6)
            {
                // Wide slots: minimal gap (1px) for visual distinction, fill the rest
                gapWidth = 1;
                lineWidthInt = actualSlotWidth - gapWidth;
            }
            else if (actualSlotWidth >= 3)
            {
                // Medium slots: 1px gap, fill the rest
                gapWidth = 1;
                lineWidthInt = actualSlotWidth - gapWidth;
            }
            else if (actualSlotWidth >= 2)
            {
                // Tight slots: no gap, fill completely
                gapWidth = 0;
                lineWidthInt = actualSlotWidth;
            }
            else
            {
                // Extremely tight (0-1px slot): no gap, 1px line
                // NEVER hide bands - always draw at least 1px
                gapWidth = 0;
                lineWidthInt = 1;
            }

            // Center the line in the slot (or at slot position if slot is tiny)
            int lineLeft = slotLeft;  // Start at slot left
            int lineRight = lineLeft + lineWidthInt;

            // For slots with room, center the line
            if (actualSlotWidth > lineWidthInt)
            {
                lineLeft = slotCenter - lineWidthInt / 2;
                lineRight = lineLeft + lineWidthInt;
                // Keep within slot bounds
                if (lineLeft < slotLeft) lineLeft = slotLeft;
                if (lineRight > slotRight) lineRight = slotRight;
                if (lineRight <= lineLeft) lineRight = lineLeft + 1;
            }

            // Store integer positions
            slotLeftsInt[(size_t) vi] = slotLeft;
            slotCentersInt[(size_t) vi] = slotCenter;
            lineLeftsInt[(size_t) vi] = lineLeft;
            lineRightsInt[(size_t) vi] = lineRight;

            // Convert to float for drawing
            lineLefts[(size_t) vi] = (float) lineLeft;
            lineRights[(size_t) vi] = (float) lineRight;
            slotLefts[(size_t) vi] = (float) slotLeft;
            slotRights[(size_t) vi] = (float) slotRight;
        }

        // Draw bands with frequency-accurate positions
        float maxBarHeight = plotArea.getHeight();

        for (size_t vi = 0; vi < visibleBands.size(); ++vi)
        {
            const auto& vb = visibleBands[vi];
            int i = vb.idx;

            float lineLeft = lineLefts[vi];
            float lineRight = lineRights[vi];  // Use pre-calculated right edge
            float xCenter = (lineLeft + lineRight) * 0.5f;  // Center of the line
            float gain = vb.gain;
            float env = vb.env;

            // Bar width is based on the line extent (smaller portion for the visual bar)
            float bandWidth = (lineRight - lineLeft) * 0.6f;
            bandWidth = juce::jlimit (3.0f, 40.0f, bandWidth);
            float x = xCenter - bandWidth * 0.5f;

            // Bar height represents envelope (absolute level, 0dB = full height)
            float barHeightNorm = juce::jlimit (0.0f, 1.0f, env / referenceLevel);
            float actualBarHeight = barHeightNorm * maxBarHeight;
            float yFull = plotArea.getBottom() - actualBarHeight;

            // Store band rectangle for hit testing (use SLOT bounds for easier clicking)
            bandRects[(size_t) i] = juce::Rectangle<float> (slotLefts[vi], plotArea.getY(), slotRights[vi] - slotLefts[vi], plotArea.getHeight());

            // Draw gain cap line Y position
            float gainCapY = plotArea.getBottom() - (gain * maxBarHeight);

            // ========== LUMINOUS LIQUID GLOW VISUALIZATION ==========
            // Smooth cyan-to-magenta gradient based on band position
            float gradientT = (numBands > 1) ? (float) vi / (float) (numBands - 1) : 0.5f;
            // Cyan (hue 0.52 = 187°) to Magenta/Pink (hue 0.85 = 306°)
            // High saturation + full brightness = vibrant luminous glow
            float hue = 0.52f + gradientT * 0.33f;
            juce::Colour barColour = juce::Colour::fromHSV (hue, 0.85f, 1.0f, 1.0f);
            juce::Colour glowColour = barColour.withAlpha (0.3f);
            juce::Colour highlightColour = juce::Colour::fromHSV (hue, 0.4f, 1.0f, 1.0f);  // Lighter for top highlight

            float cornerRadius = juce::jmin (3.0f, bandWidth * 0.3f);

            // If gain is reduced, show the cut portion in dark
            if (gain < 0.99f)
            {
                // Draw full envelope bar in dark (the "cut" portion)
                g.setColour (juce::Colour (0xFF2A2A35));
                if (actualBarHeight > 0)
                    g.fillRoundedRectangle (juce::Rectangle<float> (x, yFull, bandWidth, actualBarHeight), cornerRadius);

                // Draw gain-limited portion with luminous glow
                float limitedHeight = juce::jmin (actualBarHeight, gain * maxBarHeight);
                float yLimited = plotArea.getBottom() - limitedHeight;

                if (limitedHeight > 2.0f)
                {
                    auto barRect = juce::Rectangle<float> (x, yLimited, bandWidth, limitedHeight);

                    // 1. Outer glow (subtle, clean)
                    auto glowRect = barRect.expanded (2.0f, 1.0f);
                    g.setColour (glowColour.withAlpha (0.15f * barHeightNorm));
                    g.fillRoundedRectangle (glowRect, cornerRadius + 2.0f);

                    // 2. Main bar with vertical gradient (brighter at top = liquid surface)
                    juce::ColourGradient barGradient (
                        highlightColour.withAlpha (0.95f), barRect.getX(), barRect.getY(),
                        barColour.withAlpha (0.7f), barRect.getX(), barRect.getBottom(), false);
                    g.setGradientFill (barGradient);
                    g.fillRoundedRectangle (barRect, cornerRadius);

                    // 3. Top highlight line (light catching liquid surface)
                    g.setColour (juce::Colours::white.withAlpha (0.5f + 0.3f * barHeightNorm));
                    g.drawHorizontalLine ((int) yLimited, x + 1.0f, x + bandWidth - 1.0f);
                }
            }
            else
            {
                // Full gain - draw bar with luminous glow effect
                if (actualBarHeight > 2.0f)
                {
                    auto barRect = juce::Rectangle<float> (x, yFull, bandWidth, actualBarHeight);

                    // 1. Outer glow (subtle, clean)
                    auto glowRect = barRect.expanded (2.0f, 1.0f);
                    g.setColour (glowColour.withAlpha (0.2f * barHeightNorm));
                    g.fillRoundedRectangle (glowRect, cornerRadius + 2.0f);

                    // 2. Main bar with vertical gradient (brighter at top = liquid surface)
                    juce::ColourGradient barGradient (
                        highlightColour.withAlpha (0.95f), barRect.getX(), barRect.getY(),
                        barColour.withAlpha (0.75f), barRect.getX(), barRect.getBottom(), false);
                    g.setGradientFill (barGradient);
                    g.fillRoundedRectangle (barRect, cornerRadius);

                    // 3. Top highlight line (light catching liquid surface)
                    g.setColour (juce::Colours::white.withAlpha (0.6f + 0.3f * barHeightNorm));
                    g.drawHorizontalLine ((int) yFull, x + 1.0f, x + bandWidth - 1.0f);

                    // 4. Inner glow at top (extra luminosity)
                    if (actualBarHeight > 8.0f)
                    {
                        juce::ColourGradient innerGlow (
                            juce::Colours::white.withAlpha (0.25f * barHeightNorm), barRect.getCentreX(), barRect.getY(),
                            juce::Colours::transparentWhite, barRect.getCentreX(), barRect.getY() + 8.0f, false);
                        g.setGradientFill (innerGlow);
                        g.fillRoundedRectangle (barRect.withHeight (8.0f), cornerRadius);
                    }
                }
            }

            // ALWAYS draw gain cap line (so user can see and drag it even at 100%)
            // Use calculated bounds directly - positioning already ensures no overlap
            float lineAlpha = (gain < 0.99f) ? 0.9f : 0.4f;  // Brighter when reduced
            g.setColour (juce::Colour (0xFF64D2FF).withAlpha (lineAlpha));
            g.drawHorizontalLine ((int) gainCapY, lineLeft, lineRight);
        }

        // Draw frequency labels at bottom (showing visible range)
        // Position inside plotArea to avoid clipping at component edges
        g.setColour (juce::Colour (0x80FFFFFF));
        g.setFont (9.0f);
        g.drawText (formatFreq (viewLowHz), (int) plotArea.getX() + 2, (int) plotArea.getBottom() - 12, 45, 12, juce::Justification::centredLeft);
        g.drawText (formatFreq (viewHighHz), (int) plotArea.getRight() - 47, (int) plotArea.getBottom() - 12, 45, 12, juce::Justification::centredRight);

        // Draw draggable low/high frequency markers (vertical lines)
        // ALWAYS draw both markers - clamp to visible area edges

        // Low frequency marker
        {
            float lowNorm = (std::log (std::max (lowHz, 20.0f)) - viewLogLow) / (viewLogHigh - viewLogLow);
            lowNorm = juce::jlimit (0.0f, 1.0f, lowNorm);
            float lowX = plotArea.getX() + lowNorm * plotArea.getWidth();

            g.setColour (juce::Colour (0xFF64D2FF));
            g.drawVerticalLine ((int) lowX, plotArea.getY(), plotArea.getBottom());

            g.setFont (9.0f);
            g.drawText (formatFreq (lowHz), (int) lowX - 25, (int) plotArea.getY() - 2, 50, 12, juce::Justification::centred);

            lowFreqHandleBounds = juce::Rectangle<float> (lowX - 10, plotArea.getY(), 20, plotArea.getHeight());
        }

        // High frequency marker
        {
            float highNorm = (std::log (std::max (highHz, 21.0f)) - viewLogLow) / (viewLogHigh - viewLogLow);
            highNorm = juce::jlimit (0.0f, 1.0f, highNorm);
            float highX = plotArea.getX() + highNorm * plotArea.getWidth();

            g.setColour (juce::Colour (0xFF64D2FF));
            g.drawVerticalLine ((int) highX, plotArea.getY(), plotArea.getBottom());

            g.setFont (9.0f);
            g.drawText (formatFreq (highHz), (int) highX - 25, (int) plotArea.getY() - 2, 50, 12, juce::Justification::centred);

            highFreqHandleBounds = juce::Rectangle<float> (highX - 10, plotArea.getY(), 20, plotArea.getHeight());
        }

        // Show hovered band info
        if (hoveredBand >= 0 && hoveredBand < n)
        {
            float gain = (hoveredBand < ColorVocoderAudioProcessor::maxBands) ? bandGains[(size_t) hoveredBand] : 1.0f;
            juce::String info = "Band " + juce::String (hoveredBand + 1) + ": " + formatFreq (displayFreqs[(size_t) hoveredBand])
                              + " | Gain: " + juce::String ((int)(gain * 100)) + "%";
            g.setColour (juce::Colours::white);
            g.setFont (10.0f);
            g.drawText (info, plotArea.reduced (4, 0), juce::Justification::topLeft);
        }

        // ===== DYNAMIC THRESHOLD LINE (Pro-Q 3-style draggable expander line) =====
        // Cache plotArea + referenceLevel for mouse handlers
        dynThresholdPlotArea = plotArea;
        dynThresholdReferenceLevel = referenceLevel;
        {
            auto* dynThreshParam = processor.apvts.getRawParameterValue ("dynThreshold");
            float dynThreshDb = dynThreshParam ? dynThreshParam->load() : 0.0f;
            bool dynActive = (dynThreshDb < -0.1f);

            if (dynActive)
            {
                float thresholdLinear = juce::Decibels::decibelsToGain (dynThreshDb);
                float normPos = juce::jlimit (0.0f, 1.0f, thresholdLinear / referenceLevel);
                float lineY = plotArea.getBottom() - normPos * plotArea.getHeight();

                // Line + soft glow
                g.setColour (juce::Colour (0x40FFA040));  // Amber glow
                g.fillRect (plotArea.getX(), lineY - 2.0f, plotArea.getWidth(), 4.0f);
                g.setColour (juce::Colour (0xFFFFA040));  // Solid amber
                g.fillRect (plotArea.getX(), lineY - 0.75f, plotArea.getWidth(), 1.5f);

                // dB label right of line
                juce::String dbText = juce::String (dynThreshDb, 1) + " dB";
                g.setFont (9.5f);
                g.setColour (juce::Colour (0xFFFFC880));
                g.drawText (dbText,
                            (int) (plotArea.getRight() - 54.0f),
                            (int) (lineY - 11.0f),
                            48, 11,
                            juce::Justification::centredRight);

                // Hit zone (8px tall around line)
                dynThresholdLineBounds = juce::Rectangle<float> (
                    plotArea.getX(), lineY - 4.0f, plotArea.getWidth(), 8.0f);
            }
            else
            {
                // When off, handle lives at the TOP so user can grab & pull it DOWN into the bars
                float lineY = plotArea.getY() + 1.0f;
                dynThresholdLineBounds = juce::Rectangle<float> (
                    plotArea.getX(), lineY - 4.0f, plotArea.getWidth(), 8.0f);
                // Faint "DYN" handle hint at top-right
                g.setColour (juce::Colour (0x40FFA040));
                g.fillRect (plotArea.getRight() - 18.0f, lineY - 0.5f, 16.0f, 2.0f);
                g.setColour (juce::Colour (0x70FFA040));
                g.setFont (8.0f);
                g.drawText ("DYN ↓", plotArea.getRight() - 44, (int)(lineY + 2), 24, 9,
                            juce::Justification::centredRight);
            }
        }

        // Show MIDI export hint when Alt/Option is held
        // Use getCurrentModifiers() to actively poll the OS for current key state
        if (wasAltDown || juce::ModifierKeys::getCurrentModifiers().isAltDown())
        {
            // Semi-transparent overlay
            g.setColour (juce::Colour (0x80000000));
            g.fillRoundedRectangle (plotArea, 4.0f);

            // Hint text
            g.setColour (juce::Colour (0xFFFFD700));  // Gold
            g.setFont (juce::Font (14.0f, juce::Font::bold));
            g.drawText ("Drag to export MIDI", plotArea, juce::Justification::centred);

            // Subtext with note count
            int noteCount = 0;
            std::set<int> uniqueNotes;
            for (size_t i = 0; i < displayFreqs.size(); ++i)
            {
                float freq = displayFreqs[i];
                if (freq >= lowHz && freq <= highHz)
                {
                    int note = freqToMidiNote (freq);
                    if (note >= 0 && note <= 127)
                        uniqueNotes.insert (note);
                }
            }
            noteCount = (int) uniqueNotes.size();

            g.setFont (10.0f);
            g.setColour (juce::Colour (0xAAFFFFFF));
            g.drawText (juce::String (noteCount) + " unique MIDI notes", plotArea.translated (0, 20), juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Check dynamic threshold line drag (highest priority when hit)
        if (dynThresholdLineBounds.contains (e.position))
        {
            draggingDynThreshold = true;
            applyDynThresholdFromMouse (e.position.y);
            repaint();
            return;
        }

        // Check MIDI Follow toggle first
        if (midiFollowBounds.contains (e.position))
        {
            auto* midiFollowParam = processor.apvts.getParameter ("midiFollow");
            if (midiFollowParam)
            {
                // Toggle the parameter
                float currentValue = midiFollowParam->getValue();
                midiFollowParam->setValueNotifyingHost (currentValue > 0.5f ? 0.0f : 1.0f);
            }
            repaint();
            return;
        }

        // Check zoom bar handles and thumb
        if (zoomLeftHandle.contains (e.position))
        {
            zoomDragMode = ZoomDragMode::LeftHandle;
            zoomDragStartX = e.position.x;
            zoomDragStartViewStart = zoomCenter - 0.5f / zoomLevel;
            zoomDragStartViewEnd = zoomCenter + 0.5f / zoomLevel;
            return;
        }
        if (zoomRightHandle.contains (e.position))
        {
            zoomDragMode = ZoomDragMode::RightHandle;
            zoomDragStartX = e.position.x;
            zoomDragStartViewStart = zoomCenter - 0.5f / zoomLevel;
            zoomDragStartViewEnd = zoomCenter + 0.5f / zoomLevel;
            return;
        }
        if (zoomThumbBounds.contains (e.position))
        {
            zoomDragMode = ZoomDragMode::Pan;
            zoomDragStartX = e.position.x;
            zoomDragStartViewStart = zoomCenter - 0.5f / zoomLevel;
            zoomDragStartViewEnd = zoomCenter + 0.5f / zoomLevel;
            return;
        }

        if (lowFreqHandleBounds.contains (e.position))
        {
            draggingLow = true;
            return;
        }
        if (highFreqHandleBounds.contains (e.position))
        {
            draggingHigh = true;
            return;
        }

        // Option/Alt + click = prepare for MIDI drag export
        if (e.mods.isAltDown())
        {
            midiDragStarted = true;
            return;
        }

        // Start painting band gains - set gain based on Y position
        paintingBands = true;
        lastPaintPos = e.position;  // Store initial position for interpolation
        applyPaintAtPosition (e.position);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        // Dynamic threshold drag (highest priority while active)
        if (draggingDynThreshold)
        {
            applyDynThresholdFromMouse (e.position.y);
            repaint();
            return;
        }

        // MIDI drag export mode - Option/Alt + drag
        if (midiDragStarted)
        {
            startMidiDrag (e);
            return;
        }

        // Ableton-style zoom bar dragging
        if (zoomDragMode != ZoomDragMode::None)
        {
            auto barInner = zoomBarBounds.reduced (6, 4);
            float deltaX = e.position.x - zoomDragStartX;
            float deltaNorm = deltaX / barInner.getWidth();  // Convert to 0-1 range

            float newViewStart = zoomDragStartViewStart;
            float newViewEnd = zoomDragStartViewEnd;
            float minViewSize = 0.05f;  // Minimum 5% of full range visible

            if (zoomDragMode == ZoomDragMode::LeftHandle)
            {
                // Dragging left handle changes start position
                newViewStart = juce::jlimit (0.0f, newViewEnd - minViewSize, zoomDragStartViewStart + deltaNorm);
            }
            else if (zoomDragMode == ZoomDragMode::RightHandle)
            {
                // Dragging right handle changes end position
                newViewEnd = juce::jlimit (newViewStart + minViewSize, 1.0f, zoomDragStartViewEnd + deltaNorm);
            }
            else if (zoomDragMode == ZoomDragMode::Pan)
            {
                // Dragging middle pans the view
                float viewSize = zoomDragStartViewEnd - zoomDragStartViewStart;
                newViewStart = zoomDragStartViewStart + deltaNorm;
                newViewEnd = newViewStart + viewSize;
                // Clamp to bounds
                if (newViewStart < 0.0f) { newViewStart = 0.0f; newViewEnd = viewSize; }
                if (newViewEnd > 1.0f) { newViewEnd = 1.0f; newViewStart = 1.0f - viewSize; }
            }

            // Convert back to zoomLevel and zoomCenter
            float viewSize = newViewEnd - newViewStart;
            zoomLevel = juce::jlimit (1.0f, 20.0f, 1.0f / viewSize);
            zoomCenter = (newViewStart + newViewEnd) * 0.5f;

            repaint();
            return;
        }

        auto area = getLocalBounds().toFloat();
        area.removeFromTop (18.0f);  // Account for zoom bar
        auto plotArea = area.reduced (10, 8);

        // Frequency handle dragging
        float fullLogLow = std::log (20.0f);
        float fullLogHigh = std::log (20000.0f);

        if (draggingLow && onLowFreqChanged)
        {
            float norm = juce::jlimit (0.0f, 0.9f, (e.position.x - plotArea.getX()) / plotArea.getWidth());
            float freq = std::exp (fullLogLow + norm * (fullLogHigh - fullLogLow));
            freq = juce::jlimit (20.0f, highHz / 1.1f, freq);   // allow LO right up to HIGH (min gap 1.1x)
            onLowFreqChanged (freq);
            return;
        }
        if (draggingHigh && onHighFreqChanged)
        {
            float norm = juce::jlimit (0.1f, 1.0f, (e.position.x - plotArea.getX()) / plotArea.getWidth());
            float freq = std::exp (fullLogLow + norm * (fullLogHigh - fullLogLow));
            freq = juce::jlimit (lowHz * 1.1f, 20000.0f, freq);   // allow HIGH right down to LO (min gap 1.1x)
            onHighFreqChanged (freq);
            return;
        }

        // Paint band gains as you drag across - interpolate to catch all bands when moving fast
        if (paintingBands)
        {
            applyPaintInterpolated (lastPaintPos, e.position);
            lastPaintPos = e.position;  // Update for next drag event
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        draggingLow = false;
        draggingHigh = false;
        zoomDragMode = ZoomDragMode::None;
        paintingBands = false;
        midiDragStarted = false;
        draggingDynThreshold = false;
    }

    /** Convert a mouse Y position to a dB threshold and write to APVTS. */
    void applyDynThresholdFromMouse (float mouseY)
    {
        const auto& plot = dynThresholdPlotArea;
        if (plot.getHeight() < 1.0f) return;
        float normY = juce::jlimit (0.0f, 1.0f,
                                    (plot.getBottom() - mouseY) / plot.getHeight());
        float linear = normY * dynThresholdReferenceLevel;
        // Clamp to -80 .. 0 dB; anything below -80 displays as "off"
        float db = (linear < 1.0e-4f)
                 ? -80.0f
                 : juce::jlimit (-80.0f, 0.0f,
                                 juce::Decibels::gainToDecibels (linear, -120.0f));
        if (auto* param = processor.apvts.getParameter ("dynThreshold"))
        {
            // Normalize to 0..1 for setValueNotifyingHost (range is -80..0)
            float normValue = (db - (-80.0f)) / (0.0f - (-80.0f));
            param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normValue));
        }
    }

    /** Apply gain to band at mouse position - Y position = gain level */
    void applyPaintAtPosition (juce::Point<float> pos)
    {
        auto area = getLocalBounds().toFloat();
        area.removeFromTop (18.0f);
        auto plotArea = area.reduced (10, 8);

        // Use bandRects to find band (matches visual display exactly)
        for (int i = 0; i < (int) bandRects.size(); ++i)
        {
            auto& rect = bandRects[(size_t) i];
            if (rect.isEmpty()) continue;

            if (pos.x >= rect.getX() && pos.x < rect.getRight())
            {
                float clampedY = juce::jlimit (plotArea.getY(), plotArea.getBottom(), pos.y);
                float yNorm = (clampedY - plotArea.getY()) / plotArea.getHeight();
                float gain = juce::jlimit (0.0f, 1.0f, 1.0f - yNorm);
                processor.setBandGain (actualBandIdx (i), gain);
                return;
            }
        }
    }

    /** Interpolate between two positions - step through every pixel and use bandRects */
    void applyPaintInterpolated (juce::Point<float> from, juce::Point<float> to)
    {
        auto area = getLocalBounds().toFloat();
        area.removeFromTop (18.0f);
        auto plotArea = area.reduced (10, 8);

        // Step through in 1-pixel increments
        float dx = to.x - from.x;
        float dy = to.y - from.y;
        float distance = std::sqrt (dx * dx + dy * dy);
        int numSteps = std::max (1, (int) std::ceil (distance));

        for (int step = 0; step <= numSteps; ++step)
        {
            float t = (numSteps > 0) ? (float) step / (float) numSteps : 1.0f;
            float x = from.x + t * dx;
            float y = from.y + t * dy;

            // Use bandRects to find band at this X (matches visual display exactly)
            for (int i = 0; i < (int) bandRects.size(); ++i)
            {
                auto& rect = bandRects[(size_t) i];
                if (rect.isEmpty()) continue;

                if (x >= rect.getX() && x < rect.getRight())
                {
                    float clampedY = juce::jlimit (plotArea.getY(), plotArea.getBottom(), y);
                    float yNorm = (clampedY - plotArea.getY()) / plotArea.getHeight();
                    float gain = juce::jlimit (0.0f, 1.0f, 1.0f - yNorm);
                    processor.setBandGain (actualBandIdx (i), gain);
                    break;
                }
            }
        }

        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        // Check if hovering over zoom bar parts
        bool newHoveringZoom = zoomThumbBounds.contains (e.position) ||
                               zoomLeftHandle.contains (e.position) ||
                               zoomRightHandle.contains (e.position);
        if (newHoveringZoom != hoveringZoomBar)
        {
            hoveringZoomBar = newHoveringZoom;
            repaint();
        }

        // Check if hovering over frequency limit handles (vertical lines)
        bool newHoveringHandle = lowFreqHandleBounds.contains (e.position) ||
                                  highFreqHandleBounds.contains (e.position);

        if (newHoveringHandle != hoveringFreqHandle)
        {
            hoveringFreqHandle = newHoveringHandle;
            repaint();
        }

        // Track Alt key state for MIDI export overlay
        bool altDown = e.mods.isAltDown();
        if (altDown != wasAltDown)
        {
            wasAltDown = altDown;
            repaint();
        }

        // Also track hovered band for visual feedback
        int newHovered = -1;
        for (int i = 0; i < (int) bandRects.size(); ++i)
        {
            if (bandRects[(size_t) i].contains (e.position))
            {
                newHovered = i;
                break;
            }
        }

        if (newHovered != hoveredBand)
        {
            hoveredBand = newHovered;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoveredBand = -1;
        hoveringFreqHandle = false;
        hoveringZoomBar = false;
        repaint();
    }

    juce::MouseCursor getMouseCursor() override
    {
        auto mousePos = getMouseXYRelative().toFloat();

        // Zoom bar handles = resize cursor, middle = drag hand
        if (zoomLeftHandle.contains (mousePos) || zoomRightHandle.contains (mousePos))
            return juce::MouseCursor::LeftRightResizeCursor;
        if (zoomThumbBounds.contains (mousePos))
            return juce::MouseCursor::DraggingHandCursor;

        // Freq limit lines = resize cursor
        if (hoveringFreqHandle)
            return juce::MouseCursor::LeftRightResizeCursor;

        // Dynamic threshold line = vertical resize cursor
        if (dynThresholdLineBounds.contains (mousePos))
            return juce::MouseCursor::UpDownResizeCursor;

        return juce::MouseCursor::NormalCursor;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        // Double-click on dynamic threshold line = reset to off (0 dB = line at top)
        if (dynThresholdLineBounds.contains (e.position))
        {
            if (auto* param = processor.apvts.getParameter ("dynThreshold"))
                param->setValueNotifyingHost (1.0f);  // 1.0 of normalized range = 0 dB = off
            repaint();
            return;
        }

        // Double-click on band resets it to 100%
        for (int i = 0; i < (int) bandRects.size(); ++i)
        {
            if (bandRects[(size_t) i].contains (e.position))
            {
                processor.setBandGain (actualBandIdx (i), 1.0f);
                repaint();
                return;
            }
        }

        // Double-click elsewhere resets zoom
        zoomLevel = 1.0f;
        zoomCenter = 0.5f;
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        bool needsRepaint = false;

        // Horizontal scroll (two-finger swipe on trackpad) = pan left/right
        if (std::abs (wheel.deltaX) > 0.001f)
        {
            // Pan speed depends on zoom level - faster pan when zoomed in
            float panAmount = wheel.deltaX * 0.15f / zoomLevel;
            zoomCenter = juce::jlimit (0.0f, 1.0f, zoomCenter - panAmount);
            needsRepaint = true;
        }

        // Vertical scroll = zoom in/out (more responsive)
        if (std::abs (wheel.deltaY) > 0.001f)
        {
            float zoomDelta = wheel.deltaY * 2.0f;  // More responsive zoom
            float newZoom = juce::jlimit (1.0f, 8.0f, zoomLevel * (1.0f + zoomDelta * 0.3f));

            auto area = getLocalBounds().toFloat();
            area.removeFromTop (18.0f);
            auto plotArea = area.reduced (10, 8);

            float mouseNorm = (e.position.x - plotArea.getX()) / plotArea.getWidth();
            mouseNorm = juce::jlimit (0.0f, 1.0f, mouseNorm);

            // Zoom towards mouse position
            if (std::abs(newZoom - zoomLevel) > 0.01f)
            {
                // Shift center towards mouse when zooming in
                if (newZoom > zoomLevel)
                    zoomCenter = zoomCenter * 0.8f + mouseNorm * 0.2f;

                zoomLevel = newZoom;
                zoomCenter = juce::jlimit (0.0f, 1.0f, zoomCenter);
                needsRepaint = true;
            }
        }

        if (needsRepaint)
            repaint();
    }

    // Callbacks for frequency changes
    std::function<void(float)> onLowFreqChanged;
    std::function<void(float)> onHighFreqChanged;

    // Callback for unique band count changes (after merge-deduplication)
    std::function<void(int)> onUniqueBandCountChanged;

    // Callback for max useful band count (for auto-capping slider)
    std::function<void(int)> onMaxUsefulBandsChanged;

    // Reset all band gains
    void resetAllBands()
    {
        processor.resetAllBandGains();
        repaint();
    }

private:
    void timerCallback() override
    {
        // Get actual band data from processor
        processor.getBandSnapshot (freqs, envs, lowHz, highHz, &primaryBandIndices);

        // Smoothly interpolate displayed values towards actual values
        // This creates smooth animation when band count or positions change
        const float smoothFactor = 0.9f;  // Nearly raw — very reactive like Ableton

        // Resize display arrays if needed
        if (displayFreqs.size() != freqs.size())
        {
            displayFreqs.resize (freqs.size());
            displayEnvs.resize (envs.size());
            // Initialize to actual values on first resize
            for (size_t i = 0; i < freqs.size(); ++i)
            {
                displayFreqs[i] = freqs[i];
                displayEnvs[i] = envs[i];
            }
        }
        else
        {
            // Smoothly interpolate towards actual values
            for (size_t i = 0; i < freqs.size(); ++i)
            {
                displayFreqs[i] += (freqs[i] - displayFreqs[i]) * smoothFactor;
                displayEnvs[i] += (envs[i] - displayEnvs[i]) * smoothFactor;
            }
        }

        // Check for Alt key state change (for MIDI export overlay)
        // Use getCurrentModifiers() which actively polls the OS, not the cached currentModifiers
        bool altDown = juce::ModifierKeys::getCurrentModifiers().isAltDown();
        if (altDown != wasAltDown)
            wasAltDown = altDown;

        // Notify listener of unique band count changes (freqs already excludes merged bands)
        int uniqueCount = (int) freqs.size();
        if (uniqueCount != lastUniqueBandCount)
        {
            lastUniqueBandCount = uniqueCount;
            if (onUniqueBandCountChanged)
                onUniqueBandCountChanged (uniqueCount);
        }

        // Calculate and notify max useful bands (for slider auto-cap)
        int maxUseful = processor.calculateMaxUsefulBands();
        if (maxUseful != lastMaxUsefulBands)
        {
            lastMaxUsefulBands = maxUseful;
            if (onMaxUsefulBandsChanged)
                onMaxUsefulBandsChanged (maxUseful);
        }

        repaint();
    }

    juce::String formatFreq (float f) const
    {
        if (f >= 1000.0f)
            return juce::String (f / 1000.0f, 1) + "k";
        return juce::String ((int) f);
    }

    ColorVocoderAudioProcessor& processor;
    std::vector<float> freqs;
    std::vector<float> envs;
    std::vector<float> displayFreqs;  // Smoothly interpolated for animation
    std::vector<float> displayEnvs;   // Smoothly interpolated for animation
    float lowHz  = 80.0f;
    float highHz = 20000.0f;

    // Maps display index → actual band index (accounts for merged secondary bands)
    std::vector<int> primaryBandIndices;

    int actualBandIdx (int displayIdx) const
    {
        if (displayIdx >= 0 && displayIdx < (int) primaryBandIndices.size())
            return primaryBandIndices[(size_t) displayIdx];
        return displayIdx;
    }

    // Zoom state
    float zoomLevel = 1.0f;   // 1.0 = full view, higher = zoomed in
    float zoomCenter = 0.5f;  // 0.0-1.0, where we're centered in frequency range

    // Zoom bar bounds and dragging state (Ableton-style)
    mutable juce::Rectangle<float> zoomBarBounds;
    mutable juce::Rectangle<float> zoomThumbBounds;      // The visible portion indicator
    mutable juce::Rectangle<float> zoomLeftHandle;       // Left bracket handle
    mutable juce::Rectangle<float> zoomRightHandle;      // Right bracket handle
    enum class ZoomDragMode { None, LeftHandle, RightHandle, Pan };
    ZoomDragMode zoomDragMode = ZoomDragMode::None;
    bool hoveringZoomBar = false;
    float zoomDragStartX = 0.0f;
    float zoomDragStartViewStart = 0.0f;  // 0.0-1.0 normalized position
    float zoomDragStartViewEnd = 1.0f;    // 0.0-1.0 normalized position

    // Dragging state - frequency handles
    bool draggingLow = false;
    bool draggingHigh = false;
    mutable juce::Rectangle<float> lowFreqHandleBounds, highFreqHandleBounds;

    // Painting state - band gains (draw gains by dragging across bands)
    bool paintingBands = false;
    int hoveredBand = -1;
    bool hoveringFreqHandle = false;  // True when mouse is near vertical freq limit lines
    juce::Point<float> lastPaintPos;  // Track last position for interpolation

    // Band rectangles for hit testing
    std::vector<juce::Rectangle<float>> bandRects;

    // Cursor fixer for DAW compatibility - forces cursor updates
    struct CursorFixer : public juce::MouseListener
    {
        CursorFixer()
        {
            juce::Desktop::getInstance().addGlobalMouseListener (this);
        }
        ~CursorFixer() override
        {
            juce::Desktop::getInstance().removeGlobalMouseListener (this);
        }
        void mouseMove (const juce::MouseEvent&) override
        {
            juce::Desktop::getInstance().getMainMouseSource().forceMouseCursorUpdate();
        }
        void mouseDrag (const juce::MouseEvent&) override
        {
            juce::Desktop::getInstance().getMainMouseSource().forceMouseCursorUpdate();
        }
    };
    std::unique_ptr<CursorFixer> cursorFixer;

    // MIDI Follow toggle bounds (top-right corner)
    mutable juce::Rectangle<float> midiFollowBounds;

    // Dynamic threshold line (Pro-Q 3-style) — draggable horizontal line across plotArea
    mutable juce::Rectangle<float> dynThresholdLineBounds;
    mutable juce::Rectangle<float> dynThresholdPlotArea;   // Latest plotArea for mouseDrag math
    mutable float dynThresholdReferenceLevel = 0.30f;       // Latest referenceLevel used for display
    bool draggingDynThreshold = false;

    // Track unique band count for callback
    int lastUniqueBandCount = 0;

    // Track max useful bands for auto-cap callback
    int lastMaxUsefulBands = 100;

    // ===== MIDI DRAG EXPORT =====
    // Option/Alt + drag from band visualizer exports current band frequencies as MIDI
    bool midiDragStarted = false;
    bool wasAltDown = false;  // Track Alt state for overlay repaint
    juce::File currentMidiFile;

    // Convert frequency to nearest MIDI note
    static int freqToMidiNote (float freq)
    {
        if (freq <= 0.0f) return -1;
        int note = juce::roundToInt (12.0f * std::log2 (freq / 440.0f) + 69.0f);
        return juce::jlimit (0, 127, note);
    }

    // Create MIDI file from current band frequencies
    juce::File createMidiFileFromBands();

    // Start MIDI drag operation
    void startMidiDrag (const juce::MouseEvent& e);
};

//==============================================================================
// PianoKeyboard – Serum-style realistic piano keyboard for excluding pitch classes
// Features: Real black/white key layout, 3D depth, pressed animation states
//==============================================================================
class PianoKeyboard : public juce::Component,
                      private juce::Timer
{
public:
    PianoKeyboard (ColorVocoderAudioProcessor& p)
        : processor (p)
    {
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);
        // IMPORTANT: Intercept all mouse clicks to prevent propagation to other components
        setInterceptsMouseClicks (true, false);
    }

    void visibilityChanged() override
    {
        if (isVisible() && !isTimerRunning())
            startTimerHz (15);
        else if (!isVisible())
            stopTimer();
    }

    ~PianoKeyboard() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Dark background behind keyboard
        g.setColour (juce::Colour (0xFF0D0D10));
        g.fillRoundedRectangle (bounds, 4.0f);

        // Get current scale/chord state
        auto scaleNotes = processor.getScaleNoteFlags();
        auto chordNotes = processor.getChordNoteFlags();

        auto* scaleAmtParam = processor.apvts.getRawParameterValue ("scaleAmount");
        auto* chordAmtParam = processor.apvts.getRawParameterValue ("chordAmount");
        float scaleAmt = scaleAmtParam ? scaleAmtParam->load() : 0.0f;
        float chordAmt = chordAmtParam ? chordAmtParam->load() : 0.0f;

        // Piano layout constants
        const float padding = 2.0f;
        const float keyboardWidth = bounds.getWidth() - padding * 2.0f;
        const float keyboardHeight = bounds.getHeight() - padding * 2.0f;
        const float whiteKeyWidth = keyboardWidth / 7.0f;
        const float blackKeyWidth = whiteKeyWidth * 0.6f;
        const float blackKeyHeight = keyboardHeight * 0.6f;
        const float keyboardX = bounds.getX() + padding;
        const float keyboardY = bounds.getY() + padding;

        // White key indices in chromatic scale: C=0, D=2, E=4, F=5, G=7, A=9, B=11
        const int whiteKeyNotes[7] = { 0, 2, 4, 5, 7, 9, 11 };
        // Black key indices: C#=1, D#=3, F#=6, G#=8, A#=10
        const int blackKeyNotes[5] = { 1, 3, 6, 8, 10 };
        // Which white key each black key is positioned after (0-indexed)
        const int blackKeyPositions[5] = { 0, 1, 3, 4, 5 }; // After C, D, F, G, A

        // ===== DRAW WHITE KEYS FIRST (background layer) =====
        for (int i = 0; i < 7; ++i)
        {
            int noteIdx = whiteKeyNotes[i];
            float x = keyboardX + i * whiteKeyWidth;
            float y = keyboardY;
            float w = whiteKeyWidth - 1.0f;  // Small gap between keys
            float h = keyboardHeight;

            juce::Rectangle<float> keyBounds (x, y, w, h);
            whiteKeyRects[(size_t) i] = keyBounds;

            bool inScale = scaleNotes[(size_t) noteIdx];
            bool inChord = chordNotes[(size_t) noteIdx];
            bool isExcluded = processor.getNoteExclusion (noteIdx);

            // Calculate opacity
            float opacity = 1.0f;
            if (scaleAmt > 0.01f && !inScale)
                opacity = 0.3f + 0.7f * (1.0f - scaleAmt);

            // Draw white key with 3D effect
            drawWhiteKey (g, keyBounds, isExcluded, inChord, chordAmt, opacity);

            // Draw note name at bottom
            const char* noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            g.setColour (juce::Colour (0xFF2A2A35).withAlpha (opacity * (isExcluded ? 0.4f : 0.7f)));
            g.setFont (9.0f);
            g.drawText (noteNames[noteIdx], keyBounds.removeFromBottom (14.0f), juce::Justification::centred);
        }

        // ===== FILL GAPS BEHIND BLACK KEYS (so white key gaps don't show through) =====
        g.setColour (juce::Colour (0xFF1A1A1A));  // Dark fill to cover white key gaps
        for (int i = 0; i < 5; ++i)
        {
            int whiteKeyPos = blackKeyPositions[i];
            float whiteKeyX = keyboardX + whiteKeyPos * whiteKeyWidth;
            float x = whiteKeyX + whiteKeyWidth - blackKeyWidth / 2.0f - 2.0f;
            g.fillRect (x, keyboardY, blackKeyWidth + 4.0f, blackKeyHeight);
        }

        // ===== DRAW BLACK KEYS ON TOP =====
        for (int i = 0; i < 5; ++i)
        {
            int noteIdx = blackKeyNotes[i];
            int whiteKeyPos = blackKeyPositions[i];

            // Position black key between white keys
            float whiteKeyX = keyboardX + whiteKeyPos * whiteKeyWidth;
            float x = whiteKeyX + whiteKeyWidth - blackKeyWidth / 2.0f;
            float y = keyboardY;
            float w = blackKeyWidth;
            float h = blackKeyHeight;

            juce::Rectangle<float> keyBounds (x, y, w, h);
            blackKeyRects[(size_t) i] = keyBounds;

            bool inScale = scaleNotes[(size_t) noteIdx];
            bool inChord = chordNotes[(size_t) noteIdx];
            bool isExcluded = processor.getNoteExclusion (noteIdx);

            // Calculate opacity
            float opacity = 1.0f;
            if (scaleAmt > 0.01f && !inScale)
                opacity = 0.3f + 0.7f * (1.0f - scaleAmt);

            // Draw black key with 3D effect
            drawBlackKey (g, keyBounds, isExcluded, inChord, chordAmt, opacity);
        }

        // ===== MIDI KEYS TOGGLE BUTTON (top-right of keyboard) =====
        bool midiKeysOn = processor.isMidiBandControlActive();
        float toggleW = 62.0f;
        float toggleH = 16.0f;
        float toggleX = bounds.getRight() - toggleW - 4.0f;
        float toggleY = bounds.getY() + 4.0f;
        midiBandControlBounds = juce::Rectangle<float> (toggleX, toggleY, toggleW, toggleH);

        // Button background
        g.setColour (midiKeysOn ? juce::Colour (0xFFFF6600) : juce::Colour (0xFF2A2A35));
        g.fillRoundedRectangle (midiBandControlBounds, 3.0f);

        // Button border
        g.setColour (midiKeysOn ? juce::Colour (0xFFFF8833) : juce::Colour (0xFF4A4A55));
        g.drawRoundedRectangle (midiBandControlBounds, 3.0f, 1.0f);

        // Button text
        g.setColour (midiKeysOn ? juce::Colours::white : juce::Colour (0xFFAAAAAA));
        g.setFont (8.0f);
        g.drawText (midiKeysOn ? "MIDI CH16" : "MIDI KEYS", midiBandControlBounds, juce::Justification::centred);

        // When active, draw a subtle overlay + debug MIDI info
        if (midiKeysOn)
        {
            g.setColour (juce::Colour (0x30FF6600));
            g.fillRoundedRectangle (bounds.reduced (1.0f), 4.0f);

            // Debug: show last MIDI note received + channel + count
            int lastNote = processor.dbgLastMidiNote.load();
            int lastCh = processor.dbgLastMidiChannel.load();
            int count = processor.dbgMidiNoteCount.load();
            juce::String dbgText = (lastNote >= 0)
                ? "N:" + juce::String (lastNote) + " CH:" + juce::String (lastCh) + " #" + juce::String (count)
                : "No MIDI received";
            g.setColour (juce::Colours::yellow);
            g.setFont (10.0f);
            g.drawText (dbgText, bounds.getX() + 4, bounds.getY() + 4, 200, 14, juce::Justification::centredLeft);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Check MIDI KEYS toggle button first
        if (midiBandControlBounds.contains (e.position))
        {
            auto* param = processor.apvts.getParameter ("midiBandControl");
            if (param)
            {
                float currentValue = param->getValue();
                param->setValueNotifyingHost (currentValue > 0.5f ? 0.0f : 1.0f);
            }
            repaint();
            return;
        }

        // When MIDI Band Control is active, block manual key toggling
        if (processor.isMidiBandControlActive())
            return;

        auto* scaleAmtParam = processor.apvts.getRawParameterValue ("scaleAmount");
        float scaleAmt = scaleAmtParam ? scaleAmtParam->load() : 0.0f;
        auto scaleNotes = processor.getScaleNoteFlags();

        const int blackKeyNotes[5] = { 1, 3, 6, 8, 10 };
        const int whiteKeyNotes[7] = { 0, 2, 4, 5, 7, 9, 11 };

        // Check black keys FIRST (they're on top)
        for (int i = 0; i < 5; ++i)
        {
            if (blackKeyRects[(size_t) i].contains (e.position))
            {
                int noteIdx = blackKeyNotes[i];
                processor.toggleNoteExclusion (noteIdx);
                repaint();
                return;
            }
        }

        // Then check white keys
        for (int i = 0; i < 7; ++i)
        {
            if (whiteKeyRects[(size_t) i].contains (e.position))
            {
                int noteIdx = whiteKeyNotes[i];
                processor.toggleNoteExclusion (noteIdx);
                repaint();
                return;
            }
        }
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        // Block double-click reset when MIDI Band Control is active
        if (processor.isMidiBandControlActive())
            return;

        for (int i = 0; i < 12; ++i)
            processor.setNoteExclusion (i, false);
        repaint();
    }

    // IMPORTANT: Consume all mouse events to prevent propagation to other components
    // This fixes the bug where clicking keyboard would affect Mix slider
    void mouseDrag (const juce::MouseEvent&) override
    {
        // Intentionally empty - just consume the event
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        // Intentionally empty - just consume the event
    }

private:
    void timerCallback() override { repaint(); }

    void drawWhiteKey (juce::Graphics& g, juce::Rectangle<float> bounds, bool excluded,
                       bool inChord, float chordAmt, float opacity)
    {
        // Excluded state: key appears pushed down slightly
        auto keyBounds = excluded ? bounds.translated (0.0f, 2.0f).withTrimmedBottom (2.0f) : bounds;

        // 3D white key gradient - BRIGHT RED if excluded (more obvious)
        juce::Colour topCol, bottomCol;
        if (excluded)
        {
            topCol = juce::Colour (0xFFFF6666);     // Bright red
            bottomCol = juce::Colour (0xFFCC4444); // Darker red
        }
        else
        {
            topCol = juce::Colour (0xFFE8E8E8);
            bottomCol = juce::Colour (0xFFC8C8C8);
        }

        // Apply opacity for out-of-scale
        topCol = topCol.withMultipliedAlpha (opacity);
        bottomCol = bottomCol.withMultipliedAlpha (opacity);

        // Main key body with gradient
        juce::ColourGradient gradient (topCol, keyBounds.getX(), keyBounds.getY(),
                                        bottomCol, keyBounds.getX(), keyBounds.getBottom(), false);
        g.setGradientFill (gradient);
        g.fillRoundedRectangle (keyBounds, 2.0f);

        // Subtle shadow at bottom for depth
        g.setColour (juce::Colour (excluded ? 0xFF804040 : 0xFF888888).withMultipliedAlpha (opacity * 0.5f));
        g.fillRect (keyBounds.getX() + 1.0f, keyBounds.getBottom() - 3.0f,
                    keyBounds.getWidth() - 2.0f, 2.0f);

        // Border - red tint if excluded
        g.setColour (juce::Colour (excluded ? 0xFF8B4444 : 0xFF707070).withMultipliedAlpha (opacity));
        g.drawRoundedRectangle (keyBounds, 2.0f, 1.0f);

        // Chord tone glow (cyan) - only if not excluded
        if (inChord && chordAmt > 0.01f && !excluded)
        {
            g.setColour (juce::Colour (0xFF64D2FF).withAlpha (0.4f * chordAmt * opacity));
            g.fillRoundedRectangle (keyBounds.reduced (2.0f), 2.0f);

            g.setColour (juce::Colour (0xFF64D2FF).withAlpha (0.7f * chordAmt * opacity));
            g.drawRoundedRectangle (keyBounds.reduced (1.0f), 2.0f, 1.5f);
        }
    }

    void drawBlackKey (juce::Graphics& g, juce::Rectangle<float> bounds, bool excluded,
                       bool inChord, float chordAmt, float opacity)
    {
        // Excluded state: key appears pushed down more noticeably
        auto keyBounds = excluded ? bounds.translated (0.0f, 2.0f).withTrimmedBottom (2.0f) : bounds;

        if (excluded)
        {
            // === EXCLUDED: bright red glow + bright red body (same brightness as white keys) ===

            // Outer glow (red halo behind the key)
            g.setColour (juce::Colour (0xFFFF3333).withAlpha (0.6f * opacity));
            g.fillRoundedRectangle (keyBounds.expanded (3.0f), 4.0f);

            // Key body - same bright red as excluded white keys
            juce::ColourGradient gradient (
                juce::Colour (0xFFFF6666).withMultipliedAlpha (opacity), keyBounds.getX(), keyBounds.getY(),
                juce::Colour (0xFFCC4444).withMultipliedAlpha (opacity), keyBounds.getX(), keyBounds.getBottom(), false);
            g.setGradientFill (gradient);
            g.fillRoundedRectangle (keyBounds, 2.0f);

            // Top shine tinted pink
            g.setColour (juce::Colour (0xFFFFAAAA).withMultipliedAlpha (opacity * 0.7f));
            g.fillRect (keyBounds.getX() + 2.0f, keyBounds.getY() + 1.0f,
                        keyBounds.getWidth() - 4.0f, 2.0f);

            // Bright red border (thick)
            g.setColour (juce::Colour (0xFFFF5555).withMultipliedAlpha (opacity));
            g.drawRoundedRectangle (keyBounds, 2.0f, 2.0f);
        }
        else
        {
            // === NORMAL: standard dark black key with 3D effect ===

            // Shadow beneath black key (gives depth)
            g.setColour (juce::Colour (0xFF000000).withAlpha (0.4f * opacity));
            g.fillRoundedRectangle (keyBounds.translated (1.0f, 2.0f), 2.0f);

            // Key body gradient
            juce::ColourGradient gradient (
                juce::Colour (0xFF3A3A3A).withMultipliedAlpha (opacity), keyBounds.getX(), keyBounds.getY(),
                juce::Colour (0xFF1A1A1A).withMultipliedAlpha (opacity), keyBounds.getX(), keyBounds.getBottom(), false);
            g.setGradientFill (gradient);
            g.fillRoundedRectangle (keyBounds, 2.0f);

            // Top highlight edge (3D shine)
            g.setColour (juce::Colour (0xFF5A5A5A).withMultipliedAlpha (opacity * 0.6f));
            g.fillRect (keyBounds.getX() + 2.0f, keyBounds.getY() + 1.0f,
                        keyBounds.getWidth() - 4.0f, 2.0f);

            // Dark border
            g.setColour (juce::Colour (0xFF000000).withMultipliedAlpha (opacity));
            g.drawRoundedRectangle (keyBounds, 2.0f, 1.0f);

            // Chord tone glow (cyan)
            if (inChord && chordAmt > 0.01f)
            {
                g.setColour (juce::Colour (0xFF64D2FF).withAlpha (0.35f * chordAmt * opacity));
                g.fillRoundedRectangle (keyBounds.reduced (2.0f), 2.0f);

                g.setColour (juce::Colour (0xFF64D2FF).withAlpha (0.6f * chordAmt * opacity));
                g.drawRoundedRectangle (keyBounds.reduced (1.0f), 2.0f, 1.5f);
            }
        }
    }

    ColorVocoderAudioProcessor& processor;
    std::array<juce::Rectangle<float>, 7> whiteKeyRects;
    std::array<juce::Rectangle<float>, 5> blackKeyRects;
    mutable juce::Rectangle<float> midiBandControlBounds;  // MIDI KEYS toggle button
};

//==============================================================================
// CustomChordEditor – Edit mode overlay for defining custom chords per key
// Shows clickable QWERTY trigger grid + 3-octave clickable piano
//==============================================================================
class CustomChordEditor : public juce::Component,
                          private juce::Timer
{
public:
    CustomChordEditor (ColorVocoderAudioProcessor& p)
        : processor (p)
    {
        setInterceptsMouseClicks (true, true);
        setWantsKeyboardFocus (false);

        learnButton.setButtonText ("LEARN");
        learnButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2A35));
        learnButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        learnButton.onClick = [this]() { toggleMidiLearn(); };
        addAndMakeVisible (learnButton);

        clearButton.setButtonText ("CLEAR");
        clearButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2A35));
        clearButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        clearButton.onClick = [this]()
        {
            if (selectedKey > 0)
            {
                processor.clearCustomChord (selectedKey);
                selectedNotes.clear();
                repaint();
            }
        };
        addAndMakeVisible (clearButton);
    }

    ~CustomChordEditor() override { stopTimer(); }

    void visibilityChanged() override
    {
        if (isVisible() && !isTimerRunning())
            startTimerHz (15);
        else if (!isVisible())
            stopTimer();
    }

    // The 21 letter keys: 3 rows × 7 columns
    static constexpr int NUM_ROWS = 3;
    static constexpr int NUM_COLS = 7;
    static constexpr char keyLetters[3][7] = {
        { 'Q', 'W', 'E', 'R', 'T', 'Y', 'U' },
        { 'A', 'S', 'D', 'F', 'G', 'H', 'J' },
        { 'Z', 'X', 'C', 'V', 'B', 'N', 'M' }
    };
    static constexpr const char* degreeLabels[7] = { "I", "ii", "iii", "IV", "V", "vi", "vii" };
    static constexpr const char* rowLabels[3] = { "Oct+1", "Oct 0", "Oct-1" };

    // Piano range: C2 (MIDI 36) to B4 (MIDI 71) = 3 octaves
    static constexpr int PIANO_LOW = 36;   // C2
    static constexpr int PIANO_HIGH = 71;  // B4
    static constexpr int NUM_WHITE_KEYS = 22;  // 3 octaves × 7 + 1

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Dark background
        g.setColour (juce::Colour (0xFF0D0D10));
        g.fillRoundedRectangle (bounds, 4.0f);

        // ===== TITLE =====
        g.setColour (juce::Colour (0xFF64D2FF));
        g.setFont (juce::Font (10.0f, juce::Font::bold));
        g.drawText ("CUSTOM CHORD EDITOR — Click a key, then click piano notes",
                     bounds.removeFromTop (16.0f).reduced (8.0f, 0), juce::Justification::centredLeft);

        // ===== QWERTY TRIGGER GRID =====
        float gridY = 18.0f;
        float gridH = 70.0f;
        float cellW = (bounds.getWidth() - 60.0f) / NUM_COLS;
        float cellH = gridH / NUM_ROWS;
        float gridX = 50.0f;  // Leave room for row labels

        for (int row = 0; row < NUM_ROWS; ++row)
        {
            // Row label
            g.setColour (juce::Colour (0xFF888888));
            g.setFont (8.0f);
            g.drawText (rowLabels[row], 2.0f, gridY + row * cellH, 46.0f, cellH, juce::Justification::centredRight);

            for (int col = 0; col < NUM_COLS; ++col)
            {
                char letter = keyLetters[row][col];
                float cx = gridX + col * cellW;
                float cy = gridY + row * cellH;
                juce::Rectangle<float> cellBounds (cx + 1.0f, cy + 1.0f, cellW - 2.0f, cellH - 2.0f);

                triggerKeyRects[row][col] = cellBounds;

                bool isSelected = (selectedKey == letter);
                bool hasCustom = processor.hasCustomChord (letter);

                // Cell background
                if (isSelected)
                    g.setColour (juce::Colour (0xFF2266AA));
                else if (hasCustom)
                    g.setColour (juce::Colour (0xFF1A2A40));
                else
                    g.setColour (juce::Colour (0xFF1A1A22));

                g.fillRoundedRectangle (cellBounds, 3.0f);

                // Cell border
                if (isSelected)
                    g.setColour (juce::Colour (0xFF64D2FF));
                else
                    g.setColour (juce::Colour (0xFF333344));
                g.drawRoundedRectangle (cellBounds, 3.0f, 1.0f);

                // Text: letter + degree
                g.setFont (juce::Font (9.0f, juce::Font::bold));
                g.setColour (isSelected ? juce::Colours::white : juce::Colour (0xFFCCCCCC));
                juce::String label = juce::String::charToString (letter) + ":" + degreeLabels[col];
                if (hasCustom) label += juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x8f"));  // ● dot
                g.drawText (label, cellBounds, juce::Justification::centred);
            }
        }

        // ===== SELECTED KEY INFO =====
        float infoY = gridY + gridH + 2.0f;
        g.setColour (juce::Colour (0xFFAABBCC));
        g.setFont (9.0f);
        if (selectedKey > 0)
        {
            juce::String infoStr = "Selected: " + juce::String::charToString ((juce::juce_wchar) selectedKey) + " — ";
            if (selectedNotes.empty())
                infoStr += "(no notes)";
            else
            {
                const char* noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
                for (size_t i = 0; i < selectedNotes.size(); ++i)
                {
                    if (i > 0) infoStr += " ";
                    int note = selectedNotes[i];
                    infoStr += juce::String (noteNames[note % 12]) + juce::String (note / 12 - 1);
                }
            }
            g.drawText (infoStr, 8.0f, infoY, bounds.getWidth() - 16.0f, 14.0f, juce::Justification::centredLeft);
        }

        // ===== 3-OCTAVE CLICKABLE PIANO =====
        float pianoY = infoY + 16.0f;
        float pianoH = getHeight() - pianoY - 28.0f;  // Leave room for buttons
        float pianoX = 8.0f;
        float pianoW = getWidth() - 16.0f;

        drawClickablePiano (g, pianoX, pianoY, pianoW, pianoH);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Check QWERTY grid
        for (int row = 0; row < NUM_ROWS; ++row)
        {
            for (int col = 0; col < NUM_COLS; ++col)
            {
                if (triggerKeyRects[row][col].contains (e.position))
                {
                    selectedKey = keyLetters[row][col];
                    selectedNotes = processor.getCustomChord (selectedKey);
                    processor.lastTriggeredLetterKey.store (selectedKey);
                    repaint();
                    return;
                }
            }
        }

        // Check piano keys — black keys first (on top)
        for (auto& [note, rect] : blackPianoRects)
        {
            if (rect.contains (e.position))
            {
                togglePianoNote (note);
                return;
            }
        }
        for (auto& [note, rect] : whitePianoRects)
        {
            if (rect.contains (e.position))
            {
                togglePianoNote (note);
                return;
            }
        }
    }

    void resized() override
    {
        int bw = 60, bh = 20;
        int by = getHeight() - bh - 4;
        learnButton.setBounds (8, by, bw, bh);
        clearButton.setBounds (8 + bw + 8, by, bw, bh);
    }

    int getSelectedKey() const { return selectedKey; }

private:
    void timerCallback() override
    {
        // Update learn button appearance
        bool learning = processor.midiLearnActive.load();
        if (learning != learnButton.getToggleState())
        {
            learnButton.setToggleState (learning, juce::dontSendNotification);
            learnButton.setColour (juce::TextButton::buttonColourId,
                                    learning ? juce::Colour (0xFFFF6633) : juce::Colour (0xFF2A2A35));

            // If learn just completed, reload notes for selected key
            if (!learning && selectedKey > 0)
                selectedNotes = processor.getCustomChord (selectedKey);
        }
        repaint();
    }

    void togglePianoNote (int midiNote)
    {
        if (selectedKey <= 0) return;

        auto it = std::find (selectedNotes.begin(), selectedNotes.end(), midiNote);
        if (it != selectedNotes.end())
            selectedNotes.erase (it);
        else
            selectedNotes.push_back (midiNote);

        // Sort notes
        std::sort (selectedNotes.begin(), selectedNotes.end());

        processor.setCustomChord (selectedKey, selectedNotes);
        repaint();
    }

    void toggleMidiLearn()
    {
        if (selectedKey <= 0) return;

        bool wasLearning = processor.midiLearnActive.load();
        if (wasLearning)
        {
            processor.midiLearnActive.store (false);
        }
        else
        {
            processor.midiLearnTargetKey.store (selectedKey);
            processor.midiLearnActive.store (true);
        }
    }

    void drawClickablePiano (juce::Graphics& g, float x, float y, float w, float h)
    {
        whitePianoRects.clear();
        blackPianoRects.clear();

        // Count white keys in range
        int whiteCount = 0;
        for (int n = PIANO_LOW; n <= PIANO_HIGH; ++n)
        {
            int pc = n % 12;
            if (pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7 || pc == 9 || pc == 11)
                whiteCount++;
        }

        float whiteKeyW = w / (float) whiteCount;
        float blackKeyW = whiteKeyW * 0.6f;
        float blackKeyH = h * 0.6f;

        // Get scale notes for tinting
        auto scaleNotes = processor.getScaleNoteFlags();

        // Draw white keys
        int whiteIdx = 0;
        for (int n = PIANO_LOW; n <= PIANO_HIGH; ++n)
        {
            int pc = n % 12;
            bool isWhite = (pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7 || pc == 9 || pc == 11);
            if (!isWhite) continue;

            float kx = x + whiteIdx * whiteKeyW;
            juce::Rectangle<float> keyRect (kx, y, whiteKeyW - 1.0f, h);
            whitePianoRects.push_back ({ n, keyRect });

            bool isSelected = std::find (selectedNotes.begin(), selectedNotes.end(), n) != selectedNotes.end();
            bool inScale = scaleNotes[(size_t) pc];

            // Key color
            if (isSelected)
                g.setColour (juce::Colour (0xFF64D2FF));
            else if (inScale)
                g.setColour (juce::Colour (0xFFD8D8E0));
            else
                g.setColour (juce::Colour (0xFFE8E8E8));

            g.fillRoundedRectangle (keyRect, 1.5f);

            // Border
            g.setColour (juce::Colour (0xFF606060));
            g.drawRoundedRectangle (keyRect, 1.5f, 0.5f);

            // Note name at bottom for C notes
            if (pc == 0)
            {
                g.setColour (juce::Colour (0xFF333333));
                g.setFont (8.0f);
                g.drawText ("C" + juce::String (n / 12 - 1), keyRect.removeFromBottom (12.0f), juce::Justification::centred);
            }

            whiteIdx++;
        }

        // Draw black keys on top
        whiteIdx = 0;
        for (int n = PIANO_LOW; n <= PIANO_HIGH; ++n)
        {
            int pc = n % 12;
            bool isWhite = (pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7 || pc == 9 || pc == 11);

            if (isWhite)
            {
                // Check if next note is a black key
                if (n + 1 <= PIANO_HIGH)
                {
                    int nextPc = (n + 1) % 12;
                    bool nextIsBlack = (nextPc == 1 || nextPc == 3 || nextPc == 6 || nextPc == 8 || nextPc == 10);
                    if (nextIsBlack)
                    {
                        float kx = x + whiteIdx * whiteKeyW + whiteKeyW - blackKeyW / 2.0f;
                        juce::Rectangle<float> keyRect (kx, y, blackKeyW, blackKeyH);
                        blackPianoRects.push_back ({ n + 1, keyRect });

                        bool isSelected = std::find (selectedNotes.begin(), selectedNotes.end(), n + 1) != selectedNotes.end();
                        bool inScale = scaleNotes[(size_t) nextPc];

                        if (isSelected)
                            g.setColour (juce::Colour (0xFF3399DD));
                        else if (inScale)
                            g.setColour (juce::Colour (0xFF2A2A3A));
                        else
                            g.setColour (juce::Colour (0xFF1A1A1A));

                        g.fillRoundedRectangle (keyRect, 1.5f);

                        g.setColour (juce::Colour (0xFF000000));
                        g.drawRoundedRectangle (keyRect, 1.5f, 0.5f);
                    }
                }
                whiteIdx++;
            }
        }
    }

    ColorVocoderAudioProcessor& processor;
    int selectedKey = 0;  // 0 = none, else uppercase char
    std::vector<int> selectedNotes;  // MIDI notes for the selected key

    juce::Rectangle<float> triggerKeyRects[NUM_ROWS][NUM_COLS];
    std::vector<std::pair<int, juce::Rectangle<float>>> whitePianoRects;
    std::vector<std::pair<int, juce::Rectangle<float>>> blackPianoRects;

    juce::TextButton learnButton, clearButton;
};

//==============================================================================
// InputFilterDisplay – EQ display with spectrum analyzer (Tone Sculpt section)
// This is the complex EQ visualization component - kept from original
//==============================================================================
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
          bell1EnabledParam (apvts.getParameter ("bell1Enabled")),
          bell1FreqParam (apvts.getParameter ("bell1Freq")),
          bell1GainParam (apvts.getParameter ("bell1Gain")),
          bell1QParam (apvts.getParameter ("bell1Q")),
          bell2EnabledParam (apvts.getParameter ("bell2Enabled")),
          bell2FreqParam (apvts.getParameter ("bell2Freq")),
          bell2GainParam (apvts.getParameter ("bell2Gain")),
          bell2QParam (apvts.getParameter ("bell2Q")),
          hpQParam (apvts.getParameter ("inputHPQ")),
          lpQParam (apvts.getParameter ("inputLPQ")),
          hpQWidthParam (apvts.getParameter ("inputHPQWidth")),
          lpQWidthParam (apvts.getParameter ("inputLPQWidth")),
          complementParam (apvts.getParameter ("filterComplement")),
          compBell1EnabledParam (apvts.getParameter ("compBell1Enabled")),
          compBell1FreqParam (apvts.getParameter ("compBell1Freq")),
          compBell1GainParam (apvts.getParameter ("compBell1Gain")),
          compBell1QParam (apvts.getParameter ("compBell1Q")),
          compBell2EnabledParam (apvts.getParameter ("compBell2Enabled")),
          compBell2FreqParam (apvts.getParameter ("compBell2Freq")),
          compBell2GainParam (apvts.getParameter ("compBell2Gain")),
          compBell2QParam (apvts.getParameter ("compBell2Q")),
          subComplementLevelParam (apvts.getParameter ("subComplementLevel")),
          highComplementLevelParam (apvts.getParameter ("highComplementLevel"))
    {
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

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    int getSelectedBand() const { return selectedBand; }
    std::function<void(int)> onBandSelected;

private:
    void timerCallback() override { repaint(); }

    void paintInternal (juce::Graphics& g);
    void drawToggleButton (juce::Graphics& g, juce::Rectangle<float> area, bool enabled) const;
    void drawComplementButton (juce::Graphics& g, juce::Rectangle<float> bounds, bool complementOn, bool filterEnabled) const;
    void drawGrid (juce::Graphics& g, juce::Rectangle<float> area, bool enabled) const;
    void drawSpectrum (juce::Graphics& g, juce::Rectangle<float> area, float alpha) const;
    void drawComplementZone (juce::Graphics& g, juce::Rectangle<float> area, float alpha) const;
    void drawNode (juce::Graphics& g, float x, float y, juce::Colour colour, bool active,
                   const juce::String& freqText, const juce::String& slopeText, bool isHP,
                   juce::Rectangle<float> plotArea) const;
    void drawBellNode (juce::Graphics& g, float x, float y, juce::Colour colour, bool active,
                       const juce::String& freqText, const juce::String& gainText,
                       juce::Rectangle<float> plotArea, const juce::String& label) const;

    juce::Path createResponsePath (juce::Rectangle<float> area, float hpFreq, float lpFreq,
                                   int hpSlope, int lpSlope, float hpYOff, float lpYOff) const;
    juce::Path createComplementCurve (juce::Rectangle<float> area, float hpFreq, float lpFreq,
                                      int hpSlope, int lpSlope,
                                      float hpGainDb, float lpGainDb, float hpQWidth, float lpQWidth) const;
    juce::Path createDryInverseCurve (juce::Rectangle<float> area, float hpFreq, float lpFreq,
                                      int hpSlope, int lpSlope,
                                      float hpGainDb, float lpGainDb, float hpQWidth, float lpQWidth) const;

    float freqToX (float freq, juce::Rectangle<float> area) const;
    float xToFreq (float x, juce::Rectangle<float> area) const;
    int getFilterOrder (int slopeIdx) const;
    juce::String getSlopeLabel (int idx) const;
    juce::String formatFreq (float f) const;
    juce::String formatGain (float g) const;
    static juce::String freqToNoteName (float freq);

    ColorVocoderAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvtsRef;

    // Parameters
    juce::RangedAudioParameter* hpFreqParam = nullptr;
    juce::RangedAudioParameter* lpFreqParam = nullptr;
    juce::RangedAudioParameter* hpSlopeParam = nullptr;
    juce::RangedAudioParameter* lpSlopeParam = nullptr;
    juce::RangedAudioParameter* enabledParam = nullptr;
    juce::RangedAudioParameter* bell1EnabledParam = nullptr;
    juce::RangedAudioParameter* bell1FreqParam = nullptr;
    juce::RangedAudioParameter* bell1GainParam = nullptr;
    juce::RangedAudioParameter* bell1QParam = nullptr;
    juce::RangedAudioParameter* bell2EnabledParam = nullptr;
    juce::RangedAudioParameter* bell2FreqParam = nullptr;
    juce::RangedAudioParameter* bell2GainParam = nullptr;
    juce::RangedAudioParameter* bell2QParam = nullptr;
    juce::RangedAudioParameter* hpQParam = nullptr;
    juce::RangedAudioParameter* lpQParam = nullptr;
    juce::RangedAudioParameter* hpQWidthParam = nullptr;
    juce::RangedAudioParameter* lpQWidthParam = nullptr;
    juce::RangedAudioParameter* complementParam = nullptr;
    juce::RangedAudioParameter* compBell1EnabledParam = nullptr;
    juce::RangedAudioParameter* compBell1FreqParam = nullptr;
    juce::RangedAudioParameter* compBell1GainParam = nullptr;
    juce::RangedAudioParameter* compBell1QParam = nullptr;
    juce::RangedAudioParameter* compBell2EnabledParam = nullptr;
    juce::RangedAudioParameter* compBell2FreqParam = nullptr;
    juce::RangedAudioParameter* compBell2GainParam = nullptr;
    juce::RangedAudioParameter* compBell2QParam = nullptr;
    juce::RangedAudioParameter* subComplementLevelParam = nullptr;
    juce::RangedAudioParameter* highComplementLevelParam = nullptr;

    // Handle bounds
    mutable juce::Rectangle<float> hpHandleBounds, lpHandleBounds;
    mutable juce::Rectangle<float> bell1HandleBounds, bell2HandleBounds;
    mutable juce::Rectangle<float> compBell1HandleBounds, compBell2HandleBounds;
    mutable juce::Rectangle<float> complementButtonBounds { 0, 0, 0, 0 };

    // Dragging states
    bool draggingHP = false, draggingLP = false;
    bool draggingBell1 = false, draggingBell2 = false;
    bool draggingCompBell1 = false, draggingCompBell2 = false;
    int selectedBand = 1;
};

//==============================================================================
// VerticalFillBar - Simple vertical bar that fills 0-100% (drag to adjust)
//==============================================================================
class VerticalFillBar : public juce::Component
{
public:
    VerticalFillBar() { setWantsKeyboardFocus (false); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background (dark gray)
        g.setColour (juce::Colour (0xFF252530));
        g.fillRoundedRectangle (bounds, 2.0f);

        // Border
        g.setColour (juce::Colour (0xFF3A3A45));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 1.0f);

        // Fill from bottom (white/light)
        float fillH = value * (bounds.getHeight() - 2);
        if (fillH > 0)
        {
            auto fillRect = bounds.reduced (1).withTop (bounds.getBottom() - 1 - fillH);
            g.setColour (juce::Colour (0xFFCCCCCC));
            g.fillRoundedRectangle (fillRect, 1.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { updateValue (e); }
    void mouseDrag (const juce::MouseEvent& e) override { updateValue (e); }

    void setValue (float v) { value = juce::jlimit (0.0f, 1.0f, v); repaint(); }
    float getValue() const { return value; }

    std::function<void(float)> onValueChanged;

private:
    void updateValue (const juce::MouseEvent& e)
    {
        float newVal = 1.0f - (e.position.y / (float) getHeight());
        newVal = juce::jlimit (0.0f, 1.0f, newVal);
        if (std::abs (newVal - value) > 0.001f)
        {
            value = newVal;
            repaint();
            if (onValueChanged) onValueChanged (value);
        }
    }

    float value = 0.0f;
};

//==============================================================================
// LicensePanel - Popup panel for license activation
// Shows trial status, allows entering Gumroad license key
//==============================================================================
class LicensePanel : public juce::Component
{
public:
    LicensePanel (ColorVocoderAudioProcessor& p)
        : processor (p)
    {
        setWantsKeyboardFocus (true);
        // Status label
        addAndMakeVisible (statusLabel);
        statusLabel.setJustificationType (juce::Justification::centred);
        statusLabel.setFont (juce::Font (14.0f, juce::Font::bold));
        updateStatusLabel();

        // License key input
        addAndMakeVisible (keyLabel);
        keyLabel.setText ("License Key:", juce::dontSendNotification);
        keyLabel.setFont (juce::Font (12.0f));

        addAndMakeVisible (keyEditor);
        keyEditor.setMultiLine (false);
        keyEditor.setTextToShowWhenEmpty ("Paste your Gumroad license key...", juce::Colours::grey);
        keyEditor.setFont (juce::Font (12.0f));

        // Activate button
        addAndMakeVisible (activateButton);
        activateButton.onClick = [this]() { activateLicense(); };

        // Error/success message
        addAndMakeVisible (messageLabel);
        messageLabel.setJustificationType (juce::Justification::centred);
        messageLabel.setFont (juce::Font (11.0f));

        setSize (320, 160);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF1A1A1F));
        g.setColour (juce::Colour (0xFF3A3A45));
        g.drawRect (getLocalBounds(), 1);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (12);

        statusLabel.setBounds (bounds.removeFromTop (24));
        bounds.removeFromTop (8);

        keyLabel.setBounds (bounds.removeFromTop (16));
        bounds.removeFromTop (2);
        keyEditor.setBounds (bounds.removeFromTop (28));
        bounds.removeFromTop (8);

        activateButton.setBounds (bounds.removeFromTop (28).reduced (60, 0));
        bounds.removeFromTop (6);

        messageLabel.setBounds (bounds.removeFromTop (24));
    }

    void updateStatusLabel()
    {
        auto& lm = processor.getLicenseManager();

        if (lm.isLicensed())
        {
            statusLabel.setText ("Licensed - Thank you!", juce::dontSendNotification);
            statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF00FF7F));  // Green
            activateButton.setEnabled (false);
            keyEditor.setEnabled (false);
        }
        else if (lm.isTrialExpired())
        {
            statusLabel.setText ("Trial Expired - Please activate", juce::dontSendNotification);
            statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFFF4444));  // Red
        }
        else if (lm.isInTrialMode())
        {
            int days = lm.getTrialDaysRemaining();
            statusLabel.setText ("Trial: " + juce::String (days) + " day" + (days != 1 ? "s" : "") + " remaining",
                                 juce::dontSendNotification);
            statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFFFD700));  // Gold
        }
        else
        {
            statusLabel.setText ("Unlicensed", juce::dontSendNotification);
            statusLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFAAAAAA));
        }
    }

    // Callback when license is successfully activated
    std::function<void()> onActivationSuccess;

    // Callback to dismiss this panel (set by editor)
    std::function<void()> onDismiss;

    void focusLost (juce::Component::FocusChangeType) override
    {
        // Dismiss when clicking outside — but not if focus went to our own child (keyEditor)
        if (!hasKeyboardFocus (true) && onDismiss)
        {
            // Delay slightly so the click that caused focus loss can be processed
            juce::MessageManager::callAsync ([this]() {
                if (!hasKeyboardFocus (true) && onDismiss)
                    onDismiss();
            });
        }
    }

private:
    ColorVocoderAudioProcessor& processor;

    juce::Label statusLabel;
    juce::Label keyLabel;
    juce::TextEditor keyEditor;
    juce::TextButton activateButton { "Activate License" };
    juce::Label messageLabel;

    void activateLicense()
    {
        auto key = keyEditor.getText().trim();

        if (key.isEmpty())
        {
            messageLabel.setText ("Please paste your license key", juce::dontSendNotification);
            messageLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFFF6666));
            return;
        }

        messageLabel.setText ("Verifying with Gumroad...", juce::dontSendNotification);
        messageLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFAAAAAA));
        repaint();

        // Try to activate
        bool success = processor.getLicenseManager().tryActivate (key);

        if (success)
        {
            messageLabel.setText ("Activation successful!", juce::dontSendNotification);
            messageLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF00FF7F));
            updateStatusLabel();

            // Notify parent to refresh UI (unlock scales, microtuning, hide license button)
            if (onActivationSuccess)
                onActivationSuccess();
        }
        else
        {
            auto error = processor.getLicenseManager().getLastError();
            messageLabel.setText (error.isEmpty() ? "Activation failed" : error, juce::dontSendNotification);
            messageLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFFF6666));
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicensePanel)
};

//==============================================================================
// Helper functions to draw social media icons
//==============================================================================
namespace SocialIcons
{
    // Draw Instagram camera icon (simplified)
    inline void drawInstagram (juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour)
    {
        g.setColour (colour);
        auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto r = bounds.withSizeKeepingCentre (size, size);
        float inset = size * 0.15f;

        // Outer rounded rect
        g.drawRoundedRectangle (r.reduced (inset), size * 0.25f, size * 0.08f);

        // Inner circle (lens)
        auto center = r.getCentre();
        float lensRadius = size * 0.22f;
        g.drawEllipse (center.x - lensRadius, center.y - lensRadius,
                       lensRadius * 2, lensRadius * 2, size * 0.08f);

        // Flash dot (top right)
        float dotSize = size * 0.1f;
        g.fillEllipse (r.getRight() - inset - dotSize * 2.5f,
                       r.getY() + inset + dotSize * 0.5f,
                       dotSize, dotSize);
    }

    // Draw Discord controller icon (simplified)
    inline void drawDiscord (juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour colour)
    {
        g.setColour (colour);
        auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        auto r = bounds.withSizeKeepingCentre (size, size);

        juce::Path path;
        float cx = r.getCentreX();
        float cy = r.getCentreY();
        float w = size * 0.4f;
        float h = size * 0.3f;

        // Controller body shape
        path.startNewSubPath (cx - w, cy - h * 0.3f);
        path.quadraticTo (cx - w * 1.2f, cy - h * 1.2f, cx - w * 0.3f, cy - h);
        path.lineTo (cx + w * 0.3f, cy - h);
        path.quadraticTo (cx + w * 1.2f, cy - h * 1.2f, cx + w, cy - h * 0.3f);
        path.quadraticTo (cx + w * 1.1f, cy + h * 0.8f, cx + w * 0.5f, cy + h);
        path.lineTo (cx - w * 0.5f, cy + h);
        path.quadraticTo (cx - w * 1.1f, cy + h * 0.8f, cx - w, cy - h * 0.3f);
        path.closeSubPath();

        g.strokePath (path, juce::PathStrokeType (size * 0.08f));

        // Two eyes/dots
        float eyeSize = size * 0.12f;
        g.fillEllipse (cx - w * 0.45f - eyeSize / 2, cy - eyeSize / 2, eyeSize, eyeSize);
        g.fillEllipse (cx + w * 0.45f - eyeSize / 2, cy - eyeSize / 2, eyeSize, eyeSize);
    }
}

//==============================================================================
// AboutPanel - Comprehensive about dialog, click anywhere to dismiss
// Shows all features, social links, and license status
//==============================================================================
class AboutPanel : public juce::Component
{
public:
    AboutPanel (ColorVocoderAudioProcessor& p)
        : processor (p)
    {
        setWantsKeyboardFocus (true);
    }

    void paint (juce::Graphics& g) override
    {
        auto fullBounds = getLocalBounds();

        // ===== FULL BLACK BACKGROUND =====
        g.fillAll (juce::Colour (0xFF000000));

        // ===== SUBTLE BORDER FRAME =====
        g.setColour (juce::Colour (0xFF1A1A1A));
        g.drawRect (fullBounds.reduced (8), 1);

        auto bounds = fullBounds.reduced (40, 25);

        // ===== MAIN TITLE - LARGE AND BOLD =====
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (42.0f, juce::Font::bold));
        g.drawText ("BAND RITUAL", bounds.removeFromTop (50), juce::Justification::centred);

        // ===== THIN DECORATIVE LINE =====
        bounds.removeFromTop (5);
        g.setColour (juce::Colour (0xFF333333));
        auto lineArea = bounds.removeFromTop (1);
        g.fillRect (lineArea.reduced (80, 0));

        bounds.removeFromTop (8);

        // ===== TAGLINE - elegant italic =====
        g.setColour (juce::Colour (0xFF888888));
        g.setFont (juce::Font (15.0f, juce::Font::italic));
        g.drawText ("The world's first scale-locked vocoder", bounds.removeFromTop (24), juce::Justification::centred);

        bounds.removeFromTop (10);

        // ===== VERSION + LICENSE ROW =====
        auto statusRow = bounds.removeFromTop (26);
        auto& lm = processor.getLicenseManager();

        // Version badge on left-center
        auto badgeRect = statusRow.withSizeKeepingCentre (180, 24).toFloat();
        g.setColour (juce::Colour (0xFF111111));
        g.fillRoundedRectangle (badgeRect, 12.0f);
        g.setColour (juce::Colour (0xFF333333));
        g.drawRoundedRectangle (badgeRect, 12.0f, 1.0f);

        juce::String statusText = "V1.2";
        if (lm.isLicensed())
            statusText += "  |  Licensed";
        else if (lm.isInTrialMode() && !lm.isTrialExpired())
            statusText += "  |  Trial: " + juce::String (lm.getTrialDaysRemaining()) + " days";
        else if (lm.isTrialExpired())
            statusText += "  |  Trial Expired";

        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText (statusText, badgeRect, juce::Justification::centred);

        bounds.removeFromTop (12);

        // ===== DESCRIPTION - clean and centered =====
        g.setColour (juce::Colour (0xFFBBBBBB));
        g.setFont (12.5f);

        juce::String description =
            "A next-generation 100-band spectral vocoder built to transform\n"
            "basses, synths, vocals and noise into scale-locked harmonic shapes.";

        g.drawFittedText (description, bounds.removeFromTop (45), juce::Justification::centred, 2);

        bounds.removeFromTop (4);

        g.setColour (juce::Colour (0xFF666666));
        g.setFont (juce::Font (11.0f, juce::Font::italic));
        g.drawText ("Every band follows the scale. Zero detuning. Only harmony.",
                    bounds.removeFromTop (18), juce::Justification::centred);

        bounds.removeFromTop (10);

        // ===== FEATURES - 3 columns, minimal =====
        juce::StringArray features = {
            "100 Harmonic Bands", "15 Scale Types", "MIDI Follow",
            "6 Carrier Modes", "Microtuning Support", "Spectral Tilt Pad",
            "Complement EQ", "Smart Gate", "Real-Time Analyzer",
            "Per-Band Control", "Band Distribution", "Chord Voicing"
        };

        auto featureArea = bounds.removeFromTop (85);
        g.setFont (10.5f);

        int cols = 3;
        int rows = 4;
        int cellW = featureArea.getWidth() / cols;
        int cellH = featureArea.getHeight() / rows;

        for (int i = 0; i < features.size(); ++i)
        {
            int col = i % cols;
            int row = i / cols;
            auto cellRect = juce::Rectangle<int> (
                featureArea.getX() + col * cellW,
                featureArea.getY() + row * cellH,
                cellW, cellH);

            // Small dash instead of bullet
            g.setColour (juce::Colour (0xFF444444));
            g.fillRect ((float) cellRect.getX() + 8, (float) cellRect.getCentreY(), 6.0f, 1.0f);

            g.setColour (juce::Colour (0xFF999999));
            g.drawText (features[i], cellRect.withTrimmedLeft (20), juce::Justification::centredLeft);
        }

        bounds.removeFromTop (8);

        // ===== THIN LINE =====
        g.setColour (juce::Colour (0xFF222222));
        g.fillRect (bounds.removeFromTop (1).reduced (60, 0));

        bounds.removeFromTop (8);

        // ===== GENRES - subtle =====
        g.setColour (juce::Colour (0xFF555555));
        g.setFont (juce::Font (9.5f, juce::Font::italic));
        g.drawText ("color bass  |  future riddim  |  dubstep  |  hyperpop  |  ambient  |  trap",
                    bounds.removeFromTop (14), juce::Justification::centred);

        bounds.removeFromTop (6);

        // ===== SIGNATURE SECTION =====
        g.setColour (juce::Colour (0xFF666666));
        g.setFont (juce::Font (10.5f, juce::Font::italic));
        g.drawText ("\"Making vocoders go BRR since day one.\"", bounds.removeFromTop (16), juce::Justification::centred);

        bounds.removeFromTop (4);

        g.setColour (juce::Colour (0xFF888888));
        g.setFont (10.0f);
        g.drawText ("Enrico Quaglia  |  Ritual Audio Labs", bounds.removeFromTop (14), juce::Justification::centred);

        bounds.removeFromTop (10);

        // ===== SOCIAL ICONS - clean white =====
        int iconSize = 28;
        int spacing = 60;
        int totalW = iconSize * 2 + spacing;
        int startX = bounds.getCentreX() - totalW / 2;
        int iconY = bounds.getY();

        instagramBounds = juce::Rectangle<int> (startX, iconY, iconSize, iconSize);
        discordBounds = juce::Rectangle<int> (startX + iconSize + spacing, iconY, iconSize, iconSize);

        // Draw icons with hover-style circles behind
        g.setColour (juce::Colour (0xFF1A1A1A));
        g.fillEllipse (instagramBounds.toFloat().expanded (8));
        g.fillEllipse (discordBounds.toFloat().expanded (8));

        SocialIcons::drawInstagram (g, instagramBounds.toFloat(), juce::Colours::white);
        SocialIcons::drawDiscord (g, discordBounds.toFloat(), juce::Colours::white);

        // Labels below icons
        g.setFont (8.5f);
        g.setColour (juce::Colour (0xFF666666));
        g.drawText ("@ritualaudiolabs", instagramBounds.getCentreX() - 50, iconY + iconSize + 12, 100, 12,
                    juce::Justification::centred);
        g.drawText ("Join Discord", discordBounds.getCentreX() - 40, iconY + iconSize + 12, 80, 12,
                    juce::Justification::centred);

        // ===== COPYRIGHT - at very bottom =====
        auto bottomSection = fullBounds.removeFromBottom (30);

        // Copyright and dismiss
        g.setColour (juce::Colour (0xFF444444));
        g.setFont (8.5f);
        g.drawText ("(C) 2025 Ritual Audio Labs  |  Click anywhere to close",
                    bottomSection, juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Check if clicked on social icons
        if (instagramBounds.expanded (8).contains (e.getPosition()))
        {
            juce::URL ("https://www.instagram.com/ritualaudiolabs/").launchInDefaultBrowser();
            return;
        }
        if (discordBounds.expanded (8).contains (e.getPosition()))
        {
            juce::URL ("https://discord.gg/d4JQQPGa").launchInDefaultBrowser();
            return;
        }

        // Click anywhere else dismisses
        if (onDismiss)
            onDismiss();
    }

    std::function<void()> onDismiss;

private:
    ColorVocoderAudioProcessor& processor;
    juce::Rectangle<int> instagramBounds, discordBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutPanel)
};

//==============================================================================
// ThankYouPanel - Shown after successful activation or trial start
// User must scroll to bottom before continuing (EULA-style)
//==============================================================================
class ThankYouPanel : public juce::Component,
                      private juce::ScrollBar::Listener
{
public:
    //--------------------------------------------------------------------------
    // Inner class: SocialIconButton - draws icon + text label
    //--------------------------------------------------------------------------
    class SocialIconButton : public juce::Component
    {
    public:
        enum IconType { Instagram, Discord };

        SocialIconButton (IconType type, const juce::String& label, const juce::String& url)
            : iconType (type), buttonLabel (label), targetUrl (url)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();

            // Icon-only style - minimal circle/square
            juce::Colour iconColour (0xFF666666);

            if (isMouseOver())
                iconColour = juce::Colour (0xFFFFFFFF);

            // Just draw the icon centered, no background
            auto iconBounds = bounds.reduced (4.0f);

            if (iconType == Instagram)
                SocialIcons::drawInstagram (g, iconBounds, iconColour);
            else
                SocialIcons::drawDiscord (g, iconBounds, iconColour);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (getLocalBounds().contains (e.getPosition()))
                juce::URL (targetUrl).launchInDefaultBrowser();
        }

    private:
        IconType iconType;
        juce::String buttonLabel;
        juce::String targetUrl;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SocialIconButton)
    };

    //--------------------------------------------------------------------------
    ThankYouPanel (bool isLicensed)
        : wasLicensed (isLicensed),
          instagramButton (SocialIconButton::Instagram, "Instagram", "https://www.instagram.com/ritualaudiolabs/"),
          discordButton (SocialIconButton::Discord, "Discord", "https://discord.gg/d4JQQPGa")
    {
        // Title - clean white, not screaming
        addAndMakeVisible (titleLabel);
        titleLabel.setJustificationType (juce::Justification::centred);
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFFFFFFF));
        titleLabel.setFont (juce::Font (18.0f, juce::Font::bold));
        titleLabel.setText (wasLicensed ? "Thank You" : "Welcome", juce::dontSendNotification);

        // Version - subtle gray, not promotional
        addAndMakeVisible (versionLabel);
        versionLabel.setJustificationType (juce::Justification::centred);
        versionLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF666666));
        versionLabel.setFont (juce::Font (10.0f));
        versionLabel.setText ("v1.2", juce::dontSendNotification);

        // Scrollable message text
        addAndMakeVisible (messageViewport);
        messageViewport.setViewedComponent (&messageContent, false);
        messageViewport.setScrollBarsShown (true, false);
        messageViewport.getVerticalScrollBar().addListener (this);

        messageContent.setColour (juce::Label::textColourId, juce::Colour (0xFFDDDDDD));
        messageContent.setJustificationType (juce::Justification::topLeft);
        messageContent.setFont (juce::Font (13.0f));

        // Build message text
        juce::String message;
        if (wasLicensed)
        {
            message = "Thank you for supporting Ritual Audio Labs!\n\n"
                      "We're a small team of developers who are themselves producers, "
                      "engaged in modern sound design - producers for producers. "
                      "Your support means a lot to us and helps us continue building "
                      "creative tools for the music production community.\n\n"
                      "Feel free to follow us on social media and share any feedback "
                      "about the plugin on our Discord server - your feedback is super "
                      "valuable for us!\n\n"
                      "Become an official beta tester by joining our Discord and "
                      "requesting access to the beta tester role. You'll get early access "
                      "to new features and help shape the future of Band Ritual.\n\n"
                      "Thank you again for being part of the Ritual Audio Labs family!\n\n"
                      "- Enrico Quaglia\n"
                      "  Founder, Ritual Audio Labs";
        }
        else
        {
            // Psychology-optimized trial message:
            // - Identity reinforcement ("serious producers")
            // - Loss aversion ("everything unlocked" implies loss after)
            // - Reciprocity (we gave you full access)
            // - Social proof (producers worldwide)
            // - Soft urgency (7 days, no pressure)
            // - Clear value prop (unique selling points)
            message = "You just unlocked something special.\n\n"
                      "For the next 7 days, you have FULL access to Band Ritual - "
                      "the same vocoder and spectral toolkit used by producers worldwide "
                      "to create sounds that cut through any mix.\n\n"
                      "This isn't a limited demo. Everything is unlocked:\n"
                      "  - 32-band vocoder with real-time visualization\n"
                      "  - Spectral morphing and formant control\n"
                      "  - Pitch-class filtering for harmonic precision\n"
                      "  - Microtuning support for any scale\n\n"
                      "We built this because we needed it ourselves. As producers, "
                      "we were frustrated with vocoders that sounded dated or "
                      "lacked visual feedback. Band Ritual is our answer.\n\n"
                      "After your trial, you decide. No pressure, no spam.\n\n"
                      "Make something you're proud of.\n\n"
                      "- Enrico\n"
                      "  Ritual Audio Labs";
        }
        messageContent.setText (message, juce::dontSendNotification);

        // Instagram button with icon
        addAndMakeVisible (instagramButton);

        // Discord button with icon
        addAndMakeVisible (discordButton);

        // Continue button - minimal, text-only style
        addAndMakeVisible (continueButton);
        continueButton.setButtonText ("Continue");
        continueButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        continueButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFF2A2A2A));
        continueButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFF555555));
        continueButton.setColour (juce::TextButton::textColourOnId, juce::Colour (0xFFFFFFFF));
        continueButton.setEnabled (false);
        continueButton.onClick = [this]() {
            if (onContinue) onContinue();
        };

        // Scroll hint - very subtle
        addAndMakeVisible (scrollHintLabel);
        scrollHintLabel.setJustificationType (juce::Justification::centred);
        scrollHintLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF555555));
        scrollHintLabel.setFont (juce::Font (9.0f));
        scrollHintLabel.setText ("scroll to continue", juce::dontSendNotification);

        setSize (420, 400);
    }

    void paint (juce::Graphics& g) override
    {
        // Dark overlay
        g.fillAll (juce::Colour (0xF5000000));

        // Center panel - minimal, subtle border
        auto panelBounds = getLocalBounds().reduced (16);
        g.setColour (juce::Colour (0xFF0D0D0D));
        g.fillRoundedRectangle (panelBounds.toFloat(), 6.0f);
        g.setColour (juce::Colour (0xFF333333));
        g.drawRoundedRectangle (panelBounds.toFloat(), 6.0f, 1.0f);
    }

    void resized() override
    {
        auto panelBounds = getLocalBounds().reduced (16);
        auto bounds = panelBounds.reduced (16);

        // Title at top
        titleLabel.setBounds (bounds.removeFromTop (22));
        bounds.removeFromTop (2);
        versionLabel.setBounds (bounds.removeFromTop (12));
        bounds.removeFromTop (12);

        // Continue button at very bottom - just text, minimal
        auto bottomArea = bounds.removeFromBottom (20);
        continueButton.setBounds (bottomArea.reduced (140, 0));
        bounds.removeFromBottom (6);

        // Scroll hint
        scrollHintLabel.setBounds (bounds.removeFromBottom (12));
        bounds.removeFromBottom (6);

        // Social buttons - small icons at bottom, centered
        auto socialRow = bounds.removeFromBottom (26);
        int iconSize = 26;
        int totalWidth = iconSize * 2 + 12;  // 2 buttons + gap
        int startX = socialRow.getX() + (socialRow.getWidth() - totalWidth) / 2;
        instagramButton.setBounds (startX, socialRow.getY(), iconSize, iconSize);
        discordButton.setBounds (startX + iconSize + 12, socialRow.getY(), iconSize, iconSize);
        bounds.removeFromBottom (10);

        // Message viewport takes remaining space
        messageViewport.setBounds (bounds);

        // Size the message content to fit width, expand height for text
        int textWidth = bounds.getWidth() - 20;  // Account for scrollbar
        messageContent.setSize (textWidth, 400);  // Will be resized by text

        // Calculate actual text height needed
        juce::Font font (13.0f);
        auto textLayout = juce::TextLayout();
        juce::AttributedString attrStr;
        attrStr.append (messageContent.getText(), font, juce::Colour (0xFFDDDDDD));
        attrStr.setLineSpacing (1.2f);
        textLayout.createLayout (attrStr, (float) textWidth);
        int textHeight = (int) textLayout.getHeight() + 20;
        messageContent.setSize (textWidth, juce::jmax (textHeight, bounds.getHeight()));

        checkScrollPosition();
    }

    std::function<void()> onContinue;

private:
    bool wasLicensed;
    bool hasScrolledToBottom = false;

    juce::Label titleLabel;
    juce::Label versionLabel;
    juce::Viewport messageViewport;
    juce::Label messageContent;
    SocialIconButton instagramButton;
    SocialIconButton discordButton;
    juce::TextButton continueButton;
    juce::Label scrollHintLabel;

    void scrollBarMoved (juce::ScrollBar* scrollBar, double newRangeStart) override
    {
        juce::ignoreUnused (scrollBar, newRangeStart);
        checkScrollPosition();
    }

    void checkScrollPosition()
    {
        if (hasScrolledToBottom)
            return;

        auto& vbar = messageViewport.getVerticalScrollBar();
        float scrollPos = (float) vbar.getCurrentRangeStart();
        float scrollMax = (float) (vbar.getMaximumRangeLimit() - vbar.getCurrentRangeSize());

        // Check if scrolled to bottom (within 10px tolerance)
        if (scrollMax <= 0 || scrollPos >= scrollMax - 10.0f)
        {
            hasScrolledToBottom = true;
            continueButton.setEnabled (true);
            // Subtle enabled state - dark background with accent text
            continueButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2A35));
            continueButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
            scrollHintLabel.setVisible (false);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThankYouPanel)
};

//==============================================================================
// WelcomePanel - First launch / trial expired overlay
// Shows activation prompt with license key input or "Start Free Trial" option
//==============================================================================
class WelcomePanel : public juce::Component
{
public:
    // Custom LookAndFeel for rounded text editors
    struct RoundedTextEditorLookAndFeel : public juce::LookAndFeel_V4
    {
        void fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& te) override
        {
            auto bgColour = te.findColour (juce::TextEditor::backgroundColourId);
            g.setColour (bgColour);
            g.fillRoundedRectangle (0.0f, 0.0f, (float)w, (float)h, 6.0f);
        }

        void drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& te) override
        {
            auto outlineColour = te.hasKeyboardFocus (true)
                ? te.findColour (juce::TextEditor::focusedOutlineColourId)
                : te.findColour (juce::TextEditor::outlineColourId);
            g.setColour (outlineColour);
            g.drawRoundedRectangle (0.5f, 0.5f, (float)w - 1.0f, (float)h - 1.0f, 6.0f, 1.0f);
        }
    };

    WelcomePanel (ColorVocoderAudioProcessor& p)
        : processor (p)
    {
        auto& lm = processor.getLicenseManager();
        isExpiredMode = !lm.isFirstLaunch() && lm.isTrialExpired();

        // Load logo from binary data
        logoImage = juce::ImageCache::getFromMemory (BinaryData::ritual_logo_png, BinaryData::ritual_logo_pngSize);

        // Title
        addAndMakeVisible (titleLabel);
        titleLabel.setJustificationType (juce::Justification::centred);
        titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);

        // Subtitle
        addAndMakeVisible (subtitleLabel);
        subtitleLabel.setJustificationType (juce::Justification::centred);
        subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF888888));

        // License key input
        addAndMakeVisible (keyLabel);
        keyLabel.setText ("License Key:", juce::dontSendNotification);
        keyLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFCCCCCC));

        addAndMakeVisible (keyEditor);
        keyEditor.setMultiLine (false);
        keyEditor.setTextToShowWhenEmpty ("Paste your Gumroad license key...", juce::Colours::grey);
        keyEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF2A2A35));
        keyEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xFF3A3A45));
        keyEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xFF5A5A65));
        keyEditor.setColour (juce::TextEditor::textColourId, juce::Colours::white);
        keyEditor.setScrollbarsShown (false);
        keyEditor.setLookAndFeel (&roundedLF);
        keyEditor.onReturnKey = [this]() { activateLicense(); };

        // Activate button
        addAndMakeVisible (activateButton);
        activateButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A5A2A));
        activateButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFF00FF7F));
        activateButton.onClick = [this]() { activateLicense(); };

        // Trial code input (only in first launch mode)
        addAndMakeVisible (trialCodeLabel);
        trialCodeLabel.setText ("Trial Code:", juce::dontSendNotification);
        trialCodeLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFCCCCCC));
        trialCodeLabel.setVisible (!isExpiredMode);

        addAndMakeVisible (trialCodeEditor);
        trialCodeEditor.setMultiLine (false);
        trialCodeEditor.setTextToShowWhenEmpty ("Enter trial code (e.g. TRIAL-XXXX-XXXX-XXXX)", juce::Colours::grey);
        trialCodeEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF2A2A35));
        trialCodeEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xFF3A3A45));
        trialCodeEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xFF5A5A65));
        trialCodeEditor.setColour (juce::TextEditor::textColourId, juce::Colours::white);
        trialCodeEditor.setScrollbarsShown (false);
        trialCodeEditor.setLookAndFeel (&roundedLF);
        trialCodeEditor.onReturnKey = [this]() { startTrial(); };
        trialCodeEditor.setVisible (!isExpiredMode);

        // Start trial button (only in first launch mode)
        addAndMakeVisible (trialButton);
        trialButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF3A3A45));
        trialButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFFFD700));
        trialButton.onClick = [this]() { startTrial(); };
        trialButton.setVisible (!isExpiredMode);

        // Get trial code link (only in first launch mode)
        addAndMakeVisible (getTrialCodeButton);
        getTrialCodeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        getTrialCodeButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFFFD700));
        getTrialCodeButton.onClick = []() {
            juce::URL ("https://ritualaudiolabs.com/trial").launchInDefaultBrowser();
        };
        getTrialCodeButton.setVisible (!isExpiredMode);

        // Buy link
        addAndMakeVisible (buyButton);
        buyButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        buyButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFF64D2FF));
        buyButton.onClick = []() {
            juce::URL ("https://ritualaudiolabs.gumroad.com/l/band-ritual").launchInDefaultBrowser();
        };

        // Message label for errors/status
        addAndMakeVisible (messageLabel);
        messageLabel.setJustificationType (juce::Justification::centred);

        updateLabels();
        setSize (500, 320);  // Wider for inline buttons, shorter layout
    }

    void paint (juce::Graphics& g) override
    {
        // Semi-transparent dark overlay
        g.fillAll (juce::Colour (0xEE0A0A0F));

        // Center panel
        auto panelBounds = getLocalBounds().reduced (30);
        g.setColour (juce::Colour (0xFF1A1A1F));
        g.fillRoundedRectangle (panelBounds.toFloat(), 8.0f);

        // Draw logo as background (dimmed)
        if (logoImage.isValid())
        {
            // Calculate logo size - fit to panel height, centered
            float logoSize = (float) panelBounds.getHeight() * 0.85f;
            float logoX = panelBounds.getCentreX() - logoSize * 0.5f;
            float logoY = panelBounds.getCentreY() - logoSize * 0.5f;

            // Draw logo with low opacity (dimmed)
            g.setOpacity (0.12f);  // Very subtle, so text remains readable
            g.drawImage (logoImage,
                         logoX, logoY, logoSize, logoSize,
                         0, 0, logoImage.getWidth(), logoImage.getHeight());
            g.setOpacity (1.0f);
        }

        // Panel border
        g.setColour (juce::Colour (0xFF3A3A45));
        g.drawRoundedRectangle (panelBounds.toFloat(), 8.0f, 1.0f);
    }

    void resized() override
    {
        auto panelBounds = getLocalBounds().reduced (30);
        auto bounds = panelBounds.reduced (20);

        const int buttonWidth = 130;
        const int rowHeight = 26;  // Shorter input boxes
        const int spacing = 10;

        titleLabel.setBounds (bounds.removeFromTop (28));
        bounds.removeFromTop (6);
        subtitleLabel.setBounds (bounds.removeFromTop (18));
        bounds.removeFromTop (20);

        // License key row: [Label] then [Input | Button]
        keyLabel.setBounds (bounds.removeFromTop (18));
        bounds.removeFromTop (4);
        auto licenseRow = bounds.removeFromTop (rowHeight);
        activateButton.setBounds (licenseRow.removeFromRight (buttonWidth));
        licenseRow.removeFromRight (spacing);
        keyEditor.setBounds (licenseRow);
        bounds.removeFromTop (6);

        // Buy on Gumroad - right under license key
        buyButton.setBounds (bounds.removeFromTop (20));
        bounds.removeFromTop (20);

        if (!isExpiredMode)
        {
            // Trial code row: [Label] then [Input | Button]
            trialCodeLabel.setBounds (bounds.removeFromTop (18));
            bounds.removeFromTop (4);
            auto trialRow = bounds.removeFromTop (rowHeight);
            trialButton.setBounds (trialRow.removeFromRight (buttonWidth));
            trialRow.removeFromRight (spacing);
            trialCodeEditor.setBounds (trialRow);
            bounds.removeFromTop (6);

            // Get trial code link - right under trial code
            getTrialCodeButton.setBounds (bounds.removeFromTop (20));
            bounds.removeFromTop (12);
        }

        messageLabel.setBounds (bounds.removeFromTop (20));
    }

    // Callback when license activated or trial started
    std::function<void()> onComplete;

    ~WelcomePanel() override
    {
        keyEditor.setLookAndFeel (nullptr);
        trialCodeEditor.setLookAndFeel (nullptr);
    }

private:
    ColorVocoderAudioProcessor& processor;
    bool isExpiredMode = false;
    RoundedTextEditorLookAndFeel roundedLF;
    juce::Image logoImage;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label keyLabel;
    juce::TextEditor keyEditor;
    juce::TextButton activateButton { "Activate License" };
    juce::Label trialCodeLabel;
    juce::TextEditor trialCodeEditor;
    juce::TextButton trialButton { "Activate Trial" };
    juce::TextButton getTrialCodeButton { "Get Free Trial Code" };
    juce::TextButton buyButton { "Buy on Gumroad" };
    juce::Label messageLabel;

    // Thank you panel shown after successful activation/trial
    std::unique_ptr<ThankYouPanel> thankYouPanel;

    void updateLabels()
    {
        if (isExpiredMode)
        {
            titleLabel.setText ("Trial Expired", juce::dontSendNotification);
            titleLabel.setFont (juce::Font (20.0f, juce::Font::bold));
            titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xFFFF6666));
            subtitleLabel.setText ("Please activate to continue using Band Ritual", juce::dontSendNotification);
        }
        else
        {
            titleLabel.setText ("Welcome to BAND RITUAL", juce::dontSendNotification);
            titleLabel.setFont (juce::Font (20.0f, juce::Font::bold));
            subtitleLabel.setText ("Activate your license or start a free trial", juce::dontSendNotification);
        }
    }

    void activateLicense()
    {
        auto key = keyEditor.getText().trim();

        if (key.isEmpty())
        {
            showMessage ("Please enter your license key", true);
            return;
        }

        showMessage ("Verifying with Gumroad...", false);
        repaint();

        // Run activation
        bool success = processor.getLicenseManager().tryActivate (key);

        if (success)
        {
            // Show thank you panel instead of immediately completing
            showThankYouPanel (true);  // true = licensed
        }
        else
        {
            auto error = processor.getLicenseManager().getLastError();
            showMessage (error.isEmpty() ? "Activation failed" : error, true);
        }
    }

    void startTrial()
    {
        auto code = trialCodeEditor.getText().trim();

        if (code.isEmpty())
        {
            showMessage ("Please enter a trial code", true);
            return;
        }

        showMessage ("Validating trial code...", false);
        repaint();

        bool success = processor.getLicenseManager().startTrialWithCode (code);

        if (success)
        {
            // Show thank you panel instead of immediately completing
            showThankYouPanel (false);  // false = trial
        }
        else
        {
            auto error = processor.getLicenseManager().getLastError();
            showMessage (error.isEmpty() ? "Invalid trial code" : error, true);
        }
    }

    void showThankYouPanel (bool isLicensed)
    {
        // Hide all activation UI
        titleLabel.setVisible (false);
        subtitleLabel.setVisible (false);
        keyLabel.setVisible (false);
        keyEditor.setVisible (false);
        activateButton.setVisible (false);
        trialCodeLabel.setVisible (false);
        trialCodeEditor.setVisible (false);
        trialButton.setVisible (false);
        getTrialCodeButton.setVisible (false);
        buyButton.setVisible (false);
        messageLabel.setVisible (false);

        // Create and show thank you panel
        thankYouPanel = std::make_unique<ThankYouPanel> (isLicensed);
        addAndMakeVisible (*thankYouPanel);
        thankYouPanel->setBounds (getLocalBounds());
        thankYouPanel->onContinue = [this]() {
            if (onComplete) onComplete();
        };
    }

    void showMessage (const juce::String& msg, bool isError)
    {
        messageLabel.setText (msg, juce::dontSendNotification);
        messageLabel.setColour (juce::Label::textColourId,
            isError ? juce::Colour (0xFFFF6666) : juce::Colour (0xFFAAAAAA));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WelcomePanel)
};

//==============================================================================
// MicrotuningPanel - Popup panel for microtuning controls
// - Reference pitch (Serum-style draggable Hz display)
// - 12 cent offset sliders for each note (C, C#, D, etc.)
// - Load .scl/.tun file button
//==============================================================================
class MicrotuningPanel : public juce::Component
{
public:
    MicrotuningPanel (juce::AudioProcessorValueTreeState& apvts);

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Called by parent when loading a tuning file
    void loadTuningFile();

    // Trial mode callback - set by editor to check if microtuning import is allowed
    std::function<bool()> isLoadingAllowed = []() { return true; };

    // Update button appearance based on trial state (call after setting isLoadingAllowed)
    void updateTrialState();

    // Sliders must be public so attachments can be created from editor
    juce::Slider refPitchSlider;
    std::array<juce::Slider, 12> centSliders;

    // Attachments (also public for external creation)
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> refPitchAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 12> centAttachments;

private:
    juce::AudioProcessorValueTreeState& apvtsRef;

    // Labels
    juce::Label refPitchLabel { {}, "A4 =" };
    std::array<juce::Label, 12> noteLabels;
    static constexpr const char* noteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    // Buttons
    juce::TextButton loadButton { "Load .scl/.tun" };
    juce::TextButton resetButton { "Reset" };

    // Parse .scl (Scala) or .tun files
    bool parseScalaFile (const juce::File& file);
    bool parseTunFile (const juce::File& file);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicrotuningPanel)
};

//==============================================================================
// FavoritesManager - Global favorites storage (singleton)
//==============================================================================
class FavoritesManager
{
public:
    static FavoritesManager& getInstance()
    {
        static FavoritesManager instance;
        return instance;
    }

    bool isFavorite (const juce::String& category, const juce::String& item) const
    {
        auto it = favorites.find (category);
        if (it != favorites.end())
            return it->second.count (item) > 0;
        return false;
    }

    void setFavorite (const juce::String& category, const juce::String& item, bool fav)
    {
        if (fav)
            favorites[category].insert (item);
        else
            favorites[category].erase (item);
        save();
    }

    void toggleFavorite (const juce::String& category, const juce::String& item)
    {
        setFavorite (category, item, !isFavorite (category, item));
    }

    std::vector<juce::String> getSortedItems (const juce::String& category, const juce::StringArray& items) const
    {
        std::vector<juce::String> favs, nonFavs;
        for (const auto& item : items)
        {
            if (isFavorite (category, item))
                favs.push_back (item);
            else
                nonFavs.push_back (item);
        }
        favs.insert (favs.end(), nonFavs.begin(), nonFavs.end());
        return favs;
    }

private:
    FavoritesManager() { load(); }
    std::map<juce::String, std::set<juce::String>> favorites;

    juce::File getFile() const
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("BandRitual");
        if (!dir.exists())
            dir.createDirectory();
        return dir.getChildFile ("favorites.json");
    }

    void load()
    {
        auto file = getFile();
        if (!file.existsAsFile())
            return;

        auto json = juce::JSON::parse (file);
        if (auto* obj = json.getDynamicObject())
        {
            for (const auto& prop : obj->getProperties())
            {
                if (auto* arr = prop.value.getArray())
                {
                    for (const auto& v : *arr)
                        favorites[prop.name.toString()].insert (v.toString());
                }
            }
        }
    }

    void save()
    {
        auto obj = std::make_unique<juce::DynamicObject>();
        for (const auto& [cat, items] : favorites)
        {
            juce::Array<juce::var> arr;
            for (const auto& item : items)
                arr.add (item);
            obj->setProperty (cat, arr);
        }
        getFile().replaceWithText (juce::JSON::toString (juce::var (obj.release())));
    }
};

//==============================================================================
// StarComboPopup - Custom popup with clickable star icons
//==============================================================================
class StarComboPopup : public juce::Component
{
public:
    StarComboPopup (const juce::String& cat, const juce::StringArray& itms, int currentIdx,
                    std::function<void (int)> onSelect)
        : category (cat), items (itms), selectedIndex (currentIdx), selectCallback (onSelect)
    {
        setWantsKeyboardFocus (true);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF1E1E24));
        g.setColour (juce::Colour (0xFF4A4A55));
        g.drawRect (getLocalBounds(), 1);
    }

    void resized() override { updateLayout(); }

    void updateLayout()
    {
        auto& favMgr = FavoritesManager::getInstance();
        auto sorted = favMgr.getSortedItems (category, items);

        int y = 2;
        int rowH = 20;
        starAreas.clear();
        textAreas.clear();
        itemIndices.clear();

        for (const auto& item : sorted)
        {
            int idx = items.indexOf (item);
            // Text on LEFT, star on RIGHT (swapped for better UX)
            textAreas.push_back (juce::Rectangle<int> (4, y, getWidth() - 26, rowH));
            starAreas.push_back (juce::Rectangle<int> (getWidth() - 22, y, 18, rowH));
            itemIndices.push_back (idx);
            y += rowH;
        }
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        auto& favMgr = FavoritesManager::getInstance();
        g.setFont (12.0f);

        for (size_t i = 0; i < itemIndices.size(); ++i)
        {
            int idx = itemIndices[i];
            bool isFav = favMgr.isFavorite (category, items[idx]);
            bool isSelected = (idx == selectedIndex);
            bool isHovered = (hoveredRow == (int) i);

            if (isHovered)
            {
                g.setColour (juce::Colour (0xFF3A3A45));
                g.fillRect (textAreas[i].expanded (2, 0));
            }

            g.setColour (juce::Colour (0xFFAAAAAA));
            juce::String star = isFav ? juce::String::fromUTF8 ("★") : juce::String::fromUTF8 ("☆");
            g.drawText (star, starAreas[i], juce::Justification::centred);

            g.setColour (isSelected ? juce::Colour (0xFF64D2FF) : juce::Colour (0xFFCCCCCC));
            g.drawText (items[idx], textAreas[i], juce::Justification::centredLeft);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { updateHoveredRow (e.position); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (size_t i = 0; i < starAreas.size(); ++i)
        {
            if (starAreas[i].toFloat().contains (e.position))
            {
                FavoritesManager::getInstance().toggleFavorite (category, items[itemIndices[i]]);
                updateLayout();
                repaint();
                return;
            }
            if (textAreas[i].toFloat().contains (e.position))
            {
                if (selectCallback)
                    selectCallback (itemIndices[i]);
                if (auto* cw = findParentComponentOfClass<juce::CallOutBox>())
                    cw->dismiss();
                return;
            }
        }
    }

    void mouseExit (const juce::MouseEvent&) override { hoveredRow = -1; repaint(); }
    void focusLost (FocusChangeType) override
    {
        if (auto* cw = findParentComponentOfClass<juce::CallOutBox>())
            cw->dismiss();
    }

private:
    juce::String category;
    juce::StringArray items;
    int selectedIndex;
    std::function<void (int)> selectCallback;
    std::vector<juce::Rectangle<int>> starAreas, textAreas;
    std::vector<int> itemIndices;
    int hoveredRow = -1;

    void updateHoveredRow (juce::Point<float> pos)
    {
        int newHover = -1;
        for (size_t i = 0; i < textAreas.size(); ++i)
        {
            if (textAreas[i].toFloat().contains (pos) || starAreas[i].toFloat().contains (pos))
            {
                newHover = (int) i;
                break;
            }
        }
        if (newHover != hoveredRow)
        {
            hoveredRow = newHover;
            repaint();
        }
    }
};

//==============================================================================
// StarComboBox - ComboBox with clickable star icons for favorites
//==============================================================================
class StarComboBox : public juce::Component
{
public:
    StarComboBox (const juce::String& categoryName) : category (categoryName)
    {
        setWantsKeyboardFocus (false);
    }

    void setItems (const juce::StringArray& newItems)
    {
        items = newItems;
        if (selectedIndex < 0 && items.size() > 0)
            selectedIndex = 0;
        repaint();
    }

    void setSelectedItemIndex (int index, juce::NotificationType notify = juce::sendNotification)
    {
        if (index >= 0 && index < items.size() && index != selectedIndex)
        {
            selectedIndex = index;
            repaint();
            if (notify != juce::dontSendNotification && onChange)
                onChange();
        }
    }

    int getSelectedItemIndex() const { return selectedIndex; }
    juce::String getSelectedItem() const { return selectedIndex >= 0 ? items[selectedIndex] : ""; }

    std::function<void()> onChange;

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xFF2A2A35));
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (juce::Colour (0xFF4A4A55));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (juce::Colour (0xFFCCCCCC));
        g.setFont (12.0f);
        auto textArea = bounds.reduced (6, 0).withTrimmedRight (16);
        g.drawText (getSelectedItem(), textArea, juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xFF888888));
        auto arrowArea = bounds.removeFromRight (16).reduced (4, 6);
        juce::Path arrow;
        arrow.addTriangle (arrowArea.getX(), arrowArea.getY(),
                           arrowArea.getRight(), arrowArea.getY(),
                           arrowArea.getCentreX(), arrowArea.getBottom());
        g.fillPath (arrow);
    }

    void mouseDown (const juce::MouseEvent&) override { showPopup(); }

private:
    juce::String category;
    juce::StringArray items;
    int selectedIndex = -1;

    void showPopup()
    {
        int popupH = juce::jmin ((int) items.size() * 20 + 4, 300);
        int popupW = juce::jmax (getWidth(), 80);

        auto* popup = new StarComboPopup (category, items, selectedIndex,
                                          [this] (int idx) { setSelectedItemIndex (idx); });
        popup->setSize (popupW, popupH);

        auto& cob = juce::CallOutBox::launchAsynchronously (
            std::unique_ptr<juce::Component> (popup),
            getScreenBounds(),
            nullptr);
        cob.setDismissalMouseClicksAreAlwaysConsumed (true);
    }
};

//==============================================================================
// ScaleChordBox - Compact box for scale/chord controls
//==============================================================================
class ScaleChordBox : public juce::Component,
                      private juce::Timer
{
public:
    ScaleChordBox();
    ~ScaleChordBox() override { stopTimer(); }

    void resized() override;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

    // Controls - keyBox and chordBox use StarComboBox for favorites
    StarComboBox keyBox { "keys" };
    StarComboBox chordBox { "chords" };
    juce::ComboBox scaleBox, varsBox;
    VerticalFillBar scaleAmntBar, chordAmntBar;  // Vertical fill bars instead of knobs
    juce::Slider scaleAmntKnob, chordAmntKnob;   // Keep for parameter attachment (hidden)

    // MIDI Chord Trigger callbacks (wired by editor)
    std::function<bool()> getMidiChordState;
    std::function<void()> toggleMidiChordState;
    std::function<int()>  getCurrentChordDegree;

private:
    void timerCallback() override;

    juce::ToggleButton minMajToggle { "min/maj" };  // Hidden, kept for compatibility
    juce::Label scaleAmntLabel { {}, "S" };
    juce::Label chordAmntLabel { {}, "C" };

    juce::Rectangle<float> midiChordToggleBounds;
    int lastSyncedDegree = -1;  // For syncing chordBox display
};

//==============================================================================
// SlopeClickBox - Clickable box that cycles through slope values
//==============================================================================
class SlopeClickBox : public juce::Component
{
public:
    SlopeClickBox();

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

    void setSlopeIndex (int idx);
    int getSlopeIndex() const { return slopeIndex; }

    std::function<void(int)> onSlopeChanged;

private:
    int slopeIndex = 1;  // 0=6dB, 1=12dB, 2=24dB, 3=48dB, 4=72dB, 5=96dB
    const juce::StringArray slopeLabels { "6dB", "12dB", "24dB", "48dB", "72dB", "96dB" };
};

//==============================================================================
// NoTickToggleLookAndFeel - Toggle button without tick box (text only)
//==============================================================================
class NoTickToggleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        (void) shouldDrawButtonAsHighlighted;
        (void) shouldDrawButtonAsDown;

        auto bounds = button.getLocalBounds().toFloat();

        // Just draw the text, centered, no tick box
        g.setFont (juce::Font (10.0f, juce::Font::bold));
        g.setColour (button.findColour (juce::ToggleButton::textColourId));
        g.drawText (button.getButtonText(), bounds, juce::Justification::centred);
    }
};

//==============================================================================
// TwoLineButtonLookAndFeel - TextButton with multiline text support
//==============================================================================
class TwoLineButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool, bool) override
    {
        // Background drawn by parent paint(), so do nothing here
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        (void) shouldDrawButtonAsHighlighted;
        (void) shouldDrawButtonAsDown;

        auto bounds = button.getLocalBounds().toFloat().reduced (2, 1);
        auto text = button.getButtonText();

        // Scale font to button height (8pt for compact buttons, 9pt for taller)
        float fontSize = bounds.getHeight() < 18.0f ? 8.0f : 9.0f;
        g.setFont (juce::Font (fontSize, juce::Font::bold));
        g.setColour (button.findColour (button.getToggleState()
            ? juce::TextButton::textColourOnId
            : juce::TextButton::textColourOffId));

        // Draw single-line text (compact) or multiline if needed
        int maxLines = bounds.getHeight() < 18.0f ? 1 : 2;
        g.drawFittedText (text, bounds.toNearestInt(), juce::Justification::centred, maxLines);
    }
};

//==============================================================================
// ThinPresetLookAndFeel - Lighter font for preset bar controls
//==============================================================================
class ThinPresetLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Draw button background — preset button gets rounded rect + dropdown arrow
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();

        // Check if this is the preset selector button (has dropdown arrow)
        if (button.getProperties().contains ("isPresetButton"))
        {
            // Background
            auto bgColour = isButtonDown ? backgroundColour.darker (0.2f)
                          : isMouseOverButton ? backgroundColour.brighter (0.1f)
                          : backgroundColour;
            g.setColour (bgColour);
            g.fillRoundedRectangle (bounds, 3.0f);

            // Border
            g.setColour (button.findColour (juce::ComboBox::outlineColourId));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);

            // Dropdown arrow on right side
            auto arrowZone = bounds.removeFromRight (20.0f);
            juce::Path arrow;
            arrow.addTriangle (arrowZone.getCentreX() - 4.0f, arrowZone.getCentreY() - 2.0f,
                               arrowZone.getCentreX() + 4.0f, arrowZone.getCentreY() - 2.0f,
                               arrowZone.getCentreX(), arrowZone.getCentreY() + 3.0f);
            g.setColour (juce::Colour (0xFFAAAAAA));
            g.fillPath (arrow);
        }
        else if (button.getProperties().contains ("isSaveButton"))
        {
            // Save button — draw floppy disk icon
            auto bgColour = isButtonDown ? backgroundColour.darker (0.2f)
                          : isMouseOverButton ? backgroundColour.brighter (0.1f)
                          : backgroundColour;
            g.setColour (bgColour);
            g.fillRoundedRectangle (bounds, 3.0f);

            // Border
            g.setColour (button.findColour (juce::ComboBox::outlineColourId));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);

            // Draw floppy disk icon
            auto iconArea = bounds.reduced (4.0f, 4.0f);
            auto iconColour = isMouseOverButton ? juce::Colours::white : juce::Colour (0xFFCCCCCC);
            g.setColour (iconColour);

            // Outer disk shape
            juce::Path disk;
            float x = iconArea.getX(), y = iconArea.getY();
            float w = iconArea.getWidth(), h = iconArea.getHeight();
            float corner = w * 0.15f;
            float cutoff = w * 0.2f;  // top-right corner cut

            disk.startNewSubPath (x + corner, y);
            disk.lineTo (x + w - cutoff, y);
            disk.lineTo (x + w, y + cutoff);
            disk.lineTo (x + w, y + h - corner);
            disk.quadraticTo (x + w, y + h, x + w - corner, y + h);
            disk.lineTo (x + corner, y + h);
            disk.quadraticTo (x, y + h, x, y + h - corner);
            disk.lineTo (x, y + corner);
            disk.quadraticTo (x, y, x + corner, y);
            disk.closeSubPath();
            g.strokePath (disk, juce::PathStrokeType (1.2f));

            // Label slot (bottom center rectangle)
            float labelW = w * 0.55f;
            float labelH = h * 0.35f;
            float labelX = x + (w - labelW) / 2.0f;
            float labelY = y + h - labelH - h * 0.08f;
            g.drawRoundedRectangle (labelX, labelY, labelW, labelH, 1.0f, 1.0f);

            // Metal slider (top center rectangle)
            float sliderW = w * 0.45f;
            float sliderH = h * 0.3f;
            float sliderX = x + (w - sliderW) / 2.0f;
            float sliderY = y + h * 0.08f;
            g.fillRoundedRectangle (sliderX, sliderY, sliderW, sliderH, 1.0f);
        }
        else
        {
            // Default: standard button
            LookAndFeel_V4::drawButtonBackground (g, button, backgroundColour, isMouseOverButton, isButtonDown);
        }
    }

    // Button text — left-justified for preset button, hidden for save button
    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool, bool) override
    {
        if (button.getProperties().contains ("isSaveButton"))
            return;  // Icon-only, no text

        auto bounds = button.getLocalBounds().toFloat();
        g.setFont (juce::Font (11.0f));
        g.setColour (button.findColour (button.getToggleState()
            ? juce::TextButton::textColourOnId
            : juce::TextButton::textColourOffId));

        if (button.getProperties().contains ("isPresetButton"))
        {
            // Left-justified text, leave room for arrow on right
            auto textArea = bounds.reduced (6, 1).withTrimmedRight (16);
            auto text = button.getButtonText();

            // Truncate with "..." if too long
            auto font = juce::Font (11.0f);
            if (font.getStringWidth (text) > (int) textArea.getWidth())
            {
                while (text.length() > 3 && font.getStringWidth (text + "...") > (int) textArea.getWidth())
                    text = text.dropLastCharacters (1);
                text += "...";
            }
            g.drawText (text, textArea.toNearestInt(), juce::Justification::centredLeft);
        }
        else
        {
            g.drawText (button.getButtonText(), bounds.reduced (2, 1).toNearestInt(), juce::Justification::centred);
        }
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::Font (12.0f);
    }
};

//==============================================================================
// BandDistributionEditor - Bezier-style band frequency distribution curve
// X-axis: Band number (1 to N)
// Y-axis: Frequency (log scale, Low to High)
// Drag control points to reshape curve, double-click to add points, right-click to delete
//==============================================================================
class BandDistributionEditor : public juce::Component
{
public:
    BandDistributionEditor (ColorVocoderAudioProcessor& p)
        : processor (p)
    {
        // Initialize with linear distribution (3 control points: start, middle, end)
        resetToLinear();
    }

    // ===== GLIDE-CURVE MODE (opt-in; defaults preserve band-distribution behaviour) =====
    // When on: endpoints are fully locked at (0,0)/(1,1), middle points may be non-monotonic
    // (so the user can shape overshoot), and the plot is drawn as a time->progress easing curve
    // instead of a band->frequency map.
    bool lockEndpointsFully = false;   // reject endpoint drags entirely
    bool glideMode          = false;   // easing-curve painting + allow non-monotonic shapes
    void configureAsGlideCurve()
    {
        lockEndpointsFully = true;
        glideMode          = true;
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Dark background
        g.setColour (juce::Colour (0xFF1A1A1F));
        g.fillRoundedRectangle (bounds, 6.0f);

        // Border
        g.setColour (juce::Colour (0xFF3A3A45));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

        // Plot area with margins for axis labels
        const float leftMargin = 45.0f;
        const float rightMargin = 10.0f;
        const float topMargin = 10.0f;
        const float bottomMargin = 25.0f;

        plotArea = bounds.reduced (leftMargin, topMargin);
        plotArea.removeFromBottom (bottomMargin - topMargin);
        plotArea.removeFromLeft (leftMargin - 10.0f);
        plotArea.removeFromRight (rightMargin - 10.0f);

        // Grid background
        g.setColour (juce::Colour (0xFF252530));
        g.fillRect (plotArea);

        if (! glideMode)
        {
            // ===== BAND-DISTRIBUTION painting (frequency vs band) =====
            // Get current frequency range from processor
            float lowHz = 80.0f, highHz = 12000.0f;
            if (auto* param = processor.apvts.getRawParameterValue ("lowFreq"))
                lowHz = param->load();
            if (auto* param = processor.apvts.getRawParameterValue ("highFreq"))
                highHz = param->load();

            // Draw frequency grid lines (horizontal) - log scale
            g.setColour (juce::Colour (0xFF3A3A45));
            g.setFont (9.0f);

            const float freqLines[] = { 100, 200, 500, 1000, 2000, 5000, 10000 };
            for (float freq : freqLines)
            {
                if (freq < lowHz || freq > highHz) continue;

                float normY = freqToNormY (freq, lowHz, highHz);
                float y = plotArea.getBottom() - normY * plotArea.getHeight();

                g.setColour (juce::Colour (0xFF3A3A45));
                g.drawHorizontalLine ((int) y, plotArea.getX(), plotArea.getRight());

                // Frequency label on left
                g.setColour (juce::Colour (0xFF888888));
                juce::String label = (freq >= 1000) ? juce::String (freq / 1000.0f, 1) + "k" : juce::String ((int) freq);
                g.drawText (label, 4, (int) y - 6, 38, 12, juce::Justification::centredRight);
            }

            // Draw band grid lines (vertical)
            int bandCount = 16;  // Default
            if (auto* param = processor.apvts.getRawParameterValue ("bandCount"))
                bandCount = (int) param->load();

            // Draw a few vertical grid lines
            g.setColour (juce::Colour (0xFF3A3A45));
            for (int i = 0; i <= 4; ++i)
            {
                float normX = (float) i / 4.0f;
                float x = plotArea.getX() + normX * plotArea.getWidth();
                g.drawVerticalLine ((int) x, plotArea.getY(), plotArea.getBottom());

                // Band number label
                int bandNum = 1 + (int) (normX * (bandCount - 1));
                g.setColour (juce::Colour (0xFF888888));
                g.drawText (juce::String (bandNum), (int) x - 15, (int) plotArea.getBottom() + 4, 30, 12, juce::Justification::centred);
            }

            // X-axis label
            g.setColour (juce::Colour (0xFF888888));
            g.drawText ("Band", plotArea.getCentreX() - 20, (int) bounds.getBottom() - 14, 40, 12, juce::Justification::centred);

            // Y-axis label
            g.drawText ("Hz", 4, (int) plotArea.getY() - 2, 30, 12, juce::Justification::centredLeft);
        }
        else
        {
            // ===== GLIDE-CURVE painting (progress vs time) =====
            g.setFont (9.0f);
            // Evenly spaced grid: 4 columns (time), 4 rows (progress)
            g.setColour (juce::Colour (0xFF3A3A45));
            for (int i = 0; i <= 4; ++i)
            {
                float t = (float) i / 4.0f;
                float x = plotArea.getX() + t * plotArea.getWidth();
                float y = plotArea.getBottom() - t * plotArea.getHeight();
                g.drawVerticalLine   ((int) x, plotArea.getY(), plotArea.getBottom());
                g.drawHorizontalLine ((int) y, plotArea.getX(), plotArea.getRight());
            }

            // Y-axis markers: START (bottom) -> END (top) of the note move
            g.setColour (juce::Colour (0xFF888888));
            g.drawText ("END",   4, (int) plotArea.getY() - 2,          40, 12, juce::Justification::centredLeft);
            g.drawText ("START", 4, (int) plotArea.getBottom() - 10,    40, 12, juce::Justification::centredLeft);

            // X-axis label
            g.drawText ("TIME", plotArea.getCentreX() - 20, (int) bounds.getBottom() - 14, 40, 12, juce::Justification::centred);
        }

        // Safety check
        if (controlPoints.size() < 2)
        {
            resetToLinear();  // Recover from invalid state
        }

        // Generate interpolated curve from control points using Catmull-Rom spline
        rebuildDistributionCurve();

        // Safety check after rebuild
        if (distributionCurve.size() < 2)
            return;

        // Draw the interpolated curve
        juce::Path curvePath;
        bool first = true;

        for (int i = 0; i < (int) distributionCurve.size(); ++i)
        {
            float normX = (float) i / (float) (distributionCurve.size() - 1);
            float normY = distributionCurve[(size_t) i];

            float x = plotArea.getX() + normX * plotArea.getWidth();
            float y = plotArea.getBottom() - normY * plotArea.getHeight();

            if (first)
            {
                curvePath.startNewSubPath (x, y);
                first = false;
            }
            else
            {
                curvePath.lineTo (x, y);
            }
        }

        // Draw curve glow
        g.setColour (juce::Colour (0x4064D2FF));
        g.strokePath (curvePath, juce::PathStrokeType (4.0f));

        // Draw main curve
        g.setColour (juce::Colour (0xFF64D2FF));
        g.strokePath (curvePath, juce::PathStrokeType (2.0f));

        // Draw control points
        for (int i = 0; i < (int) controlPoints.size(); ++i)
        {
            auto& pt = controlPoints[(size_t) i];
            float x = plotArea.getX() + pt.x * plotArea.getWidth();
            float y = plotArea.getBottom() - pt.y * plotArea.getHeight();

            bool isHovered = (hoveredPoint == i);
            bool isDraggedPt = (draggedPoint == i);
            bool isEndpoint = (i == 0 || i == (int) controlPoints.size() - 1);
            float radius = (isHovered || isDraggedPt) ? 7.0f : 5.0f;

            // Outer ring (white when hovered)
            g.setColour ((isHovered || isDraggedPt) ? juce::Colour (0xFFFFFFFF) : juce::Colour (0xFF64D2FF));
            g.drawEllipse (x - radius, y - radius, radius * 2, radius * 2, 2.0f);

            // Inner fill - solid for endpoints, hollow for middle points
            if (isEndpoint)
            {
                g.setColour (juce::Colour (0xFF64D2FF));
                g.fillEllipse (x - radius + 2, y - radius + 2, (radius - 2) * 2, (radius - 2) * 2);
            }
            else
            {
                // Hollow circle for draggable middle points
                g.setColour (juce::Colour (0xFF252530));
                g.fillEllipse (x - radius + 2, y - radius + 2, (radius - 2) * 2, (radius - 2) * 2);
            }
        }

        // Instructions
        g.setColour (juce::Colour (0x80FFFFFF));
        g.setFont (10.0f);
        g.drawText ("Drag points  |  Dbl-click: add/delete  |  Shift+dbl-click: reset",
                    plotArea.getX(), plotArea.getY() + 2, plotArea.getWidth(), 14, juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (plotArea.isEmpty())
            return;

        // Find if we clicked on a control point
        draggedPoint = findPointAt (e.position);

        // Glide mode: endpoints (0,0)/(1,1) are fully locked - never start a drag on them.
        if (lockEndpointsFully && draggedPoint >= 0
            && (draggedPoint == 0 || draggedPoint == (int) controlPoints.size() - 1))
        {
            draggedPoint = -1;
            return;
        }

        if (draggedPoint >= 0)
        {
            // Don't allow dragging endpoints horizontally (they stay at x=0 and x=1)
            isDragging = true;
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (!isDragging || draggedPoint < 0)
            return;

        // Convert mouse position to normalized coordinates
        float normX = (e.position.x - plotArea.getX()) / plotArea.getWidth();
        float normY = 1.0f - (e.position.y - plotArea.getY()) / plotArea.getHeight();

        normY = juce::jlimit (0.0f, 1.0f, normY);

        // Endpoints can only move vertically
        bool isStartPoint = (draggedPoint == 0);
        bool isEndPoint = (draggedPoint == (int) controlPoints.size() - 1);

        // MONOTONIC CONSTRAINT: Y must be >= previous point and <= next point
        // This ensures the distribution curve always goes from low to high frequency
        float minY = 0.0f;
        float maxY = 1.0f;

        if (! glideMode)   // glide curves may overshoot (non-monotonic); distribution stays monotonic
        {
            if (draggedPoint > 0)
                minY = controlPoints[(size_t) (draggedPoint - 1)].y + 0.001f;
            if (draggedPoint < (int) controlPoints.size() - 1)
                maxY = controlPoints[(size_t) (draggedPoint + 1)].y - 0.001f;

            // Ensure minY <= maxY (safety)
            if (minY > maxY)
                minY = maxY;
        }

        normY = juce::jlimit (minY, maxY, normY);

        if (isStartPoint)
        {
            // Start point: X fixed at 0, Y can move but must be <= next point
            controlPoints[0].y = normY;
        }
        else if (isEndPoint)
        {
            // End point: X fixed at 1, Y can move but must be >= previous point
            controlPoints[(size_t) draggedPoint].y = normY;
        }
        else
        {
            // Middle points can move in X (between neighbors) and Y (monotonic)
            normX = juce::jlimit (0.0f, 1.0f, normX);

            // Constrain X to be between neighboring points
            float minX = controlPoints[(size_t) (draggedPoint - 1)].x + 0.01f;
            float maxX = controlPoints[(size_t) (draggedPoint + 1)].x - 0.01f;
            normX = juce::jlimit (minX, maxX, normX);

            controlPoints[(size_t) draggedPoint].x = normX;
            controlPoints[(size_t) draggedPoint].y = normY;
        }

        rebuildDistributionCurve();
        // Don't notify during drag - too expensive (causes rebuildBands on every mouse move)
        // Will notify on mouseUp instead
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        // Right-click to delete a point (but not endpoints)
        if (e.mods.isRightButtonDown())
        {
            int pointToDelete = findPointAt (e.position);
            if (pointToDelete > 0 && pointToDelete < (int) controlPoints.size() - 1)
            {
                controlPoints.erase (controlPoints.begin() + pointToDelete);
                rebuildDistributionCurve();
                notifyChange();
                repaint();
            }
        }

        // Notify processor of final curve only when drag ends
        if (isDragging && draggedPoint >= 0)
        {
            notifyChange();
        }

        isDragging = false;
        draggedPoint = -1;
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        if (plotArea.isEmpty())
            return;

        int newHovered = findPointAt (e.position);
        if (newHovered != hoveredPoint)
        {
            hoveredPoint = newHovered;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoveredPoint = -1;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (plotArea.isEmpty() || controlPoints.size() < 2)
            return;

        // Check if clicked on existing MIDDLE point — Shift+double-click to delete
        int clickedPoint = findPointAt (e.position);
        if (clickedPoint > 0 && clickedPoint < (int) controlPoints.size() - 1 && e.mods.isShiftDown())
        {
            controlPoints.erase (controlPoints.begin() + clickedPoint);
            rebuildDistributionCurve();
            notifyChange();
            repaint();
            return;
        }

        // Double-click on empty space or endpoint = RESET to linear
        if (clickedPoint < 0 || clickedPoint == 0 || clickedPoint == (int) controlPoints.size() - 1)
        {
            resetToLinear();
            rebuildDistributionCurve();
            notifyChange();
            repaint();
            return;
        }

        // Add a new control point at this position
        float normX = (e.position.x - plotArea.getX()) / plotArea.getWidth();
        float normY = 1.0f - (e.position.y - plotArea.getY()) / plotArea.getHeight();

        // Safety checks for NaN/Inf
        if (std::isnan (normX) || std::isnan (normY) || std::isinf (normX) || std::isinf (normY))
            return;

        normX = juce::jlimit (0.02f, 0.98f, normX);  // Don't allow at exact endpoints
        normY = juce::jlimit (0.0f, 1.0f, normY);

        // Find where to insert (keep sorted by X)
        size_t insertIdx = 1;
        for (size_t i = 1; i < controlPoints.size(); ++i)
        {
            if (controlPoints[i].x > normX)
            {
                insertIdx = i;
                break;
            }
            if (i == controlPoints.size() - 1)
                insertIdx = i;  // Insert before last point
        }

        // Safety: clamp insert index to valid range
        insertIdx = juce::jlimit ((size_t) 1, controlPoints.size() - 1, insertIdx);

        // MONOTONIC CONSTRAINT: Y must be between neighboring points
        float minY = controlPoints[insertIdx - 1].y + 0.001f;
        float maxY = controlPoints[insertIdx].y - 0.001f;
        if (minY > maxY)
        {
            // No room for a point here (neighbors too close), use midpoint
            normY = (controlPoints[insertIdx - 1].y + controlPoints[insertIdx].y) * 0.5f;
        }
        else
        {
            normY = juce::jlimit (minY, maxY, normY);
        }

        // Limit max control points to prevent memory issues
        if (controlPoints.size() >= 20)
            return;

        controlPoints.insert (controlPoints.begin() + (int) insertIdx, juce::Point<float> (normX, normY));
        rebuildDistributionCurve();
        notifyChange();
        repaint();
    }

    // Reset to linear (straight diagonal line with single middle control point)
    void resetToLinear()
    {
        controlPoints.clear();
        controlPoints.push_back (juce::Point<float> (0.0f, 0.0f));   // Start
        controlPoints.push_back (juce::Point<float> (0.5f, 0.5f));   // Middle control
        controlPoints.push_back (juce::Point<float> (1.0f, 1.0f));   // End
        rebuildDistributionCurve();
    }

    // Get the distribution curve (normalized 0-1 values) - 101 points for processor compatibility
    const std::vector<float>& getDistributionCurve() const { return distributionCurve; }

    // Set distribution curve from external source (e.g., preset load)
    // This reconstructs control points from the curve (simplified - just uses key points)
    void setDistributionCurve (const std::vector<float>& curve)
    {
        if (curve.size() < 2)
            return;

        // For preset compatibility, sample the curve at a few key points to create control points
        controlPoints.clear();
        controlPoints.push_back (juce::Point<float> (0.0f, curve.front()));

        // Add middle point(s) based on curve deviation from linear
        controlPoints.push_back (juce::Point<float> (0.5f, curve[curve.size() / 2]));

        controlPoints.push_back (juce::Point<float> (1.0f, curve.back()));

        distributionCurve = curve;
        repaint();
    }

    // Get control points for preset saving
    const std::vector<juce::Point<float>>& getControlPoints() const { return controlPoints; }

    // Set control points from preset
    void setControlPoints (const std::vector<juce::Point<float>>& points)
    {
        if (points.size() >= 2)
        {
            controlPoints = points;
            rebuildDistributionCurve();
            repaint();
        }
    }

    // Callback when distribution changes
    std::function<void(const std::vector<float>&)> onDistributionChanged;

private:
    ColorVocoderAudioProcessor& processor;

    // Control points: sparse points that define the curve shape
    std::vector<juce::Point<float>> controlPoints;

    // Distribution curve: 101 points interpolated from control points (for processor)
    std::vector<float> distributionCurve;

    mutable juce::Rectangle<float> plotArea;
    bool isDragging = false;
    int draggedPoint = -1;
    int hoveredPoint = -1;

    // Convert frequency to normalized Y (0-1) using log scale
    float freqToNormY (float freq, float lowHz, float highHz) const
    {
        float logLow = std::log (lowHz);
        float logHigh = std::log (highHz);
        float logFreq = std::log (juce::jlimit (lowHz, highHz, freq));
        return (logFreq - logLow) / (logHigh - logLow);
    }

    // Find control point at mouse position (-1 if none)
    int findPointAt (juce::Point<float> pos) const
    {
        const float hitRadius = 12.0f;

        for (int i = 0; i < (int) controlPoints.size(); ++i)
        {
            auto& pt = controlPoints[(size_t) i];
            float x = plotArea.getX() + pt.x * plotArea.getWidth();
            float y = plotArea.getBottom() - pt.y * plotArea.getHeight();

            if (pos.getDistanceFrom (juce::Point<float> (x, y)) < hitRadius)
                return i;
        }
        return -1;
    }

    // Catmull-Rom spline interpolation (with safety checks)
    float catmullRom (float p0, float p1, float p2, float p3, float t) const
    {
        // Clamp t to valid range
        t = juce::jlimit (0.0f, 1.0f, t);

        float t2 = t * t;
        float t3 = t2 * t;
        float result = 0.5f * ((2.0f * p1) +
                               (-p0 + p2) * t +
                               (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                               (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);

        // Safety: check for NaN or Inf
        if (std::isnan (result) || std::isinf (result))
            return (p1 + p2) * 0.5f;  // Fallback to linear interpolation

        return result;
    }

    // Rebuild the 101-point distribution curve from control points
    // ENSURES MONOTONICALLY INCREASING: each value >= previous value
    void rebuildDistributionCurve()
    {
        distributionCurve.resize (101);

        if (controlPoints.size() < 2)
        {
            // Fallback to linear
            for (int i = 0; i < 101; ++i)
                distributionCurve[(size_t) i] = (float) i / 100.0f;
            return;
        }

        // For each output point, find which segment it falls in and interpolate
        for (int i = 0; i < 101; ++i)
        {
            float x = (float) i / 100.0f;

            // Find the segment this x falls into
            int segIdx = 0;
            for (int j = 0; j < (int) controlPoints.size() - 1; ++j)
            {
                if (x >= controlPoints[(size_t) j].x && x <= controlPoints[(size_t) (j + 1)].x)
                {
                    segIdx = j;
                    break;
                }
                segIdx = j;
            }

            // Get the 4 points for Catmull-Rom (with clamping at edges)
            int i0 = juce::jmax (0, segIdx - 1);
            int i1 = segIdx;
            int i2 = juce::jmin ((int) controlPoints.size() - 1, segIdx + 1);
            int i3 = juce::jmin ((int) controlPoints.size() - 1, segIdx + 2);

            // Calculate local t within segment
            float x1 = controlPoints[(size_t) i1].x;
            float x2 = controlPoints[(size_t) i2].x;
            float t = (x2 > x1) ? (x - x1) / (x2 - x1) : 0.0f;
            t = juce::jlimit (0.0f, 1.0f, t);

            // Interpolate Y using Catmull-Rom
            float y0 = controlPoints[(size_t) i0].y;
            float y1 = controlPoints[(size_t) i1].y;
            float y2 = controlPoints[(size_t) i2].y;
            float y3 = controlPoints[(size_t) i3].y;

            // Glide mode: reflect the phantom endpoint (p0 = 2*p1 - p2) instead of clamping it to
            // the endpoint. Clamping halves the boundary tangent and bows collinear points into an
            // S; reflection makes a straight line render dead straight and gives natural end slopes.
            if (glideMode)
            {
                if (segIdx == 0)                                        // no real point before i1
                    y0 = 2.0f * y1 - y2;
                if (segIdx + 2 > (int) controlPoints.size() - 1)        // no real point after i2
                    y3 = 2.0f * y2 - y1;
            }

            float y = catmullRom (y0, y1, y2, y3, t);

            distributionCurve[(size_t) i] = juce::jlimit (0.0f, 1.0f, y);
        }

        // ENFORCE MONOTONICALLY INCREASING: band distribution must always go up.
        // Glide curves are allowed to dip/overshoot, so skip this in glide mode.
        if (! glideMode)
        {
            for (int i = 1; i < 101; ++i)
            {
                if (distributionCurve[(size_t) i] < distributionCurve[(size_t) (i - 1)])
                    distributionCurve[(size_t) i] = distributionCurve[(size_t) (i - 1)];
            }
        }
    }

    void notifyChange()
    {
        if (onDistributionChanged)
            onDistributionChanged (distributionCurve);
    }
};

//==============================================================================
// BankKeytrackPopup - CallOutBox panel for the bank-keytrack POSITION + GLIDE knobs
// and the editable glide easing curve. Opened from the KEYTRACK EDIT button; stays open
// while dragging (so POSITION can be swept like a filter cutoff), dismisses on outside click.
//==============================================================================
class BankKeytrackPopup : public juce::Component,
                          private juce::Timer
{
public:
    BankKeytrackPopup (ColorVocoderAudioProcessor& p, juce::LookAndFeel& knobLaf)
        : processor (p), curveEditor (p)
    {
        startTimerHz (12);   // TEMP: drive the live diagnostic readout (remove after verification)
        auto setupKnob = [&] (juce::Slider& s, juce::Label& lab, const juce::String& text)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            s.setMouseDragSensitivity (150);
            s.setPopupDisplayEnabled (true, true, this);   // value shows only during drag
            s.setLookAndFeel (&knobLaf);
            addAndMakeVisible (s);
            lab.setText (text, juce::dontSendNotification);
            lab.setJustificationType (juce::Justification::centred);
            lab.setColour (juce::Label::textColourId, juce::Colour (0xFFE0E0E0));
            lab.setFont (11.0f);
            addAndMakeVisible (lab);
        };

        setupKnob (posKnob,   posLabel,   "POSITION");
        setupKnob (glideKnob, glideLabel, "GLIDE");
        posAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                       processor.apvts, "bankPosition", posKnob);
        glideAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                       processor.apvts, "bankGlide", glideKnob);

        // Unit-labelled drag popups: GLIDE in ms, POSITION in semitones (signed).
        glideKnob.textFromValueFunction = [] (double v) {
            return juce::String (juce::roundToInt (v)) + " ms";
        };
        posKnob.textFromValueFunction = [] (double v) {
            return (v > 0.0 ? "+" : "") + juce::String (v, 1) + " st";
        };
        glideKnob.updateText();
        posKnob.updateText();

        curveEditor.configureAsGlideCurve();
        curveEditor.onDistributionChanged = [this] (const std::vector<float>& c) {
            processor.setBankGlideCurve (c);
        };
        curveEditor.setDistributionCurve (processor.getBankGlideCurve());
        addAndMakeVisible (curveEditor);

        titleLabel.setText ("BANK KEYTRACK", juce::dontSendNotification);
        titleLabel.setJustificationType (juce::Justification::centred);
        titleLabel.setColour (juce::Label::textColourId, juce::Colour (0xFF00DDFF));
        titleLabel.setFont (12.0f);
        addAndMakeVisible (titleLabel);

        curveHint.setText ("GLIDE CURVE  -  drag / dbl-click points", juce::dontSendNotification);
        curveHint.setJustificationType (juce::Justification::centred);
        curveHint.setColour (juce::Label::textColourId, juce::Colour (0xFF888888));
        curveHint.setFont (9.0f);
        addAndMakeVisible (curveHint);

        setSize (300, 282);   // +18 for the temp diagnostic strip
    }

    ~BankKeytrackPopup() override
    {
        stopTimer();
        posKnob.setLookAndFeel (nullptr);
        glideKnob.setLookAndFeel (nullptr);
    }

    void timerCallback() override { repaint(); }   // TEMP: refresh diagnostic readout

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFF17171C));
        g.setColour (juce::Colour (0xFF3A3A45));
        g.drawRect (getLocalBounds(), 1);

        // ===== TEMP LIVE DIAGNOSTIC (remove after verification) =====
        // Shows what the AUDIO THREAD actually sees. If FREE=0 you are NOT in LOG/LIN mode
        // (keytrack is gated off). If FREE=1 and FACTOR changes as you drag POSITION, the DSP
        // chain is working. If FACTOR stays 1.00 while POSITION!=0 in FREE mode -> real bug.
        int   rawScale = -1;
        if (auto* p = processor.apvts.getRawParameterValue ("scaleType"))
            rawScale = (int) p->load();
        const bool  freeMode = processor.dbgBankFreeMode.load();
        const float factor   = processor.dbgBankFactor.load();
        const float shift    = processor.getDbgBankShift();

        juce::String dbg = "DBG  scale=" + juce::String (rawScale)
                         + "  FREE=" + juce::String (freeMode ? 1 : 0)
                         + "  shift=" + juce::String (shift, 2)
                         + "  FACTOR=" + juce::String (factor, 3);
        g.setColour (freeMode ? juce::Colour (0xFF00DDFF) : juce::Colour (0xFFFF6060));
        g.setFont (10.0f);
        g.drawText (dbg, getLocalBounds().removeFromBottom (14).reduced (6, 0),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (10);
        titleLabel.setBounds (b.removeFromTop (18));
        b.removeFromTop (4);

        // Two 44px knobs side by side with labels beneath.
        auto knobRow = b.removeFromTop (44 + 14);
        const int kw = 44;
        auto left  = knobRow.removeFromLeft (knobRow.getWidth() / 2);
        auto right = knobRow;
        posKnob.setBounds   (left.getCentreX()  - kw / 2, left.getY(),  kw, 44);
        posLabel.setBounds  (left.getX(),  left.getY()  + 44, left.getWidth(),  14);
        glideKnob.setBounds (right.getCentreX() - kw / 2, right.getY(), kw, 44);
        glideLabel.setBounds(right.getX(), right.getY() + 44, right.getWidth(), 14);

        b.removeFromTop (8);
        curveHint.setBounds (b.removeFromTop (12));
        b.removeFromTop (2);
        b.removeFromBottom (16);     // TEMP: reserve the diagnostic strip
        curveEditor.setBounds (b);   // fills the rest
    }

private:
    ColorVocoderAudioProcessor& processor;
    juce::Slider posKnob, glideKnob;
    juce::Label  posLabel, glideLabel, titleLabel, curveHint;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> posAtt, glideAtt;
    BandDistributionEditor curveEditor;   // reused as a locked-endpoint glide curve

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BankKeytrackPopup)
};

//==============================================================================
// BandOffsetEditor - Band routing offset curve editor
// X-axis: Modulator band (1 to N)
// Y-axis: Carrier band (1 to N)
// Default: diagonal line (1:1 mapping)
// Allows shifting which modulator band controls which carrier band
//==============================================================================
class BandOffsetEditor : public juce::Component
{
public:
    BandOffsetEditor (ColorVocoderAudioProcessor& p)
        : processor (p)
    {
        // Initialize with linear 1:1 mapping (diagonal line)
        resetToLinear();

        // Global Offset Slider - shifts entire curve up/down (-12 to +12 bands)
        globalOffsetSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        globalOffsetSlider.setRange (-12.0, 12.0, 0.1);
        globalOffsetSlider.setValue (processor.getGlobalBandOffset());
        globalOffsetSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 20);
        globalOffsetSlider.setColour (juce::Slider::thumbColourId, juce::Colour (0xFF88D4A0));  // Pastel green
        globalOffsetSlider.setColour (juce::Slider::trackColourId, juce::Colour (0xFF3A3A45));
        globalOffsetSlider.onValueChange = [this]()
        {
            processor.setGlobalBandOffset ((float) globalOffsetSlider.getValue());
        };
        addAndMakeVisible (globalOffsetSlider);

        // Reset button
        resetButton.setButtonText ("Reset");
        resetButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2A35));
        resetButton.onClick = [this]()
        {
            resetToLinear();
            globalOffsetSlider.setValue (0.0);
            processor.setGlobalBandOffset (0.0f);
            rebuildOffsetCurve();
            notifyChange();
            repaint();
        };
        addAndMakeVisible (resetButton);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Bottom controls area — compact 22px to match dist editor's plot size
        int controlsHeight = 22;
        auto controlsArea = bounds.removeFromBottom (controlsHeight);

        // Layout: [OFFSET: label] [slider] [value] [Reset button]
        controlsArea.removeFromLeft (52);  // Label space drawn in paint()
        resetButton.setBounds (controlsArea.removeFromRight (44).reduced (1));
        globalOffsetSlider.setBounds (controlsArea.reduced (1));
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Dark background
        g.setColour (juce::Colour (0xFF1A1A1F));
        g.fillRoundedRectangle (bounds, 6.0f);

        // Border
        g.setColour (juce::Colour (0xFF3A3A45));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

        // Draw global offset label
        g.setColour (juce::Colours::white);
        g.setFont (9.0f);
        g.drawText ("OFFSET:", 6, (int) bounds.getHeight() - 20, 48, 18, juce::Justification::centredLeft);

        // Plot area — match dist editor margins for consistent graph size
        const float leftMargin = 45.0f;
        const float rightMargin = 10.0f;
        const float topMargin = 10.0f;
        const float bottomMargin = 47.0f;  // Room for axis label + compact controls

        plotArea = bounds.reduced (leftMargin, topMargin);
        plotArea.removeFromBottom (bottomMargin - topMargin);
        plotArea.removeFromLeft (leftMargin - 10.0f);
        plotArea.removeFromRight (rightMargin - 10.0f);

        // Grid background
        g.setColour (juce::Colour (0xFF252530));
        g.fillRect (plotArea);

        // Get band count from processor
        int bandCount = 16;
        if (auto* param = processor.apvts.getRawParameterValue ("bandCount"))
            bandCount = (int) param->load();

        // Draw carrier band grid lines (horizontal) - Y axis
        g.setColour (juce::Colour (0xFF3A3A45));
        g.setFont (9.0f);

        for (int i = 0; i <= 4; ++i)
        {
            float normY = (float) i / 4.0f;
            float y = plotArea.getBottom() - normY * plotArea.getHeight();

            g.setColour (juce::Colour (0xFF3A3A45));
            g.drawHorizontalLine ((int) y, plotArea.getX(), plotArea.getRight());

            // Carrier band label on left
            int carrierBand = 1 + (int) (normY * (bandCount - 1));
            g.setColour (juce::Colour (0xFF888888));
            g.drawText (juce::String (carrierBand), 4, (int) y - 6, 42, 12, juce::Justification::centredRight);
        }

        // Draw modulator band grid lines (vertical) - X axis
        for (int i = 0; i <= 4; ++i)
        {
            float normX = (float) i / 4.0f;
            float x = plotArea.getX() + normX * plotArea.getWidth();

            g.setColour (juce::Colour (0xFF3A3A45));
            g.drawVerticalLine ((int) x, plotArea.getY(), plotArea.getBottom());

            // Modulator band label
            int modBand = 1 + (int) (normX * (bandCount - 1));
            g.setColour (juce::Colour (0xFF888888));
            g.drawText (juce::String (modBand), (int) x - 15, (int) plotArea.getBottom() + 4, 30, 12, juce::Justification::centred);
        }

        // Axis labels
        g.setColour (juce::Colour (0xFFAAAAAA));
        g.setFont (10.0f);
        g.drawText ("MODULATOR BAND", plotArea.getCentreX() - 60, (int) plotArea.getBottom() + 2, 120, 12, juce::Justification::centred);

        // Rotate for vertical Y-axis label (Carrier)
        g.saveState();
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, 15.0f, plotArea.getCentreY()));
        g.drawText ("CARRIER BAND", 15, (int) plotArea.getCentreY() - 40, 80, 12, juce::Justification::centred);
        g.restoreState();

        // Safety check
        if (controlPoints.size() < 2)
        {
            resetToLinear();
        }

        // Generate interpolated curve from control points
        rebuildOffsetCurve();

        if (offsetCurve.size() < 2)
            return;

        // Draw the interpolated curve
        juce::Path curvePath;
        bool first = true;

        for (int i = 0; i < (int) offsetCurve.size(); ++i)
        {
            float normX = (float) i / (float) (offsetCurve.size() - 1);
            float normY = offsetCurve[(size_t) i];

            float x = plotArea.getX() + normX * plotArea.getWidth();
            float y = plotArea.getBottom() - normY * plotArea.getHeight();

            if (first)
            {
                curvePath.startNewSubPath (x, y);
                first = false;
            }
            else
            {
                curvePath.lineTo (x, y);
            }
        }

        // Draw curve glow (pastel green)
        g.setColour (juce::Colour (0x4088D4A0));
        g.strokePath (curvePath, juce::PathStrokeType (4.0f));

        // Draw main curve
        g.setColour (juce::Colour (0xFF88D4A0));
        g.strokePath (curvePath, juce::PathStrokeType (2.0f));

        // Draw control points
        for (int i = 0; i < (int) controlPoints.size(); ++i)
        {
            auto& pt = controlPoints[(size_t) i];
            float x = plotArea.getX() + pt.x * plotArea.getWidth();
            float y = plotArea.getBottom() - pt.y * plotArea.getHeight();

            bool isHovered = (hoveredPoint == i);
            bool isDraggedPt = (draggedPoint == i);
            bool isEndpoint = (i == 0 || i == (int) controlPoints.size() - 1);
            float radius = (isHovered || isDraggedPt) ? 7.0f : 5.0f;

            // Outer ring (pastel green)
            g.setColour ((isHovered || isDraggedPt) ? juce::Colour (0xFFFFFFFF) : juce::Colour (0xFF88D4A0));
            g.drawEllipse (x - radius, y - radius, radius * 2, radius * 2, 2.0f);

            // Inner fill
            if (isEndpoint)
            {
                g.setColour (juce::Colour (0xFF88D4A0));
                g.fillEllipse (x - radius + 2, y - radius + 2, (radius - 2) * 2, (radius - 2) * 2);
            }
            else
            {
                g.setColour (juce::Colour (0xFF252530));
                g.fillEllipse (x - radius + 2, y - radius + 2, (radius - 2) * 2, (radius - 2) * 2);
            }
        }

        // Instructions
        g.setColour (juce::Colour (0x80FFFFFF));
        g.setFont (10.0f);
        g.drawText ("Drag points  |  Dbl-click: add/delete  |  Shift+dbl-click: reset",
                    plotArea.getX(), plotArea.getY() + 2, plotArea.getWidth(), 14, juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (plotArea.isEmpty())
            return;

        draggedPoint = findPointAt (e.position);

        if (draggedPoint >= 0)
            isDragging = true;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (!isDragging || draggedPoint < 0)
            return;

        float normX = (e.position.x - plotArea.getX()) / plotArea.getWidth();
        float normY = 1.0f - (e.position.y - plotArea.getY()) / plotArea.getHeight();

        normX = juce::jlimit (0.0f, 1.0f, normX);
        normY = juce::jlimit (0.0f, 1.0f, normY);

        bool isStartPoint = (draggedPoint == 0);
        bool isEndPoint = (draggedPoint == (int) controlPoints.size() - 1);

        // Unlike distribution curve, offset curve does NOT need to be monotonic
        // Bands can map to any other band (cross-mapping is allowed)

        if (isStartPoint)
        {
            controlPoints[0].y = normY;
        }
        else if (isEndPoint)
        {
            controlPoints[(size_t) draggedPoint].y = normY;
        }
        else
        {
            // Constrain X to be between neighbors
            float minX = controlPoints[(size_t) (draggedPoint - 1)].x + 0.01f;
            float maxX = controlPoints[(size_t) (draggedPoint + 1)].x - 0.01f;
            normX = juce::jlimit (minX, maxX, normX);

            controlPoints[(size_t) draggedPoint].x = normX;
            controlPoints[(size_t) draggedPoint].y = normY;
        }

        rebuildOffsetCurve();
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        // Right-click to delete a point (but not endpoints)
        if (e.mods.isRightButtonDown())
        {
            int pointToDelete = findPointAt (e.position);
            if (pointToDelete > 0 && pointToDelete < (int) controlPoints.size() - 1)
            {
                controlPoints.erase (controlPoints.begin() + pointToDelete);
                rebuildOffsetCurve();
                notifyChange();
                repaint();
            }
        }

        if (isDragging && draggedPoint >= 0)
        {
            notifyChange();
        }

        isDragging = false;
        draggedPoint = -1;
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        if (plotArea.isEmpty())
            return;

        int newHovered = findPointAt (e.position);
        if (newHovered != hoveredPoint)
        {
            hoveredPoint = newHovered;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoveredPoint = -1;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (plotArea.isEmpty() || controlPoints.size() < 2)
            return;

        // Check if clicked on existing MIDDLE point — Shift+double-click to delete
        int clickedPoint = findPointAt (e.position);
        if (clickedPoint > 0 && clickedPoint < (int) controlPoints.size() - 1 && e.mods.isShiftDown())
        {
            controlPoints.erase (controlPoints.begin() + clickedPoint);
            rebuildOffsetCurve();
            notifyChange();
            repaint();
            return;
        }

        // Double-click on empty space or endpoint = RESET to linear
        if (clickedPoint < 0 || clickedPoint == 0 || clickedPoint == (int) controlPoints.size() - 1)
        {
            resetToLinear();
            globalOffsetSlider.setValue (0.0);
            processor.setGlobalBandOffset (0.0f);
            rebuildOffsetCurve();
            notifyChange();
            repaint();
            return;
        }

        if (!plotArea.contains (e.position))
            return;

        // Add a new control point
        float normX = (e.position.x - plotArea.getX()) / plotArea.getWidth();
        float normY = 1.0f - (e.position.y - plotArea.getY()) / plotArea.getHeight();

        if (std::isnan (normX) || std::isnan (normY) || std::isinf (normX) || std::isinf (normY))
            return;

        normX = juce::jlimit (0.02f, 0.98f, normX);
        normY = juce::jlimit (0.0f, 1.0f, normY);

        // Find where to insert (keep sorted by X)
        size_t insertIdx = 1;
        for (size_t i = 1; i < controlPoints.size(); ++i)
        {
            if (controlPoints[i].x > normX)
            {
                insertIdx = i;
                break;
            }
            if (i == controlPoints.size() - 1)
                insertIdx = i;
        }

        insertIdx = juce::jlimit ((size_t) 1, controlPoints.size() - 1, insertIdx);

        if (controlPoints.size() >= 20)
            return;

        controlPoints.insert (controlPoints.begin() + (int) insertIdx, juce::Point<float> (normX, normY));
        rebuildOffsetCurve();
        notifyChange();
        repaint();
    }

    void resetToLinear()
    {
        controlPoints.clear();
        controlPoints.push_back (juce::Point<float> (0.0f, 0.0f));   // Start: Mod 1 -> Carrier 1
        controlPoints.push_back (juce::Point<float> (0.5f, 0.5f));   // Middle
        controlPoints.push_back (juce::Point<float> (1.0f, 1.0f));   // End: Mod N -> Carrier N
        rebuildOffsetCurve();
    }

    const std::vector<float>& getOffsetCurve() const { return offsetCurve; }

    void setOffsetCurve (const std::vector<float>& curve)
    {
        if (curve.size() < 2)
            return;

        controlPoints.clear();
        controlPoints.push_back (juce::Point<float> (0.0f, curve.front()));
        controlPoints.push_back (juce::Point<float> (0.5f, curve[curve.size() / 2]));
        controlPoints.push_back (juce::Point<float> (1.0f, curve.back()));

        offsetCurve = curve;
        repaint();
    }

    const std::vector<juce::Point<float>>& getControlPoints() const { return controlPoints; }

    void setControlPoints (const std::vector<juce::Point<float>>& points)
    {
        if (points.size() >= 2)
        {
            controlPoints = points;
            rebuildOffsetCurve();
            repaint();
        }
    }

    std::function<void(const std::vector<float>&)> onOffsetChanged;

private:
    ColorVocoderAudioProcessor& processor;

    std::vector<juce::Point<float>> controlPoints;
    std::vector<float> offsetCurve;

    mutable juce::Rectangle<float> plotArea;
    bool isDragging = false;
    int draggedPoint = -1;
    int hoveredPoint = -1;

    // Global offset slider and reset button
    juce::Slider globalOffsetSlider;
    juce::TextButton resetButton;

    int findPointAt (juce::Point<float> pos) const
    {
        const float hitRadius = 12.0f;

        for (int i = 0; i < (int) controlPoints.size(); ++i)
        {
            auto& pt = controlPoints[(size_t) i];
            float x = plotArea.getX() + pt.x * plotArea.getWidth();
            float y = plotArea.getBottom() - pt.y * plotArea.getHeight();

            if (pos.getDistanceFrom (juce::Point<float> (x, y)) < hitRadius)
                return i;
        }
        return -1;
    }

    float catmullRom (float p0, float p1, float p2, float p3, float t) const
    {
        t = juce::jlimit (0.0f, 1.0f, t);

        float t2 = t * t;
        float t3 = t2 * t;
        float result = 0.5f * ((2.0f * p1) +
                               (-p0 + p2) * t +
                               (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                               (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);

        if (std::isnan (result) || std::isinf (result))
            return (p1 + p2) * 0.5f;

        return result;
    }

    void rebuildOffsetCurve()
    {
        offsetCurve.resize (101);

        if (controlPoints.size() < 2)
        {
            for (int i = 0; i < 101; ++i)
                offsetCurve[(size_t) i] = (float) i / 100.0f;
            return;
        }

        for (int i = 0; i < 101; ++i)
        {
            float x = (float) i / 100.0f;

            int segIdx = 0;
            for (int j = 0; j < (int) controlPoints.size() - 1; ++j)
            {
                if (x >= controlPoints[(size_t) j].x && x <= controlPoints[(size_t) (j + 1)].x)
                {
                    segIdx = j;
                    break;
                }
                segIdx = j;
            }

            int i0 = juce::jmax (0, segIdx - 1);
            int i1 = segIdx;
            int i2 = juce::jmin ((int) controlPoints.size() - 1, segIdx + 1);
            int i3 = juce::jmin ((int) controlPoints.size() - 1, segIdx + 2);

            float x1 = controlPoints[(size_t) i1].x;
            float x2 = controlPoints[(size_t) i2].x;
            float t = (x2 > x1) ? (x - x1) / (x2 - x1) : 0.0f;
            t = juce::jlimit (0.0f, 1.0f, t);

            float y = catmullRom (
                controlPoints[(size_t) i0].y,
                controlPoints[(size_t) i1].y,
                controlPoints[(size_t) i2].y,
                controlPoints[(size_t) i3].y,
                t
            );

            offsetCurve[(size_t) i] = juce::jlimit (0.0f, 1.0f, y);
        }
    }

    void notifyChange()
    {
        if (onOffsetChanged)
            onOffsetChanged (offsetCurve);
    }
};

//==============================================================================
// PerBandQCarousel - Carousel-style per-band Q editor with iOS-like momentum
// Shows band filter shapes with perspective effect
// Center band in focus, scroll with momentum to navigate between bands
// Note name fades in after scrolling stops
//==============================================================================
class PerBandQCarousel : public juce::Component,
                         public juce::Timer
{
public:
    PerBandQCarousel (ColorVocoderAudioProcessor& p)
        : processor (p)
    {
        // Initialize position
        displayPosition = 0.0f;
        scrollVelocity = 0.0f;
        noteNameAlpha = 1.0f;
        lastScrollTime = juce::Time::getMillisecondCounter();

        // Enable mouse wheel events
        setWantsKeyboardFocus (true);

        // Q slider REMOVED per plan - use drag/scroll interaction only
        // The qSlider member is kept for compatibility but hidden
        qSlider.setVisible (false);

        // Lock button - drawn padlock icon, click to reset Q to default
        lockButton.onClick = [this]()
        {
            int bandIdx = getActualBandIndex (juce::roundToInt (displayPosition));
            if (processor.isPerBandQLocked (bandIdx))
            {
                processor.setPerBandQLocked (bandIdx, false);  // Unlock + reset Q
                updateLockButtonAppearance();
                repaint();
            }
        };
        lockButton.setVisible (false);

        // ===== NOTE FILTER BUTTONS =====
        // Create 12 toggle buttons for pitch classes (C, C#, D, D#, E, F, F#, G, G#, A, A#, B)
        static const char* noteLabels[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < 12; ++i)
        {
            noteFilterButtons[i].setButtonText (noteLabels[i]);
            noteFilterButtons[i].setClickingTogglesState (true);
            noteFilterButtons[i].setToggleState (false, juce::dontSendNotification);
            noteFilterButtons[i].setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF2A2A35));
            noteFilterButtons[i].setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFFC44569));
            noteFilterButtons[i].setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            noteFilterButtons[i].setColour (juce::TextButton::textColourOffId, juce::Colour (0xFF888888));
            noteFilterButtons[i].onClick = [this]() { updateFilteredBands(); repaint(); };
            addAndMakeVisible (noteFilterButtons[i]);
        }

        // Clear filter button
        clearFilterButton.setButtonText ("All");
        clearFilterButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xFF3A3A45));
        clearFilterButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        clearFilterButton.onClick = [this]()
        {
            for (int i = 0; i < 12; ++i)
                noteFilterButtons[i].setToggleState (false, juce::dontSendNotification);
            updateFilteredBands();
            repaint();
        };
        addAndMakeVisible (clearFilterButton);

        startTimerHz (60);  // 60fps for smooth momentum animation
    }

    ~PerBandQCarousel() override
    {
        stopTimer();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Dark background
        g.setColour (juce::Colour (0xFF1A1A1F));
        g.fillRoundedRectangle (bounds, 6.0f);

        // Border
        g.setColour (juce::Colour (0xFF3A3A45));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

        // Get band frequencies from processor
        std::vector<float> freqs, envs;
        float lowHz, highHz;
        processor.getBandSnapshot (freqs, envs, lowHz, highHz, &cachedPrimaryIndices);

        // Get merge group sizes for stacking display (×N labels)
        std::vector<int> groupSizes;
        processor.getBandMergeGroupSizes (groupSizes);

        // ===== REFRESH NOTE FILTER from latest freqs =====
        // Rebuild filteredBandIndices every frame using the freqs we just got.
        // This fixes the race where updateFilteredBands() failed to acquire
        // bandsLock (audio thread in processBlock) and left the filter empty.
        if (isFilterActive() && !freqs.empty())
        {
            filteredBandIndices.clear();
            for (int i = 0; i < (int) freqs.size(); ++i)
            {
                int pitchClass = frequencyToPitchClass (freqs[(size_t) i]);
                if (noteFilterButtons[pitchClass].getToggleState())
                    filteredBandIndices.push_back (i);
            }
        }

        // ===== UPDATE NOTE FILTER BUTTON COLORS based on scale =====
        auto scaleFlags = processor.getScaleNoteFlags();
        for (int i = 0; i < 12; ++i)
        {
            bool inScale = scaleFlags[(size_t) i];
            // Notes in scale: bright text, notes outside scale: dim
            noteFilterButtons[i].setColour (juce::TextButton::textColourOffId,
                inScale ? juce::Colour (0xFFCCCCCC) : juce::Colour (0xFF555555));
        }

        // Get displayable band count (filtered or all)
        // When no filter is active, use actual unique band count from snapshot
        // (not paramBandCount, which includes merged secondaries)
        int displayBandCount;
        if (isFilterActive())
        {
            // Filter is active — use only matched bands (could be 0)
            displayBandCount = (int) filteredBandIndices.size();
        }
        else if (!freqs.empty())
        {
            // No filter — show all primary bands from snapshot
            displayBandCount = (int) freqs.size();
        }
        else
        {
            displayBandCount = getDisplayableBandCount();
        }

        if (displayBandCount <= 0)
        {
            // No bands match filter - show message
            g.setColour (juce::Colour (0xFF888888));
            g.setFont (14.0f);
            juce::String msg = isFilterActive() ? "No bands match this note" : "No bands";
            g.drawText (msg, bounds, juce::Justification::centred);
            return;
        }

        // Clamp display position to filtered range
        displayPosition = juce::jlimit (0.0f, (float) (displayBandCount - 1), displayPosition);
        int centerDisplayIdx = juce::roundToInt (displayPosition);

        // Helper: map a filtered display index to the actual freqs array index
        // When note filter is active, displayIdx is a position in the filtered list,
        // not a direct index into the freqs array
        auto toFreqIdx = [&](int displayIdx) -> int {
            if (! filteredBandIndices.empty() && displayIdx >= 0 && displayIdx < (int) filteredBandIndices.size())
                return filteredBandIndices[(size_t) displayIdx];
            return displayIdx;
        };

        // Get current sample rate for filter calculations
        double sampleRate = processor.getCurrentSampleRate();
        if (sampleRate <= 0.0)
            sampleRate = 44100.0;

        // Carousel area - leave room for lock button and note filter buttons at bottom
        float carouselBottom = bounds.getHeight() - 26.0f;
        juce::Rectangle<float> carouselArea (10.0f, 10.0f, bounds.getWidth() - 20.0f, carouselBottom - 10.0f);

        // ===== SPECTRUM BACKGROUND WITH VIGNETTE =====
        drawSpectrumBackground (g, carouselArea, toFreqIdx (centerDisplayIdx), freqs);

        // Draw bands with carousel perspective effect
        float centerX = carouselArea.getCentreX();
        float bandWidth = carouselArea.getWidth() * 0.18f;
        float bandHeight = carouselArea.getHeight() * 0.85f;
        float bandCenterY = carouselArea.getCentreY();

        // Calculate fractional offset from displayPosition
        float fractionalOffset = displayPosition - (float) centerDisplayIdx;

        // Compute visual Q from bandWidth param directly (GUI-thread safe, no audio dependency)
        float bandWidthParam = processor.apvts.getRawParameterValue ("bandWidth")->load();
        float bwNorm = (bandWidthParam - 0.1f) / (2.0f - 0.1f);
        bwNorm = juce::jlimit (0.0f, 1.0f, bwNorm);
        float visualGlobalQ = 1.0f * std::pow (60.0f, 1.0f - bwNorm);

        // Draw bands from back to front (furthest first)
        for (int offset = 4; offset >= -4; --offset)
        {
            int displayIdx = centerDisplayIdx + offset;
            if (displayIdx < 0 || displayIdx >= displayBandCount)
                continue;

            // Get actual band index (accounts for filtering)
            int bandIdx = getActualBandIndex (displayIdx);

            // Use smooth fractional position for animation
            float effectiveOffset = (float) offset - fractionalOffset;
            float absOffset = std::abs (effectiveOffset);

            // Perspective calculations with smooth interpolation
            float scale = 1.0f - absOffset * 0.12f;
            float alpha = 1.0f - absOffset * 0.22f;
            float xOffset = effectiveOffset * bandWidth * 0.65f;

            if (scale <= 0.0f || alpha <= 0.0f)
                continue;

            scale = juce::jlimit (0.15f, 1.0f, scale);
            alpha = juce::jlimit (0.0f, 1.0f, alpha);

            // Get this band's frequency and Q — use toFreqIdx to map filtered position to freqs index
            int freqIdx = toFreqIdx (displayIdx);
            float freq = (freqIdx < (int) freqs.size()) ? freqs[(size_t) freqIdx] : 1000.0f;
            float q = juce::jlimit (1.0f, 60.0f, visualGlobalQ * processor.getPerBandQMultiplier (bandIdx));
            bool isLocked = processor.isPerBandQLocked (bandIdx);
            int stackCount = (freqIdx < (int) groupSizes.size()) ? groupSizes[(size_t) freqIdx] : 1;

            // Draw filter shape
            // WIDTH scales for carousel perspective (narrower = further away)
            // HEIGHT has SUBTLE scale (0.8 to 1.0) - center slightly bigger
            // But Q value is the PRIMARY factor for visual height within the shape
            float shapeW = bandWidth * scale;
            float heightScale = 0.8f + 0.2f * scale;  // Side bands 80%, center 100%
            float gain = processor.getBandGain(bandIdx);
            float fullShapeH = bandHeight * heightScale;
            float shapeH = fullShapeH * gain;
            float shapeX = centerX + xOffset - shapeW * 0.5f;
            float shapeBottom = bandCenterY + fullShapeH * 0.5f;
            float shapeY = shapeBottom - shapeH;

            bool isCenter = (absOffset < 0.5f);

            // Draw filter curve path
            juce::Path filterPath;
            drawBandpassFilterShape (filterPath, shapeX, shapeY, shapeW, shapeH, freq, q, sampleRate);

            // Stacked bands get slightly thicker strokes
            float strokeBonus = (stackCount > 1) ? 0.5f : 0.0f;

            // THIN STROKE LINES (no fill) - cleaner, more professional look
            // Lipstick red for center band, dark red/grey for others
            if (isCenter)
            {
                // Center band: bright lipstick red with thicker stroke
                float centerBlend = 1.0f - absOffset * 2.0f;
                centerBlend = juce::jlimit (0.0f, 1.0f, centerBlend);
                g.setColour (juce::Colour (0xFFC44569).withAlpha (alpha * centerBlend));
                g.strokePath (filterPath, juce::PathStrokeType (2.5f + strokeBonus));

                if (gain < 0.99f)
                {
                    g.setColour (juce::Colour (0xFF64D2FF).withAlpha (0.9f));
                    g.setFont (10.0f);
                    g.drawText (juce::String (juce::roundToInt (gain * 100)) + "%",
                                (int) (shapeX + shapeW * 0.5f - 15), (int) shapeY - 14, 30, 12,
                                juce::Justification::centred);
                }
            }
            else
            {
                // Non-center bands: thin dark red/grey lines
                juce::Colour lineColor = juce::Colour (0xFF8B4444).withAlpha (alpha * 0.7f);
                g.setColour (lineColor);
                g.strokePath (filterPath, juce::PathStrokeType (1.5f + strokeBonus));
            }

            // ×N stacking label for bands with multiple merged bands
            if (stackCount > 1 && alpha > 0.3f)
            {
                g.setColour (juce::Colour (0xFF00DDFF).withAlpha (alpha * 0.8f));
                g.setFont (juce::Font (10.0f * scale, juce::Font::bold));
                g.drawText ("x" + juce::String (stackCount),
                            (int) (shapeX + shapeW * 0.5f - 10), (int) shapeY + 2, 20, 14,
                            juce::Justification::centred);
            }

            // Lock icon for locked bands (drawn padlock)
            if (isLocked)
            {
                g.setColour (juce::Colour (0xFFD4A843).withAlpha (alpha));
                float lockSize = 12.0f * scale;
                drawLockIcon (g, shapeX + shapeW - lockSize - 2.0f, shapeY + 2.0f, lockSize, true, alpha);
            }
        }

        // ===== LEFT/RIGHT EDGE FADES =====
        {
            juce::Colour bgCol (0xFF1A1A1F);
            // Left fade: 121px wide
            juce::ColourGradient leftFade (bgCol, carouselArea.getX(), 0.0f,
                                           bgCol.withAlpha (0.0f), carouselArea.getX() + 121.0f, 0.0f, false);
            g.setGradientFill (leftFade);
            g.fillRect (carouselArea.getX(), carouselArea.getY(), 121.0f, carouselArea.getHeight());

            // Right fade: 108px wide
            juce::ColourGradient rightFade (bgCol.withAlpha (0.0f), carouselArea.getRight() - 108.0f, 0.0f,
                                            bgCol, carouselArea.getRight(), 0.0f, false);
            g.setGradientFill (rightFade);
            g.fillRect (carouselArea.getRight() - 108.0f, carouselArea.getY(), 108.0f, carouselArea.getHeight());
        }

        // Draw center band info — use toFreqIdx to get correct frequency
        int centerFreqIdx = toFreqIdx (centerDisplayIdx);
        float centerFreq = (centerFreqIdx < (int) freqs.size()) ? freqs[(size_t) centerFreqIdx] : 1000.0f;

        g.setColour (juce::Colours::white);
        g.setFont (14.0f);

        // Show the actual band number (1-based), not the filtered position
        juce::String bandInfo = "Band " + juce::String (centerFreqIdx + 1);
        g.drawText (bandInfo, carouselArea.getX(), carouselArea.getBottom() - 48, carouselArea.getWidth(), 20, juce::Justification::centred);

        // Frequency display
        juce::String freqInfo;
        if (centerFreq >= 1000.0f)
            freqInfo = juce::String (centerFreq / 1000.0f, 2) + " kHz";
        else
            freqInfo = juce::String ((int) centerFreq) + " Hz";

        g.setFont (12.0f);
        g.setColour (juce::Colour (0xFFAAAAAA));
        g.drawText (freqInfo, carouselArea.getX(), carouselArea.getBottom() - 32, carouselArea.getWidth(), 16, juce::Justification::centred);

        // ===== NOTE NAME with fade-in after scroll stops =====
        if (noteNameAlpha > 0.01f)
        {
            juce::String noteName = frequencyToNoteName (centerFreq);
            g.setColour (juce::Colour (0xFF64D2FF).withAlpha (noteNameAlpha * 0.9f));
            g.setFont (juce::Font (11.0f, juce::Font::bold));
            g.drawText (noteName, carouselArea.getX(), carouselArea.getBottom() - 16, carouselArea.getWidth(), 14, juce::Justification::centred);
        }

        // Navigation hints (use display indices for filtered navigation)
        float navAlpha = std::abs (scrollVelocity) < 0.1f ? 0.4f : 0.15f;
        g.setColour (juce::Colour (0xFFFFFFFF).withAlpha (navAlpha));
        g.setFont (12.0f);
        if (centerDisplayIdx > 0)
            g.drawText ("<", carouselArea.getX() + 5, (int) bandCenterY - 8, 24, 16, juce::Justification::centred);
        if (centerDisplayIdx < displayBandCount - 1)
            g.drawText (">", (int) carouselArea.getRight() - 29, (int) bandCenterY - 8, 24, 16, juce::Justification::centred);

        // Q label next to slider
        g.setColour (juce::Colours::white);
        // Q label removed - Q slider hidden per plan
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // ===== NOTE FILTER BUTTONS at bottom =====
        int buttonH = 18;
        int buttonW = 22;  // Narrow for sharps
        int naturalW = 26; // Slightly wider for naturals
        int spacing = 2;
        int bottomY = bounds.getHeight() - buttonH - 4;

        // Calculate total width for centering
        // C, D, E, F, G, A, B = 7 naturals, C#, D#, F#, G#, A# = 5 sharps
        int totalWidth = 7 * naturalW + 5 * buttonW + 12 * spacing + 30; // +30 for "All" button
        int startX = (bounds.getWidth() - totalWidth) / 2;

        // Position buttons: C C# D D# E F F# G G# A A# B [All]
        int x = startX;
        for (int i = 0; i < 12; ++i)
        {
            bool isSharp = (i == 1 || i == 3 || i == 6 || i == 8 || i == 10);
            int w = isSharp ? buttonW : naturalW;
            noteFilterButtons[i].setBounds (x, bottomY, w, buttonH);
            x += w + spacing;
        }
        clearFilterButton.setBounds (x + 4, bottomY, 28, buttonH);

        // Lock button - positioned inside carousel area
        lockButton.setBounds (292, 0, 23, 23);
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        int bandCount = getDisplayableBandCount();
        if (bandCount <= 0) return;

        float deltaX = wheel.deltaX;  // Horizontal (two-finger left/right)
        float deltaY = wheel.deltaY;  // Vertical (two-finger up/down)

        auto now = juce::Time::getMillisecondCounter();

        // ===== GESTURE-LEVEL AXIS LOCKING =====
        // Once a scroll gesture begins, lock to the dominant axis for its
        // entire duration. This prevents horizontal swipes from accidentally
        // triggering Q changes when a single event has slightly more deltaY.
        // A gesture is considered "new" if >200ms have passed since last event.
        bool isNewGesture = (now - lastWheelEventTime) > 200;

        if (isNewGesture)
        {
            // Accumulate initial deltas to determine direction
            gestureAccumX = 0.0f;
            gestureAccumY = 0.0f;
            gestureLocked = false;
            gestureIsVertical = false;
        }

        lastWheelEventTime = now;

        if (! gestureLocked)
        {
            gestureAccumX += std::abs (deltaX);
            gestureAccumY += std::abs (deltaY);

            // Lock once we have enough total movement to be confident
            float totalMovement = gestureAccumX + gestureAccumY;
            if (totalMovement > 0.02f)
            {
                gestureIsVertical = gestureAccumY > gestureAccumX * 1.5f;
                gestureLocked = true;
            }
            else
            {
                return;  // Not enough data yet, wait for more events
            }
        }

        // Q adjustment only allowed when carousel is settled on a band
        // (no scroll momentum AND position is snapped to an integer)
        bool isSettled = std::abs (scrollVelocity) < 0.1f
                      && std::abs (displayPosition - std::round (displayPosition)) < 0.05f;

        if (gestureIsVertical && isSettled && std::abs (deltaY) > 0.001f)
        {
            // ===== VERTICAL SCROLL = CHANGE Q of center band =====
            int actualBandIdx = getActualBandIndex (juce::roundToInt (displayPosition));

            // Auto-lock band on first Q scroll (so the change persists)
            if (! processor.isPerBandQLocked (actualBandIdx))
            {
                processor.setPerBandQLocked (actualBandIdx, true);
                updateLockButtonAppearance();
            }

            float currentQ = processor.getPerBandQ (actualBandIdx);
            float qSensitivity = 30.0f;
            if (! wheel.isSmooth && ! wheel.isInertial)
                qSensitivity = 3.0f;

            float newQ = currentQ + deltaY * qSensitivity;
            newQ = juce::jlimit (1.0f, 60.0f, newQ);
            processor.setPerBandQ (actualBandIdx, newQ);
            qSlider.setValue (newQ, juce::dontSendNotification);
            repaint();
        }
        else if (! gestureIsVertical && std::abs (deltaX) > 0.001f)
        {
            // ===== HORIZONTAL SCROLL = NAVIGATE between bands =====
            float rawDelta = -deltaX;

            if (! wheel.isSmooth && ! wheel.isInertial)
                scrollVelocity = (rawDelta > 0) ? 5.0f : -5.0f;
            else
                scrollVelocity += rawDelta * 15.0f;

            scrollVelocity = juce::jlimit (-30.0f, 30.0f, scrollVelocity);
            lastScrollTime = juce::Time::getMillisecondCounter();
            noteNameAlpha = 0.0f;
            repaint();
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Stop momentum on click
        scrollVelocity = 0.0f;

        auto bounds = getLocalBounds().toFloat();
        float carouselBottom = bounds.getHeight() - 26.0f;  // Account for note filter buttons

        int bandCount = getDisplayableBandCount();
        if (bandCount <= 0) return;

        // Any click in the carousel area (above note filter buttons) starts a potential Q drag
        // The lock button is a child component, so JUCE routes its clicks separately
        bool inCarouselArea = e.position.y < carouselBottom;

        if (inCarouselArea)
        {
            // Prepare gain drag on any click — actual gain changes only happen in mouseDrag
            isDraggingQ = true;
            didDragQ = false;  // Track if user actually dragged vertically
            dragStartY = e.position.y;
            mouseDownX = e.position.x;  // Remember click X for left/right navigation fallback
            // Snap carousel to nearest band and kill momentum
            displayPosition = std::round (displayPosition);
            scrollVelocity = 0.0f;
            int actualBandIdx = getActualBandIndex (juce::roundToInt (displayPosition));
            dragStartGain = processor.getBandGain (actualBandIdx);
        }

        updateSliderFromBand();
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (isDraggingQ)
        {
            float dragDelta = e.position.y - dragStartY;

            // Only start actual gain changes after 4px vertical movement (dead zone to distinguish from clicks)
            if (! didDragQ && std::abs (dragDelta) < 4.0f)
                return;

            didDragQ = true;

            // Vertical drag to adjust gain: drag up = boost, drag down = cut
            float sensitivity = 0.005f;
            float newGain = dragStartGain - dragDelta * sensitivity;  // negative because Y increases downward
            newGain = juce::jlimit (0.0f, 1.0f, newGain);

            int actualBandIdx = getActualBandIndex (juce::roundToInt (displayPosition));
            processor.setBandGain (actualBandIdx, newGain);
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        // If user clicked without dragging vertically, handle left/right navigation
        if (isDraggingQ && ! didDragQ)
        {
            auto bounds = getLocalBounds().toFloat();
            float centerX = bounds.getCentreX();
            int bandCount = getDisplayableBandCount();

            if (mouseDownX < centerX - 40 && displayPosition > 0.0f)
            {
                scrollVelocity = -4.5f;
                lastScrollTime = juce::Time::getMillisecondCounter();
                noteNameAlpha = 0.0f;
            }
            else if (mouseDownX > centerX + 40 && displayPosition < (float) (bandCount - 1))
            {
                scrollVelocity = 4.5f;
                lastScrollTime = juce::Time::getMillisecondCounter();
                noteNameAlpha = 0.0f;
            }
        }
        isDraggingQ = false;
        didDragQ = false;
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        // Double-click resets band gain AND Q to defaults
        int bandIdx = getActualBandIndex (juce::roundToInt (displayPosition));
        processor.setBandGain (bandIdx, 1.0f);
        processor.setPerBandQLocked (bandIdx, false);
        updateLockButtonAppearance();
        repaint();
    }

    void timerCallback() override
    {
        bool needsRepaint = false;

        // Use filtered band count
        int bandCount = getDisplayableBandCount();
        if (bandCount <= 0) return;

        // ===== MOMENTUM PHYSICS (Apple-like, buttery smooth) =====
        if (std::abs (scrollVelocity) > 0.01f)
        {
            // Apply velocity to position
            displayPosition += scrollVelocity * 0.018f;

            // Clamp position with bounce-back feel at edges
            if (displayPosition < 0.0f)
            {
                displayPosition = 0.0f;
                scrollVelocity = 0.0f;
            }
            else if (displayPosition > (float) (bandCount - 1))
            {
                displayPosition = (float) (bandCount - 1);
                scrollVelocity = 0.0f;
            }

            // Adaptive friction: fast = longer coast, slow = quicker settle
            float friction = (std::abs (scrollVelocity) > 3.0f) ? 0.92f : 0.87f;
            scrollVelocity *= friction;

            // Let momentum coast longer before snapping
            if (std::abs (scrollVelocity) < 0.04f)
            {
                scrollVelocity = 0.0f;
            }

            needsRepaint = true;
        }
        else
        {
            // When velocity is zero, smoothly snap to nearest integer band
            int nearestBand = juce::roundToInt (displayPosition);
            float diff = (float) nearestBand - displayPosition;
            if (std::abs (diff) > 0.001f)
            {
                displayPosition += diff * 0.12f;  // Gentler, slower settle
                needsRepaint = true;
            }
            else
            {
                displayPosition = (float) nearestBand;
            }
        }

        // ===== NOTE NAME FADE-IN AFTER SCROLL STOPS =====
        juce::uint32 now = juce::Time::getMillisecondCounter();
        juce::uint32 timeSinceScroll = now - lastScrollTime;

        if (std::abs (scrollVelocity) < 0.1f && timeSinceScroll > 200)
        {
            // Fade in note name after 0.2s of stopping
            float targetAlpha = 1.0f;
            if (noteNameAlpha < targetAlpha)
            {
                noteNameAlpha += 0.08f;  // Smooth fade in
                noteNameAlpha = juce::jmin (noteNameAlpha, targetAlpha);
                needsRepaint = true;
            }
        }
        else
        {
            // Hide during scroll
            if (noteNameAlpha > 0.0f)
            {
                noteNameAlpha = 0.0f;
                needsRepaint = true;
            }
        }

        // Update slider when band changes
        int currentBandIdx = juce::roundToInt (displayPosition);
        if (currentBandIdx != lastDisplayedBand)
        {
            lastDisplayedBand = currentBandIdx;
            updateSliderFromBand();
        }

        // Repaint when bandWidth (width knob) changes so shapes update visually
        float currentBW = processor.apvts.getRawParameterValue ("bandWidth")->load();
        if (std::abs (currentBW - lastBandWidthParam) > 0.001f)
        {
            lastBandWidthParam = currentBW;
            needsRepaint = true;
        }

        if (needsRepaint)
            repaint();
    }

    void setCurrentBand (int bandIndex)
    {
        // If filtering is active, find the display position for this band
        if (!filteredBandIndices.empty())
        {
            for (int i = 0; i < (int) filteredBandIndices.size(); ++i)
            {
                if (filteredBandIndices[(size_t) i] == bandIndex)
                {
                    displayPosition = (float) i;
                    break;
                }
            }
        }
        else
        {
            int bandCount = getDisplayableBandCount();
            displayPosition = (float) juce::jlimit (0, bandCount - 1, bandIndex);
        }
        scrollVelocity = 0.0f;
        updateSliderFromBand();
        repaint();
    }

    // Returns the ACTUAL band index (not display position)
    int getCurrentBand() const { return getActualBandIndex (juce::roundToInt (displayPosition)); }

private:
    ColorVocoderAudioProcessor& processor;

    // Momentum scrolling state
    float displayPosition = 0.0f;      // Smooth band position (can be fractional) - INDEX INTO FILTERED BANDS
    float scrollVelocity = 0.0f;       // Current scroll velocity
    juce::uint32 lastScrollTime = 0;   // Time of last scroll input

    // Gesture-level axis locking (prevents accidental Q changes during horizontal scroll)
    juce::uint32 lastWheelEventTime = 0;
    float gestureAccumX = 0.0f;
    float gestureAccumY = 0.0f;
    bool gestureLocked = false;
    bool gestureIsVertical = false;
    float noteNameAlpha = 1.0f;        // Fade-in alpha for note name
    int lastDisplayedBand = -1;        // Track band changes for slider update
    float lastBandWidthParam = -1.0f;  // Track bandWidth changes for repaint

    // Drag state (drag on center band to adjust gain)
    bool isDraggingQ = false;      // Reusing name for backwards compat (now controls gain, not Q)
    bool didDragQ = false;         // True once user actually drags vertically (past dead zone)
    float dragStartY = 0.0f;
    float dragStartGain = 1.0f;
    float mouseDownX = 0.0f;      // Remember click X for left/right nav fallback on mouseUp

    // Maps display index → actual band index (accounts for merged secondary bands)
    std::vector<int> cachedPrimaryIndices;

    juce::Slider qSlider;

    // Custom lock button that draws a padlock icon instead of emoji
    class LockIconButton : public juce::Component
    {
    public:
        std::function<void()> onClick;
        bool isLocked = false;
        juce::Colour iconColour { 0xFF555555 };

        void paint (juce::Graphics& g) override
        {
            float size = juce::jmin ((float) getWidth(), (float) getHeight());
            float x = (getWidth() - size) * 0.5f;
            float y = (getHeight() - size) * 0.5f;
            g.setColour (iconColour);
            drawLockIcon (g, x, y, size, isLocked, 1.0f);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (contains (e.getPosition()) && onClick)
                onClick();
        }

        void mouseEnter (const juce::MouseEvent&) override { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    };

    LockIconButton lockButton;

    // ===== NOTE FILTER SYSTEM =====
    juce::TextButton noteFilterButtons[12];  // One button per pitch class
    juce::TextButton clearFilterButton;      // "All" button to clear filter
    std::vector<int> filteredBandIndices;    // Indices of bands matching filter (empty = show all)

    // Get actual band index from display position (accounts for filtering + merged bands)
    int getActualBandIndex (int filteredIdx) const
    {
        if (! filteredBandIndices.empty())
        {
            if (filteredIdx < 0 || filteredIdx >= (int) filteredBandIndices.size())
                return 0;
            int displayIdx = filteredBandIndices[(size_t) filteredIdx];
            // Map through primary indices to get actual band index
            if (displayIdx >= 0 && displayIdx < (int) cachedPrimaryIndices.size())
                return cachedPrimaryIndices[(size_t) displayIdx];
            return displayIdx;
        }
        // No filter: map display index through primary band indices
        if (filteredIdx >= 0 && filteredIdx < (int) cachedPrimaryIndices.size())
            return cachedPrimaryIndices[(size_t) filteredIdx];
        return filteredIdx;
    }

    // Get the pitch class (0-11) for a given frequency
    static int frequencyToPitchClass (float freq)
    {
        if (freq <= 0.0f) return 0;
        float midiNote = 12.0f * std::log2 (freq / 440.0f) + 69.0f;
        int noteNum = juce::roundToInt (midiNote);
        return ((noteNum % 12) + 12) % 12;  // Ensure positive modulo
    }

    // Check if any note filter is active
    bool isFilterActive() const
    {
        for (int i = 0; i < 12; ++i)
            if (noteFilterButtons[i].getToggleState())
                return true;
        return false;
    }

    // Update the filtered band indices based on selected notes
    void updateFilteredBands()
    {
        filteredBandIndices.clear();

        if (!isFilterActive())
            return;  // No filter = show all bands (empty vector signals this)

        // Get band frequencies from processor
        std::vector<float> freqs, envs;
        float lowHz, highHz;
        processor.getBandSnapshot (freqs, envs, lowHz, highHz);

        // Build list of bands matching selected pitch classes
        for (int i = 0; i < (int) freqs.size(); ++i)
        {
            int pitchClass = frequencyToPitchClass (freqs[(size_t) i]);
            if (noteFilterButtons[pitchClass].getToggleState())
                filteredBandIndices.push_back (i);
        }

        // Reset display position to valid range
        if (!filteredBandIndices.empty())
        {
            displayPosition = juce::jlimit (0.0f, (float) (filteredBandIndices.size() - 1), displayPosition);
        }
    }

    // Get total number of displayable bands (filtered or all)
    int getDisplayableBandCount() const
    {
        if (isFilterActive())
        {
            // Filter active — return exact match count (could be 0)
            return (int) filteredBandIndices.size();
        }
        // No filter — show all unique bands
        int unique = processor.getUniqueBandCount();
        if (unique > 0)
            return unique;
        // Fallback: if no bands built yet, use parameter
        if (auto* param = processor.apvts.getRawParameterValue ("bandCount"))
            return (int) param->load();
        return 16;
    }

    void updateSliderFromBand()
    {
        // Get actual band index (accounts for filtering)
        int actualBandIdx = getActualBandIndex (juce::roundToInt (displayPosition));
        float q = processor.getPerBandQ (actualBandIdx);
        qSlider.setValue (q, juce::dontSendNotification);
        updateLockButtonAppearance();
    }

    // Draw a lock/padlock icon at the given position using JUCE Path graphics
    static void drawLockIcon (juce::Graphics& g, float x, float y, float size, bool locked, float alpha)
    {
        float bodyW = size * 0.65f;
        float bodyH = size * 0.42f;
        float bodyX = x + (size - bodyW) * 0.5f;
        float bodyY = y + size * 0.55f;

        // Lock body (rounded rect)
        g.setOpacity (alpha);
        g.fillRoundedRectangle (bodyX, bodyY, bodyW, bodyH, 2.0f);

        // Shackle (U-shape arc on top)
        juce::Path shackle;
        float shackleW = bodyW * 0.55f;
        float shackleH = size * 0.35f;
        float shackleX = x + (size - shackleW) * 0.5f;
        float shackleYTop = bodyY - shackleH;

        if (locked)
        {
            // Closed: centered U on top of body
            shackle.addArc (shackleX, shackleYTop, shackleW, shackleH * 2.0f,
                            juce::MathConstants<float>::pi, 0.0f, true);
        }
        else
        {
            // Open: shift shackle up and slightly right
            shackle.addArc (shackleX, shackleYTop - size * 0.12f, shackleW, shackleH * 2.0f,
                            juce::MathConstants<float>::pi, -0.1f, true);
        }

        g.strokePath (shackle, juce::PathStrokeType (juce::jmax (1.5f, size * 0.1f)));
    }

    void updateLockButtonAppearance()
    {
        // Get actual band index (accounts for filtering)
        int actualBandIdx = getActualBandIndex (juce::roundToInt (displayPosition));
        bool locked = processor.isPerBandQLocked (actualBandIdx);
        lockButton.isLocked = locked;
        lockButton.iconColour = locked ? juce::Colour (0xFFD4A843) : juce::Colour (0xFF555555);
        lockButton.repaint();
    }

    // Convert frequency to note name (e.g., "G3", "A#4")
    static juce::String frequencyToNoteName (float freq)
    {
        if (freq <= 0.0f)
            return "";

        // MIDI note number from frequency (A4 = 440Hz = note 69)
        float midiNote = 12.0f * std::log2 (freq / 440.0f) + 69.0f;
        int noteNum = juce::roundToInt (midiNote);

        if (noteNum < 0 || noteNum > 127)
            return "";

        static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        int octave = (noteNum / 12) - 1;
        int noteIdx = noteNum % 12;

        return juce::String (noteNames[noteIdx]) + juce::String (octave);
    }

    // Draw spectrum background with vignette effect
    void drawSpectrumBackground (juce::Graphics& g, juce::Rectangle<float> area,
                                  int centerBandIdx, const std::vector<float>& freqs)
    {
        // Get spectrum data from processor (if available)
        std::array<float, ColorVocoderAudioProcessor::fftSize / 2> spectrumData;
        processor.getSpectrumData (spectrumData);

        // Draw subtle spectrum bars centered around current band frequency
        float centerFreq = (centerBandIdx < (int) freqs.size()) ? freqs[(size_t) centerBandIdx] : 1000.0f;

        double sampleRate = processor.getCurrentSampleRate();
        if (sampleRate <= 0.0)
            sampleRate = 44100.0;

        // Draw a 4-octave window of spectrum centered on the current frequency
        float lowFreq = centerFreq / 4.0f;
        float highFreq = centerFreq * 4.0f;

        lowFreq = juce::jmax (20.0f, lowFreq);
        highFreq = juce::jmin (20000.0f, highFreq);

        float logLow = std::log10 (lowFreq);
        float logHigh = std::log10 (highFreq);
        float logRange = logHigh - logLow;

        if (logRange < 0.1f)
            return;

        // Draw spectrum as subtle grey/white bars
        const int numBars = 64;
        float barWidth = area.getWidth() / (float) numBars;

        for (int i = 0; i < numBars; ++i)
        {
            float t = (float) i / (float) (numBars - 1);
            float freq = std::pow (10.0f, logLow + t * logRange);

            // Get FFT bin for this frequency
            int bin = (int) (freq * (float) ColorVocoderAudioProcessor::fftSize / (float) sampleRate);
            bin = juce::jlimit (0, (int) spectrumData.size() - 1, bin);

            float magnitude = spectrumData[(size_t) bin];

            // Asymmetric smoothing would be applied in processor, just use direct value
            float barHeight = magnitude * area.getHeight() * 0.6f;
            barHeight = juce::jmin (barHeight, area.getHeight() * 0.8f);

            float x = area.getX() + i * barWidth;
            float y = area.getBottom() - barHeight;

            // Vignette effect: fade edges horizontally
            float edgeFade = 1.0f - std::pow (std::abs (t - 0.5f) * 2.0f, 2.0f);
            // Bottom fade: bars fade toward the bottom of the area
            float bottomFade = 1.0f - std::pow ((area.getBottom() - y) < 0.001f ? 0.0f : barHeight / area.getHeight(), 2.0f);
            float alpha = 0.22f * edgeFade * bottomFade;

            g.setColour (juce::Colours::white.withAlpha (alpha));
            g.fillRect (x, y, barWidth - 1.0f, barHeight);
        }

        // Apply radial vignette overlay
        juce::ColourGradient vignette (
            juce::Colours::transparentBlack,
            area.getCentreX(), area.getCentreY(),
            juce::Colours::black.withAlpha (0.5f),
            area.getX(), area.getCentreY(),
            true  // radial
        );
        g.setGradientFill (vignette);
        g.fillRect (area);
    }

    // Draw bandpass filter shape — visual only
    // Low Q (wide BW) → smooth rounded dome/semicircle
    // High Q (narrow BW) → narrow sharp triangle/spike
    void drawBandpassFilterShape (juce::Path& path, float x, float y, float w, float h,
                                   float centerFreq, float Q, double /*sampleRate*/)
    {
        path.clear();

        // Normalize Q to 0-1 range (Q ranges 1 to 60)
        float qNorm = juce::jlimit (0.0f, 1.0f, (Q - 1.0f) / 59.0f);

        // Dynamic frequency range: wider for low Q, narrower for high Q
        float octaves = 3.5f - qNorm * 1.5f;
        float freqRatio = std::pow (2.0f, octaves);
        float lowFreq = centerFreq / freqRatio;
        float highFreq = centerFreq * freqRatio;

        // Bandpass formula with sqrt-compressed Q
        float visualQ = std::sqrt (juce::jmax (1.0f, Q));

        // Power sharpening for high Q spike
        float shapePower = 1.0f + qNorm * 1.5f;

        // Blend: dome at low Q, bandpass spike at high Q
        // sqrt so dome persists through lower Q values
        float blendToBP = std::pow (qNorm, 0.5f);

        const int numPoints = 60;
        float baseline = y + h;

        for (int i = 0; i < numPoints; ++i)
        {
            float t = (float) i / (float) (numPoints - 1);
            float freq = lowFreq * std::pow (highFreq / lowFreq, t);

            // Bandpass magnitude with power sharpening
            float ratio = freq / centerFreq - centerFreq / freq;
            float bpMag = 1.0f / std::sqrt (1.0f + visualQ * visualQ * ratio * ratio);
            bpMag = std::pow (bpMag, shapePower);

            // Cosine dome: smooth semicircle (1 at center, 0 at edges)
            float domeMag = 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * (2.0f * t - 1.0f)));

            // Blend between dome and bandpass spike
            float mag = domeMag * (1.0f - blendToBP) + bpMag * blendToBP;

            float px = x + t * w;
            float py = baseline - mag * h;

            if (i == 0)
                path.startNewSubPath (px, py);
            else
                path.lineTo (px, py);
        }
    }
};

//==============================================================================
// SpectralXYPad - X/Y pad for spectral tilt control
// X = Tilt (0=low bias, 0.5=flat, 1=high bias)
// Y = Intensity (0=flat/no effect, 1=maximum tilt)
//==============================================================================
class SpectralXYPad : public juce::Component,
                      public juce::AudioProcessorValueTreeState::Listener
{
public:
    SpectralXYPad (juce::AudioProcessorValueTreeState& apvts)
        : apvtsRef (apvts)
    {
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);

        // Listen to parameter changes
        apvtsRef.addParameterListener ("spectralTilt", this);
        apvtsRef.addParameterListener ("spectralIntensity", this);

        // Initialize from current parameter values
        if (auto* tiltPtr = apvtsRef.getRawParameterValue ("spectralTilt"))
            tiltX = tiltPtr->load();
        if (auto* intPtr = apvtsRef.getRawParameterValue ("spectralIntensity"))
            intensityY = intPtr->load();
    }

    ~SpectralXYPad() override
    {
        apvtsRef.removeParameterListener ("spectralTilt", this);
        apvtsRef.removeParameterListener ("spectralIntensity", this);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Dark background
        g.setColour (juce::Colour (0xFF1A1A1F));
        g.fillRoundedRectangle (bounds, 6.0f);

        // Border
        g.setColour (juce::Colour (0xFF3A3A45));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

        // Plot area (outer)
        auto plotArea = bounds.reduced (4);

        // Dot radius (reduced by 1/3 from original 8.0f)
        float radius = 5.0f;

        // Inner movement area - dot center stays within this, so dot never clips
        auto moveArea = plotArea.reduced (radius + 1);

        // Draw dashed boundary rectangle showing movement area
        g.setColour (juce::Colour (0xFF2A2A2A));
        float dashLengths[] = { 3.0f, 3.0f };
        g.drawDashedLine (juce::Line<float> (moveArea.getX(), moveArea.getY(), moveArea.getRight(), moveArea.getY()), dashLengths, 2);
        g.drawDashedLine (juce::Line<float> (moveArea.getRight(), moveArea.getY(), moveArea.getRight(), moveArea.getBottom()), dashLengths, 2);
        g.drawDashedLine (juce::Line<float> (moveArea.getRight(), moveArea.getBottom(), moveArea.getX(), moveArea.getBottom()), dashLengths, 2);
        g.drawDashedLine (juce::Line<float> (moveArea.getX(), moveArea.getBottom(), moveArea.getX(), moveArea.getY()), dashLengths, 2);

        // Draw grid lines (within move area)
        g.setColour (juce::Colour (0xFF2A2A35));

        // Vertical center line (flat tilt)
        float centerX = moveArea.getCentreX();
        g.drawVerticalLine ((int) centerX, moveArea.getY(), moveArea.getBottom());

        // Horizontal lines at 25%, 50%, 75%
        for (float y : { 0.25f, 0.5f, 0.75f })
        {
            float lineY = moveArea.getY() + moveArea.getHeight() * (1.0f - y);
            g.drawHorizontalLine ((int) lineY, moveArea.getX(), moveArea.getRight());
        }

        // Calculate point position WITHIN moveArea (never clips)
        float pointX = moveArea.getX() + tiltX * moveArea.getWidth();
        float pointY = moveArea.getBottom() - intensityY * moveArea.getHeight();

        // Draw crosshairs through point
        g.setColour (juce::Colour (0xFF404050));
        g.drawVerticalLine ((int) pointX, moveArea.getY(), moveArea.getBottom());
        g.drawHorizontalLine ((int) pointY, moveArea.getX(), moveArea.getRight());

        // Draw glow around point (smaller glow for smaller dot)
        for (int i = 2; i > 0; --i)
        {
            float glowRadius = radius + i * 1.5f;
            g.setColour (juce::Colour (0xFF64D2FF).withAlpha (0.12f / (float) i));
            g.fillEllipse (pointX - glowRadius, pointY - glowRadius, glowRadius * 2, glowRadius * 2);
        }

        // Draw main point
        g.setColour (juce::Colour (0xFF64D2FF));
        g.fillEllipse (pointX - radius, pointY - radius, radius * 2, radius * 2);

        // Point outline
        g.setColour (juce::Colours::white);
        g.drawEllipse (pointX - radius, pointY - radius, radius * 2, radius * 2, 1.0f);

        // Draw labels (in outer plot area corners)
        g.setColour (juce::Colour (0x50FFFFFF));
        g.setFont (8.0f);
        g.drawText ("LOW", (int) plotArea.getX() + 2, (int) plotArea.getBottom() - 10, 24, 10, juce::Justification::left);
        g.drawText ("HIGH", (int) plotArea.getRight() - 26, (int) plotArea.getBottom() - 10, 24, 10, juce::Justification::right);

        // Title (faded to appear in background, doesn't clash with dot)
        g.setColour (juce::Colour (0x35FFFFFF));
        g.setFont (9.0f);
        g.drawText ("SPECTRAL", bounds.removeFromTop (12), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        updateFromMouse (e.position);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        updateFromMouse (e.position);
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        // Launch popup window instead of reset
        showPopup();
    }

    void parameterChanged (const juce::String& paramId, float newValue) override
    {
        if (paramId == "spectralTilt")
            tiltX = newValue;
        else if (paramId == "spectralIntensity")
            intensityY = newValue;

        // Trigger repaint on message thread
        juce::MessageManager::callAsync ([this]() { repaint(); });
    }

private:
    juce::AudioProcessorValueTreeState& apvtsRef;
    float tiltX = 0.5f;
    float intensityY = 0.0f;

    void updateFromMouse (juce::Point<float> pos)
    {
        auto bounds = getLocalBounds().toFloat();
        auto plotArea = bounds.reduced (4);
        float radius = 5.0f;
        auto moveArea = plotArea.reduced (radius + 1);

        // Calculate normalized position within moveArea
        float newTiltX = (pos.x - moveArea.getX()) / moveArea.getWidth();
        float newIntensityY = 1.0f - (pos.y - moveArea.getY()) / moveArea.getHeight();

        newTiltX = juce::jlimit (0.0f, 1.0f, newTiltX);
        newIntensityY = juce::jlimit (0.0f, 1.0f, newIntensityY);

        // Update parameters
        if (auto* tiltParam = apvtsRef.getParameter ("spectralTilt"))
            tiltParam->setValueNotifyingHost (newTiltX);
        if (auto* intParam = apvtsRef.getParameter ("spectralIntensity"))
            intParam->setValueNotifyingHost (newIntensityY);
    }

    void showPopup();  // Implemented after SpectralXYPadPopup is defined
};

//==============================================================================
// SpectralXYPadPopup - Resizable floating window for spectral control
//==============================================================================
class SpectralXYPadPopup : public juce::Component,
                           public juce::AudioProcessorValueTreeState::Listener
{
public:
    SpectralXYPadPopup (juce::AudioProcessorValueTreeState& apvts)
        : apvtsRef (apvts)
    {
        setWantsKeyboardFocus (false);
        setAlwaysOnTop (true);  // Keep popup above plugin window

        // Listen to parameter changes
        apvtsRef.addParameterListener ("spectralTilt", this);
        apvtsRef.addParameterListener ("spectralIntensity", this);

        // Initialize from current parameter values
        if (auto* tiltPtr = apvtsRef.getRawParameterValue ("spectralTilt"))
            tiltX = tiltPtr->load();
        if (auto* intPtr = apvtsRef.getRawParameterValue ("spectralIntensity"))
            intensityY = intPtr->load();
    }

    ~SpectralXYPadPopup() override
    {
        apvtsRef.removeParameterListener ("spectralTilt", this);
        apvtsRef.removeParameterListener ("spectralIntensity", this);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Dark background with title bar area
        g.setColour (juce::Colour (0xFF1A1A1F));
        g.fillRoundedRectangle (bounds, 8.0f);

        // Border
        g.setColour (juce::Colour (0xFF4A4A55));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.5f);

        // Title bar area
        auto titleBar = bounds.removeFromTop (24);

        // Close button (X) in top-left
        closeButtonArea = juce::Rectangle<int> (6, 6, 16, 16);
        g.setColour (juce::Colour (0xFFFF6666));
        g.fillEllipse (closeButtonArea.toFloat());
        g.setColour (juce::Colours::white);
        g.setFont (juce::Font (11.0f, juce::Font::bold));
        g.drawText ("X", closeButtonArea.toFloat(), juce::Justification::centred);

        // Title
        g.setColour (juce::Colour (0xAAFFFFFF));
        g.setFont (12.0f);
        g.drawText ("SPECTRAL TILT", titleBar.withTrimmedLeft (26), juce::Justification::centredLeft);

        // Resize grip in bottom-right
        resizeCorner = juce::Rectangle<int> (getWidth() - 16, getHeight() - 16, 14, 14);
        g.setColour (juce::Colour (0xFF4A4A55));
        for (int i = 0; i < 3; ++i)
        {
            int offset = i * 4;
            g.drawLine ((float) (resizeCorner.getRight() - 2), (float) (resizeCorner.getY() + 4 + offset),
                        (float) (resizeCorner.getX() + 4 + offset), (float) (resizeCorner.getBottom() - 2), 1.5f);
        }

        // Plot area
        padArea = getLocalBounds().reduced (10);
        padArea.removeFromTop (20);  // Below title bar
        padArea.removeFromBottom (12);  // Above resize corner

        auto plotArea = padArea.toFloat();

        // Grid background
        g.setColour (juce::Colour (0xFF252530));
        g.fillRoundedRectangle (plotArea, 4.0f);

        // Dot radius (reduced by 1/3 from original 12.0f)
        float radius = 8.0f;

        // Inner movement area - dot center stays within this, so dot never clips
        auto moveArea = plotArea.reduced (radius + 2);

        // Draw dashed boundary rectangle showing movement area
        g.setColour (juce::Colour (0xFF2A2A2A));
        float dashLengths[] = { 4.0f, 4.0f };
        g.drawDashedLine (juce::Line<float> (moveArea.getX(), moveArea.getY(), moveArea.getRight(), moveArea.getY()), dashLengths, 2);
        g.drawDashedLine (juce::Line<float> (moveArea.getRight(), moveArea.getY(), moveArea.getRight(), moveArea.getBottom()), dashLengths, 2);
        g.drawDashedLine (juce::Line<float> (moveArea.getRight(), moveArea.getBottom(), moveArea.getX(), moveArea.getBottom()), dashLengths, 2);
        g.drawDashedLine (juce::Line<float> (moveArea.getX(), moveArea.getBottom(), moveArea.getX(), moveArea.getY()), dashLengths, 2);

        // Draw grid lines (within move area)
        g.setColour (juce::Colour (0xFF3A3A45));

        // Vertical center line (flat tilt)
        float centerX = moveArea.getCentreX();
        g.drawVerticalLine ((int) centerX, moveArea.getY(), moveArea.getBottom());

        // Horizontal lines at 25%, 50%, 75%
        for (float y : { 0.25f, 0.5f, 0.75f })
        {
            float lineY = moveArea.getY() + moveArea.getHeight() * (1.0f - y);
            g.drawHorizontalLine ((int) lineY, moveArea.getX(), moveArea.getRight());
        }

        // Calculate point position WITHIN moveArea (never clips)
        float pointX = moveArea.getX() + tiltX * moveArea.getWidth();
        float pointY = moveArea.getBottom() - intensityY * moveArea.getHeight();

        // Draw crosshairs through point
        g.setColour (juce::Colour (0xFF505060));
        g.drawVerticalLine ((int) pointX, moveArea.getY(), moveArea.getBottom());
        g.drawHorizontalLine ((int) pointY, moveArea.getX(), moveArea.getRight());

        // Draw glow around point (smaller glow)
        for (int i = 3; i > 0; --i)
        {
            float glowRadius = radius + i * 2.0f;
            g.setColour (juce::Colour (0xFF64D2FF).withAlpha (0.12f / (float) i));
            g.fillEllipse (pointX - glowRadius, pointY - glowRadius, glowRadius * 2, glowRadius * 2);
        }

        // Draw main point
        g.setColour (juce::Colour (0xFF64D2FF));
        g.fillEllipse (pointX - radius, pointY - radius, radius * 2, radius * 2);

        // Point outline
        g.setColour (juce::Colours::white);
        g.drawEllipse (pointX - radius, pointY - radius, radius * 2, radius * 2, 1.5f);

        // Draw labels (in outer plot area corners)
        g.setColour (juce::Colour (0x90FFFFFF));
        g.setFont (10.0f);
        g.drawText ("LOW", (int) plotArea.getX() + 4, (int) plotArea.getBottom() - 14, 32, 12, juce::Justification::left);
        g.drawText ("HIGH", (int) plotArea.getRight() - 36, (int) plotArea.getBottom() - 14, 32, 12, juce::Justification::right);
        g.drawText ("100%", (int) plotArea.getX() + 4, (int) plotArea.getY() + 2, 36, 12, juce::Justification::left);
        g.drawText ("0%", (int) plotArea.getX() + 4, (int) plotArea.getCentreY() - 6, 24, 12, juce::Justification::left);

        // Current value display
        juce::String tiltStr = (tiltX < 0.45f) ? "Low" : (tiltX > 0.55f) ? "High" : "Flat";
        juce::String intStr = juce::String (juce::roundToInt (intensityY * 100)) + "%";
        g.setColour (juce::Colour (0xCCFFFFFF));
        g.setFont (11.0f);
        g.drawText ("Tilt: " + tiltStr + "  |  Intensity: " + intStr,
                    plotArea.getX(), plotArea.getY() - 18, plotArea.getWidth(), 16, juce::Justification::centred);
    }

    void resized() override
    {
        // Update padArea bounds
        padArea = getLocalBounds().reduced (10);
        padArea.removeFromTop (20);
        padArea.removeFromBottom (12);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Check close button
        if (closeButtonArea.toFloat().contains (e.position))
        {
            if (onClose)
                onClose();
            return;
        }

        // Check resize corner
        if (resizeCorner.toFloat().contains (e.position))
        {
            isDraggingResize = true;
            dragStartSize = { getWidth(), getHeight() };
            return;
        }

        // Check pad area
        if (padArea.toFloat().contains (e.position))
        {
            isDraggingPad = true;
            updateFromMouse (e.position);
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (isDraggingResize)
        {
            int newW = juce::jmax (160, dragStartSize.x + e.getDistanceFromDragStartX());
            int newH = juce::jmax (140, dragStartSize.y + e.getDistanceFromDragStartY());
            setSize (newW, newH);
            return;
        }

        if (isDraggingPad)
        {
            updateFromMouse (e.position);
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        isDraggingPad = false;
        isDraggingResize = false;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        // Double-click on pad resets to center (flat, no effect)
        if (padArea.toFloat().contains (e.position))
        {
            if (auto* tiltParam = apvtsRef.getParameter ("spectralTilt"))
                tiltParam->setValueNotifyingHost (0.5f);
            if (auto* intParam = apvtsRef.getParameter ("spectralIntensity"))
                intParam->setValueNotifyingHost (0.0f);
        }
    }

    void parameterChanged (const juce::String& paramId, float newValue) override
    {
        if (paramId == "spectralTilt")
            tiltX = newValue;
        else if (paramId == "spectralIntensity")
            intensityY = newValue;

        juce::MessageManager::callAsync ([this]() { repaint(); });
    }

    std::function<void()> onClose;

private:
    juce::AudioProcessorValueTreeState& apvtsRef;
    float tiltX = 0.5f;
    float intensityY = 0.0f;

    juce::Rectangle<int> closeButtonArea;
    juce::Rectangle<int> padArea;
    juce::Rectangle<int> resizeCorner;

    bool isDraggingPad = false;
    bool isDraggingResize = false;
    juce::Point<int> dragStartSize;

    void updateFromMouse (juce::Point<float> pos)
    {
        auto plotArea = padArea.toFloat();
        float radius = 8.0f;
        auto moveArea = plotArea.reduced (radius + 2);

        // Calculate normalized position within moveArea
        float newTiltX = (pos.x - moveArea.getX()) / moveArea.getWidth();
        float newIntensityY = 1.0f - (pos.y - moveArea.getY()) / moveArea.getHeight();

        newTiltX = juce::jlimit (0.0f, 1.0f, newTiltX);
        newIntensityY = juce::jlimit (0.0f, 1.0f, newIntensityY);

        if (auto* tiltParam = apvtsRef.getParameter ("spectralTilt"))
            tiltParam->setValueNotifyingHost (newTiltX);
        if (auto* intParam = apvtsRef.getParameter ("spectralIntensity"))
            intParam->setValueNotifyingHost (newIntensityY);
    }
};

// SpectralXYPad::showPopup() implementation (must be after SpectralXYPadPopup is defined)
inline void SpectralXYPad::showPopup()
{
    // Create popup window
    auto* popup = new SpectralXYPadPopup (apvtsRef);
    popup->setSize (240, 260);
    popup->addToDesktop (juce::ComponentPeer::windowIsTemporary |
                         juce::ComponentPeer::windowHasDropShadow |
                         juce::ComponentPeer::windowIsSemiTransparent);

    // Position near the small pad
    auto screenPos = localPointToGlobal (juce::Point<int> (0, 0));
    popup->setTopLeftPosition (screenPos.x - 90, screenPos.y - 100);

    // MUST call setAlwaysOnTop AFTER addToDesktop for it to work
    popup->setAlwaysOnTop (true);
    popup->setVisible (true);
    popup->toFront (true);

    // Setup close callback to delete the popup
    popup->onClose = [popup]() {
        popup->removeFromDesktop();
        delete popup;
    };
}

//==============================================================================
// KnobContextMenu – Right-click menu for knobs with professional options
//==============================================================================
class KnobContextMenu : public juce::MouseListener
{
public:
    // Static clipboard for copy/paste across all knobs
    static inline float clipboardValue = 0.0f;
    static inline bool hasClipboardValue = false;
    static inline juce::String clipboardParamId;

    // User defaults storage
    static juce::File getUserDefaultsFile()
    {
        auto appData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
        #if JUCE_MAC
        return appData.getChildFile ("Audio/Presets/BandRitual/user_defaults.json");
        #else
        return appData.getChildFile ("BandRitual/user_defaults.json");
        #endif
    }

    static std::map<juce::String, float>& getUserDefaults()
    {
        static std::map<juce::String, float> defaults;
        static bool loaded = false;
        if (!loaded)
        {
            loaded = true;
            auto file = getUserDefaultsFile();
            if (file.existsAsFile())
            {
                auto json = juce::JSON::parse (file);
                if (auto* obj = json.getDynamicObject())
                {
                    for (auto& prop : obj->getProperties())
                        defaults[prop.name.toString()] = (float) prop.value;
                }
            }
        }
        return defaults;
    }

    static void saveUserDefaults()
    {
        auto file = getUserDefaultsFile();
        file.getParentDirectory().createDirectory();

        juce::DynamicObject obj;
        for (auto& [key, value] : getUserDefaults())
            obj.setProperty (key, value);

        file.replaceWithText (juce::JSON::toString (juce::var (&obj)));
    }

    // Locked parameters (won't change when loading presets)
    static std::set<juce::String>& getLockedParams()
    {
        static std::set<juce::String> locked;
        return locked;
    }

    // MIDI Learn state
    static inline juce::String midiLearnParamId;
    static inline bool isMidiLearning = false;
    static inline std::map<juce::String, int> midiCCAssignments;

    KnobContextMenu (juce::Slider& s, juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId)
        : slider (s), apvtsRef (apvts), parameterID (paramId)
    {
        // Disable JUCE's built-in slider popup menu so we can show our own
        slider.setPopupMenuEnabled (false);
        // Use true to receive events from child components (needed for filmstrip knobs)
        slider.addMouseListener (this, true);
    }

    ~KnobContextMenu() override
    {
        slider.removeMouseListener (this);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())  // Right-click
        {
            showContextMenu();
        }
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        // Double-click shows inline value editor (like Serum)
        showInlineValueEntry();
    }

    void showInlineValueEntry()
    {
        auto* param = apvtsRef.getParameter (parameterID);
        if (param == nullptr) return;

        // Create inline text editor popup
        auto* popup = new InlineValueEditor (slider, param);
        popup->addToDesktop (juce::ComponentPeer::windowIsTemporary);

        // Position centered on the knob
        auto screenBounds = slider.getScreenBounds();
        int popupW = 60;
        int popupH = 22;
        popup->setBounds (
            screenBounds.getCentreX() - popupW / 2,
            screenBounds.getCentreY() - popupH / 2,
            popupW, popupH);

        popup->setVisible (true);
        popup->grabFocus();
    }

    // Inline value editor - small text box that appears on double-click
    class InlineValueEditor : public juce::Component, public juce::TextEditor::Listener
    {
    public:
        InlineValueEditor (juce::Slider& s, juce::RangedAudioParameter* p)
            : slider (s), param (p)
        {
            // Dark background with border
            setOpaque (true);

            // Text editor
            addAndMakeVisible (editor);
            editor.setMultiLine (false);
            editor.setReturnKeyStartsNewLine (false);
            editor.setScrollbarsShown (false);
            editor.setCaretVisible (true);
            editor.setPopupMenuEnabled (false);
            editor.setSelectAllWhenFocused (true);
            editor.setJustification (juce::Justification::centred);
            editor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xFF1A1A24));
            editor.setColour (juce::TextEditor::textColourId, juce::Colours::white);
            editor.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xFF64D2FF));
            editor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xFF64D2FF));
            editor.addListener (this);

            // Set current value as text
            float currentReal = param->convertFrom0to1 (param->getValue());
            editor.setText (juce::String (currentReal, 2), false);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xFF1A1A24));
            g.setColour (juce::Colour (0xFF64D2FF));
            g.drawRect (getLocalBounds(), 1);
        }

        void resized() override
        {
            editor.setBounds (getLocalBounds().reduced (2));
        }

        void grabFocus()
        {
            editor.grabKeyboardFocus();
            editor.selectAll();
        }

        void textEditorReturnKeyPressed (juce::TextEditor&) override
        {
            applyValue();
            closeEditor();
        }

        void textEditorEscapeKeyPressed (juce::TextEditor&) override
        {
            closeEditor();
        }

        void textEditorFocusLost (juce::TextEditor&) override
        {
            // Apply value when clicking away
            applyValue();
            closeEditor();
        }

        void applyValue()
        {
            float newValue = editor.getText().getFloatValue();
            param->setValueNotifyingHost (param->convertTo0to1 (newValue));
        }

        void closeEditor()
        {
            // Use async to avoid deleting while in callback
            juce::MessageManager::callAsync ([this]() {
                removeFromDesktop();
                delete this;
            });
        }

    private:
        juce::Slider& slider;
        juce::RangedAudioParameter* param;
        juce::TextEditor editor;
    };

    void showContextMenu()
    {
        juce::PopupMenu menu;

        auto* param = apvtsRef.getParameter (parameterID);
        if (param == nullptr) return;

        float currentValue = param->getValue();
        float defaultValue = param->getDefaultValue();

        // Get user default if exists
        auto& userDefaults = getUserDefaults();
        bool hasUserDefault = userDefaults.find (parameterID) != userDefaults.end();
        float userDefault = hasUserDefault ? userDefaults[parameterID] : defaultValue;

        bool isLocked = getLockedParams().count (parameterID) > 0;
        bool hasMidiCC = midiCCAssignments.find (parameterID) != midiCCAssignments.end();

        // Menu items with IDs
        enum MenuIDs {
            ResetToDefault = 1,
            SaveAsDefault,
            ClearUserDefault,
            CopyValue,
            PasteValue,
            MidiLearn,
            ClearMidi,
            LockParam,
            UnlockParam
        };

        // 1. Reset to Default
        juce::String resetText = "Reset to Default";
        if (hasUserDefault)
            resetText += " (User: " + juce::String (param->convertFrom0to1 (userDefault), 2) + ")";
        else
            resetText += " (Factory)";
        menu.addItem (ResetToDefault, resetText);

        // 2. Save as Default
        menu.addItem (SaveAsDefault, "Save as Default");

        // 3. Clear User Default (only if user default exists)
        if (hasUserDefault)
            menu.addItem (ClearUserDefault, "Clear User Default (Use Factory)");

        menu.addSeparator();

        // 4. Copy Value (Enter Value removed - use double-click instead)
        menu.addItem (CopyValue, "Copy Value");

        // 6. Paste Value
        juce::String pasteText = "Paste Value";
        if (hasClipboardValue)
            pasteText += " (" + juce::String (clipboardValue, 2) + ")";
        menu.addItem (PasteValue, pasteText, hasClipboardValue);

        menu.addSeparator();

        // 7. MIDI Learn
        if (hasMidiCC)
        {
            menu.addItem (MidiLearn, "MIDI Learn (CC " + juce::String (midiCCAssignments[parameterID]) + ")");
            menu.addItem (ClearMidi, "Clear MIDI Assignment");
        }
        else
        {
            menu.addItem (MidiLearn, isMidiLearning && midiLearnParamId == parameterID
                ? "Learning... (move a CC)" : "MIDI Learn");
        }

        menu.addSeparator();

        // 8. Lock/Unlock Parameter
        if (isLocked)
            menu.addItem (UnlockParam, juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x94\x92")) + " Unlock Parameter");
        else
            menu.addItem (LockParam, juce::String (juce::CharPointer_UTF8 ("\xf0\x9f\x94\x93")) + " Lock Parameter (Preset Protected)");

        // Show menu and handle selection
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slider),
            [this, param, userDefault, hasUserDefault, defaultValue, currentValue, isLocked, hasMidiCC] (int result)
            {
                switch (result)
                {
                    case ResetToDefault:
                        param->setValueNotifyingHost (hasUserDefault ? userDefault : defaultValue);
                        break;

                    case SaveAsDefault:
                        getUserDefaults()[parameterID] = currentValue;
                        saveUserDefaults();
                        break;

                    case ClearUserDefault:
                        getUserDefaults().erase (parameterID);
                        saveUserDefaults();
                        break;

                    case CopyValue:
                        clipboardValue = param->convertFrom0to1 (currentValue);
                        clipboardParamId = parameterID;
                        hasClipboardValue = true;
                        break;

                    case PasteValue:
                        if (hasClipboardValue)
                            param->setValueNotifyingHost (param->convertTo0to1 (clipboardValue));
                        break;

                    case MidiLearn:
                        if (isMidiLearning && midiLearnParamId == parameterID)
                        {
                            // Cancel learning
                            isMidiLearning = false;
                            midiLearnParamId = "";
                        }
                        else
                        {
                            // Start learning
                            isMidiLearning = true;
                            midiLearnParamId = parameterID;
                        }
                        break;

                    case ClearMidi:
                        midiCCAssignments.erase (parameterID);
                        break;

                    case LockParam:
                        getLockedParams().insert (parameterID);
                        break;

                    case UnlockParam:
                        getLockedParams().erase (parameterID);
                        break;
                }
            });
    }

    // Call this from MIDI input to complete MIDI learn
    static void processMidiCC (int ccNumber, juce::AudioProcessorValueTreeState& apvts)
    {
        if (isMidiLearning && midiLearnParamId.isNotEmpty())
        {
            midiCCAssignments[midiLearnParamId] = ccNumber;
            isMidiLearning = false;
            midiLearnParamId = "";
        }

        // Apply CC to assigned parameters
        for (auto& [paramId, cc] : midiCCAssignments)
        {
            if (cc == ccNumber)
            {
                // Note: actual CC value application would happen in processBlock
            }
        }
    }

private:
    juce::Slider& slider;
    juce::AudioProcessorValueTreeState& apvtsRef;
    juce::String parameterID;
};

//==============================================================================
// Tiny self-repainting level meter bar (for input/output gain)
//==============================================================================
class LevelMeterBar : public juce::Component, private juce::Timer
{
public:
    std::function<float()> getLevel;  // Callback to read current RMS level

    LevelMeterBar() { startTimerHz (30); }
    ~LevelMeterBar() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        float level = getLevel ? juce::jlimit (0.0f, 1.0f, getLevel()) : 0.0f;

        // Smoothed decay (fast attack, slow release)
        if (level > smoothed)
            smoothed = level;
        else
            smoothed *= 0.9f;

        auto b = getLocalBounds().toFloat();

        // Background
        g.setColour (juce::Colour (0xFF1A1A20));
        g.fillRoundedRectangle (b, 1.5f);

        // Level bar (bottom-up)
        float barH = smoothed * b.getHeight();
        if (barH > 0.5f)
        {
            juce::Colour col = smoothed < 0.5f  ? juce::Colour (0xFF44CC44)
                             : smoothed < 0.8f  ? juce::Colour (0xFFCCCC44)
                             :                     juce::Colour (0xFFCC4444);
            g.setColour (col);
            g.fillRoundedRectangle (b.getX(), b.getBottom() - barH, b.getWidth(), barH, 1.5f);
        }
    }

private:
    float smoothed = 0.0f;
    void timerCallback() override { repaint(); }
};

//==============================================================================
// Main editor
//==============================================================================
class ColorVocoderAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::Slider::Listener
{
public:
    ColorVocoderAudioProcessorEditor (ColorVocoderAudioProcessor&);
    ~ColorVocoderAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Native (unscaled) design size - all layout is done at this resolution
    static constexpr int NATIVE_WIDTH = 722;
    static constexpr int NATIVE_HEIGHT = 485;

    // Slider::Listener for popup display
    void sliderValueChanged (juce::Slider* slider) override;
    void sliderDragStarted (juce::Slider* slider) override;
    void sliderDragEnded (juce::Slider* slider) override;

    // Keyboard shortcut for screenshot export (Cmd+Shift+E)
    bool keyPressed (const juce::KeyPress& key) override;
    void exportScreenshot();

    // Re-grab keyboard focus when returning from the host DAW
    void focusGained (FocusChangeType cause) override
    {
        juce::ignoreUnused (cause);
        grabKeyboardFocus();
    }

private:
    ColorVocoderAudioProcessor& processor;

    // Content wrapper for proportional scaling (Serum-style)
    std::unique_ptr<juce::Component> scaledContent;
    void layoutComponents();  // Sets up all component bounds at native resolution

    // ===== LOOK AND FEEL =====
    BigKnobFilmstripLookAndFeel bigKnobLAF;
    MediumKnobFilmstripLookAndFeel mediumKnobLAF;
    SmallKnobFilmstripLookAndFeel smallKnobLAF;
    NoTickToggleLookAndFeel noTickToggleLAF;
    TwoLineButtonLookAndFeel twoLineButtonLAF;
    ThinPresetLookAndFeel thinPresetLAF;

    // ===== PRESET MANAGEMENT =====
    PresetManager presetManager;
    juce::TextButton presetButton { "Select Preset..." };
    juce::TextButton savePresetButton { "" };  // Floppy disk icon, no text

    void showPresetMenu();
    void updatePresetButtonText();
    void savePresetWithDialog();
    void deleteCurrentPreset();
    void createNewFolderDialog (std::function<void (const juce::String&)> onCreated = nullptr);

    // Type aliases
    using Slider = juce::Slider;
    using Combo  = juce::ComboBox;
    using Button = juce::ToggleButton;

    // ===== VISUALIZERS =====
    BandDisplay bandDisplay;
    BandDistributionEditor bandDistEditor;  // Vocodex-style distribution curve
    BandOffsetEditor bandOffsetEditor;      // Band routing offset curve
    PerBandQCarousel perBandQCarousel;      // Per-band Q carousel view
    std::unique_ptr<InputFilterDisplay> inputFilterDisplay;
    std::unique_ptr<PianoKeyboard> pianoKeyboard;  // Pitch class exclusion keyboard

    // ===== CUSTOM CHORD EDITOR =====
    std::unique_ptr<CustomChordEditor> customChordEditor;
    juce::TextButton editButton { "EDIT" };
    bool editModeActive = false;
    void toggleEditMode();

    // ===== LEFT PANEL =====
    Button bypassButton;
    Combo carrierModeBox;
    juce::Label carrierModeLabel { {}, "MODE" };

    // ===== ALGORITHM MODE SECTION =====
    Combo algorithmModeBox;
    juce::Label algorithmModeLabel { {}, "ALGORITHM" };
    Slider carrierQKnob;
    juce::Label carrierQLabel { {}, "Q" };
    Slider modulatorQKnob;  // Only visible in SPLIT mode
    juce::Label modulatorQLabel { {}, "MOD Q" };
    Slider modBwKnob;  // Only visible in LINKED/RITUAL (Vocodex-style analysis bandwidth multiplier)
    juce::Label modBwLabel { {}, "mod bw" };

    void updateQKnobVisibility();  // Update visibility based on algorithm mode

    Slider mixSlider;
    juce::Label mixLabel { {}, "MIX" };
    VerticalFillBar modLevelBar;
    Slider modLevelSlider;  // Hidden, for parameter attachment
    juce::Label modLevelLabel { {}, "MOD" };
    ScaleChordBox scaleChordBox;

    // ===== MICROTUNING PANEL (CallOutBox popup) =====
    std::unique_ptr<MicrotuningPanel> microtuningPanel;
    juce::Component::SafePointer<juce::CallOutBox> microtuningCallOut;
    juce::TextButton tuningButton { "TUNING" };
    void toggleMicrotuningPanel();

    // ===== LICENSE PANEL (CallOutBox popup) =====
    std::unique_ptr<LicensePanel> licensePanel;
    juce::Component::SafePointer<juce::CallOutBox> licenseCallOut;
    juce::TextButton licenseButton { "LICENSE" };
    void toggleLicensePanel();

    // ===== ABOUT PANEL =====
    std::unique_ptr<AboutPanel> aboutPanel;
    CircleInfoButton aboutButton;
    void toggleAboutPanel();

    // ===== FEEDBACK BUTTON (top bar "+" icon) =====
    juce::TextButton feedbackButton { "+" };
    bool showFeedbackMessage = false;

    // ===== INPUT / OUTPUT GAIN KNOBS =====
    juce::Slider inputGainKnob, outputGainKnob;
    juce::Label inputGainLabel, outputGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment, outputGainAttachment;
    LevelMeterBar inputMeter, outputMeter;

    // ===== WELCOME PANEL (first launch / expired overlay) =====
    std::unique_ptr<WelcomePanel> welcomePanel;
    void showWelcomePanelIfNeeded();
    void hideWelcomePanel();

    // ===== DYNAMIC UI REFRESH AFTER LICENSE ACTIVATION =====
    void refreshTrialRestrictedUI();  // Removes lock icons and enables features after activation

    // ===== CENTER PANEL - Vocoder Knobs =====
    Slider bandCountSlider;  // Draggable 1-100 band count
    juce::ToggleButton additiveModeToggle;  // STACK button
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> additiveModeAttachment;
    juce::TextButton bandDistButton;  // DIST button (toggles distribution view)
    juce::TextButton bandOffsetButton;  // OFFSET button (toggles offset view)
    juce::TextButton perBandQButton;    // BANDS button (toggles per-band Q carousel)

    // ===== BANK KEYTRACK controls =====
    // Inline: KEYTRK on/off toggle + EDIT button. POSITION, GLIDE and the glide curve live in
    // the BankKeytrackPopup (CallOutBox) so the dense main panel stays uncluttered and knob sizes
    // stay legal.
    juce::ToggleButton bankKeytrackToggle;   // KEYTRACK on/off
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bankKeytrackAttachment;
    juce::TextButton bankKeytrackEditButton; // opens the POSITION/GLIDE/curve popup
    void showBankKeytrackPopup();            // launches the CallOutBox keytrack panel

    // Which view is shown in band display area
    enum class BandViewMode { Normal, Distribution, Offset, PerBandQ };
    BandViewMode currentBandView = BandViewMode::Normal;
    void updateBandViewVisibility();  // Updates visibility of band views based on currentBandView
    Slider bandWidthSlider;
    Slider attackSlider, releaseSlider;
    Slider tightnessSlider;
    Slider shapeSlider;
    Slider gateSlider, depthSlider;
    juce::Label bandCountLabel { {}, "bands" };
    juce::Label bandWidthLabel { {}, "width" };
    juce::Label attackLabel { {}, "attack" };
    juce::Label releaseLabel { {}, "release" };
    juce::Label tightnessLabel { {}, "tight" };
    juce::Label shapeLabel { {}, "shape" };
    juce::Label gateLabel { {}, "gate" };
    juce::Label depthLabel { {}, "depth" };

    // ===== RIGHT PANEL =====
    Slider formantSlider, harmShiftSlider;
    juce::Label formantLabel { {}, "formant" };
    juce::Label harmShiftLabel { {}, "Harm Shift" };

    Slider unvoicedSlider, hpBlendSlider, noiseColorSlider, sibSlider, noiseHpSlider, airDelaySlider;
    juce::Label unvoicedLabel { {}, "noise" };
    juce::Label hpBlendLabel { {}, "AIR" };
    juce::Label noiseColorLabel { {}, "noise color" };
    juce::Label sibLabel { {}, "sib" };
    juce::Label noiseHpLabel { {}, "nhp" };
    juce::Label airDelayLabel { {}, "dly" };
    SlopeClickBox hpBlendSlopeBox;

    // Noise Mode selector (Auto/Vocal/Synth)
    Combo noiseModeCombo;

    // ===== SPECTRAL X/Y PAD =====
    std::unique_ptr<SpectralXYPad> spectralPad;

    // ===== BOTTOM PANEL - Tone Sculpt =====
    juce::Label toneSculptLabel { {}, "tone sculpt" };
    Button complementToggle { "complement" };
    Slider freqKnob, gainKnob, qKnob;
    juce::Label freqLabel { {}, "freq" };
    juce::Label gainLabel { {}, "gain" };
    juce::Label qLabel { {}, "Q" };

    // Helper for double-click reset on freq knob (per-band defaults)
    struct FreqKnobDoubleClickHandler : public juce::MouseListener
    {
        Slider& knob;
        std::function<double()> getDefaultValue;

        FreqKnobDoubleClickHandler (Slider& k) : knob (k) {}

        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            if (getDefaultValue)
                knob.setValue (getDefaultValue(), juce::sendNotificationSync);
        }
    };
    std::unique_ptr<FreqKnobDoubleClickHandler> freqKnobDoubleClickHandler;

    // ===== LOW/HIGH FREQ SLIDERS (hidden, controlled by band display) =====
    Slider lowFreqSlider, highFreqSlider;
    Slider carrierPitchSlider;

    // ===== ATTACHMENTS =====
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> carrierModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modLevelAttachment;

    // Algorithm mode attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algorithmModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> carrierQAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modulatorQAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> modBwAttachment;


    // Scale/Chord attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> scaleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> scaleAmtAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> chordDegreeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> chordVariationAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chordAmountAttachment;

    // Vocoder knob attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bandCountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bandWidthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tightnessAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shapeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttachment;

    // Right panel attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> formantAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> harmShiftAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> unvoicedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sibAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseHpAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> airDelayAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hpBlendAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> hpBlendSlopeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> noiseColorAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> noiseModeAttachment;

    // Hidden attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highFreqAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> carrierPitchAttachment;

    // Complement toggle attachment
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> complementAttachment;

    // ===== HELPERS =====
    void setupKnob (Slider& slider, juce::Label& label, FilmstripLookAndFeel& laf);
    void setupCombo (Combo& box);

    // Stored bounds for painting borders
    juce::Rectangle<int> vocoderKnobsArea;
    juce::Rectangle<int> harmonicKnobsArea;   // Formant + Harm Shift section
    juce::Rectangle<int> noiseKnobsArea;      // Noise section (noise, hp-blend, noise mode, noise color, spectral pad)

    // ===== KNOB VALUE POPUP =====
    KnobValuePopup knobPopup;

    // Knob info for popup display
    enum class ValueFormat { Percent, SignedPercent, Ms, Semitones, Frequency, Db, Number, Int, NoiseColor };

    struct KnobInfo {
        juce::String name;
        ValueFormat format;
        int decimals = 1;
    };

    std::map<juce::Slider*, KnobInfo> knobInfoMap;

    void registerKnobForPopup (juce::Slider& slider, const juce::String& name, ValueFormat format, int decimals = 1);
    juce::String formatKnobValue (juce::Slider* slider);

    // ===== AUTO-MAX BAND COUNT ON SCALE/CHORD CHANGE =====
    // Automatically sets bandCount to max useful value when scale/chord settings change
    struct AutoMaxBandListener : public juce::AudioProcessorValueTreeState::Listener
    {
        ColorVocoderAudioProcessorEditor* editor = nullptr;

        void parameterChanged (const juce::String&, float) override
        {
            if (editor)
                juce::MessageManager::callAsync ([this]() {
                    if (editor)
                        editor->updateBandCountToMax();
                });
        }
    };
    AutoMaxBandListener autoMaxBandListener;
    void updateBandCountToMax();

    // ===== KNOB CONTEXT MENUS (Right-click) =====
    // Stores context menu handlers for all registered knobs
    std::vector<std::unique_ptr<KnobContextMenu>> knobContextMenus;
    void registerKnobContextMenu (juce::Slider& slider, const juce::String& paramId);

    // ===== STACK SLIDER VALUE RESTORE =====
    // Stores the slider value before STACK OFF caps it, so STACK ON can restore
    int preCapBandCountValue = -1;

    // "-> X" readout under the band slider. X is how many bands actually SOUND, which in a scale-locked mode
    // is fewer than the number requested (several requested bands snap onto the same scale note). Cached here
    // because the display has to refresh on BOTH events: the unique count changing, and the user dragging the
    // slider while the unique count stays put -- the second is the case that used to look frozen.
    int lastUniqueBandsShown = 0;
    // How many distinct snap targets the current scale/range offers. Fewer bands than this and some scale
    // notes get NO band at all -- playing them produces only the -23/-36 dB leakage from a neighbour. The
    // "-> X" readout cannot show this on its own: with 51 bands over 70 scale notes nothing collapses, so
    // X reads 51 of 51 and looks perfectly healthy while 19 notes are silent.
    int lastMaxUsefulShown = 0;
    void refreshBandCountLabel();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ColorVocoderAudioProcessorEditor)
};
