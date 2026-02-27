//
//  IconMenu.hpp
//  Light Host
//
//  Created by Rolando Islas on 12/26/15.
//
//

#ifndef IconMenu_hpp
#define IconMenu_hpp

#include "LanguageManager.hpp"

// ==================== Windows 平台特定類別 ====================
#if JUCE_WINDOWS
class VoicemeeterAudioIODeviceType;

/**
 * LightHostAudioDeviceManager 類別
 * 
 * Windows 平台自訂音頻設備管理器
 * 
 * 功能：
 * - 繼承自 JUCE AudioDeviceManager
 * - 註冊 Voicemeeter 虛擬音頻設備作為可用設備
 * - 支持與 Voicemeeter 的音頻互通
 * - Voicemeeter 允許應用程式互相連接音頻提供高度的靈活性
 */
class LightHostAudioDeviceManager : public AudioDeviceManager
{
public:
    /**
     * 覆蓋 createAudioDeviceTypes() 方法
     * 建立並註冊自訂的音頻設備類型
     * 包括標準系統設備和 Voicemeeter 虛擬設備
     * 
     * @param types 用於儲存新建立的音頻設備類型的陣列
     */
    void createAudioDeviceTypes (OwnedArray<AudioIODeviceType>& types) override;
};
#endif

// ==================== 全局函數宣告 ====================
/**
 * getAppProperties() 函數
 * 取得全局應用程式屬性實例
 * 用於存取持久化設置
 * 
 * @return ApplicationProperties 參考
 */
ApplicationProperties& getAppProperties();

class IconMenu : public SystemTrayIconComponent, private Timer, public ChangeListener
{
public:
    IconMenu();
    ~IconMenu();
    void mouseDown(const MouseEvent&);
    static void menuInvocationCallback(int id, IconMenu*);
    void changeListenerCallback(ChangeBroadcaster* changed);
	static String getKey(String type, PluginDescription plugin);

	const int INDEX_EDIT, INDEX_BYPASS, INDEX_DELETE, INDEX_MOVE_UP, INDEX_MOVE_DOWN;
private:
	#if JUCE_MAC
    std::string exec(const char* cmd);
	#endif
    void timerCallback();
    void reloadPlugins();
    void showAudioSettings();
    void loadActivePlugins();
    void savePluginStates();
    void deletePluginStates();
	PluginDescription getNextPluginOlderThanTime(int &time);
	void removePluginsLackingInputOutput();
	std::vector<PluginDescription> getTimeSortedList();
	void setIcon();

    // ==================== 音頻處理成員 ====================
    
    /**
     * 條件編譯：Windows vs 其他平台
     * Windows 使用自訂的 LightHostAudioDeviceManager
     * 其他平台使用標準的 JUCE AudioDeviceManager
     */
    #if JUCE_WINDOWS
    LightHostAudioDeviceManager deviceManager;  // Windows 自訂設備管理器
    #else
    AudioDeviceManager deviceManager;  // 其他平台標準設備管理器
    #endif
    
    AudioPluginFormatManager formatManager;
    KnownPluginList knownPluginList;
    Array<PluginDescription> pluginMenuTypes;
    KnownPluginList activePluginList;
    KnownPluginList::SortMethod pluginSortMethod;
    PopupMenu menu;
    std::unique_ptr<PluginDirectoryScanner> scanner;
    bool menuIconLeftClicked;
    AudioProcessorGraph graph;
    AudioProcessorPlayer player;
    AudioProcessorGraph::Node *inputNode;
    AudioProcessorGraph::Node *outputNode;
	#if JUCE_WINDOWS
	int x, y;
	#endif

	class PluginListWindow;
	std::unique_ptr<PluginListWindow> pluginListWindow;
};

#endif /* IconMenu_hpp */
