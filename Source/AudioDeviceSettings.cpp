#include "AudioDeviceSettings.h"
#include "LanguageManager.hpp"
#include <Windows.h>

//==============================================================================
// ScaleSettingsManager 實現
// 負責管理和持久化 UI 縮放設定
//==============================================================================

/// 取得全域單例實例
ScaleSettingsManager &ScaleSettingsManager::getInstance()
{
    static ScaleSettingsManager instance;
    return instance;
}

/// 取得當前縮放因子（1.0 = 100%）
float ScaleSettingsManager::getScaleFactor() const { return scaleFactor; }

/// 設定新的縮放因子並自動保存設定
void ScaleSettingsManager::setScaleFactor(float scale)
{
    scaleFactor = scale;
    saveSettings();
}

/// 從應用屬性中載入已保存的縮放設定
void ScaleSettingsManager::loadSettings()
{
    try
    {
        auto &props = getAppProperties();
        float v = props.getUserSettings()->getValue("uiScaleFactor", "1.0").getFloatValue();
        // 驗證縮放因子在有效範圍內（0.5 ~ 3.0）
        if (v >= 0.5f && v <= 3.0f)
            scaleFactor = v;
    }
    catch (...)
    {
    }
}

/// 將縮放設定保存到應用屬性
void ScaleSettingsManager::saveSettings()
{
    try
    {
        auto &props = getAppProperties();
        props.getUserSettings()->setValue("uiScaleFactor", String(scaleFactor));
        props.saveIfNeeded();
    }
    catch (...)
    {
    }
}

//==============================================================================
// 內聯輔助函數
//==============================================================================

/// 取得 DPI 縮放因子
inline float getDPIScaleFactor() { return ScaleSettingsManager::getInstance().getScaleFactor(); }

/// 取得組合縮放因子（DPI × 語言字體縮放）
inline float getFontScaleFactor() { return getDPIScaleFactor() * LanguageManager::getInstance().getFontScaling(); }

/// 取得系統窗框高度（包括標題欄下方的邊框）
inline int getSystemFrameHeight()
{
    // SM_CYFRAME: 調整不可調整大小的窗口的邊框的厚度（垂直）
    int frameHeight = GetSystemMetrics(SM_CYFRAME);
    // SM_CYSIZE: 視窗按鈕（最小化、最大化、關閉）的高度
    int buttonHeight = GetSystemMetrics(SM_CYSIZE);
    return frameHeight + buttonHeight;
}

class ScaledSelectorLookAndFeel : public LookAndFeel_V3
{
public:
    void setScaleFactor(float newScale)
    {
        scaleFactor = jmax(0.5f, newScale);
    }

    Font getComboBoxFont(ComboBox& box) override
    {
        const float baseHeight = LookAndFeel_V3::getComboBoxFont(box).getHeight();
        const float scaledHeight = baseHeight * scaleFactor;
        const float maxHeight = static_cast<float>(box.getHeight()) * 0.85f;
        return Font(FontOptions{}.withHeight(jmin(scaledHeight, maxHeight)));
    }

    Font getPopupMenuFont() override
    {
        const float baseHeight = LookAndFeel_V3::getPopupMenuFont().getHeight();
        return Font(FontOptions{}.withHeight(baseHeight * scaleFactor));
    }

    Font getTextButtonFont(TextButton& button, int buttonHeight) override
    {
        const float baseHeight = LookAndFeel_V3::getTextButtonFont(button, buttonHeight).getHeight();
        const float scaledHeight = baseHeight * scaleFactor;
        const float maxHeight = static_cast<float>(buttonHeight) * 0.7f;
        return Font(FontOptions{}.withHeight(jmin(scaledHeight, maxHeight)));
    }

    void drawToggleButton(Graphics& g, ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        const auto fontSize = jmin(15.0f * scaleFactor, static_cast<float>(button.getHeight()) * 0.75f);
        const auto tickWidth = fontSize * 1.1f;

        drawTickBox(g, button, 4.0f, (static_cast<float>(button.getHeight()) - tickWidth) * 0.5f,
                    tickWidth, tickWidth,
                    button.getToggleState(),
                    button.isEnabled(),
                    shouldDrawButtonAsHighlighted,
                    shouldDrawButtonAsDown);

        g.setColour(button.findColour(ToggleButton::textColourId));
        g.setFont(fontSize);

        if (!button.isEnabled())
            g.setOpacity(0.5f);

        g.drawFittedText(button.getButtonText(),
                         button.getLocalBounds().withTrimmedLeft(roundToInt(tickWidth) + 5)
                                                .withTrimmedRight(2),
                         Justification::centredLeft, 10);
    }

private:
    float scaleFactor = 1.0f;
};

inline ScaledSelectorLookAndFeel& getScaledSelectorLookAndFeel()
{
    static ScaledSelectorLookAndFeel lookAndFeel;
    return lookAndFeel;
}

//==============================================================================
// DeviceSelectorDialog 實現
// 顯示 JUCE AudioDeviceSelectorComponent 並應用動態縮放
//==============================================================================

/// 建構函數：初始化對話框，創建音訊設備選擇器並應用縮放
DeviceSelectorDialog::DeviceSelectorDialog(AudioDeviceManager &dm, int maxIn, int maxOut)
    : mgr(dm), initialMaxIn(maxIn), initialMaxOut(maxOut)
{
    updateSelectorComponent();
    startTimer(150);
}

/// 解構函數
DeviceSelectorDialog::~DeviceSelectorDialog()
{
    stopTimer();
    if (sel)
        sel->setLookAndFeel(nullptr);
}

/// 計時器回調：偵測縮放因子變化
void DeviceSelectorDialog::timerCallback()
{
    float cur = getFontScaleFactor();
    if (std::abs(cur - lastScale) < 0.005f)
        return;
    // 縮放改變：重建 selector（後續 async block 會量測高度並呼叫 onScaleChanged）
    updateSelectorComponent();
}

void DeviceSelectorDialog::updateSelectorComponent()
{
    // 如果已經存在舊的 selector 元件，先從畫面上移除
    // 避免重複疊加或殘留舊狀態
    if (sel)
    {
        sel->setLookAndFeel(nullptr);
        removeChildComponent(sel.get());
    }

    // 重置與字體縮放相關的快取資料
    // naturalFontHeight：儲存原始字體高度（未縮放）
    naturalFontHeight.clear();

    // naturalItemHeight：AudioDeviceSelectorComponent 原始 item 高度
    naturalItemHeight = 0;

    // baselinesReady：表示是否已收集完成基準字體資料
    baselinesReady = false;

    // lastScale：記錄上一次套用的縮放比例
    // 設為 -1 表示強制重新計算
    lastScale = -1.0f;

    // computedContentHeight：重建時歸零，由 async block 重新量測
    computedContentHeight = 0;

    // 建立新的 AudioDeviceSelectorComponent
    //
    // 參數說明：
    // mgr           : AudioDeviceManager
    // 0             : 最小輸入通道數
    // initialMaxIn  : 最大輸入通道數
    // 0             : 最小輸出通道數
    // initialMaxOut : 最大輸出通道數
    // false ×4     : 不顯示 MIDI、音訊設定、進階選項
    sel = std::make_unique<AudioDeviceSelectorComponent>(
        mgr, 0, initialMaxIn, 0, initialMaxOut,
        false, false, false, false);

    auto& selectorLookAndFeel = getScaledSelectorLookAndFeel();
    selectorLookAndFeel.setScaleFactor(getFontScaleFactor());
    sel->setLookAndFeel(&selectorLookAndFeel);

    // 加入畫面並設為可見
    addAndMakeVisible(sel.get());

    // 使用 callAsync 是因為：
    // 1. 需要等元件真正加入並完成 layout
    // 2. 某些字體與 itemHeight 要在 UI thread 完整建立後才準確
    MessageManager::callAsync([this]
                              {
        // 若在這期間 sel 被刪除，直接安全退出
        if (!sel) return;

        // 取得原始 item 高度（未經縮放）
        naturalItemHeight = sel->getItemHeight();

        // 收集所有子元件的原始字體大小
        // 這樣之後才能按比例縮放
        collectNaturalFonts(sel.get());

        // 標記基準資料已準備完成
        baselinesReady = true;

        // 計算總縮放比例（通常包含 DPI + 語系縮放）
        float totalScale = getFontScaleFactor();
        lastScale = totalScale;

        getScaledSelectorLookAndFeel().setScaleFactor(totalScale);

        // 依照縮放比例調整 item 高度
        // jmax(1, ...) 確保高度至少為 1，避免意外變 0
        sel->setItemHeight(
            jmax(1, static_cast<int>(naturalItemHeight * totalScale))
        );

        // 對所有子元件套用整體縮放（字體、間距等）
        applyTotalScale(sel.get(), totalScale);

        // 給 sel 一個足夠大的臨時大小，讓 JUCE 內部完成 layout
        sel->setSize(static_cast<int>(420 * totalScale), 2000);
        sel->resized();

        // 量測所有可見子元件的最低封底邊緣，作為實際內容高度
        int maxBottom = 0;
        for (int i = 0; i < sel->getNumChildComponents(); ++i)
        {
            auto* c = sel->getChildComponent(i);
            if (c->isVisible())
                maxBottom = jmax(maxBottom, c->getBottom());
        }
        computedContentHeight = (maxBottom > 30) ? maxBottom : static_cast<int>(300 * totalScale);

        // 通知父視窗依實際高度重新計算大小
        if (onScaleChanged)
            onScaleChanged();

        // 重新 layout（此時大小由 Window 設定，會再觸發 resized())
        if (getHeight() > 0)
            resized(); });
}

/// 調整子組件大小和位置
void DeviceSelectorDialog::resized()
{
    // sel 直接填滿整個 Dialog（由 DeviceSelectorWindow 控制總體大小）
    if (sel)
        sel->setBounds(getLocalBounds());

    // 重新應用字體縮放（如果縮放因子已改變）
    if (baselinesReady)
    {
        float totalScale = getFontScaleFactor();
        if (std::abs(totalScale - lastScale) > 0.005f)
        {
            lastScale = totalScale;
            getScaledSelectorLookAndFeel().setScaleFactor(totalScale);
            sel->setItemHeight(jmax(1, static_cast<int>(naturalItemHeight * totalScale)));
            applyTotalScale(sel.get(), totalScale);
            sel->resized();
        }
    }
}

// ---- private helpers ----

void DeviceSelectorDialog::collectNaturalFonts(Component *comp)
{
    if (!comp)
        return;
    if (auto *lbl = dynamic_cast<Label *>(comp))
        naturalFontHeight[comp] = lbl->getFont().getHeight();
    for (int i = 0; i < comp->getNumChildComponents(); ++i)
        collectNaturalFonts(comp->getChildComponent(i));
}

void DeviceSelectorDialog::applyTotalScale(Component *comp, float totalScale)
{
    if (!comp)
        return;
    if (auto *lbl = dynamic_cast<Label *>(comp))
    {
        auto it = naturalFontHeight.find(comp);
        if (it != naturalFontHeight.end())
            lbl->setFont(lbl->getFont().withHeight(it->second * totalScale));
    }
    for (int i = 0; i < comp->getNumChildComponents(); ++i)
        applyTotalScale(comp->getChildComponent(i), totalScale);
}

String DeviceSelectorDialog::getCurrentDeviceName() const
{
    String name = LanguageManager::getInstance().getText("audioDevice");
    if (auto *d = mgr.getCurrentAudioDevice())
        name = d->getName();
    return name;
}

//==============================================================================
// DeviceSelectorWindow 實現
// 可調整大小的視窗，包含 DeviceSelectorDialog
//==============================================================================

/// 建構函數：創建視窗並初始化對話框組件
DeviceSelectorWindow::DeviceSelectorWindow(const String &title, AudioDeviceManager &dm,
                                           int maxIn, int maxOut,
                                           std::function<void(const String &)> cb)
    : DocumentWindow(title, Colours::lightgrey,
                     DocumentWindow::closeButton | DocumentWindow::minimiseButton, true),
      onConfirmCallback(std::move(cb))
{
    // 創建並設定對話框組件
    auto *dlg = new DeviceSelectorDialog(dm, maxIn, maxOut);

    // Dialog 偵測到縮放變化時通知 Window 重新計算大小
    dlg->onScaleChanged = [this]
    { updateWindowSize(); };

    // 設定視窗風格
    setContentOwned(dlg, true);   // 視窗擁有對話框生命週期
    setUsingNativeTitleBar(true); // 使用 Windows 原生標題欄
    setResizable(false, false);   // 禁用調整大小（由計時器控制）
    setBackgroundColour(Colour::fromRGB(236, 236, 236));

    // 初始化視窗大小
    updateWindowSize();
    setTopLeftPosition(250, 150); // 預設視窗左上角座標
}

/// 解構函數：清理資源
DeviceSelectorWindow::~DeviceSelectorWindow() { stopTimer(); }

/// 計時器回調：監控縮放因子變化並更新視窗大小
void DeviceSelectorWindow::timerCallback()
{
    float cur = getFontScaleFactor();
    if (std::abs(cur - lastAppliedScale) < 0.005f)
        return;
    lastAppliedScale = cur;
    updateWindowSize();
}

/// 關閉視窗並執行清理
void DeviceSelectorWindow::closeWindow()
{
    // 當用戶點擊窗口的X按鈕時，獲取當前選擇的設備並調用回調
    if (auto *dlg = dynamic_cast<DeviceSelectorDialog *>(getContentComponent()))
    {
        String name = dlg->getCurrentDeviceName();
        if (onConfirmCallback)
            onConfirmCallback(name);
    }

    removeFromDesktop();
    if (onWindowClosed)
        onWindowClosed();
    delete this; // 自我銷毀（由 addToDesktop 管理的視窗）
}

/// DocumentWindow 虛函數：當用戶點擊視窗關閉按鈕時調用
void DeviceSelectorWindow::closeButtonPressed()
{
    closeWindow();
}

void DeviceSelectorDialog::getPreferredSize(int &outWidth, int &outHeight) const
{
    float scale = getFontScaleFactor();
    outWidth = static_cast<int>(420 * scale);

    // 優先使用動態量測、否則用估算値
    int contentH = (computedContentHeight > 30)
                       ? computedContentHeight
                       : static_cast<int>(300 * scale);

    outHeight = contentH + getSystemFrameHeight();

    // 上限：不超過主螢幕可用區高度 − 50px
    auto &displays = Desktop::getInstance().getDisplays();
    if (auto *primary = displays.getPrimaryDisplay())
    {
        int screenH = primary->userArea.getHeight();
        outHeight = jmin(outHeight, screenH - 50);
    }
}

void DeviceSelectorWindow::updateWindowSize()
{
    if (auto *dlg = dynamic_cast<DeviceSelectorDialog *>(getContentComponent()))
    {
        int w = 0, h = 0;
        dlg->getPreferredSize(w, h);
        setSize(w, h);
    }
}
