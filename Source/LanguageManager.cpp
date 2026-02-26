//
//  LanguageManager.cpp
//  Light Host
//
//  Language and localization support
//

#include "LanguageManager.hpp"

namespace BD = BinaryData;

LanguageManager& LanguageManager::getInstance()
{
    static LanguageManager instance;
    return instance;
}

LanguageManager::LanguageManager() : currentLanguageId("English")
{
    loadLanguageById("English");
}

void LanguageManager::setLanguageById(const String& languageId)
{
    if (languageId != currentLanguageId)
    {
        currentLanguageId = languageId;
        loadLanguageById(languageId);
    }
}

String LanguageManager::getText(const String& key) const
{
    if (languageData.isObject())
    {
        var translation = languageData[Identifier(key)];
        if (!translation.isVoid())
            return translation.toString();
    }
    return key;
}

var LanguageManager::loadJsonById(const String& languageId) const
{
    // Try external file first
    File languageFile = File::getSpecialLocation(File::userApplicationDataDirectory)
        .getChildFile("LightHost")
        .getChildFile("Languages")
        .getChildFile(languageId + ".json");

    if (languageFile.existsAsFile())
    {
        String fileContent = languageFile.loadFileAsString();
        return JSON::parse(fileContent);
    }

    // Look up in embedded binary resources: resource name is "LanguageId_json"
    String resourceName = languageId + "_json";
    int dataSize = 0;
    const char* data = BD::getNamedResource(resourceName.toRawUTF8(), dataSize);

    if (data != nullptr && dataSize > 0)
    {
        String jsonString = String::fromUTF8(data, dataSize);
        return JSON::parse(jsonString);
    }

    return var();
}

void LanguageManager::loadLanguageById(const String& languageId)
{
    var data = loadJsonById(languageId);

    if (!data.isVoid())
    {
        languageData = data;
        applyJuceLocalisedStrings(data, languageId);
        return;
    }

    // Fallback to English
    if (languageId != "English")
        loadLanguageById("English");
}

void LanguageManager::applyJuceLocalisedStrings(const var& data, const String& languageId)
{
    // For English, clear any existing mapping (JUCE built-ins are already in English)
    if (languageId == "English")
    {
        LocalisedStrings::setCurrentMappings(nullptr);
        return;
    }

    // Read the "juceStrings" section which maps original English JUCE TRANS() strings to translations
    var juceStrings = data[Identifier("juceStrings")];
    if (!juceStrings.isObject())
    {
        LocalisedStrings::setCurrentMappings(nullptr);
        return;
    }

    // Build a LocalisedStrings mapping content string in JUCE translation file format:
    // "Original English string" = "Translated string"
    String mappingContent = "language: " + languageId + "\n\n";

    auto* obj = juceStrings.getDynamicObject();
    if (obj != nullptr)
    {
        for (auto& prop : obj->getProperties())
        {
            String original   = prop.name.toString();
            String translated = prop.value.toString();
            // Escape any embedded quotes in the strings
            original   = original.replace("\\", "\\\\").replace("\"", "\\\"");
            translated = translated.replace("\\", "\\\\").replace("\"", "\\\"");
            mappingContent += "\"" + original + "\" = \"" + translated + "\"\n";
        }
    }

    // Apply as the current JUCE translation mappings
    // This makes all TRANS() calls in JUCE built-in components return localized text
    LocalisedStrings::setCurrentMappings(new LocalisedStrings(mappingContent, false));
}

Array<LanguageManager::LanguageInfo> LanguageManager::getAvailableLanguages() const
{
    Array<LanguageInfo> languages;

    // Scan all embedded binary resources for entries ending in "_json"
    for (int i = 0; i < BD::namedResourceListSize; ++i)
    {
        String resourceName(BD::namedResourceList[i]);

        if (!resourceName.endsWith("_json"))
            continue;

        // Strip the "_json" suffix to get the language ID
        String langId = resourceName.dropLastCharacters(5); // remove "_json"

        var data = loadJsonById(langId);
        if (data.isObject())
        {
            var nameVar = data[Identifier("languageName")];
            String displayName = nameVar.isVoid() ? langId : nameVar.toString();
            languages.add({ langId, displayName });
        }
    }

    return languages;
}

