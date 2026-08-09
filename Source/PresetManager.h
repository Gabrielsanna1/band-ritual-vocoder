#pragma once

#include <JuceHeader.h>

class PresetManager
{
public:
    PresetManager (juce::AudioProcessorValueTreeState& apvts);

    // Get the directory where presets are stored
    juce::File getPresetDirectory() const;

    // Save current state as a preset
    bool savePreset (const juce::String& presetName);

    // Load a preset by name
    bool loadPreset (const juce::String& presetName);

    // Delete a preset by name
    bool deletePreset (const juce::String& presetName);

    // Get list of available preset names
    juce::StringArray getPresetList() const;

    // Get/set current preset name
    juce::String getCurrentPresetName() const { return currentPresetName; }
    void setCurrentPresetName (const juce::String& name) { currentPresetName = name; }

    // Get/set current preset folder ("" for root)
    juce::String getCurrentPresetFolder() const { return currentPresetFolder; }
    void setCurrentPresetFolder (const juce::String& folder) { currentPresetFolder = folder; }

    // Check if there are unsaved changes
    bool hasUnsavedChanges() const { return unsavedChanges; }
    void markAsModified() { unsavedChanges = true; }
    void markAsSaved() { unsavedChanges = false; }

    // Folder-aware preset management
    bool savePreset (const juce::String& presetName, const juce::String& folder);
    bool loadPreset (const juce::String& presetName, const juce::String& folder);
    bool deletePreset (const juce::String& presetName, const juce::String& folder);

    // Folder operations
    std::map<juce::String, juce::StringArray> getFolderOrganizedPresets() const;
    juce::StringArray getFolderList() const;
    bool createFolder (const juce::String& folderName);

    // File extension for presets
    static constexpr const char* presetExtension = ".brpreset";

private:
    juce::AudioProcessorValueTreeState& apvtsRef;
    juce::String currentPresetName { "Init" };
    juce::String currentPresetFolder;  // "" for root
    bool unsavedChanges = false;

    juce::File getPresetFile (const juce::String& presetName) const;
    juce::File getPresetFile (const juce::String& presetName, const juce::String& folder) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};

//==============================================================================
// Implementation (header-only for simplicity)
//==============================================================================

inline PresetManager::PresetManager (juce::AudioProcessorValueTreeState& apvts)
    : apvtsRef (apvts)
{
    // Ensure preset directory exists
    getPresetDirectory().createDirectory();
}

inline juce::File PresetManager::getPresetDirectory() const
{
    // Store presets in user's Audio Presets folder (standard location)
    // macOS: ~/Library/Audio/Presets/BandRitual/
    // Windows: C:\Users\<user>\Documents\Audio Presets\BandRitual\

#if JUCE_MAC
    auto presetDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                        .getChildFile ("Library/Audio/Presets/BandRitual");
#elif JUCE_WINDOWS
    auto presetDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                        .getChildFile ("Audio Presets/BandRitual");
#else
    auto presetDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                        .getChildFile ("BandRitual/Presets");
#endif

    return presetDir;
}

inline juce::File PresetManager::getPresetFile (const juce::String& presetName) const
{
    return getPresetDirectory().getChildFile (presetName + presetExtension);
}

inline bool PresetManager::savePreset (const juce::String& presetName)
{
    if (presetName.isEmpty())
        return false;

    auto file = getPresetFile (presetName);

    // Get current state as XML
    auto state = apvtsRef.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    if (xml == nullptr)
        return false;

    // Write to file
    if (!xml->writeTo (file))
        return false;

    currentPresetName = presetName;
    unsavedChanges = false;
    return true;
}

inline bool PresetManager::loadPreset (const juce::String& presetName)
{
    auto file = getPresetFile (presetName);

    if (!file.existsAsFile())
        return false;

    // Read XML from file
    std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse (file);

    if (xml == nullptr)
        return false;

    // Check if valid state
    if (!xml->hasTagName (apvtsRef.state.getType()))
        return false;

    // Apply the state
    apvtsRef.replaceState (juce::ValueTree::fromXml (*xml));

    currentPresetName = presetName;
    unsavedChanges = false;
    return true;
}

inline bool PresetManager::deletePreset (const juce::String& presetName)
{
    auto file = getPresetFile (presetName);

    if (!file.existsAsFile())
        return false;

    return file.deleteFile();
}

inline juce::StringArray PresetManager::getPresetList() const
{
    juce::StringArray presets;

    auto dir = getPresetDirectory();
    if (!dir.isDirectory())
        return presets;

    // Find all preset files
    for (const auto& entry : juce::RangedDirectoryIterator (dir, false, juce::String ("*") + presetExtension))
    {
        presets.add (entry.getFile().getFileNameWithoutExtension());
    }

    presets.sort (true);  // Case-insensitive sort
    return presets;
}

//==============================================================================
// Folder-aware implementations
//==============================================================================

inline juce::File PresetManager::getPresetFile (const juce::String& presetName, const juce::String& folder) const
{
    if (folder.isEmpty())
        return getPresetDirectory().getChildFile (presetName + presetExtension);

    return getPresetDirectory().getChildFile (folder).getChildFile (presetName + presetExtension);
}

inline bool PresetManager::savePreset (const juce::String& presetName, const juce::String& folder)
{
    if (presetName.isEmpty())
        return false;

    // Ensure folder exists
    if (folder.isNotEmpty())
    {
        auto folderDir = getPresetDirectory().getChildFile (folder);
        folderDir.createDirectory();
    }

    auto file = getPresetFile (presetName, folder);

    auto state = apvtsRef.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    if (xml == nullptr)
        return false;

    if (!xml->writeTo (file))
        return false;

    currentPresetName = presetName;
    currentPresetFolder = folder;
    unsavedChanges = false;
    return true;
}

inline bool PresetManager::loadPreset (const juce::String& presetName, const juce::String& folder)
{
    auto file = getPresetFile (presetName, folder);

    if (!file.existsAsFile())
        return false;

    std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse (file);

    if (xml == nullptr)
        return false;

    if (!xml->hasTagName (apvtsRef.state.getType()))
        return false;

    apvtsRef.replaceState (juce::ValueTree::fromXml (*xml));

    currentPresetName = presetName;
    currentPresetFolder = folder;
    unsavedChanges = false;
    return true;
}

inline bool PresetManager::deletePreset (const juce::String& presetName, const juce::String& folder)
{
    auto file = getPresetFile (presetName, folder);

    if (!file.existsAsFile())
        return false;

    return file.deleteFile();
}

inline std::map<juce::String, juce::StringArray> PresetManager::getFolderOrganizedPresets() const
{
    std::map<juce::String, juce::StringArray> result;

    auto dir = getPresetDirectory();
    if (!dir.isDirectory())
        return result;

    // Root-level presets (key = "")
    for (const auto& entry : juce::RangedDirectoryIterator (dir, false, juce::String ("*") + presetExtension))
        result[""].add (entry.getFile().getFileNameWithoutExtension());

    // Subfolder presets (1 level deep)
    for (const auto& folderEntry : juce::RangedDirectoryIterator (dir, false, "*", juce::File::findDirectories))
    {
        auto folderName = folderEntry.getFile().getFileName();
        auto& presets = result[folderName];

        for (const auto& presetEntry : juce::RangedDirectoryIterator (folderEntry.getFile(), false, juce::String ("*") + presetExtension))
            presets.add (presetEntry.getFile().getFileNameWithoutExtension());

        presets.sort (true);
    }

    // Sort root presets
    if (result.count (""))
        result[""].sort (true);

    return result;
}

inline juce::StringArray PresetManager::getFolderList() const
{
    juce::StringArray folders;

    auto dir = getPresetDirectory();
    if (!dir.isDirectory())
        return folders;

    for (const auto& entry : juce::RangedDirectoryIterator (dir, false, "*", juce::File::findDirectories))
        folders.add (entry.getFile().getFileName());

    folders.sort (true);
    return folders;
}

inline bool PresetManager::createFolder (const juce::String& folderName)
{
    if (folderName.isEmpty())
        return false;

    auto folderDir = getPresetDirectory().getChildFile (folderName);
    return folderDir.createDirectory().wasOk();
}
