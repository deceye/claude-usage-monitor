# DLL 依賴清單

`ClaudeUsageMonitor.exe` 的完整 DLL 依賴，由 `objdump -p` 分析。

---

## 需隨 exe 附帶發布

| DLL | 說明 |
|-----|------|
| `libwinpthread-1.dll` | MSYS2 UCRT64 POSIX 執行緒支援。無法靜態連結（原因詳見 WORKLOG.md），必須與 exe 放在同一目錄 |

---

## Windows 系統 DLL（內建，無需附帶）

### 核心系統
| DLL | 說明 |
|-----|------|
| `KERNEL32.dll` | Windows 核心 API |
| `USER32.dll` | 視窗、訊息、輸入處理 |
| `GDI32.dll` | 圖形裝置介面繪圖 |
| `ADVAPI32.dll` | 登錄檔、服務、安全性 API |
| `SHELL32.dll` | Shell 整合（系統匣等） |
| `SHLWAPI.dll` | Shell 輔助函式 |
| `ole32.dll` | COM 物件基礎 |
| `OLEAUT32.dll` | COM 自動化 |
| `VERSION.dll` | 版本資訊查詢 |

### 安全性 / 加密
| DLL | 說明 |
|-----|------|
| `CRYPT32.dll` | Windows DPAPI（憑證加密用） |
| `AUTHZ.dll` | 授權 API |
| `bcrypt.dll` | 現代加密 API |
| `ncrypt.dll` | CNG 金鑰儲存 |
| `Secur32.dll` | 安全性支援提供者 |

### 網路
| DLL | 說明 |
|-----|------|
| `WINHTTP.dll` | HTTP 用戶端（Qt Network 使用） |
| `WS2_32.dll` | Winsock 2（TCP/IP socket） |
| `IPHLPAPI.DLL` | IP 輔助 API |
| `NETAPI32.dll` | 網路管理 API |

### 圖形 / Direct3D（Qt 渲染後端）
| DLL | 說明 |
|-----|------|
| `d3d11.dll` | Direct3D 11 |
| `d3d12.dll` | Direct3D 12 |
| `d3d9.dll` | Direct3D 9 |
| `dxgi.dll` | DirectX Graphics Infrastructure |
| `DWrite.dll` | DirectWrite 文字渲染 |
| `dwmapi.dll` | 桌面視窗管理員 API |
| `UxTheme.dll` | 視覺主題 |

### UI / 輸入
| DLL | 說明 |
|-----|------|
| `IMM32.dll` | 輸入法管理員（中文輸入支援） |
| `comdlg32.dll` | 通用對話框 |
| `SHCORE.dll` | DPI 感知等 Shell 核心功能 |
| `SETUPAPI.dll` | 裝置安裝 API |
| `USERENV.dll` | 使用者環境（設定檔路徑等） |
| `WINMM.dll` | 多媒體計時器 |
| `WTSAPI32.dll` | Terminal Services API |

### WinRT（Qt 6 整合）
| DLL | 說明 |
|-----|------|
| `api-ms-win-core-winrt-l1-1-0.dll` | WinRT 核心 |
| `api-ms-win-core-winrt-string-l1-1-0.dll` | WinRT 字串 |
| `api-ms-win-core-synch-l1-2-0.dll` | 同步原語 |

### CRT（C 執行環境，屬於 Windows 系統）
| DLL | 說明 |
|-----|------|
| `api-ms-win-crt-runtime-l1-1-0.dll` | CRT 執行環境 |
| `api-ms-win-crt-math-l1-1-0.dll` | 數學函式 |
| `api-ms-win-crt-stdio-l1-1-0.dll` | 標準 I/O |
| `api-ms-win-crt-string-l1-1-0.dll` | 字串處理 |
| `api-ms-win-crt-heap-l1-1-0.dll` | 記憶體管理 |
| `api-ms-win-crt-convert-l1-1-0.dll` | 型別轉換 |
| `api-ms-win-crt-locale-l1-1-0.dll` | 地區設定 |
| `api-ms-win-crt-time-l1-1-0.dll` | 時間函式 |
| `api-ms-win-crt-filesystem-l1-1-0.dll` | 檔案系統 |
| `api-ms-win-crt-environment-l1-1-0.dll` | 環境變數 |
| `api-ms-win-crt-utility-l1-1-0.dll` | 雜項工具函式 |
| `api-ms-win-crt-private-l1-1-0.dll` | CRT 內部私有 API |

---

## 備註

- 以上系統 DLL 均內建於 Windows 10 / 11，目標平台使用者無需另行安裝
- `libgcc_s_seh-1.dll` 和 `libstdc++-6.dll` 已透過 `-static-libgcc -static-libstdc++` 靜態連結進 exe，不需另外附帶
- 分析指令：`objdump -p ClaudeUsageMonitor.exe | grep "DLL Name"`

---
---

# DLL Dependency List

Full DLL dependencies of `ClaudeUsageMonitor.exe`, analyzed via `objdump -p`.

---

## Must Be Distributed with the exe

| DLL | Description |
|-----|-------------|
| `libwinpthread-1.dll` | MSYS2 UCRT64 POSIX thread support. Cannot be statically linked (see WORKLOG.md for details). Must be placed in the same directory as the exe. |

---

## Windows System DLLs (built-in, no distribution required)

### Core System
| DLL | Description |
|-----|-------------|
| `KERNEL32.dll` | Windows core API |
| `USER32.dll` | Window management, messaging, input handling |
| `GDI32.dll` | Graphics Device Interface drawing |
| `ADVAPI32.dll` | Registry, services, security API |
| `SHELL32.dll` | Shell integration (system tray, etc.) |
| `SHLWAPI.dll` | Shell utility functions |
| `ole32.dll` | COM object foundation |
| `OLEAUT32.dll` | COM automation |
| `VERSION.dll` | Version information query |

### Security / Cryptography
| DLL | Description |
|-----|-------------|
| `CRYPT32.dll` | Windows DPAPI (used for credential encryption) |
| `AUTHZ.dll` | Authorization API |
| `bcrypt.dll` | Modern cryptography API |
| `ncrypt.dll` | CNG key storage |
| `Secur32.dll` | Security support provider |

### Network
| DLL | Description |
|-----|-------------|
| `WINHTTP.dll` | HTTP client (used by Qt Network) |
| `WS2_32.dll` | Winsock 2 (TCP/IP sockets) |
| `IPHLPAPI.DLL` | IP helper API |
| `NETAPI32.dll` | Network management API |

### Graphics / Direct3D (Qt rendering backend)
| DLL | Description |
|-----|-------------|
| `d3d11.dll` | Direct3D 11 |
| `d3d12.dll` | Direct3D 12 |
| `d3d9.dll` | Direct3D 9 |
| `dxgi.dll` | DirectX Graphics Infrastructure |
| `DWrite.dll` | DirectWrite text rendering |
| `dwmapi.dll` | Desktop Window Manager API |
| `UxTheme.dll` | Visual themes |

### UI / Input
| DLL | Description |
|-----|-------------|
| `IMM32.dll` | Input Method Manager (CJK input support) |
| `comdlg32.dll` | Common dialogs |
| `SHCORE.dll` | Shell core functions (DPI awareness, etc.) |
| `SETUPAPI.dll` | Device installation API |
| `USERENV.dll` | User environment (profile paths, etc.) |
| `WINMM.dll` | Multimedia timers |
| `WTSAPI32.dll` | Terminal Services API |

### WinRT (Qt 6 integration)
| DLL | Description |
|-----|-------------|
| `api-ms-win-core-winrt-l1-1-0.dll` | WinRT core |
| `api-ms-win-core-winrt-string-l1-1-0.dll` | WinRT strings |
| `api-ms-win-core-synch-l1-2-0.dll` | Synchronization primitives |

### CRT (C Runtime, part of Windows)
| DLL | Description |
|-----|-------------|
| `api-ms-win-crt-runtime-l1-1-0.dll` | CRT runtime |
| `api-ms-win-crt-math-l1-1-0.dll` | Math functions |
| `api-ms-win-crt-stdio-l1-1-0.dll` | Standard I/O |
| `api-ms-win-crt-string-l1-1-0.dll` | String handling |
| `api-ms-win-crt-heap-l1-1-0.dll` | Memory management |
| `api-ms-win-crt-convert-l1-1-0.dll` | Type conversion |
| `api-ms-win-crt-locale-l1-1-0.dll` | Locale settings |
| `api-ms-win-crt-time-l1-1-0.dll` | Time functions |
| `api-ms-win-crt-filesystem-l1-1-0.dll` | File system |
| `api-ms-win-crt-environment-l1-1-0.dll` | Environment variables |
| `api-ms-win-crt-utility-l1-1-0.dll` | Miscellaneous utilities |
| `api-ms-win-crt-private-l1-1-0.dll` | CRT internal private API |

---

## Notes

- All system DLLs listed above are built into Windows 10 / 11 and require no separate installation
- `libgcc_s_seh-1.dll` and `libstdc++-6.dll` are statically linked into the exe via `-static-libgcc -static-libstdc++` and do not need to be distributed
- Analysis command: `objdump -p ClaudeUsageMonitor.exe | grep "DLL Name"`