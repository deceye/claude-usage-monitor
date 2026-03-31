# Claude Usage Monitor

![Version](https://img.shields.io/badge/version-1.0.0-blue)
![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-lightgrey)
![License](https://img.shields.io/badge/license-MIT-green)

Windows 桌面懸浮列，即時顯示 claude.ai 的 API 使用量。

---

## 功能

### 懸浮視窗

- 常駐於畫面頂部，永遠置頂顯示，不影響其他視窗操作
- **超級置頂模式** — 可透過右鍵選單開啟，每 300ms 重新強制置頂，適用於影片播放器、瀏覽器全螢幕等情境，狀態會記憶至下次啟動
- 半透明深色背景，可拖曳至任意位置，關閉後自動記憶位置
- 點擊名稱文字（「Claude」/ 「Claude Pro」/ 「Claude Max」）可直接開啟 claude.ai 使用量頁面；程式啟動時自動偵測帳號方案並顯示對應名稱

### 使用量顯示

- **進度條** — 視覺化呈現當前使用率，顏色隨用量變化（紫 → 黃 → 紅）
- **百分比** — 即時顯示目前使用率數字
- **重置倒數** — 顯示距離下次重置的剩餘時間；剩餘 60 秒內自動切換為每秒更新；使用率為 0% 時改為紅白閃爍顯示「已重置循環，倒數未運行!」
- **額外用量** — 若帳號啟用月費超額額度，另行顯示已用金額與上限
- **視窗切換** — 可在「5 小時循環」與「7 天循環」兩種統計窗口間切換

### 自動刷新

- 每 30 秒自動向 claude.ai 取得最新資料
- 亦可點擊 ↺ 按鈕立即手動刷新

### 系統匣

- 最小化後縮至系統匣，圖示顯示當前使用率數字
- 顏色與懸浮視窗同步（紫 / 黃 / 紅）
- 右鍵選單可快速重新整理、開啟設定、切換超級置頂模式或結束程式

---

## 設定

### 驗證方式

支援兩種驗證方式：

| 方式 | 適用對象 | 憑證格式 |
|------|----------|----------|
| Session Cookie | Claude.ai 個人帳號 | 瀏覽器 `sessionKey` cookie 值 |
| Admin API 金鑰 | Anthropic 組織帳號 | `sk-ant-admin-…` |

### 取得 Session Cookie

1. 用 Chrome 開啟 claude.ai 並登入
2. 按 F12 → Application → Cookies → `https://claude.ai`
3. 找到 `sessionKey` 並複製其值
4. 貼入設定視窗的憑證欄位，再點「自動取得 UUID」

### UUID

- 可點擊「自動取得 UUID」讓程式自動向 API 查詢
- 亦可手動貼上 UUID，或貼上含有 UUID 的完整 claude.ai API 網址，程式會自動擷取 UUID 部分

### 其他設定功能

- **測試連線** — 以當前填入的憑證與 UUID 進行驗證，成功後分別顯示 5 小時循環與 7 天循環的使用率及重置倒數
- **清除設定** — 清除所有儲存的憑證與設定
- **關於** — 顯示版本與作者資訊

---

## 安全性

憑證（sessionKey）使用 Windows DPAPI（`CryptProtectData`）加密後儲存於登錄檔：

```
HKEY_CURRENT_USER\Software\ClaudeMonitor\ClaudeUsageMonitor
```

加密金鑰與目前登入的 Windows 使用者帳號綁定，其他帳號無法讀取。

程式會在此路徑寫入以下欄位：

| 欄位 | 類型 | 說明 |
|------|------|------|
| `credential` | DPAPI 加密（Base64） | sessionKey cookie 值，加密儲存 |
| `orgId` | 字串 | 組織 UUID，明文儲存 |
| `authMethod` | 整數（0 / 1） | 驗證方式，0 = Session Cookie，1 = Admin API 金鑰 |
| `overlay/pos` | QPoint | 懸浮視窗上次的螢幕座標 |
| `hyperTop` | 布林值 | 超級置頂模式是否啟用 |

---

## 系統需求

- Windows 10 / 11 x64
- 無需安裝 Qt 或其他執行環境

---

## 發布檔案

```
ClaudeUsageMonitor.exe
libwinpthread-1.dll
```

將兩個檔案放在同一目錄下直接執行即可，首次啟動會自動開啟設定視窗。

---

## 授權

MIT License

Copyright (c) 2026 Jonathan

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.