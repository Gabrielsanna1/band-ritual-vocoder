#pragma once

#include <JuceHeader.h>

// Forward declaration
class ColorVocoderAudioProcessor;
// Alias for convenience
using BandRitualAudioProcessor = ColorVocoderAudioProcessor;

//==============================================================================
/**
 * WebView-based GUI Editor for Band Ritual
 * Uses HTML/CSS/JS for the interface, rendered in an embedded browser
 *
 * Benefits:
 * - Design GUI with web technologies (easier styling, animations)
 * - Hot-reload during development (edit HTML/CSS/JS without recompiling)
 * - Modern look and feel with CSS
 * - Works in all major DAWs just like the regular C++ GUI
 */
class WebViewEditor : public juce::AudioProcessorEditor,
                      public juce::Timer
{
public:
    WebViewEditor(BandRitualAudioProcessor& processor);
    ~WebViewEditor() override;

    //==============================================================================
    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    BandRitualAudioProcessor& audioProcessor;

    // WebView component
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // Loading
    void loadWebUI();
    juce::String getEmbeddedHTML();

    // Parameter handling
    void sendParameterToWebView(const juce::String& paramId, float value);
    void sendParameterUpdatesToWebView();
    void sendBandLevelsToWebView();
    void sendSpectrumToWebView();

    // Message handling from WebView
    void handleWebViewMessage(const juce::String& message);

    // Size constraints
    static constexpr int DEFAULT_WIDTH = 1280;
    static constexpr int DEFAULT_HEIGHT = 740;
    static constexpr int MIN_WIDTH = 900;
    static constexpr int MIN_HEIGHT = 600;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WebViewEditor)
};
