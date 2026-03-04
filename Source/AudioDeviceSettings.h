#pragma once

#include "JuceHeader.h"

ApplicationProperties &getAppProperties();

//==============================================================================
// 顯示縮放設定管理器（ScaleSettingsManager）
// 說明：
//   • 管理應用程式的 DPI 縮放因子（0.5 ~ 3.0）
//   • 使用單例模式確保全域只有一個實例
//   • 自動載入/保存設定到應用屬性中的 "uiScaleFactor" 鍵
//   • 縮放因子影響 UI 元素大小、按鈕尺寸和字體高度
//==============================================================================
class ScaleSettingsManager
{
public:
    /// 取得全域單例實例
    static ScaleSettingsManager &getInstance();

    /// 取得當前縮放因子（1.0 = 100%）
    float getScaleFactor() const;

    /// 設定新的縮放因子並自動保存
    void setScaleFactor(float scale);

    /// 從應用屬性中載入已保存的縮放設定
    void loadSettings();

    /// 將縮放設定保存到應用屬性
    void saveSettings();

private:
    /// 當前的縮放因子，預設值為 1.0（100%）
    float scaleFactor = 1.0f;
};

//==============================================================================
// 音訊裝置選擇對話內容組件（DeviceSelectorDialog）
// 說明：
//   • 包含 JUCE 的 AudioDeviceSelectorComponent 並應用動態縮放
//   • 根據 DPI 和語言設定自動縮放列表項高度和字體大小
//   • 100% 基線 = 原始 JUCE AudioDeviceSelectorComponent（無人工置中）
//   • 作為 DeviceSelectorWindow 的內容組件運作
//   • 視窗關閉按鈕由父視窗管理
//==============================================================================
class DeviceSelectorDialog : public Component, private Timer
{
public:
    /// 當縮放因子改變時，通知父視窗重新計算大小
    std::function<void()> onScaleChanged;

    /// 建構函式
    /// @param dm     音訊裝置管理器
    /// @param maxIn  最大輸入聲道數（0 表示隱藏輸入選項）
    /// @param maxOut 最大輸出聲道數（0 表示隱藏輸出選項）
    DeviceSelectorDialog(AudioDeviceManager &dm, int maxIn, int maxOut);

    /// 解構函式
    ~DeviceSelectorDialog() override;

    /// 計時器回調：偵測縮放變化
    void timerCallback() override;

    /// 重新建立 AudioDeviceSelectorComponent 實例並應用縮放
    void updateSelectorComponent();

    /// 調整子組件大小和位置
    void resized() override;

    /// 取得當前選擇的音訊設備名稱
    String getCurrentDeviceName() const;

    /// 取得此對話框的偏好大小（用於窗口尺寸計算）
    /// 會在首次 layout 完成後包含動態量測的實際高度
    void getPreferredSize(int &outWidth, int &outHeight) const;

private:
    /// 遞迴收集所有 Label 組件的原始字體高度
    void collectNaturalFonts(Component *comp);

    /// 遞迴應用縮放因子到所有 Label 組件的字體
    void applyTotalScale(Component *comp, float totalScale);

    /// JUCE 音訊裝置選擇器組件
    std::unique_ptr<AudioDeviceSelectorComponent> sel;

    /// 音訊裝置管理器參考
    AudioDeviceManager &mgr;

    /// 初始時的最大輸入/輸出聲道數
    int initialMaxIn, initialMaxOut;

    /// 儲存每個 Label 組件的原始字體高度（用於動態縮放）
    std::map<Component *, float> naturalFontHeight;

    /// 選擇器的原始列表項高度
    int naturalItemHeight = 0;

    /// 是否已準備好應用字體縮放
    bool baselinesReady = false;

    /// 上次應用的縮放因子
    float lastScale = -1.0f;

    /// 動態量測的實際內容高度（px），0 表示尚未量測
    int computedContentHeight = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeviceSelectorDialog)
};

//==============================================================================
// 音訊裝置選擇視窗（DeviceSelectorWindow）
// 說明：
//   • 文檔視窗，包含 DeviceSelectorDialog 組件
//   • 提供原生標題欄、關閉按鈕和窗口控制
//   • 100% 縮放時的大小約為 500x400
//   • 其他縮放時：視窗大小 = 基礎大小 × DPI × 語言縮放因子
//   • 監控縮放因子變化並自動調整視窗大小
//==============================================================================
class DeviceSelectorWindow : public DocumentWindow, private Timer
{
public:
    /// 視窗被關閉時調用的回調函式
    std::function<void()> onWindowClosed;

    /// 建構函式，建立並初始化選擇器視窗
    /// @param title      視窗標題（例如 "音訊輸入" 或 "音訊輸出"）
    /// @param dm         音訊裝置管理器
    /// @param maxIn      最大輸入聲道數
    /// @param maxOut     最大輸出聲道數
    /// @param cb         使用者選擇裝置後調用的回調函式
    DeviceSelectorWindow(const String &title, AudioDeviceManager &dm,
                         int maxIn, int maxOut,
                         std::function<void(const String &)> cb);

    /// 解構函式
    ~DeviceSelectorWindow() override;

    /// 計時器回調函式，監控縮放因子變化
    void timerCallback() override;

    /// 關閉視窗並執行清理操作
    void closeWindow();

    /// DocumentWindow 虛函數：當用戶點擊視窗關閉按鈕時調用
    void closeButtonPressed() override;

    /// 根據當前縮放因子更新視窗大小
    void updateWindowSize();

private:
    /// 上次應用的縮放因子，用於偵測變化
    float lastAppliedScale = -1.0f;

    /// 使用者選擇裝置時調用的回調函式
    std::function<void(const String &)> onConfirmCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DeviceSelectorWindow)
};