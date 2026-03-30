# Claude 使用量監控器 — 開發工作紀錄

## 專案概述

開發一個 Windows 原生桌面應用程式，以常駐懸浮列的形式即時顯示 claude.ai 的 API 使用量。使用者透過貼上瀏覽器 Session Cookie 進行驗證，程式定期從 claude.ai 的內部 API 取得使用資料並顯示於畫面上。

**技術選型：** C++ / Qt 6 / Windows DPAPI / QNetworkAccessManager
**建置環境：** MSYS2 UCRT64 / CMake / Ninja
**目標平台：** Windows 11 x64

---

## 專案架構

```
Claude_calculator/
├── src/
│   ├── main.cpp
│   ├── UsageData.h              # 資料結構定義
│   ├── ClaudeApiClient.h/.cpp   # HTTP 客戶端，負責 API 呼叫
│   ├── SecureStorage.h/.cpp     # Windows DPAPI 憑證加密儲存
│   ├── SettingsDialog.h/.cpp    # 設定介面
│   ├── OverlayWindow.h/.cpp     # 主懸浮視窗
│   ├── SystemTrayIcon.h/.cpp    # 系統匣圖示
│   └── resources/
│       ├── sessionkey.png       # 說明圖片（工具提示用）
│       ├── icons/
│       │   ├── app.ico          # 程式圖示（Windows 資源用，16/32/48/256px）
│       │   └── app_icon.png     # 程式圖示（關於對話框用，PNG 格式）
│       ├── app.rc               # Windows 資源描述檔（嵌入圖示）
│       └── resources.qrc        # Qt 資源描述檔
└── CMakeLists.txt
```

---

## 功能開發過程

### 一、API 串接

**端點發現：**
透過瀏覽器 DevTools 分析 claude.ai 的 Network 請求，確認以下端點：

- `GET /api/organizations` — 取得組織 UUID（回傳陣列格式 `[{"uuid":"..."}]`）
- `GET /api/organizations/{uuid}/usage` — 取得使用量資料

**驗證方式（兩種）：**
- **Session Cookie**：使用者從瀏覽器複製 `sessionKey` cookie 值，附加於 `Cookie` 標頭
- **Admin API 金鑰**：以 `x-api-key` 標頭呼叫 Anthropic 管理端點（`sk-ant-admin-…`）

**UUID 自動取得：**
加入重試機制：失敗後等待 5 秒再試一次，兩次均失敗才回報錯誤，並提示使用者手動輸入。

**資料結構（UsageData.h）：**
- `UsageWindow` — 含 utilization（0~100）與 resetsAt（重置時間）
- 支援 5 小時循環窗口與 7 天循環窗口
- `ExtraUsage` — 超額用量（月費制額度）

### 二、主懸浮視窗（OverlayWindow）

**視窗特性：**
- `Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool`
- `WA_TranslucentBackground`，在 `showEvent` 中呼叫 `SetWindowPos(HWND_TOPMOST)` 確保永遠置頂（修正 Qt::Tool 失去焦點後下沉的問題）

**拖曳移動：**
使用 Qt 6 的 `globalPosition().toPoint()`（Qt 5 的 `globalPos()` 已移除），於 `mouseReleaseEvent` 儲存位置至 QSettings。

**UI 元件：**
- 點擊「Claude」文字可開啟 `https://claude.ai/settings/usage`
- 切換按鈕（5小時循環 / 7天循環），點一下自動切換，同步更新系統匣顯示
- 進度條 + 百分比 + 重置倒數 + 額外用量標示
- 齒輪按鈕（⚙）直接開啟設定
- 關閉按鈕（×）最小化至系統匣，不結束程式

**自動刷新：**
每 30 秒從 API 取得新資料；倒數計時器在剩餘時間 ≤ 60 秒時自動切換為每秒更新，使倒數更精確。

**重啟機制（UUID 變更時）：**
當設定儲存後偵測到 UUID 與先前不同，程式先主動釋放 Windows Mutex（`ReleaseMutex` + `CloseHandle`），再以 `QProcess::startDetached` 啟動新實例，最後呼叫 `QApplication::quit()` 結束舊實例。提前釋放 Mutex 是為了讓新實例能立即取得鎖，不需等待舊進程完全退出。

### 三、設定介面（SettingsDialog）

**版面：**
- 驗證方式選擇（Session Cookie / Admin API 金鑰）
- 憑證輸入區（含顯示/隱藏切換按鈕）
- 說明文字（依驗證方式切換）：Session Cookie 模式下附 4 個步驟說明，包含「填入 sessionKey 後可嘗試下方的自動取得 UUID」提示
- 「取得 Session Cookie 的方法：」標題旁附 `(?)` 按鈕，滑鼠移上去顯示 600×340 說明圖片（`ImageTooltipLabel` 自訂元件）
- UUID 輸入區，可手動填入或自動取得；貼上完整 URL 時自動擷取 UUID 部分
- 操作按鈕列：「測試連線」｜「自動取得 UUID」｜「清除設定」
- 底部按鈕列：「關於」（左）｜「取消」｜「儲存」（右）

**自動取得 UUID：**
按下後呼叫 `/api/organizations` 取得 UUID，成功後立即覆寫欄位並顯示綠色狀態訊息，同時儲存至 QSettings。

**清除設定：**
刪除 DPAPI 加密儲存的憑證（`SecureStorage::remove`）及 QSettings 中所有設定，並清空畫面上的欄位。

**驗證規則：**
儲存時檢查 sessionKey 與 UUID 均不為空（Admin API 模式不需 UUID），否則阻擋並提示。

**Auth Expired 防抖：**
儲存設定後設立 10 秒的 `m_ignoreAuthExpiry` 旗標，避免因 API 正在重新驗證期間觸發「驗證已過期」彈窗。

**關於對話框：**
以 `Qt::Popup | Qt::FramelessWindowHint` 方式呈現，點擊外部自動關閉。顯示程式圖示（72×72）、名稱（Claude Usage Monitor）、版本（Version 1.0.0）、製作人（Developed by Jonathan）、信箱（deceye@komica7.org，可選取複製）。對話框位置對齊設定視窗正中央。

### 四、系統匣圖示（SystemTrayIcon）

- 以 `QPainter` 繪製純數字圖示（22×22 px）
- 字體大小：2位數 18px，3位數 13px
- 顏色依使用率變化：紫色（< 75%）、黃色（< 90%）、紅色（≥ 90%）
- 工具提示顯示當前窗口與使用率（例：「5小時循環 已使用 83%」）
- 切換 5 小時 / 7 天視窗時系統匣同步更新

### 五、憑證安全儲存（SecureStorage）

- Windows 平台使用 DPAPI（`CryptProtectData` / `CryptUnprotectData`）
- 非 Windows 平台以 QSettings 備用，並發出 `#warning`
- CMake 連結 `crypt32`

**儲存位置：**

```
HKEY_CURRENT_USER\Software\ClaudeMonitor\ClaudeUsageMonitor
```

**所有寫入欄位：**

| 欄位 | 儲存方式 | 說明 |
|------|----------|------|
| `credential` | DPAPI 加密（Base64）| sessionKey cookie 值，以目前登入的 Windows 使用者金鑰加密，其他帳號無法解密 |
| `orgId` | 明文字串 | 組織 UUID，本身不具敏感性，直接儲存 |
| `authMethod` | 整數（0 / 1）| 驗證方式，0 = Session Cookie，1 = Admin API 金鑰 |
| `overlay/pos` | QPoint | 懸浮視窗上次的螢幕座標，拖曳放開時寫入 |
| `hyperTop` | 布林值 | 超級置頂模式是否啟用，切換時寫入，啟動時讀取還原 |

程式啟動時從上述位置讀取，若 `"credential"` 為空則自動開啟設定視窗要求輸入。`"orgId"` 可由使用者手動填入，或在測試連線 / 自動取得 UUID 時由程式寫入並覆蓋。

### 六、使用率 0% 閃爍提示

當 API 回傳的 `utilization` 為 0 時，重置倒數標籤改為顯示紅白交替閃爍的「已重置循環，倒數未運行!」，而非顯示一般的倒數時間。

- 以獨立的 `m_blinkTimer`（600ms 間隔）切換標籤顏色（`#ff4444` ↔ `#ffffff`）
- 進入閃爍狀態時 `setFixedWidth(168)` 加寬標籤，避免文字被截斷；恢復正常時還原為 `90`
- 條件：`pct == 0.0`，與 `resetsAt` 是否有效無關

### 七、超級置頂模式

標準的 `Qt::WindowStaysOnTopHint` 在影片播放器或瀏覽器全螢幕等情境下可能失效。超級置頂模式以 timer 每 300ms 主動重新呼叫 `SetWindowPos(HWND_TOPMOST)` 強制維持置頂。

- 透過右鍵選單「📌 超級置頂」切換開關
- 狀態寫入 QSettings `"hyperTop"`，下次啟動自動恢復
- `m_hyperTopTimer` 全域 handle 宣告為成員變數，以便隨時啟停

### 八、程式圖示（app.ico）

原始圖片為紫色「5hr」風格 PNG，使用 PowerShell `System.Drawing` 轉換為多尺寸 ICO（16 / 32 / 48 / 256 px）。ICO 透過 Windows 資源描述檔 `app.rc` 嵌入 exe，CMake 設定 `COMPILE_FLAGS "-I..."` 讓 windres 能找到相對路徑的圖示檔。PNG 版本另存為 `app_icon.png` 供關於對話框使用。

### 九、單一實例保護

使用 Windows 具名 Mutex（`CreateMutexW` + `WaitForSingleObject`）防止多重開啟：

```cpp
HANDLE g_singleInstanceMutex = CreateMutexW(nullptr, FALSE, L"ClaudeUsageMonitor_SingleInstance");
if (WaitForSingleObject(g_singleInstanceMutex, 3000) != WAIT_OBJECT_0) {
    CloseHandle(g_singleInstanceMutex);
    return 0;  // 另一個實例已在執行，直接結束
}
```

Mutex handle 宣告為全域變數（`g_singleInstanceMutex`），以便 UUID 變更觸發重啟時可從 `OverlayWindow::openSettings()` 主動釋放，避免新實例等待逾時。

---

## 遇到的主要問題與解決方案

| 問題 | 原因 | 解決方式 |
|------|------|----------|
| Qt::Tool 視窗失去置頂 | Qt 在焦點切換後未重設 HWND_TOPMOST | showEvent 中呼叫 Win32 SetWindowPos |
| globalPos() 編譯錯誤 | Qt 6 移除此 API | 改用 globalPosition().toPoint() |
| UUID 儲存為空值 | dialog 關閉後讀值順序錯誤 | 以欄位值優先，捕捉 prevOrgId 在 exec() 之前 |
| 驗證過期無限循環 | 儲存後 API 回傳 401 觸發 authExpired | m_ignoreAuthExpiry 旗標壓制 10 秒 |
| (?) 圖示位置錯誤 | 放在憑證輸入欄旁而非標題旁 | 拆分說明文字為標題列 + 內容，ImageTooltipLabel 置於標題右側 |
| 程式圖示未更新 | windres 找不到相對路徑的 .ico 檔 | CMakeLists.txt 加入 COMPILE_FLAGS 指定 windres include 路徑 |
| UUID 變更後只關閉不重啟 | 舊實例 Mutex 持有至進程完全退出，新實例等待 3 秒逾時後放棄 | 重啟前先呼叫 ReleaseMutex + CloseHandle 主動釋放，新實例可立即取得鎖 |
| 帳號方案未顯示（永遠只顯示「Claude」）| `fetchAccount()` 原指向 `/api/account`，但 `capabilities` 實際在 `/api/organizations` 回應中；且 `openSettings()` 儲存後未呼叫 `fetchAccount()` | 改用已有的 `kOrganizationsUrl`，解析陣列第一個元素的 `capabilities` 欄位；並在儲存設定後補呼叫 `fetchAccount()` |
| 第二次測試連線顯示「驗證已過期」| `m_testClient` 殘留上次請求的狀態，舊連線的 authExpired 信號在第二次觸發 | 每次按下測試連線前 `delete m_testClient` 重建，確保從乾淨狀態發起請求 |
| 帳號方案名稱在懸浮列被截斷 | `m_nameLabel` 無最小寬度限制，被其他固定寬度元件擠壓 | 加入 `setMinimumWidth(88)` 確保「Claude Max」等較長文字不被截斷 |
| `.vscode/c_cpp_properties.json` 含本機絕對路徑 | Qt 和 MSYS2 路徑寫死，其他開發者 clone 後無法直接使用 | 改用環境變數 `${env:Qt6StaticDir}` 和 `${env:MSYS2_ROOT}` 取代絕對路徑，使用者只需設定對應環境變數即可 |

---

## 打包發布

### 目標：盡可能精簡發布檔案

原始目標為單一 .exe 完全獨立，但受限於 MinGW 執行環境的相容性問題（詳見第 5 點），最終結果為：

- `ClaudeUsageMonitor.exe` — 包含 Qt、C++ 標準函式庫、所有應用程式邏輯
- `libwinpthread-1.dll` — 唯一需附帶的外部 DLL

除此之外不需要安裝 Qt、Visual C++ 執行環境或任何其他套件。

**方案選擇：** 從 Qt 原始碼靜態編譯

**編譯環境問題：**

1. **init-repository 失敗** — Qt 6 中 qtnetwork 已併入 qtbase，不是獨立模組，改用 `--module-subset=qtbase`

2. **GCC 15 多重定義錯誤** — GCC 15 對 `_Float16` 的 `inline constexpr` 處理方式改變，導致 `QtPrivate::IsFloatType_v<_Float16>` 重複定義
   解法：configure 時加入 `-DCMAKE_EXE_LINKER_FLAGS=-Wl,--allow-multiple-definition`

3. **Freetype / JPEG 為動態連結** — Qt configure 抓到 Strawberry Perl 路徑下的 import library（.dll.a）
   解法：改用 `-qt-freetype -qt-libjpeg -qt-zlib` 強制使用 Qt 內建版本

4. **libzstd 無法靜態連結** — CMake 優先選 libzstd.dll.a
   解法：Qt configure 加入 `-no-feature-zstd` 直接排除依賴

5. **clock_gettime64 入口點錯誤（libwinpthread 無法靜態連結）** — MSYS2 UCRT64 的 `libwinpthread` 靜態庫在內部呼叫 `clock_gettime64`，此函式存在於較新版本的 `ucrtbase.dll` 中。靜態連結後，執行時期動態查找 `ucrtbase.dll` 的入口點，若版本不符就會出現「找不到程序輸入點」錯誤。嘗試降版（MSYS2 無獨立 gcc13 套件）或強制靜態路徑均無法解決。
   解法：放棄靜態連結 winpthread，改為動態連結，並將 `libwinpthread-1.dll` 隨 exe 一併發布。libgcc 與 libstdc++ 則仍可正常靜態連結。

**最終 Qt 靜態編譯參數：**
```
-static -release -optimize-size -platform win32-g++
-qt-freetype -qt-libjpeg -qt-zlib
-no-feature-zstd -no-feature-sql -no-feature-dbus -no-feature-accessibility
-- -DCMAKE_EXE_LINKER_FLAGS=-Wl,--allow-multiple-definition
   -DCMAKE_SHARED_LINKER_FLAGS=-Wl,--allow-multiple-definition
```

**專案連結參數：**
```
-Wl,--allow-multiple-definition -static-libgcc -static-libstdc++ -Wl,-Bdynamic
```

**發布檔案：**
- `ClaudeUsageMonitor.exe`
- `libwinpthread-1.dll`

---

## 專案清理

編譯完成後清除無用檔案，保留必要的原始碼與建置環境。

**已刪除：**

| 路徑 | 原因 |
|------|------|
| `build/` | 舊的動態連結建置資料夾，已被 `build-static/` 取代，內含大量 Qt DLL |
| `petr/sessionkey.png` | 原始說明圖片，已複製至 `src/resources/sessionkey.png` |
| `openspec/config.yaml` | AI 工具的空白設定範本，與專案無關 |
| `src/AppSettings.h` | 未被任何原始碼引用的遺留標頭檔 |
| `build-static/libClaudeUsageMonitor.dll.a` | 連結時自動產生的 import library 副產品，發布不需要 |
| `dist/` | Docker 交叉編譯流程的空輸出目錄，已不使用 |
| `Dockerfile` / `docker-compose.yml` | 原本保留作為備用參考，專案確定為 Windows 原生建置後刪除 |
| `Image_3akxxs3akxxs3akx.png` | 程式圖示原始來源圖，已轉換至 `src/resources/icons/` 後刪除 |
| `qt6-static-build/'` | 名稱含有多餘單引號的異常空目錄，意外建立後刪除 |

**保留：**

| 路徑 | 原因 |
|------|------|
| `qt5/` | Qt 原始碼，重新編譯 Qt 靜態版本時需要 |
| `qt6-static-build/` | Qt 靜態函式庫的編譯產物，已安裝至 `C:\Qt\6-static`，作為備用保留 |

---

## 版本紀錄

| 項目 | 內容 |
|------|------|
| 開發期間 | 2025 ~ 2026 年 |
| 語言／框架 | C++17 / Qt 6 |
| 編譯器 | GCC 15.2 (MSYS2 UCRT64) |
| Qt 版本 | 6.x（從源碼靜態編譯） |
| 目標平台 | Windows 11 x64 |
| 版本 | 1.0.0 |