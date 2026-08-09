#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    FilmstripLookAndFeel - Renders rotary sliders using filmstrip images.

    Uses the Julian Behrens Analog GUI Kit 01 filmstrip knobs:
    - Big: 180x180 px per frame (128 frames)
    - Medium: 100x100 px per frame (128 frames)
    - Small: 70x70 px per frame (128 frames)
*/
class FilmstripLookAndFeel : public juce::LookAndFeel_V4
{
public:
    enum KnobSize
    {
        Big,    // 180x180 px
        Medium, // 100x100 px
        Small   // 70x70 px
    };

    FilmstripLookAndFeel();
    ~FilmstripLookAndFeel() override = default;

    /** Load filmstrip images from binary resources */
    void loadFilmstrips();

    /** Set which knob size to use for drawing */
    void setKnobSize (KnobSize size);

    /** Get current knob size */
    KnobSize getKnobSize() const noexcept { return currentSize; }

    //==============================================================================
    // LookAndFeel overrides

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override;

    /** Custom label font */
    juce::Font getLabelFont (juce::Label& label) override;

    /** Custom combo box styling */
    void drawComboBox (juce::Graphics& g, int width, int height,
                       bool isButtonDown, int buttonX, int buttonY,
                       int buttonW, int buttonH,
                       juce::ComboBox& box) override;

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu,
                            const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon,
                            const juce::Colour* textColour) override;

    /** Custom toggle button for complement, etc. */
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    /** Custom text button styling */
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    /** Compact popup menu font */
    juce::Font getPopupMenuFont() override;

    /** Compact popup menu item size */
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                    int standardMenuItemHeight, int& idealWidth, int& idealHeight) override;

private:
    juce::Image bigKnob;
    juce::Image mediumKnob;
    juce::Image smallKnob;

    KnobSize currentSize = Medium;
    static constexpr int numFrames = 128;

    /** Get the appropriate filmstrip image for current size */
    juce::Image& getCurrentFilmstrip();

    /** Get frame height for current size */
    int getCurrentFrameHeight() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FilmstripLookAndFeel)
};

//==============================================================================
/**
    Convenience class that sets a specific knob size on construction.
    Use this to easily apply different sizes to different sliders.
*/
class BigKnobFilmstripLookAndFeel : public FilmstripLookAndFeel
{
public:
    BigKnobFilmstripLookAndFeel() { setKnobSize (Big); }
};

class MediumKnobFilmstripLookAndFeel : public FilmstripLookAndFeel
{
public:
    MediumKnobFilmstripLookAndFeel() { setKnobSize (Medium); }
};

class SmallKnobFilmstripLookAndFeel : public FilmstripLookAndFeel
{
public:
    SmallKnobFilmstripLookAndFeel() { setKnobSize (Small); }
};
