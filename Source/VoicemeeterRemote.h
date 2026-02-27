/*
 * VoicemeeterRemote.h
 *
 * Light Host 的簡化 Voicemeeter Remote API 宣告
 *
 * 功能說明：
 * - 定義 Voicemeeter Remote API 的 C 語言介面
 * - 包含數據結構、回調函數、API 函數指針
 * - 用於從 C++ 代碼動態加載和調用 Voicemeeter DLL
 * - 支援音頻回調和參數控制
 *
 * 原始來源：
 * - Voicemeeter Remote API by Vincent Burel
 * - Copyright (c) 2015-2021
 * - Official: https://voicemeeter.com/
 *
 * 此頭文件是官方 API 的簡化版本，
 * 僅包含 Light Host 所需的必要結構和函數
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    // ==================== 音頻信息結構 ====================

    /**
     * VBVMR_T_AUDIOINFO 結構
     *
     * 包含音頻會話的基本信息
     * 在音頻回調 VBVMR_CBCOMMAND_STARTING 時傳遞
     *
     * 成員：
     * - samplerate：採樣率（Hz）
     *   常見值：44100, 48000, 96000, 192000
     * - nbSamplePerFrame：每個音頻幀的樣本數
     *   即一次回調時處理的樣本數（緩衝區大小）
     *   常見值：256, 512, 1024, 2048, 4096
     *
     * 用途：
     * - 在回調開始時告知應用程式音頻配置
     * - 應用程式可據此分配緩衝區等資源
     */
    typedef struct tagVBVMR_AUDIOINFO
    {
        long samplerate;       // 採樣率（Hz）
        long nbSamplePerFrame; // 每幀的樣本數
    } VBVMR_T_AUDIOINFO, *VBVMR_LPT_AUDIOINFO;

    /**
     * VBVMR_T_AUDIOBUFFER 結構
     *
     * 音頻回調中傳遞的實際音頻數據緩衝區
     *
     * 成員說明：
     * - audiobuffer_sr：採樣率（Hz）
     * - audiobuffer_nbs：每幀的樣本數
     * - audiobuffer_nbi：輸入數量（最多 128）
     * - audiobuffer_nbo：輸出數量（最多 128）
     * - audiobuffer_r[128]：輸入讀指針陣列
     *   指向每個輸入通道的音頻樣本數據
     *   audiobuffer_r[i] 指向第 i 個輸入的浮點樣本
     * - audiobuffer_w[128]：輸出寫指針陣列
     *   指向每個輸出通道的音頻樣本數據
     *   audiobuffer_w[i] 指向第 i 個輸出的浮點樣本
     *
     * 工作流程：
     * 1. Voicemeeter 設置 audiobuffer_r 指向輸入數據
     * 2. 應用程式從 audiobuffer_r 讀取輸入
     * 3. 應用程式處理音頻
     * 4. 應用程式寫入結果到 audiobuffer_w
     * 5. Voicemeeter 取得處理後的輸出
     *
     * 數據格式：
     * - 浮點樣本（float，32 位）
     * - 範圍通常為 [-1.0f, 1.0f]
     * - 超過此範圍表示削波（飽和）
     */
    typedef struct tagVBVMR_AUDIOBUFFER
    {
        long audiobuffer_sr;       // 採樣率
        long audiobuffer_nbs;      // each frame sample quantity
        long audiobuffer_nbi;      // number of inputs
        long audiobuffer_nbo;      // number of outputs
        float *audiobuffer_r[128]; // input read pointers - 輸入讀指針
        float *audiobuffer_w[128]; // output write pointers - 輸出寫指針
    } VBVMR_T_AUDIOBUFFER, *VBVMR_LPT_AUDIOBUFFER;

    // ==================== 回調命令定義 ====================
    // 音頻回調函數被調用時 nCommand 的值

    /**
     * VBVMR_CBCOMMAND_STARTING 命令
     *
     * 音頻會話啟動時的初始化回調
     * 值：1
     *
     * lpData 參數：指向 VBVMR_T_AUDIOINFO 結構，包含採樣率和緩衝區大小
     *
     * 應用程式應在此時：
     * - 讀取音頻配置信息
     * - 分配內部緩衝區
     * - 初始化 DSP 狀態
     * - 反場播放器等
     */
    inline constexpr long VBVMR_CBCOMMAND_STARTING = 1;

    /**
     * VBVMR_CBCOMMAND_ENDING 命令
     *
     * 音頻會話結束時的清理回調
     * 值：2
     *
     * lpData 參數：通常為 NULL
     *
     * 應用程式應在此時：
     * - 停止播放和處理
     * - 釋放分配的資源
     * - 清理狀態
     */
    inline constexpr long VBVMR_CBCOMMAND_ENDING = 2;

    /**
     * VBVMR_CBCOMMAND_CHANGE 命令
     *
     * 音頻配置改變時的回調
     * 值：3
     *
     * lpData 參數：可能指向新的 VBVMR_T_AUDIOINFO
     *
     * 發生情況：
     * - 採樣率改變
     * - 音頻通道數改變
     * - 其他配置改變需要重新啟動
     *
     * 應用程式應：
     * - 停止當前音頻處理
     * - 重新配置音頻參數
     * - 準備使用新的音頻配置
     */
    inline constexpr long VBVMR_CBCOMMAND_CHANGE = 3;

    /**
     * VBVMR_CBCOMMAND_BUFFER_IN 命令
     *
     * 輸入插入回調
     * 值：10
     *
     * lpData 參數：指向 VBVMR_T_AUDIOBUFFER 結構
     * nnn 參數：輸入索引（哪個輸入）
     *
     * 此模式允許在 Voicemeeter 輸入端插入效果
     */
    inline constexpr long VBVMR_CBCOMMAND_BUFFER_IN = 10;

    /**
     * VBVMR_CBCOMMAND_BUFFER_OUT 命令
     *
     * 輸出總線插入回調（Light Host 使用這個）
     * 值：11
     *
     * lpData 參數：指向 VBVMR_T_AUDIOBUFFER 結構
     * nnn 參數：總線索引（A1~A5 對應 0~4）
     *
     * Light Host 採用此模式：
     * - 在 Voicemeeter 輸出總線端插入效果
     * - 接收該總線的音頻
     * - 通過插件鏈處理
     * - 寫回處理後的音頻
     */
    inline constexpr long VBVMR_CBCOMMAND_BUFFER_OUT = 11;

    /**
     * VBVMR_CBCOMMAND_BUFFER_MAIN 命令
     *
     * 主輸入輸出回調
     * 值：20
     *
     * lpData 參數：指向 VBVMR_T_AUDIOBUFFER 結構
     * nnn 參數：未使用
     *
     * 此模式在所有 I/O 上同時操作，
     * 較少使用，較複雜
     */
    inline constexpr long VBVMR_CBCOMMAND_BUFFER_MAIN = 20;

    // ==================== 音頻回調註冊模式 ====================
    // 註冊回調時的 mode 參數值組合

    /**
     * VBVMR_AUDIOCALLBACK_IN 模式
     * 值：0x00000001
     *
     * 在輸入插入點註冊回調
     * 回調會接收 VBVMR_CBCOMMAND_BUFFER_IN 命令
     */
    inline constexpr long VBVMR_AUDIOCALLBACK_IN = 0x00000001;

    /**
     * VBVMR_AUDIOCALLBACK_OUT 模式
     * 值：0x00000002
     *
     * 在輸出總線插入點註冊回調
     * 回調會接收 VBVMR_CBCOMMAND_BUFFER_OUT 命令
     * Light Host 使用此模式
     */
    inline constexpr long VBVMR_AUDIOCALLBACK_OUT = 0x00000002;

    /**
     * VBVMR_AUDIOCALLBACK_MAIN 模式
     * 值：0x00000004
     *
     * 在主 I/O 點註冊回調
     * 回調會接收 VBVMR_CBCOMMAND_BUFFER_MAIN 命令
     */
    inline constexpr long VBVMR_AUDIOCALLBACK_MAIN = 0x00000004;

    // ==================== 回調函數類型 ====================

    /**
     * T_VBVMR_VBAUDIOCALLBACK 函數指針類型
     *
     * 音頻回調函數簽名
     * 當 Voicemeeter 有音頻要處理時，此函數被調用
     *
     * 調用約定：__stdcall（Windows 標準）
     * 返回值：long（通常返回 0）
     *
     * 參數：
     * - lpUser (void*)：用戶提供的上下文指針
     *   在註冊回調時傳遞，每次回調都會傳回
     *   可用於訪問應用程式狀態
     *
     * - nCommand (long)：回調命令
     *   VBVMR_CBCOMMAND_STARTING：初始化
     *   VBVMR_CBCOMMAND_ENDING：結束
     *   VBVMR_CBCOMMAND_CHANGE：配置改變
     *   VBVMR_CBCOMMAND_BUFFER_IN/OUT/MAIN：音頻數據
     *
     * - lpData (void*)：取決於命令
     *   指向 VBVMR_T_AUDIOINFO 或 VBVMR_T_AUDIOBUFFER 結構
     *
     * - nnn (long)：額外參數
     *   對於 BUFFER_IN/OUT：索引（輸入/總線號）
     *   其他命令：未使用（0）
     *
     * 使用範例：
     * long myAudioCallback(void* lpUser, long nCommand,
     *                      void* lpData, long busIndex)
     * {
     *     MyAudioEngine* engine = (MyAudioEngine*)lpUser;
     *     switch (nCommand) {
     *         case VBVMR_CBCOMMAND_STARTING:
     *             engine->initialize((VBVMR_LPT_AUDIOINFO)lpData);
     *             break;
     *         case VBVMR_CBCOMMAND_BUFFER_OUT:
     *             engine->process((VBVMR_LPT_AUDIOBUFFER)lpData, busIndex);
     *             break;
     *         case VBVMR_CBCOMMAND_ENDING:
     *             engine->shutdown();
     *             break;
     *     }
     *     return 0;
     * }
     */
    typedef long(__stdcall *T_VBVMR_VBAUDIOCALLBACK)(void *lpUser, long nCommand,
                                                     void *lpData, long nnn);

    // ==================== 動態 DLL 加載的函數指針類型 ====================
    //

    /**
     * T_VBVMR_Login 函數指針
     * 登入 Voicemeeter
     * 必須在使用其他 API 前調用
     * 返回值：0=成功，<0=錯誤
     */
    typedef long(__stdcall *T_VBVMR_Login)(void);

    /**
     * T_VBVMR_Logout 函數指針
     * 登出 Voicemeeter
     * 完成後應調用此函數清理
     * 返回值：0=成功，<0=錯誤
     */
    typedef long(__stdcall *T_VBVMR_Logout)(void);

    /**
     * T_VBVMR_GetVoicemeeterType 函數指針
     * 取得 Voicemeeter 版本類型
     * 返回值：0=成功，<0=錯誤
     * pType 輸出：1=Standard, 2=Banana, 3=Potato
     */
    typedef long(__stdcall *T_VBVMR_GetVoicemeeterType)(long *pType);

    /**
     * T_VBVMR_GetVoicemeeterVersion 函數指針
     * 取得 Voicemeeter 版本號
     * 返回值：0=成功，<0=錯誤
     * pVersion 輸出：版本號（例如 0x00020600 = v2.6）
     */
    typedef long(__stdcall *T_VBVMR_GetVoicemeeterVersion)(long *pVersion);

    /**
     * T_VBVMR_IsParametersDirty 函數指針
     * 檢查參數是否有改變
     * 用於異步參數詢問
     * 返回值：1=有改變，0=無改變
     */
    typedef long(__stdcall *T_VBVMR_IsParametersDirty)(void);

    /**
     * T_VBVMR_AudioCallbackRegister 函數指針
     * 註冊音頻回調函數
     *
     * 參數：
     * - mode：回調模式（IN/OUT/MAIN 之一或組合）
     * - pCallback：回調函數指針
     * - lpUser：用戶上下文指針（每次回調傳回）
     * - szClientName：應用程式名稱（64 字元限制）
     *
     * 返回值：0=成功，<0=錯誤
     */
    typedef long(__stdcall *T_VBVMR_AudioCallbackRegister)(long mode,
                                                           T_VBVMR_VBAUDIOCALLBACK pCallback,
                                                           void *lpUser,
                                                           char szClientName[64]);

    /**
     * T_VBVMR_AudioCallbackStart 函數指針
     * 啟動音頻回調
     * 必須在註冊後調用以開始接收音頻
     * 返回值：0=成功，<0=錯誤
     */
    typedef long(__stdcall *T_VBVMR_AudioCallbackStart)(void);

    /**
     * T_VBVMR_AudioCallbackStop 函數指針
     * 停止音頻回調
     * 暫時停止接收音頻但不註銷回調
     * 之後可再次調用 AudioCallbackStart
     * 返回值：0=成功，<0=錯誤
     */
    typedef long(__stdcall *T_VBVMR_AudioCallbackStop)(void);

    /**
     * T_VBVMR_AudioCallbackUnregister 函數指針
     * 註銷音頻回調
     * 停止接收音頻並清理回調
     * 返回值：0=成功，<0=錯誤
     */
    typedef long(__stdcall *T_VBVMR_AudioCallbackUnregister)(void);

    // ==================== API 函數結構 ====================

    /**
     * T_VBVMR_INTERFACE 結構
     *
     * 包含所有 Voicemeeter Remote API 函數指針
     * 通過動態加載 DLL 填充此結構
     * 之後可通過此結構調用所有 API 函數
     *
     * 使用範例：
     * T_VBVMR_INTERFACE vmr = ...;  // 從 DLL 加載填充
     * vmr.VBVMR_Login();            // 登入
     * vmr.VBVMR_AudioCallbackRegister(...);  // 註冊回調
     * vmr.VBVMR_AudioCallbackStart();  // 啟動回調
     */
    typedef struct tagVBVMR_INTERFACE
    {
        /**
         * 登入/登出函數
         */
        T_VBVMR_Login VBVMR_Login;
        T_VBVMR_Logout VBVMR_Logout;

        /**
         * 版本和狀態查詢函數
         */
        /**
         * 版本和狀態查詢函數
         */
        T_VBVMR_GetVoicemeeterType VBVMR_GetVoicemeeterType;
        T_VBVMR_GetVoicemeeterVersion VBVMR_GetVoicemeeterVersion;

        /**
         * 參數狀態查詢
         */
        T_VBVMR_IsParametersDirty VBVMR_IsParametersDirty;

        /**
         * 音頻回調管理函數
         */
        T_VBVMR_AudioCallbackRegister VBVMR_AudioCallbackRegister;
        T_VBVMR_AudioCallbackStart VBVMR_AudioCallbackStart;
        T_VBVMR_AudioCallbackStop VBVMR_AudioCallbackStop;
        T_VBVMR_AudioCallbackUnregister VBVMR_AudioCallbackUnregister;
    } T_VBVMR_INTERFACE; // ==================== API 函數結構結束 ====================

#ifdef __cplusplus
}
#endif
