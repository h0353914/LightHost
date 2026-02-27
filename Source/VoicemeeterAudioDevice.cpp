/*
 * VoicemeeterAudioDevice.cpp
 * Light Host - Voicemeeter 音頻設備整合實現
 *
 * 實現 JUCE AudioIODevice 和相關類別，以支援 Voicemeeter 虛擬混音器
 *
 * 核心概念：
 * - 通過 Voicemeeter Remote API 建立音頻接口
 * - 使用音頻回調機制交換音頻數據
 * - 實現「插入效果」模式（輸出總線）
 * - Windows 平台專用
 *
 * 實現細節：
 * - 動態加載 Voicemeeter DLL（32/64 位版本）
 * - 從 Windows Registry 檢測 Voicemeeter 安裝位置
 * - 非同步音頻處理通過 JUCE 回調機制
 * - 支援多個 Voicemeeter 總線（A1-A5）
 */

//==============================================================================
//  VoicemeeterAudioDevice.cpp
//  LightHost - Voicemeeter Audio Device Integration
//==============================================================================

#include "JuceHeader.h"
#include "VoicemeeterAudioDevice.h"

#if JUCE_WINDOWS // Windows 平台專用
#include <array>
#include <string_view>
#include <ranges>

#include <windows.h>
#include <cstring>

// ==================== 日誌系統 ====================

/**
 * vmLog() 函數
 * 輸出調試日誌到 Windows 調試視圖（DebugView）
 *
 * 用途：
 * - 記錄 Voicemeeter API 操作
 * - 追蹤音頻回調事件
 * - 偵錯連接和配置問題
 *
 * 日誌格式：時間 + 訊息
 * 例如："2024-01-15 14:32:45.123  VBVMR_Login() returned 0"
 *
 * 查看日誌：
 * - Windows 系統：使用 DebugView (Sysinternals)
 * - Visual Studio：Debug > Windows > Output
 *
 * @param msg 要輸出的訊息字符串
 */
static void vmLog(const juce::String &msg)
{
    // 建立帶時間戳的日誌行
    juce::String line = juce::Time::getCurrentTime().toString(true, true, true, true) + "  " + msg + "\n";
    // 發送到 Windows 調試系統
    OutputDebugStringW(line.toWideCharPointer());
}

// 方便的日誌宏
#define VMLOG(x) vmLog(x)

// ==================== Windows Registry 設定 ====================

/**
 * Voicemeeter Registry 金鑰位置
 *
 * 用於偵測 Voicemeeter 安裝
 * Microsoft 官方的卸載註冊表位置
 * 包含 DisplayName、InstallLocation 等信息
 */
constexpr std::wstring_view vmRegKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\VB:Voicemeeter {17359A74-1236-5467}";

/**
 * 64 位 Windows 上的 Wow6432Node Registry 路徑
 * 用於兼容 32 位應用程式在 64 位 Windows 上的運行
 */
constexpr std::wstring_view vmRegKeyWow = L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\VB:Voicemeeter {17359A74-1236-5467}";

// ==================== DLL 名稱（按位數） ====================

/**
 * Voicemeeter Remote DLL 名稱
 * 根據目標平台選擇合適的 DLL
 *
 * 編譯和加載選擇：
 * - 64 位編譯：加載 VoicemeeterRemote64.dll
 * - 32 位編譯：加載 VoicemeeterRemote.dll
 *
 * DLL 提供 Voicemeeter Remote API 函數實現
 */
#ifdef _WIN64
constexpr std::wstring_view vmDllName = L"VoicemeeterRemote64.dll"; // 64 位 DLL
#else
constexpr std::wstring_view vmDllName = L"VoicemeeterRemote.dll"; // 32 位 DLL
#endif

// ==================== 靜態音頻回調 ====================

/**
 * voicemeeterStaticCallback() 靜態回調函數
 *
 * Voicemeeter 音頻回調的靜態適配函數
 *
 * 原因：
 * - Voicemeeter API 要求回調為 C __stdcall 函數
 * - C++ 成員函數無法直接作為 C 回調
 * - 使用靜態函數作為轉接，轉發到成員函數
 *
 * 流程：
 * 1. Voicemeeter 調用此靜態函數
 * 2. lpUser 包含 VoicemeeterAudioIODevice 指針
 * 3. 轉發調用到設備的成員函數
 * 4. 返回 0 表示成功
 *
 * @param lpUser 用戶上下文指針（VoicemeeterAudioIODevice*）
 * @param nCommand 回調命令
 * @param lpData 回調數據
 * @param nnn 額外參數
 * @return 0 表示成功
 */
static long __stdcall voicemeeterStaticCallback(void *lpUser, long nCommand,
                                                void *lpData, long nnn)
{
    // 轉換 lpUser 為設備實例指針
    if (auto *device = static_cast<VoicemeeterAudioIODevice *>(lpUser))
    {
        // 轉發到設備成員函數
        device->handleVoicemeeterCallback(nCommand, lpData, nnn);
    }
    return 0; // 返回成功
}

// ==================== Registry 讀取輔助函數 ====================

/**
 * readRegistryString() 函數
 * 從 Windows Registry 讀取字符串值
 *
 * 用與讀取以下信息：
 * - Voicemeeter 安裝目錄
 * - 版本和其他元數據
 *
 * @param keyPath Registry 金鑰路徑（wchar_t*）
 * @param valueName 要讀取的值名稱
 * @return 讀取的字符串，若失敗返回空字符串
 */
static juce::String readRegistryString(std::wstring_view keyPath, std::wstring_view valueName)
{
    HKEY hkey = nullptr;
    std::array<wchar_t, 512> buffer{};
    DWORD size = sizeof(buffer);

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keyPath.data(), 0, KEY_READ, &hkey) == ERROR_SUCCESS)
    {
        RegQueryValueExW(hkey, valueName.data(), nullptr, nullptr, reinterpret_cast<LPBYTE>(buffer.data()), &size);
        RegCloseKey(hkey);
    }

    return juce::String(buffer.data());
}

//==============================================================================
// VoicemeeterAPI Singleton
//==============================================================================

VoicemeeterAPI &VoicemeeterAPI::getInstance()
{
    static VoicemeeterAPI instance;
    return instance;
}

VoicemeeterAPI::VoicemeeterAPI()
{
    memset(&vmr, 0, sizeof(vmr));

    // Find Voicemeeter install directory from registry
    juce::String uninstallString = readRegistryString(vmRegKey, L"UninstallString");
    VMLOG("Registry [HKLM\\...VB:Voicemeeter] UninstallString = " + uninstallString);

    if (uninstallString.isEmpty())
    {
        uninstallString = readRegistryString(vmRegKeyWow, L"UninstallString");
    }

    if (uninstallString.isEmpty())
    {
        VMLOG("ERROR: Voicemeeter not found in registry");
        return; // Voicemeeter not installed
    }

    // Extract directory from uninstall string path
    int lastSlash = uninstallString.lastIndexOfChar('\\');
    if (lastSlash < 0)
    {
        VMLOG("ERROR: Cannot extract directory from: " + uninstallString);
        return;
    }

    installDirectory = uninstallString.substring(0, lastSlash);
    VMLOG("installDirectory = " + installDirectory);

    // Load the DLL
    juce::String dllPath = installDirectory + "\\" + juce::String(vmDllName.data());
    VMLOG("Loading DLL: " + dllPath);
    dllModule = (void *)LoadLibraryW(dllPath.toWideCharPointer());

    if (dllModule == nullptr)
    {
        VMLOG("ERROR: LoadLibraryW failed, GetLastError=" + juce::String((int)GetLastError()));
        return;
    }
    VMLOG("DLL loaded OK");

    HMODULE hModule = (HMODULE)dllModule;

// Load function pointers
#define VM_LOAD(name) \
    vmr.name = (T_##name)GetProcAddress(hModule, #name)
    VM_LOAD(VBVMR_Login);
    VM_LOAD(VBVMR_Logout);
    VM_LOAD(VBVMR_GetVoicemeeterType);
    VM_LOAD(VBVMR_GetVoicemeeterVersion);
    VM_LOAD(VBVMR_IsParametersDirty);
    VM_LOAD(VBVMR_AudioCallbackRegister);
    VM_LOAD(VBVMR_AudioCallbackStart);
    VM_LOAD(VBVMR_AudioCallbackStop);
    VM_LOAD(VBVMR_AudioCallbackUnregister);
#undef VM_LOAD

    // Verify essential functions are loaded
    dllLoaded = (vmr.VBVMR_Login != nullptr && vmr.VBVMR_Logout != nullptr && vmr.VBVMR_GetVoicemeeterType != nullptr && vmr.VBVMR_AudioCallbackRegister != nullptr && vmr.VBVMR_AudioCallbackStart != nullptr && vmr.VBVMR_AudioCallbackStop != nullptr && vmr.VBVMR_AudioCallbackUnregister != nullptr);
    VMLOG(juce::String("dllLoaded = ") + (dllLoaded ? "true" : "false"));
}

VoicemeeterAPI::~VoicemeeterAPI()
{
    if (dllModule != nullptr)
        FreeLibrary((HMODULE)dllModule);
}

int VoicemeeterAPI::detectTypeFromRegistry() const
{
    // Detect Voicemeeter variant from the uninstaller executable name
    // (same approach as Equalizer APO's VoicemeeterAPOInfo)
    juce::String uninstallString = readRegistryString(vmRegKey, L"UninstallString");

    if (uninstallString.isEmpty())
        uninstallString = readRegistryString(vmRegKeyWow, L"UninstallString");

    if (uninstallString.isEmpty())
        return 0;

    juce::String lower = uninstallString.toLowerCase();

    if (lower.contains("voicemeeter8setup") || lower.contains("voicemeeterpotato"))
        return 3; // Potato (5 output buses A1-A5)
    else if (lower.contains("voicemeeterprosetup"))
        return 2; // Banana (3 output buses A1-A3)
    else
        return 1; // Standard (1 output bus A1)
}

//==============================================================================
// VoicemeeterAudioIODevice
//==============================================================================

VoicemeeterAudioIODevice::VoicemeeterAudioIODevice(const juce::String &outputBusName,
                                                   const juce::String &inBusName,
                                                   int inBusIdx,
                                                   int outBusIdx)
    : AudioIODevice(outputBusName, "Voicemeeter"),
      inputBusIndex(inBusIdx),
      outputBusIndex(outBusIdx),
      inputBusName(inBusName)
{
}

VoicemeeterAudioIODevice::~VoicemeeterAudioIODevice()
{
    aliveFlag->store(false);
    close();
}

juce::StringArray VoicemeeterAudioIODevice::getOutputChannelNames()
{
    juce::StringArray names;
    for (int i : std::views::iota(1, channelsPerBus + 1))
        names.add("Channel " + juce::String(i));
    return names;
}

juce::StringArray VoicemeeterAudioIODevice::getInputChannelNames()
{
    return getOutputChannelNames(); // Same channels for insert mode
}

std::optional<juce::BigInteger> VoicemeeterAudioIODevice::getDefaultOutputChannels() const
{
    // Default to stereo (channels 0 and 1)
    juce::BigInteger channels;
    channels.setBit(0);
    channels.setBit(1);
    return channels;
}

std::optional<juce::BigInteger> VoicemeeterAudioIODevice::getDefaultInputChannels() const
{
    return getDefaultOutputChannels();
}

juce::Array<double> VoicemeeterAudioIODevice::getAvailableSampleRates()
{
    return {22050.0, 24000.0, 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0};
}

juce::Array<int> VoicemeeterAudioIODevice::getAvailableBufferSizes()
{
    return {64, 128, 256, 480, 512, 1024, 2048};
}

int VoicemeeterAudioIODevice::getDefaultBufferSize()
{
    return 512;
}

juce::String VoicemeeterAudioIODevice::open(const juce::BigInteger &inputChannels,
                                            const juce::BigInteger &outputChannels,
                                            double sampleRate,
                                            int bufferSizeSamples)
{
    VMLOG("=== open() outBus=" + getName() + "(" + juce::String(outputBusIndex) + ")" + " inBus=" + inputBusName + "(" + juce::String(inputBusIndex) + ")" + " sr=" + juce::String(sampleRate) + " buf=" + juce::String(bufferSizeSamples));
    VMLOG("  inChans=" + inputChannels.toString(2) + " outChans=" + outputChannels.toString(2));
    close();

    auto &api = VoicemeeterAPI::getInstance();
    if (!api.isAvailable())
        return "Voicemeeter Remote API not available";

    auto &vmr = api.getInterface();

    // Login to Voicemeeter
    long loginResult = vmr.VBVMR_Login();
    VMLOG("VBVMR_Login() = " + juce::String(loginResult));
    if (loginResult < 0)
        return "Failed to login to Voicemeeter (error " + juce::String(loginResult) + "). Is Voicemeeter installed and running?";

    loggedIn = true;

    // Register audio callback — always use OUT mode which provides all bus
    // read channels (audiobuffer_r) and write channels (audiobuffer_w).
    // This lets us read from any input bus and write to any output bus.
    char clientName[64] = "LightHost";
    long mode = VBVMR_AUDIOCALLBACK_OUT;
    VMLOG("VBVMR_AudioCallbackRegister mode=" + juce::String(mode));
    long regResult = vmr.VBVMR_AudioCallbackRegister(mode,
                                                     voicemeeterStaticCallback,
                                                     this, clientName);
    VMLOG("VBVMR_AudioCallbackRegister() = " + juce::String(regResult) + " clientName=" + juce::String(clientName));

    if (regResult == 1)
    {
        // Another application has already registered - show the conflicting app name
        juce::String errorMsg = "Voicemeeter bus already in use by: ";
        errorMsg += juce::String(clientName);
        VMLOG("ERROR: " + errorMsg);
        close();
        return errorMsg;
    }
    else if (regResult != 0)
    {
        // Registration failed - Voicemeeter may not be running
        juce::String errorMsg = "Failed to register with Voicemeeter (error " + juce::String(regResult) + "). Please make sure Voicemeeter is running.";
        VMLOG("ERROR: " + errorMsg);
        close();
        return errorMsg;
    }

    callbackRegistered = true;

    // Store requested settings.
    // Limit active channels to channelsPerBus — the raw bitmask from JUCE may
    // have hundreds of bits set, which would confuse AudioProcessorPlayer.
    activeInputChannels = 0;
    activeOutputChannels = 0;
    for (int ch : std::views::iota(0, channelsPerBus))
    {
        if (inputChannels[ch])
            activeInputChannels.setBit(ch);
        if (outputChannels[ch])
            activeOutputChannels.setBit(ch);
    }
    currentSampleRate = sampleRate;
    currentBufferSize = bufferSizeSamples;
    VMLOG("open() activeChannels set to " + juce::String(channelsPerBus) + " channels");

    deviceOpen = true;
    lastError = {};
    VMLOG("open() SUCCESS");

    return {};
}

void VoicemeeterAudioIODevice::close()
{
    VMLOG("=== close() deviceOpen=" + juce::String((int)deviceOpen) + " devicePlaying=" + juce::String((int)devicePlaying) + " loggedIn=" + juce::String((int)loggedIn));
    stop();

    auto &api = VoicemeeterAPI::getInstance();
    if (api.isAvailable())
    {
        auto &vmr = api.getInterface();

        if (callbackRegistered)
        {
            VMLOG("  AudioCallbackUnregister...");
            vmr.VBVMR_AudioCallbackUnregister();
            callbackRegistered = false;
            VMLOG("  AudioCallbackUnregister done");
        }

        if (loggedIn)
        {
            VMLOG("  Logout...");
            vmr.VBVMR_Logout();
            loggedIn = false;
            VMLOG("  Logout done");
        }
    }

    deviceOpen = false;
    VMLOG("=== close() done");
}

bool VoicemeeterAudioIODevice::isOpen()
{
    return deviceOpen;
}

void VoicemeeterAudioIODevice::start(juce::AudioIODeviceCallback *callback)
{
    VMLOG("=== start() callback=" + juce::String(callback != nullptr ? "non-null" : "null") + " deviceOpen=" + juce::String((int)deviceOpen));
    if (callback != nullptr && deviceOpen)
    {
        // Store callback first so the audio thread can see it immediately.
        juceCallback.store(callback);

        // ALWAYS notify the callback with current settings so the plugin graph
        // is prepared before any audio buffers arrive.
        VMLOG("Calling audioDeviceAboutToStart sr=" + juce::String(currentSampleRate) + " buf=" + juce::String(currentBufferSize));
        callback->audioDeviceAboutToStart(this);
        VMLOG("audioDeviceAboutToStart() returned OK");

        auto &api = VoicemeeterAPI::getInstance();
        if (api.isAvailable())
        {
            long result = api.getInterface().VBVMR_AudioCallbackStart();
            VMLOG("VBVMR_AudioCallbackStart() = " + juce::String(result));
            if (result != 0)
            {
                lastError = "Failed to start Voicemeeter audio callback (error " + juce::String(result) + ")";
                VMLOG("ERROR: " + lastError);
            }
        }

        devicePlaying = true;
        VMLOG("start() complete");
    }
}

void VoicemeeterAudioIODevice::stop()
{
    VMLOG("=== stop() devicePlaying=" + juce::String((int)devicePlaying));
    if (devicePlaying)
    {
        auto &api = VoicemeeterAPI::getInstance();
        if (api.isAvailable())
            api.getInterface().VBVMR_AudioCallbackStop();

        auto *cb = juceCallback.exchange(nullptr);
        devicePlaying = false;

        if (cb != nullptr)
            cb->audioDeviceStopped();
    }
}

bool VoicemeeterAudioIODevice::isPlaying()
{
    return devicePlaying;
}

int VoicemeeterAudioIODevice::getCurrentBufferSizeSamples()
{
    return currentBufferSize;
}

double VoicemeeterAudioIODevice::getCurrentSampleRate()
{
    return currentSampleRate;
}

int VoicemeeterAudioIODevice::getCurrentBitDepth()
{
    return 32; // Voicemeeter uses 32-bit float
}

int VoicemeeterAudioIODevice::getOutputLatencyInSamples()
{
    return currentBufferSize;
}

int VoicemeeterAudioIODevice::getInputLatencyInSamples()
{
    return currentBufferSize;
}

juce::BigInteger VoicemeeterAudioIODevice::getActiveOutputChannels() const
{
    return activeOutputChannels;
}

juce::BigInteger VoicemeeterAudioIODevice::getActiveInputChannels() const
{
    return activeInputChannels;
}

juce::String VoicemeeterAudioIODevice::getLastError()
{
    return lastError;
}

void VoicemeeterAudioIODevice::handleVoicemeeterCallback(long nCommand, void *lpData, long /*nnn*/)
{
    switch (nCommand)
    {
    case VBVMR_CBCOMMAND_STARTING:
    {
        auto *info = (VBVMR_LPT_AUDIOINFO)lpData;
        currentSampleRate = (double)info->samplerate;
        currentBufferSize = (int)info->nbSamplePerFrame;
        VMLOG("STARTING: sr=" + juce::String(currentSampleRate) + " buf=" + juce::String(currentBufferSize));

        // audioDeviceAboutToStart MUST be called on the message thread
        // (it calls prepareToPlay which may allocate memory).
        // Post it asynchronously; BUFFER callbacks may arrive before it runs
        // but the player was already prepared in start(), so audio still flows.
        auto flag = aliveFlag;
        auto sr = currentSampleRate;
        auto bs = currentBufferSize;
        juce::MessageManager::callAsync([this, flag, sr, bs]()
                                        {
                if (! flag->load()) return;
                auto* cb = juceCallback.load();
                if (cb != nullptr)
                {
                    VMLOG ("STARTING async: audioDeviceAboutToStart sr=" + juce::String (sr)
                           + " buf=" + juce::String (bs));
                    cb->audioDeviceAboutToStart (this);
                    VMLOG ("STARTING async: done");
                } });
        break;
    }

    case VBVMR_CBCOMMAND_ENDING:
        VMLOG("ENDING: Voicemeeter audio stream ended");
        break;

    case VBVMR_CBCOMMAND_CHANGE:
    {
        // Audio stream parameters changed - restart the stream.
        // Post to the message thread so we don't block the audio thread.
        auto flag = aliveFlag;
        juce::MessageManager::callAsync([this, flag]()
                                        {
                if (flag->load() && devicePlaying)
                {
                    auto& api = VoicemeeterAPI::getInstance();
                    if (api.isAvailable())
                        api.getInterface().VBVMR_AudioCallbackStart();
                } });
        break;
    }

    case VBVMR_CBCOMMAND_BUFFER_IN:
    case VBVMR_CBCOMMAND_BUFFER_OUT:
    {
        auto *callback = juceCallback.load();
        if (callback == nullptr)
            break;

        auto *buffer = (VBVMR_LPT_AUDIOBUFFER)lpData;
        const int nbs = buffer->audiobuffer_nbs;
        const int nbi = buffer->audiobuffer_nbi;
        const int nbo = buffer->audiobuffer_nbo;
        const int inBase = inputBusIndex * channelsPerBus;
        const int outBase = outputBusIndex * channelsPerBus;

        // Log only on first few calls to avoid spam
        static std::atomic<int> bufferCallCount{0};
        if (bufferCallCount.fetch_add(1) < 3)
            VMLOG("BUFFER nCommand=" + juce::String(nCommand) + " nbs=" + juce::String(nbs) + " nbi=" + juce::String(nbi) + " nbo=" + juce::String(nbo) + " inBase=" + juce::String(inBase) + " outBase=" + juce::String(outBase));

        // Check our bus channels are within the buffer range
        const int availableIn = juce::jmin(channelsPerBus, juce::jmax(0, nbi - inBase));
        const int availableOut = juce::jmin(channelsPerBus, juce::jmax(0, nbo - outBase));

        if (availableIn <= 0 && availableOut <= 0)
        {
            VMLOG("WARNING: buses out of range inBase=" + juce::String(inBase) + " outBase=" + juce::String(outBase) + " nbi=" + juce::String(nbi) + " nbo=" + juce::String(nbo));
            break;
        }

        // Build active channel pointer arrays for JUCE callback.
        // Read from input bus, write to output bus (independent routing).
        const float *inputPtrs[channelsPerBus] = {};
        float *outputPtrs[channelsPerBus] = {};
        int numActiveIn = 0;
        int numActiveOut = 0;

        for (int ch : std::views::iota(0, channelsPerBus))
        {
            if (activeInputChannels[ch])
            {
                float *r = nullptr;
                if (ch < availableIn)
                    r = buffer->audiobuffer_r[inBase + ch];
                inputPtrs[numActiveIn++] = r;
            }
        }

        for (int ch : std::views::iota(0, channelsPerBus))
        {
            if (activeOutputChannels[ch])
            {
                float *w = nullptr;
                if (ch < availableOut)
                    w = buffer->audiobuffer_w[outBase + ch];
                outputPtrs[numActiveOut++] = w;
            }
        }

        if (bufferCallCount.load() <= 3)
            VMLOG("  numActiveIn=" + juce::String(numActiveIn) + " numActiveOut=" + juce::String(numActiveOut));

        if (numActiveIn > 0 || numActiveOut > 0)
        {
            juce::AudioIODeviceCallbackContext context;
            callback->audioDeviceIOCallbackWithContext(
                numActiveIn > 0 ? inputPtrs : nullptr, numActiveIn,
                numActiveOut > 0 ? outputPtrs : nullptr, numActiveOut,
                nbs, context);
        }
        break;
    }

    default:
        break;
    }
}

//==============================================================================
// VoicemeeterAudioIODeviceType
//==============================================================================

VoicemeeterAudioIODeviceType::VoicemeeterAudioIODeviceType()
    : AudioIODeviceType("Voicemeeter")
{
}

VoicemeeterAudioIODeviceType::~VoicemeeterAudioIODeviceType() = default;

void VoicemeeterAudioIODeviceType::scanForDevices()
{
    deviceNames.clear();
    deviceBusIndices.clear();
    deviceIsInput.clear();
    voicemeeterType = 0;

    VMLOG("=== scanForDevices ===");

    auto &api = VoicemeeterAPI::getInstance();
    if (!api.isAvailable())
    {
        VMLOG("API not available - skipping scan");
        return;
    }

    // Try to detect Voicemeeter type by logging in and querying the API.
    // Fall back to registry-based detection if Voicemeeter is not running.
    auto &vmr = api.getInterface();
    long loginRep = vmr.VBVMR_Login();
    VMLOG("VBVMR_Login() = " + juce::String(loginRep) + " (0=ok, 1=ok-not-launched, <0=error)");
    if (loginRep >= 0) // 0 = ok, 1 = ok but app not launched
    {
        long vmType = 0;
        if (vmr.VBVMR_IsParametersDirty != nullptr &&
            vmr.VBVMR_GetVoicemeeterType != nullptr)
        {
            long dirty = vmr.VBVMR_IsParametersDirty();
            VMLOG("VBVMR_IsParametersDirty() = " + juce::String(dirty) + " (>=0 means server alive)");
            // IsParametersDirty >= 0 means Voicemeeter server is alive
            if (dirty >= 0)
            {
                long typeRet = vmr.VBVMR_GetVoicemeeterType(&vmType);
                VMLOG("VBVMR_GetVoicemeeterType() ret=" + juce::String(typeRet) + " type=" + juce::String(vmType));
            }
        }
        vmr.VBVMR_Logout();

        if (vmType > 0)
            voicemeeterType = (int)vmType;
    }

    // If runtime detection failed, fall back to registry heuristic
    if (voicemeeterType == 0)
    {
        voicemeeterType = api.detectTypeFromRegistry();
        VMLOG("Registry fallback type = " + juce::String(voicemeeterType));
    }

    // Last resort: assume Standard
    if (voicemeeterType == 0)
        voicemeeterType = 1;

    VMLOG("Final voicemeeterType = " + juce::String(voicemeeterType) + " (1=Standard, 2=Banana, 3=Potato)");

    int numHwOut = 2, numVirtOut = 1;
    switch (voicemeeterType)
    {
    case 1:
        numHwOut = 2;
        numVirtOut = 1;
        break; // Standard:  A1+A2, B1
    case 2:
        numHwOut = 3;
        numVirtOut = 2;
        break; // Banana:    A1-A3, B1-B2
    case 3:
        numHwOut = 5;
        numVirtOut = 3;
        break; // Potato:    A1-A5, B1-B3
    default:
        break;
    }

    // Add Output (hardware) buses
    for (int i = 0; i < numHwOut; ++i)
    {
        deviceNames.add("Output A" + juce::String(i + 1));
        deviceBusIndices.add(i);
        deviceIsInput.add(false);
    }
    // Add Output (virtual) buses
    for (int i = 0; i < numVirtOut; ++i)
    {
        deviceNames.add("Output B" + juce::String(i + 1));
        deviceBusIndices.add(numHwOut + i);
        deviceIsInput.add(false);
    }

    VMLOG("Device list: " + deviceNames.joinIntoString(", "));
}

juce::StringArray VoicemeeterAudioIODeviceType::getDeviceNames(bool /*wantInputNames*/) const
{
    return deviceNames;
}

int VoicemeeterAudioIODeviceType::getDefaultDeviceIndex(bool /*forInput*/) const
{
    return deviceNames.isEmpty() ? -1 : 0;
}

int VoicemeeterAudioIODeviceType::getIndexOfDevice(juce::AudioIODevice *device, bool asInput) const
{
    if (device == nullptr)
        return -1;
    auto *vmDev = dynamic_cast<VoicemeeterAudioIODevice *>(device);
    if (vmDev == nullptr)
        return -1;
    // For input dropdown match by input bus name, for output match by output bus name
    juce::String name = asInput ? vmDev->getInputBusName() : vmDev->getOutputBusName();
    return deviceNames.indexOf(name);
}

bool VoicemeeterAudioIODeviceType::hasSeparateInputsAndOutputs() const
{
    return true; // Show separate input and output bus dropdowns
}

juce::AudioIODevice *VoicemeeterAudioIODeviceType::createDevice(const juce::String &outputDeviceName,
                                                                const juce::String &inputDeviceName)
{
    // Resolve output bus index
    int outIndex = deviceNames.indexOf(outputDeviceName);
    if (outIndex < 0 && outputDeviceName.isNotEmpty())
        return nullptr;

    // Resolve input bus index. If not specified, default to same as output (insert mode).
    int inIndex = inputDeviceName.isNotEmpty() ? deviceNames.indexOf(inputDeviceName) : outIndex;
    if (inIndex < 0)
        inIndex = outIndex;

    // Use output bus name as device name (shown in audio settings)
    juce::String devName = outIndex >= 0 ? deviceNames[outIndex] : deviceNames[inIndex];
    juce::String inName = inIndex >= 0 ? deviceNames[inIndex] : devName;
    int inBus = inIndex >= 0 ? deviceBusIndices[inIndex] : 0;
    int outBus = outIndex >= 0 ? deviceBusIndices[outIndex] : inBus;

    VMLOG("createDevice outBus=" + devName + "(" + juce::String(outBus) + ")" + " inBus=" + inName + "(" + juce::String(inBus) + ")");

    return new VoicemeeterAudioIODevice(devName, inName, inBus, outBus);
}

#endif // JUCE_WINDOWS
