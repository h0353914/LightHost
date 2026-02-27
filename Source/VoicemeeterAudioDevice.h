/*
 * VoicemeeterAudioDevice.h
 * LightHost - Voicemeeter 音頻設備整合模組
 *
 * 功能說明：
 * - 實現 JUCE AudioIODevice 和 AudioIODeviceType 介面
 * - 與 Voicemeeter 虛擬音頻混音器整合
 * - 使用 Voicemeeter Remote API 進行音頻處理
 * - 通過輸出總線插入點實現音頻效果處理
 * - 支援多個 Voicemeeterr 總線（A1-A5）
 *
 * Voicemeeter 背景：
 * Voicemeeter 是功能強大的虛擬音頻混音機，允許：
 * - 虛擬音頻設備的創建和管理
 * - 多個音頻源和目標的靈活混音
 * - 應用程式之間的高級音頻路由
 * - 通過 Remote API 實現程式控制和效果處理
 *
 * Light Host 集成原理：
 * 1. 將 Light Host 註冊為 Voicemeeter 的音頻回調
 * 2. 接收來自 Voicemeeter 總線的音頻輸入
 * 3. 通過插件鏈處理音頻
 * 4. 將處理結果寫回 Voicemeeter 總線（插入效果模式）
 * 5. 實現實時音頻效果的插入監聽
 *
 * 使用方式：
 * - Windows 平台專用
 * - 需要安裝 Voicemeeter
 * - 自動檢測 registry 查找 Voicemeeter 安裝位置
 */

#pragma once

#include "JuceHeader.h"

#if JUCE_WINDOWS // Windows 平台專用

#include "VoicemeeterRemote.h"
#include <atomic>
#include <memory>

// ==================== VoicemeeterAPI 單例類別 ====================

/**
 * VoicemeeterAPI 類別
 *
 * Voicemeeter Remote DLL 加載管理單例
 *
 * 職責：
 * 1. 檢測 Windows registry 中的 Voicemeeter 安裝
 * 2. 加載適當的 DLL（32 位或 64 位）
 * 3. 提供 Voicemeeter API 介面
 * 4. 管理 DLL 的生命週期
 *
 * Voicemeeter Remote API：
 * - 由 Voicemeeter 官方提供
 * - 允許外部應用程式控制音頻參數
 * - 支援音頻回調以實現插入效果
 * - 需要 Voicemeeter 執行且登入
 *
 * DLL 位置：
 * - Voicemeeter 安裝目錄（從 registry 檢測）
 * - 64 位系統：VB-AUdio Virtual Cable 目錄
 * - 32 位系統：相應的 Windows 檔案位置
 */
class VoicemeeterAPI
{
public:
    /**
     * getInstance() 靜態方法
     * 取得 VoicemeeterAPI 單例實例
     *
     * @return VoicemeeterAPI 實例的靜態引用
     */
    static VoicemeeterAPI &getInstance();

    /**
     * isAvailable() 方法
     * 檢查 Voicemeeter Remote API 是否可用
     *
     * 檢查內容：
     * - DLL 是否成功加載
     * - 所有必要的函數指針是否有效
     * - 系統是否安裝了 Voicemeeter
     *
     * @return true 如果 API 可用；否則 false
     */
    [[nodiscard]] bool isAvailable() const noexcept { return dllLoaded; }

    /**
     * getInterface() 方法
     * 取得 Voicemeeter API 函數介面
     *
     * 返回的介面包含所有必要的函數指針：
     * - VBVMR_Login / VBVMR_Logout
     * - AudioCallbackRegister / AudioCallbackStart / AudioCallbackStop
     * - GetVoicemeeterType / GetVoicemeeterVersion
     * - 等等
     *
     * @return T_VBVMR_INTERFACE 引用（包含所有 API 函數指針）
     */
    [[nodiscard]] T_VBVMR_INTERFACE &getInterface() noexcept { return vmr; }

    /**
     * detectTypeFromRegistry() 方法
     * 從 Windows Registry 檢測 Voicemeeter 類型
     * 無需登入或啟動 Voicemeeter 即可調用
     *
     * 返回值：
     * - 1 = Voicemeeter Standard（標準版）
     *   基礎功能，適合簡單混音
     * - 2 = Voicemeeter Banana（香蕉版）
     *   增強功能，更多輸入輸出
     * - 3 = Voicemeeter Potato（馬鈴薯版）
     *   最高版本，最多功能和靈活性
     * - 0 = 未知或未安裝
     *
     * 用途：
     * - 在 Voicemeeter 啟動前檢測版本
     * - 決定可用的音頻總線數量
     * - 顯示相應的 UI 選項
     *
     * @return Voicemeeter 類型（1~3）或 0 如不可用
     */
    [[nodiscard]] int detectTypeFromRegistry() const;

private:
    /**
     * 私有建構子
     * 初始化時：
     * - 檢測 Voicemeeter 安裝位置
     * - 加載適當位數的 DLL
     * - 設置所有 API 函數指針
     * - 設置 dllLoaded 標誌
     */
    VoicemeeterAPI();

    /**
     * 私有解構子
     * 清理：
     * - 卸載 DLL 模塊
     * - 釋放相關資源
     */
    ~VoicemeeterAPI();

    // ==================== 私有成員 ====================

    /**
     * DLL 模塊句柄
     * 實際類型是 HMODULE，使用 void* 以避免包含 windows.h
     * 由 LoadLibrary() 返回
     * 用於稍後卸載 DLL（FreeLibrary）
     */
    void *dllModule = nullptr;

    /**
     * Voicemeeter API 函數介面
     * 包含所有從 DLL 加載的函數指針
     * 詳見 VoicemeeterRemote.h T_VBVMR_INTERFACE 定義
     */
    T_VBVMR_INTERFACE vmr{};

    /**
     * DLL 加載成功標誌
     * true = DLL 已成功加載且所有函數指針有效
     * false = 加載失敗或 Voicemeeter 不可用
     */
    bool dllLoaded = false;

    /**
     * Voicemeeter 安裝目錄路徑
     * 從 Windows Registry 檢測
     * 用於定位和加載 Voicemeeter DLL
     */
    juce::String installDirectory;
};

// ==================== VoicemeeterAudioIODevice 類別 ====================

/**
 * VoicemeeterAudioIODevice 類別
 *
 * JUCE AudioIODevice 的 Voicemeeter 實現
 *
 * 功能：
 * - 將 JUCE 音頻系統與 Voicemeeter 虛擬音頻混音器整合
 * - 每個實例對應 Voicemeeter 的一個輸出總線（A1, A2, ..., A5）
 * - 實現「插入效果」模式：接收 → 處理 → 回寫
 *
 * 工作流程：
 * 1. 註冊音頻回調函數到 Voicemeeter
 * 2. 在 open() 時啟動中介 DLL 的音頻回調
 * 3. Voicemeeter 定期調用回調，提供音頻資料
 * 4. Light Host 處理音頻（通過插件鏈）
 * 5. 將結果寫回緩衝區
 * 6. Voicemeeter 取得處理後的音頻並路由到輸出
 *
 * 特點：
 * - 支援多個 Voicemeeter 總線（不同實例使用不同總線索引）
 * - 提供 JUCE AudioIODevice 標準介面
 * - 支援不同的採樣率和緩衝區大小
 * - 非同步音頻回調處理
 */
class VoicemeeterAudioIODevice : public juce::AudioIODevice
{
public:
    /**
     * VoicemeeterAudioIODevice 建構子
     *
     * 初始化 Voicemeeter 音頻設備
     * 立即檢測和保存 Voicemeeter 配置
     * 但不啟動音頻處理（在 start() 時啟動）
     *
     * @param outputBusName Voicemeeter 輸出總線的顯示名稱
     *                      例如："A1", "A2", "Voicemeeter AUX"
     * @param inputBusName Voicemeeter 輸入總線的顯示名稱
     *                     例如："VAIO3"
     * @param inputBusIndex 輸入總線索引（對應 Voicemeeter 物理輸入）
     *                      用於 Voicemeeter API 調用
     * @param outputBusIndex 輸出總線索引（A1~A5 對應 0~4）
     *                       用於音頻回調的主線索引
     */
    VoicemeeterAudioIODevice(const juce::String &outputBusName,
                             const juce::String &inputBusName,
                             int inputBusIndex,
                             int outputBusIndex);

    /**
     * ~VoicemeeterAudioIODevice 解構子
     *
     * 清理資源：
     * - 停止音頻回調（如果運行）
     * - 註銷 Voicemeeter 回調
     * - 登出 Voicemeeter
     * - 释放本地資源
     */
    ~VoicemeeterAudioIODevice() override;

    // ==================== JUCE AudioIODevice 介面實現 ====================
    // 這些方法由 JUCE 框架調用以查詢和控制音頻設備

    /**
     * getOutputChannelNames() 方法
     * 返回此設備可用的輸出音頻通道
     *
     * 對於 Voicemeeter：
     * - 通常為立體聲（2 通道）
     * - 返回 ["Left", "Right"]
     *
     * @return 通道名稱的字符串陣列
     */
    [[nodiscard]] juce::StringArray getOutputChannelNames() override;

    /**
     * getInputChannelNames() 方法
     * 返回此設備可用的輸入音頻通道
     *
     * 對於 Voicemeeter：
     * - 通常為立體聲（2 通道）
     * - 返回 ["Left", "Right"]
     *
     * @return 通道名稱的字符串陣列
     */
    [[nodiscard]] juce::StringArray getInputChannelNames() override;

    /**
     * getDefaultOutputChannels() 方法
     * 返回建議使用的預設輸出通道掩碼
     *
     * @return BigInteger 位掩碼（位 0-1 代表通道 0-1）
     */
    [[nodiscard]] std::optional<juce::BigInteger> getDefaultOutputChannels() const override;

    /**
     * getDefaultInputChannels() 方法
     * 返回建議使用的預設輸入通道掩碼
     *
     * @return BigInteger 位掩碼（位 0-1 代表通道 0-1）
     */
    [[nodiscard]] std::optional<juce::BigInteger> getDefaultInputChannels() const override;

    /**
     * getAvailableSampleRates() 方法
     * 返回設備支援的採樣率列表
     *
     * Voicemeeter 支援的常見採樣率：
     * - 44100 Hz（CD 品質）
     * - 48000 Hz（專業音頻標準）
     * - 96000 Hz（高解析）
     *
     * @return 支援的採樣率陣列（Hz）
     */
    [[nodiscard]] juce::Array<double> getAvailableSampleRates() override;

    /**
     * getAvailableBufferSizes() 方法
     * 返回設備支援的音頻緩衝區大小列表
     *
     * 常見大小（樣本數量）：
     * - 256, 512, 1024, 2048, 4096
     *
     * 緩衝區大小與延遲的關係：
     * - 較小的緩衝區 = 較低的延遲 = 更高的 CPU 使用
     * - 較大的緩衝區 = 較高的延遲 = 更低的 CPU 使用
     *
     * @return 支援的緩衝區大小陣列（樣本數）
     */
    [[nodiscard]] juce::Array<int> getAvailableBufferSizes() override;

    /**
     * getDefaultBufferSize() 方法
     * 返回建議的預設緩衝區大小
     *
     * 預設通常是一個平衡選擇，例如 512 或 1024 樣本
     *
     * @return 預設緩衝區大小（樣本數）
     */
    [[nodiscard]] int getDefaultBufferSize() override;

    /**
     * open() 方法
     * 開啟音頻設備
     *
     * 步驟：
     * 1. 驗證選定的輸入/輸出通道
     * 2. 檢查採樣率和緩衝區大小是否支援
     * 3. 登入 Voicemeeter
     * 4. 註冊音頻回調到 Voicemeeter
     * 5. 配置音頻參數
     * 6. 標記為已開啟（但尚未處理音頻）
     *
     * 此時不會啟動音頻回調，需要調用 start()
     *
     * @param inputChannels 要使用的輸入通道位掩碼
     * @param outputChannels 要使用的輸出通道位掩碼
     * @param sampleRate 採樣率（Hz）
     * @param bufferSizeSamples 緩衝區大小（樣本數）
     * @return 空字符串如成功；錯誤訊息如失敗
     */
    [[nodiscard]] juce::String open(const juce::BigInteger &inputChannels,
                                    const juce::BigInteger &outputChannels,
                                    double sampleRate,
                                    int bufferSizeSamples) override;

    /**
     * close() 方法
     * 關閉音頻設備
     *
     * 步驟：
     * 1. 停止音頻回調（如果運行）
     * 2. 註銷 Voicemeeter 回調
     * 3. 登出 Voicemeeter
     * 4. 清理緩衝區
     * 5. 標記為已關閉
     */
    void close() override;

    /**
     * isOpen() 方法
     * 檢查設備是否已開啟
     *
     * @return true 如果設備已開啟；否則 false
     */
    [[nodiscard]] bool isOpen() override;

    /**
     * start() 方法
     * 開始音頻處理
     *
     * 前置條件：設備必須已通過 open() 打開
     *
     * 步驟：
     * 1. 儲存回調物件指針
     * 2. 在 Voicemeeter 中啟動音頻回調
     * 3. Voicemeeter 開始定期調用回調函數
     * 4. 每次回調時，Light Host 處理音頻
     *
     * @param callback AudioIODeviceCallback 實例
     *                 其 audioDeviceIOCall() 方法會定期被調用
     */
    void start(juce::AudioIODeviceCallback *callback) override;

    /**
     * stop() 方法
     * 停止音頻處理
     *
     * 步驟：
     * 1. 在 Voicemeeter 中停止音頻回調
     * 2. 清空回調指針
     * 3. 音頻處理停止，不再調用 audioDeviceIOCall()
     */
    void stop() override;

    /**
     * isPlaying() 方法
     * 檢查音頻設備是否正在進行音頻處理
     *
     * @return true 如果已調用 start() 且未調用 stop()；否則 false
     */
    [[nodiscard]] bool isPlaying() override;

    [[nodiscard]] int getCurrentBufferSizeSamples() override;
    [[nodiscard]] double getCurrentSampleRate() override;
    [[nodiscard]] int getCurrentBitDepth() override;
    [[nodiscard]] int getOutputLatencyInSamples() override;
    [[nodiscard]] int getInputLatencyInSamples() override;
    [[nodiscard]] juce::BigInteger getActiveOutputChannels() const override;
    [[nodiscard]] juce::BigInteger getActiveInputChannels() const override;
    [[nodiscard]] juce::String getLastError() override;

    /** Returns the selected input/output bus names for index lookup. */
    [[nodiscard]] const juce::String &getInputBusName() const noexcept { return inputBusName; }
    [[nodiscard]] const juce::String &getOutputBusName() const { return getName(); }

    /** Called from the static Voicemeeter callback on the audio thread. */
    void handleVoicemeeterCallback(long nCommand, void *lpData, long nnn);

private:
    static constexpr int channelsPerBus = 8;
    int inputBusIndex = 0;
    int outputBusIndex = 0;
    juce::String inputBusName;

    bool deviceOpen = false;
    bool devicePlaying = false;
    bool loggedIn = false;
    bool callbackRegistered = false;

    std::atomic<juce::AudioIODeviceCallback *> juceCallback{nullptr};

    juce::BigInteger activeInputChannels;
    juce::BigInteger activeOutputChannels;

    double currentSampleRate = 48000.0;
    int currentBufferSize = 512;
    juce::String lastError;

    // Flag for safe async restart on CHANGE command
    std::shared_ptr<std::atomic<bool>> aliveFlag = std::make_shared<std::atomic<bool>>(true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoicemeeterAudioIODevice)
};

//==============================================================================
/**
    AudioIODeviceType for Voicemeeter integration.
    Lists available Voicemeeter output buses as selectable audio devices.
    Automatically detects the installed Voicemeeter variant
    (Standard, Banana, or Potato) and exposes the corresponding buses.
*/
class VoicemeeterAudioIODeviceType : public juce::AudioIODeviceType
{
public:
    VoicemeeterAudioIODeviceType();
    ~VoicemeeterAudioIODeviceType() override;

    void scanForDevices() override;
    [[nodiscard]] juce::StringArray getDeviceNames(bool wantInputNames = false) const override;
    [[nodiscard]] int getDefaultDeviceIndex(bool forInput) const override;
    [[nodiscard]] int getIndexOfDevice(juce::AudioIODevice *device, bool asInput) const override;
    [[nodiscard]] bool hasSeparateInputsAndOutputs() const override;
    [[nodiscard]] juce::AudioIODevice *createDevice(const juce::String &outputDeviceName,
                                                    const juce::String &inputDeviceName) override;

private:
    juce::StringArray deviceNames;
    juce::Array<int> deviceBusIndices;
    juce::Array<bool> deviceIsInput;
    int voicemeeterType = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VoicemeeterAudioIODeviceType)
};

#endif // JUCE_WINDOWS
