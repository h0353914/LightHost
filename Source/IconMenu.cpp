//
//  IconMenu.cpp
//  Light Host
//
//  Created by Rolando Islas on 12/26/15.
//
//

#include "JuceHeader.h"
#include "IconMenu.hpp"
#include "LanguageManager.hpp"
#include "PluginWindow.h"
#include <ctime>
#include <limits.h>
#if JUCE_WINDOWS
#include "Windows.h"
#include "VoicemeeterAudioDevice.h"  // Windows 專用：Voicemeeter 音頻設備支援
#endif

// ==================== Windows 平台特定實現 ====================

#if JUCE_WINDOWS
/**
 * LightHostAudioDeviceManager::createAudioDeviceTypes() 實現
 * 
 * 建立並註冊所有可用的音頻設備類型
 * 包括標準系統音頻設備和 Voicemeeter 虛擬設備
 * 
 * 步驟：
 * 1. 調用基類方法註冊標準設備
 * 2. 添加 Voicemeeter 自訂設備類型
 */
void LightHostAudioDeviceManager::createAudioDeviceTypes (OwnedArray<AudioIODeviceType>& types)
{
    // 首先調用基類方法添加標準 JUCE 設備
    AudioDeviceManager::createAudioDeviceTypes (types);
    
    // 然後添加 Voicemeeter 設備
    types.add (new VoicemeeterAudioIODeviceType());
}
#endif

namespace
{
constexpr int languageMenuItemBase = 2000000000;  // 語言菜單項 ID 基數
}

class IconMenu::PluginListWindow : public DocumentWindow
{
public:
	PluginListWindow(IconMenu& owner_, AudioPluginFormatManager& pluginFormatManager)
		: DocumentWindow(LanguageManager::getInstance().getText("availablePlugins"), Colours::white,
			DocumentWindow::minimiseButton | DocumentWindow::closeButton),
		owner(owner_)
	{
		const File deadMansPedalFile(getAppProperties().getUserSettings()
			->getFile().getSiblingFile("RecentlyCrashedPluginsList"));

		setContentOwned(new PluginListComponent(pluginFormatManager,
			owner.knownPluginList,
			deadMansPedalFile,
			getAppProperties().getUserSettings()), true);

		setUsingNativeTitleBar(true);
		setResizable(true, false);
		setResizeLimits(300, 400, 800, 1500);
		setTopLeftPosition(60, 60);

		restoreWindowStateFromString(getAppProperties().getUserSettings()->getValue("listWindowPos"));
		setVisible(true);
	}

	~PluginListWindow()
	{
		getAppProperties().getUserSettings()->setValue("listWindowPos", getWindowStateAsString());

		clearContentComponent();
	}

	void closeButtonPressed()
	{
        owner.removePluginsLackingInputOutput();

private:
	IconMenu& owner;
};

IconMenu::IconMenu() : INDEX_EDIT(1000000), INDEX_BYPASS(2000000), INDEX_DELETE(3000000), INDEX_MOVE_UP(4000000), INDEX_MOVE_DOWN(5000000)
{
    // Initiialization
    addDefaultFormatsToManager(formatManager);
    
    // Load saved language preference and apply it
    String savedLanguageId = getAppProperties().getUserSettings()->getValue("language", "English");
    LanguageManager::getInstance().setLanguageById(savedLanguageId);
    
    // Audio device
    std::unique_ptr<XmlElement> savedAudioState (getAppProperties().getUserSettings()->getXmlValue("audioDeviceState"));
    deviceManager.initialise(256, 256, savedAudioState.get(), true);
    player.setProcessor(&graph);
    deviceManager.addAudioCallback(&player);
    // Plugins - all
    std::unique_ptr<XmlElement> savedPluginList(getAppProperties().getUserSettings()->getXmlValue("pluginList"));
    if (savedPluginList != nullptr)
        knownPluginList.recreateFromXml(*savedPluginList);
    pluginSortMethod = KnownPluginList::sortByManufacturer;
    knownPluginList.addChangeListener(this);
    // Plugins - active
    std::unique_ptr<XmlElement> savedPluginListActive(getAppProperties().getUserSettings()->getXmlValue("pluginListActive"));
    if (savedPluginListActive != nullptr)
        activePluginList.recreateFromXml(*savedPluginListActive);
    loadActivePlugins();
    activePluginList.addChangeListener(this);
	setIcon();
	setIconTooltip(LanguageManager::getInstance().getText("appName"));
}

IconMenu::~IconMenu()
{
	savePluginStates();
}

void IconMenu::setIcon()
{
	// Set menu icon - Windows only
	Image icon;
	String defaultColor = "white";

	if (!getAppProperties().getUserSettings()->containsKey("icon"))
		getAppProperties().getUserSettings()->setValue("icon", defaultColor);

	String color = getAppProperties().getUserSettings()->getValue("icon");
	if (color.equalsIgnoreCase("white"))
		icon = ImageFileFormat::loadFrom(BinaryData::menu_icon_white_png, BinaryData::menu_icon_white_pngSize);
	else if (color.equalsIgnoreCase("black"))
		icon = ImageFileFormat::loadFrom(BinaryData::menu_icon_png, BinaryData::menu_icon_pngSize);
	setIconImage(icon, icon);
}

void IconMenu::loadActivePlugins()
{
	const int INPUT = 1000000;
	const int OUTPUT = INPUT + 1;
	const int CHANNEL_ONE = 0;
	const int CHANNEL_TWO = 1;
	PluginWindow::closeAllCurrentlyOpenWindows();
    graph.clear();
	inputNode = graph.addNode(std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor> (AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode), AudioProcessorGraph::NodeID(INPUT));
    outputNode = graph.addNode(std::make_unique<AudioProcessorGraph::AudioGraphIOProcessor> (AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode), AudioProcessorGraph::NodeID(OUTPUT));
    if (activePluginList.getNumTypes() == 0)
    {
		graph.addConnection ({ { AudioProcessorGraph::NodeID(INPUT), CHANNEL_ONE }, { AudioProcessorGraph::NodeID(OUTPUT), CHANNEL_ONE } });
		graph.addConnection ({ { AudioProcessorGraph::NodeID(INPUT), CHANNEL_TWO }, { AudioProcessorGraph::NodeID(OUTPUT), CHANNEL_TWO } });
    }
	int pluginTime = 0;
	int lastId = 0;
	bool hasInputConnected = false;
	// NOTE: Node ids cannot begin at 0.
    for (int i = 1; i <= activePluginList.getNumTypes(); i++)
    {
        PluginDescription plugin = getNextPluginOlderThanTime(pluginTime);
        String errorMessage;
        std::unique_ptr<AudioPluginInstance> instance = formatManager.createPluginInstance(plugin, graph.getSampleRate(), graph.getBlockSize(), errorMessage);
		String pluginUid = getKey("state", plugin);
        String savedPluginState = getAppProperties().getUserSettings()->getValue(pluginUid);
        MemoryBlock savedPluginBinary;
        savedPluginBinary.fromBase64Encoding(savedPluginState);
        instance->setStateInformation(savedPluginBinary.getData(), static_cast<int>(savedPluginBinary.getSize()));
        graph.addNode(std::move(instance), AudioProcessorGraph::NodeID(i)); // TODO https://stackoverflow.com/a/17473958
		String key = getKey("bypass", plugin);
		bool bypass = getAppProperties().getUserSettings()->getBoolValue(key, false);
        // Input to plugin
        if ((!hasInputConnected) && (!bypass))
        {
            graph.addConnection({ { AudioProcessorGraph::NodeID(INPUT), CHANNEL_ONE }, { AudioProcessorGraph::NodeID(i), CHANNEL_ONE } });
            graph.addConnection({ { AudioProcessorGraph::NodeID(INPUT), CHANNEL_TWO }, { AudioProcessorGraph::NodeID(i), CHANNEL_TWO } });
			hasInputConnected = true;
        }
        // Connect previous plugin to current
        else if ((!bypass))
        {
            graph.addConnection({ { AudioProcessorGraph::NodeID(lastId), CHANNEL_ONE }, { AudioProcessorGraph::NodeID(i), CHANNEL_ONE } });
            graph.addConnection({ { AudioProcessorGraph::NodeID(lastId), CHANNEL_TWO }, { AudioProcessorGraph::NodeID(i), CHANNEL_TWO } });
        }
		if (!bypass)
		    lastId = i;
    }
	if (lastId > 0)
	{
		// Last active plugin to output
		graph.addConnection({ { AudioProcessorGraph::NodeID(lastId), CHANNEL_ONE }, { AudioProcessorGraph::NodeID(OUTPUT), CHANNEL_ONE } });
		graph.addConnection({ { AudioProcessorGraph::NodeID(lastId), CHANNEL_TWO }, { AudioProcessorGraph::NodeID(OUTPUT), CHANNEL_TWO } });
	}
}

PluginDescription IconMenu::getNextPluginOlderThanTime(int &time)
{
	int timeStatic = time;
	PluginDescription closest;
	int diff = INT_MAX;
	for (const auto& plugin : activePluginList.getTypes())
	{
		String key = getKey("order", plugin);
		String pluginTimeString = getAppProperties().getUserSettings()->getValue(key);
		int pluginTime = atoi(pluginTimeString.toStdString().c_str());
		if (pluginTime > timeStatic && abs(timeStatic - pluginTime) < diff)
		{
			diff = abs(timeStatic - pluginTime);
			closest = plugin;
			time = pluginTime;
		}
	}
	return closest;
}

void IconMenu::changeListenerCallback(ChangeBroadcaster* changed)
{
    if (changed == &knownPluginList)
    {
        std::unique_ptr<XmlElement> savedPluginList (knownPluginList.createXml());
        if (savedPluginList != nullptr)
        {
            getAppProperties().getUserSettings()->setValue ("pluginList", savedPluginList.get());
            getAppProperties().saveIfNeeded();
        }
    }
    else if (changed == &activePluginList)
    {
        std::unique_ptr<XmlElement> savedPluginList (activePluginList.createXml());
        if (savedPluginList != nullptr)
        {
            getAppProperties().getUserSettings()->setValue ("pluginListActive", savedPluginList.get());
            getAppProperties().saveIfNeeded();
        }
    }
}


void IconMenu::timerCallback()
{
    stopTimer();
    menu.clear();
    menu.addSectionHeader(LanguageManager::getInstance().getText("appName"));

    // Edit Plugins - simple menu item
    menu.addItem(2, LanguageManager::getInstance().getText("editPlugins"));

    // Language selection - Dynamically generated from available languages
    PopupMenu languageMenu;
    int languageMenuItemId = languageMenuItemBase;
    auto availableLanguages = LanguageManager::getInstance().getAvailableLanguages();

    for (const auto& lang : availableLanguages)
    {
        bool isCurrent = (lang.id == LanguageManager::getInstance().getCurrentLanguageId());
        languageMenu.addItem(languageMenuItemId, lang.displayName, true, isCurrent);
        languageMenuItemId++;
    }

    menu.addSubMenu(LanguageManager::getInstance().getText("languageMenuLabel"), languageMenu);

    // Invert Icon Color
    menu.addItem(3, LanguageManager::getInstance().getText("invertIconColor"));

    menu.addSeparator();

    // Quit
    menu.addItem(1, LanguageManager::getInstance().getText("quit"));

	menu.showMenuAsync(PopupMenu::Options().withMousePosition(), ModalCallbackFunction::forComponent(menuInvocationCallback, this));
}

void IconMenu::mouseDown(const MouseEvent& e)
{
    // Only show menu on right-click
    if (!e.mods.isLeftButtonDown())
    {
        Process::makeForegroundProcess();
        startTimer(50);
    }
}

void IconMenu::menuInvocationCallback(int id, IconMenu* im)
{
    // ID 1: Quit
    if (id == 1)
    {
        im->savePluginStates();
        return JUCEApplication::getInstance()->quit();
    }

    // ID 2: Edit Plugins (reload plugins)
    if (id == 2)
    {
        return im->reloadPlugins();
    }

    // ID 3: Invert Icon Color
    if (id == 3)
    {
        String color = getAppProperties().getUserSettings()->getValue("icon");
        getAppProperties().getUserSettings()->setValue("icon", color.equalsIgnoreCase("black") ? "white" : "black");
        return im->setIcon();
    }

    // Language selection - Handle dynamic language menu items
    if (id >= languageMenuItemBase)
    {
        auto availableLanguages = LanguageManager::getInstance().getAvailableLanguages();
        int languageIndex = id - languageMenuItemBase;

        if (languageIndex >= 0 && languageIndex < availableLanguages.size())
        {
            const auto& selectedLanguage = availableLanguages[languageIndex];
            LanguageManager::getInstance().setLanguageById(selectedLanguage.id);

            // Save language preference
            getAppProperties().getUserSettings()->setValue("language", selectedLanguage.id);
            getAppProperties().saveIfNeeded();
            im->startTimer(50);
            return;
        }
    }
}

std::vector<PluginDescription> IconMenu::getTimeSortedList()
{
	int time = 0;
	std::vector<PluginDescription> list;
	for (int i = 0; i < activePluginList.getNumTypes(); i++)
		list.push_back(getNextPluginOlderThanTime(time));
	return list;
		
}

String IconMenu::getKey(String type, PluginDescription plugin)
{
	String key = "plugin-" + type.toLowerCase() + "-" + plugin.name + plugin.version + plugin.pluginFormatName;
	return key;
}

void IconMenu::deletePluginStates()
{
	std::vector<PluginDescription> list = getTimeSortedList();
    for (int i = 0; i < activePluginList.getNumTypes(); i++)
    {
		String pluginUid = getKey("state", list[i]);
        getAppProperties().getUserSettings()->removeValue(pluginUid);
        getAppProperties().saveIfNeeded();
    }
}

void IconMenu::savePluginStates()
{
	std::vector<PluginDescription> list = getTimeSortedList();
    for (int i = 0; i < activePluginList.getNumTypes(); i++)
    {
		AudioProcessorGraph::Node* node = graph.getNodeForId(AudioProcessorGraph::NodeID(i + 1));
		if (node == nullptr)
			break;
        AudioProcessor& processor = *node->getProcessor();
		String pluginUid = getKey("state", list[i]);
        MemoryBlock savedStateBinary;
        processor.getStateInformation(savedStateBinary);
        getAppProperties().getUserSettings()->setValue(pluginUid, savedStateBinary.toBase64Encoding());
        getAppProperties().saveIfNeeded();
    }
}

void IconMenu::showAudioSettings()
{
    AudioDeviceSelectorComponent audioSettingsComp (deviceManager, 0, 256, 0, 256, false, false, true, false);
    audioSettingsComp.setSize(500, 600);
    
    DialogWindow::LaunchOptions o;
    o.content.setNonOwned(&audioSettingsComp);
    o.dialogTitle                   = LanguageManager::getInstance().getText("audioSettings");
    o.componentToCentreAround       = this;
    o.dialogBackgroundColour        = Colour::fromRGB(236, 236, 236);
    o.escapeKeyTriggersCloseButton  = true;
    o.useNativeTitleBar             = true;
    o.resizable                     = false;

    o.runModal();
        
    std::unique_ptr<XmlElement> audioState(deviceManager.createStateXml());
        
    getAppProperties().getUserSettings()->setValue("audioDeviceState", audioState.get());
    getAppProperties().getUserSettings()->saveIfNeeded();
}

void IconMenu::reloadPlugins()
{
	if (pluginListWindow == nullptr)
		pluginListWindow.reset (new PluginListWindow(*this, formatManager));
	pluginListWindow->toFront(true);
}

void IconMenu::removePluginsLackingInputOutput()
{
	// TODO needs sanity check
    for (const auto& plugin : knownPluginList.getTypes())
    {
        if (plugin.numInputChannels < 2 || plugin.numOutputChannels < 2)
		    knownPluginList.removeType(plugin);
    }
}
