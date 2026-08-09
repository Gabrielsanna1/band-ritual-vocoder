cmake_minimum_required(VERSION 3.15)

project(BandRitualFinal1 VERSION 0.1.0)

# Universal Binary — works on both Intel and Apple Silicon Macs
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)

# Minimum macOS version (10.13 High Sierra — broadest compatibility)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version" FORCE)

# Download JUCE 8 from GitHub (or use local folder named JUCE8)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE8/CMakeLists.txt")
    message(STATUS "Using local JUCE8 folder")
    add_subdirectory(JUCE8 JUCE)
else()
    message(STATUS "Downloading JUCE 8 from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.6
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# Add Plugin GUI Magic (disable examples/tests to avoid duplicate JUCE)
set(FOLEYS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FOLEYS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(modules/foleys_gui_magic)

juce_add_plugin(BandRitual
    COMPANY_NAME "YourCompany"
    BUNDLE_ID "com.ritualaudiolabs.bandritual-final1"
    VERSION "0.1.0"
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Juce
    PLUGIN_CODE BrF1
    PRODUCT_NAME "Band Ritual Final 1"
)

target_sources(BandRitual PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/LicenseManager.cpp
    Source/CarrierGenerator.cpp
    Source/FilmstripLookAndFeel.cpp
    Source/STFTVocoder.cpp
)

# WebViewEditor only on Mac (requires WebView2 SDK on Windows)
if(APPLE)
    target_sources(BandRitual PRIVATE Source/WebViewEditor.cpp)
endif()

# Add binary resources (filmstrip knob images + popup frame + webui)
juce_add_binary_data(BandRitualBinaryData SOURCES
    Source/Assets/hise_knob_big.png
    Source/Assets/hise_Knob_medium.png
    Source/Assets/hise_knob_small.png
    Source/Assets/popup_frame_trimmed.png
    Source/Resources/ritual_logo.png
    Source/webui/index.html
    Source/webui/styles.css
    Source/webui/knob.js
    Source/webui/visualizer.js
    Source/webui/bridge.js
)

target_compile_features(BandRitual PRIVATE cxx_std_17)

# Suppress VST2/VST3 compatibility warning (we're only building VST3)
target_compile_definitions(BandRitual
    PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0)

juce_generate_juce_header(BandRitual)

target_link_libraries(BandRitual
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra  # For WebBrowserComponent
        foleys_gui_magic
        BandRitualBinaryData
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# WebView settings - enable on Mac, disable on Windows
if(APPLE)
    target_compile_definitions(BandRitual
        PRIVATE
            JUCE_USE_WIN_WEBVIEW2=0
            JUCE_WEB_BROWSER=1)
else()
    target_compile_definitions(BandRitual
        PRIVATE
            JUCE_USE_WIN_WEBVIEW2=0
            JUCE_WEB_BROWSER=0)
endif()

