//
//  LanguageManager.hpp
//  Light Host
//
//  Language and localization support
//

#ifndef LanguageManager_hpp
#define LanguageManager_hpp

#include "JuceHeader.h"

class LanguageManager
{
public:
    struct LanguageInfo
    {
        String id;           // Resource name (e.g. "English", "TraditionalChinese")
        String displayName;  // Display name read from JSON "languageName" field
    };

    static LanguageManager& getInstance();

    // Set language by resource ID
    void setLanguageById(const String& languageId);
    String getCurrentLanguageId() const { return currentLanguageId; }

    // Returns all available languages by scanning embedded binary resources
    Array<LanguageInfo> getAvailableLanguages() const;

    String getText(const String& key) const;

private:
    LanguageManager();
    ~LanguageManager() {}

    String currentLanguageId;
    var languageData;

    // Load JSON from embedded binary data or external file
    void loadLanguageById(const String& languageId);
    var loadJsonById(const String& languageId) const;

    // Apply JUCE built-in component translations via LocalisedStrings
    void applyJuceLocalisedStrings(const var& data, const String& languageId);

    // Prevent copying
    LanguageManager(const LanguageManager&) = delete;
    LanguageManager& operator=(const LanguageManager&) = delete;
};

#endif /* LanguageManager_hpp */
