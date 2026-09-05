#include "PresetManager.h"
#include <BinaryData.h>
#include <algorithm>

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& apvts)
    : apvts_ (apvts)
{
    ensureUserPresetsDirectory();
    loadFactoryPresets();
    loadUserPresetsFromDisk();
}

int PresetManager::getCurrentPresetIndex() const { return currentPresetIndex_; }

juce::String PresetManager::getCurrentPresetName() const
{
    auto all = getPresetList();
    if (currentPresetIndex_ >= 0 && currentPresetIndex_ < static_cast<int> (all.size()))
        return all[static_cast<size_t> (currentPresetIndex_)].name;
    return "Unknown";
}

juce::File PresetManager::getUserPresetsDirectory() const
{
#if JUCE_WINDOWS
    auto appData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
    return appData.getChildFile ("Outflank").getChildFile ("presets");
#elif JUCE_MAC
    auto appSupport = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                 .getChildFile ("Application Support");
    return appSupport.getChildFile ("Outflank").getChildFile ("presets");
#else // JUCE_LINUX
    auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
    return home.getChildFile (".config").getChildFile ("Outflank").getChildFile ("presets");
#endif
}

void PresetManager::ensureUserPresetsDirectory() const
{
    auto dir = getUserPresetsDirectory();
    if (!dir.exists())
        dir.createDirectory();
}

std::unique_ptr<juce::XmlElement> PresetManager::serializeAPVTS() const
{
    auto state = apvts_.state;
    return state.createXml();
}

bool PresetManager::deserializeAPVTS (const juce::XmlElement& xmlElement)
{
    auto tree = juce::ValueTree::fromXml (xmlElement);
    if (tree.isValid() && tree.getType() == apvts_.state.getType())
    {
        apvts_.replaceState (tree);
        return true;
    }
    return false;
}

void PresetManager::loadFactoryPresets()
{
    const std::vector<juce::String> factoryNames = {
        "Tight Mono Bass",
        "Wide Airy",
        "Subtle",
    };

    factoryPresets_.clear();
    for (size_t i = 0; i < factoryNames.size(); ++i)
    {
        PresetInfo info;
        info.name      = factoryNames[i];
        info.isFactory = true;
        info.index     = static_cast<int> (i);
        factoryPresets_.push_back (info);
    }
}

void PresetManager::loadUserPresetsFromDisk()
{
    userPresets_.clear();
    auto dir = getUserPresetsDirectory();
    if (!dir.exists())
        return;

    for (const auto& file : dir.findChildFiles (juce::File::findFiles, false, "*.xml"))
    {
        PresetInfo info;
        info.name      = file.getFileNameWithoutExtension();
        info.isFactory = false;
        info.index     = static_cast<int> (factoryPresets_.size() + userPresets_.size());
        userPresets_.push_back (info);
    }

    std::sort (userPresets_.begin(), userPresets_.end(),
               [] (const PresetInfo& a, const PresetInfo& b) { return a.name < b.name; });

    for (size_t i = 0; i < userPresets_.size(); ++i)
        userPresets_[i].index = static_cast<int> (factoryPresets_.size() + i);
}

std::vector<PresetInfo> PresetManager::getPresetList() const
{
    std::vector<PresetInfo> result = factoryPresets_;
    result.insert (result.end(), userPresets_.begin(), userPresets_.end());
    return result;
}

void PresetManager::refreshUserPresets() { loadUserPresetsFromDisk(); }

bool PresetManager::loadPreset (int index)
{
    auto all = getPresetList();
    if (index < 0 || index >= static_cast<int> (all.size()))
        return false;
    return loadPresetInternal (all[static_cast<size_t> (index)]);
}

bool PresetManager::loadPresetInternal (const PresetInfo& preset)
{
    std::unique_ptr<juce::XmlElement> xmlElement;

    if (preset.isFactory)
    {
        // Format: _NNN_PresetNameNoSpaces_xml, e.g. index 0, "Tight Mono Bass" -> "_001_TightMonoBass_xml"
        juce::String paddedIndex  = juce::String (preset.index + 1).paddedLeft ('0', 3);
        juce::String nameNoSpaces = preset.name.removeCharacters (" ");
        juce::String resourceName = "_" + paddedIndex + "_" + nameNoSpaces + "_xml";

        int dataSize = 0;
        const char* data = Outflank_BinaryData::getNamedResource (resourceName.toRawUTF8(), dataSize);
        if (data == nullptr || dataSize <= 0)
        {
            DBG ("PresetManager: binary resource not found: " + resourceName);
            return false;
        }
        xmlElement = juce::parseXML (juce::String::fromUTF8 (data, dataSize));
        if (!xmlElement)
        {
            DBG ("PresetManager: failed to parse factory preset XML: " + resourceName);
            return false;
        }
    }
    else
    {
        auto dir        = getUserPresetsDirectory();
        auto presetFile = dir.getChildFile (preset.name + ".xml");
        if (!presetFile.exists())
            return false;
        xmlElement = juce::parseXML (presetFile);
        if (!xmlElement)
        {
            DBG ("PresetManager: failed to parse preset XML: " + presetFile.getFullPathName());
            return false;
        }
    }

    bool success = deserializeAPVTS (*xmlElement);
    if (success)
        currentPresetIndex_ = preset.index;
    return success;
}

bool PresetManager::savePreset (const juce::String& presetName)
{
    if (presetName.isEmpty())
        return false;

    if (presetName.containsAnyOf ("/\\"))
        return false;

    for (const auto& factory : factoryPresets_)
        if (factory.name == presetName)
            return false;

    ensureUserPresetsDirectory();

    auto xmlElement = serializeAPVTS();
    if (!xmlElement)
        return false;

    auto dir        = getUserPresetsDirectory();
    auto presetFile = dir.getChildFile (presetName + ".xml");
    bool success    = xmlElement->writeTo (presetFile);

    if (success)
    {
        refreshUserPresets();
        for (const auto& p : getPresetList())
        {
            if (!p.isFactory && p.name == presetName)
            {
                currentPresetIndex_ = p.index;
                break;
            }
        }
    }
    return success;
}

bool PresetManager::deletePreset (int index)
{
    auto all = getPresetList();
    if (index < 0 || index >= static_cast<int> (all.size()))
        return false;

    const auto& preset = all[static_cast<size_t> (index)];
    if (preset.isFactory)
        return false;

    auto dir        = getUserPresetsDirectory();
    auto presetFile = dir.getChildFile (preset.name + ".xml");
    if (presetFile.exists() && !presetFile.deleteFile())
        return false;

    bool wasCurrent = (currentPresetIndex_ == index);
    bool currentWasFactory = true;
    juce::String currentName;
    if (!wasCurrent && currentPresetIndex_ >= 0 && currentPresetIndex_ < static_cast<int> (all.size()))
    {
        currentWasFactory = all[static_cast<size_t> (currentPresetIndex_)].isFactory;
        currentName       = all[static_cast<size_t> (currentPresetIndex_)].name;
    }

    refreshUserPresets();

    if (wasCurrent)
    {
        currentPresetIndex_ = 0;
    }
    else if (!currentWasFactory)
    {
        bool found = false;
        for (const auto& p : getPresetList())
        {
            if (!p.isFactory && p.name == currentName)
            {
                currentPresetIndex_ = p.index;
                found = true;
                break;
            }
        }
        if (!found)
            currentPresetIndex_ = 0;
    }

    if (currentPresetIndex_ >= static_cast<int> (getPresetList().size()))
        currentPresetIndex_ = 0;
    return true;
}

bool PresetManager::renamePreset (int index, const juce::String& newName)
{
    if (newName.isEmpty())
        return false;

    if (newName.containsAnyOf ("/\\"))
        return false;

    auto all = getPresetList();
    if (index < 0 || index >= static_cast<int> (all.size()))
        return false;

    const auto& preset = all[static_cast<size_t> (index)];
    if (preset.isFactory)
        return false;

    auto dir     = getUserPresetsDirectory();
    auto oldFile = dir.getChildFile (preset.name + ".xml");
    auto newFile = dir.getChildFile (newName + ".xml");
    if (!oldFile.exists() || newFile.exists())
        return false;

    bool success = oldFile.moveFileTo (newFile);
    if (success)
    {
        bool wasCurrent = (currentPresetIndex_ == index);
        bool currentWasFactory = true;
        juce::String currentName;
        if (!wasCurrent && currentPresetIndex_ >= 0 && currentPresetIndex_ < static_cast<int> (all.size()))
        {
            currentWasFactory = all[static_cast<size_t> (currentPresetIndex_)].isFactory;
            currentName       = all[static_cast<size_t> (currentPresetIndex_)].name;
        }

        refreshUserPresets();

        if (wasCurrent)
        {
            for (const auto& p : getPresetList())
            {
                if (!p.isFactory && p.name == newName)
                {
                    currentPresetIndex_ = p.index;
                    break;
                }
            }
        }
        else if (!currentWasFactory)
        {
            bool found = false;
            for (const auto& p : getPresetList())
            {
                if (!p.isFactory && p.name == currentName)
                {
                    currentPresetIndex_ = p.index;
                    found = true;
                    break;
                }
            }
            if (!found)
                currentPresetIndex_ = 0;
        }
    }
    return success;
}
