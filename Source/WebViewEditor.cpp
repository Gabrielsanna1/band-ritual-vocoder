#include "WebViewEditor.h"
#include "PluginProcessor.h"

// Check if BinaryData has webui files (will be available after cmake regeneration)
#if __has_include("BinaryData.h")
#include "BinaryData.h"
#define HAS_WEBUI_BINARY_DATA 1
#else
#define HAS_WEBUI_BINARY_DATA 0
#endif

//==============================================================================
WebViewEditor::WebViewEditor(BandRitualAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Create WebView - uses native WebKit on Mac, WebView2 on Windows
    juce::WebBrowserComponent::Options options;

    #if JUCE_WINDOWS
    options = options.withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
                     .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2{}
                         .withUserDataFolder(juce::File::getSpecialLocation(juce::File::tempDirectory)));
    #endif
    // macOS uses default WebKit backend automatically

    webView = std::make_unique<juce::WebBrowserComponent>(options);
    addAndMakeVisible(*webView);

    // Load the WebUI
    loadWebUI();

    // Set size
    setSize(DEFAULT_WIDTH, DEFAULT_HEIGHT);
    setResizable(true, true);
    setResizeLimits(MIN_WIDTH, MIN_HEIGHT, 2560, 1440);

    // Start timer for visualization updates (30fps)
    startTimerHz(30);
}

WebViewEditor::~WebViewEditor()
{
    stopTimer();
}

//==============================================================================
void WebViewEditor::paint(juce::Graphics& g)
{
    // Background shown briefly while WebView loads
    g.fillAll(juce::Colour(0xff1a1a1a));

    if (!webView)
    {
        g.setColour(juce::Colours::white);
        g.setFont(16.0f);
        g.drawText("Loading WebView...", getLocalBounds(), juce::Justification::centred);
    }
}

void WebViewEditor::resized()
{
    if (webView)
        webView->setBounds(getLocalBounds());
}

void WebViewEditor::timerCallback()
{
    // Send visualization data to WebView periodically
    // For now, skip if methods don't exist - just update parameters
    sendParameterUpdatesToWebView();
}

//==============================================================================
void WebViewEditor::loadWebUI()
{
    if (!webView) return;

    // Try to load from local file first (for development)
    juce::File sourceDir = juce::File(__FILE__).getParentDirectory();
    juce::File webUIFile = sourceDir.getChildFile("webui").getChildFile("index.html");

    if (webUIFile.existsAsFile())
    {
        // Development mode: load from file system
        webView->goToURL(juce::URL(webUIFile).toString(true));
        DBG("WebViewEditor: Loading from file: " + webUIFile.getFullPathName());
    }
    else
    {
        // Production mode: load from embedded binary data
        #if HAS_WEBUI_BINARY_DATA
        auto htmlContent = getEmbeddedHTML();
        juce::String dataUrl = "data:text/html;charset=utf-8," +
                               juce::URL::addEscapeChars(htmlContent, true);
        webView->goToURL(dataUrl);
        DBG("WebViewEditor: Loading from embedded binary data");
        #else
        // Fallback: show a simple placeholder
        juce::String fallbackHTML = R"(
            <!DOCTYPE html>
            <html>
            <head><title>Band Ritual</title></head>
            <body style="background:#1a1a1a;color:#fff;font-family:sans-serif;display:flex;justify-content:center;align-items:center;height:100vh;margin:0;">
                <div style="text-align:center;">
                    <h1 style="color:#00d4ff;">BAND RITUAL</h1>
                    <p>WebView GUI - Rebuild with CMake to load full UI</p>
                </div>
            </body>
            </html>
        )";
        juce::String fallbackUrl = "data:text/html;charset=utf-8," +
                                   juce::URL::addEscapeChars(fallbackHTML, true);
        webView->goToURL(fallbackUrl);
        DBG("WebViewEditor: Using fallback HTML (binary data not available)");
        #endif
    }
}

juce::String WebViewEditor::getEmbeddedHTML()
{
    #if HAS_WEBUI_BINARY_DATA
    // Build complete HTML with embedded CSS and JS
    juce::String html = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Band Ritual</title>
    <style>
)";

    // Add CSS
    if (BinaryData::styles_cssSize > 0)
        html += juce::String(BinaryData::styles_css, BinaryData::styles_cssSize);

    html += R"(
    </style>
</head>
<body>
)";

    // Add HTML body content (extract from index.html, skip the head section)
    if (BinaryData::index_htmlSize > 0)
    {
        juce::String indexContent(BinaryData::index_html, BinaryData::index_htmlSize);
        // Find body content
        int bodyStart = indexContent.indexOf("<body>");
        int bodyEnd = indexContent.indexOf("</body>");
        if (bodyStart >= 0 && bodyEnd > bodyStart)
        {
            html += indexContent.substring(bodyStart + 6, bodyEnd);
        }
    }

    // Add JavaScript
    html += R"(
    <script>
)";

    if (BinaryData::knob_jsSize > 0)
        html += juce::String(BinaryData::knob_js, BinaryData::knob_jsSize);

    html += R"(
    </script>
    <script>
)";

    if (BinaryData::visualizer_jsSize > 0)
        html += juce::String(BinaryData::visualizer_js, BinaryData::visualizer_jsSize);

    html += R"(
    </script>
    <script>
)";

    if (BinaryData::bridge_jsSize > 0)
        html += juce::String(BinaryData::bridge_js, BinaryData::bridge_jsSize);

    html += R"(
    </script>
</body>
</html>
)";

    return html;
    #else
    return "";
    #endif
}

//==============================================================================
void WebViewEditor::sendParameterToWebView(const juce::String& paramId, float value)
{
    if (webView)
    {
        juce::String js = "if(window.updateParameter) window.updateParameter('" +
                          paramId + "', " + juce::String(value) + ");";
        webView->evaluateJavascript(js, nullptr);
    }
}

void WebViewEditor::sendParameterUpdatesToWebView()
{
    // This could be called to sync parameter states
    // For now, keep it minimal
}

void WebViewEditor::sendBandLevelsToWebView()
{
    if (!webView) return;

    // Placeholder - connect to your actual band envelope data
    // Example: auto* levels = audioProcessor.getBandEnvelopeLevels();

    // For demo, send simulated data
    juce::String json = "[";
    for (int i = 0; i < 32; ++i)
    {
        if (i > 0) json += ",";
        json += juce::String(juce::Random::getSystemRandom().nextFloat() * 0.5f, 3);
    }
    json += "]";

    juce::String js = "if(window.updateBandLevels) window.updateBandLevels('" + json + "');";
    webView->evaluateJavascript(js, nullptr);
}

void WebViewEditor::sendSpectrumToWebView()
{
    if (!webView) return;

    // Placeholder - connect to your actual spectrum analyzer
    // Example: auto* spectrum = audioProcessor.getSpectrumData();

    // For demo, send empty/simulated data
}

void WebViewEditor::handleWebViewMessage(const juce::String& message)
{
    // Parse JSON message from WebView
    auto json = juce::JSON::parse(message);

    if (json.isObject() && json.hasProperty("type"))
    {
        juce::String type = json["type"].toString();

        if (type == "parameterChanged")
        {
            juce::String paramId = json["param"].toString();
            float value = static_cast<float>(json["value"]);

            // Find and update the parameter in APVTS
            if (auto* param = audioProcessor.apvts.getParameter(paramId))
            {
                param->setValueNotifyingHost(param->convertTo0to1(value));
            }
        }
    }
}
