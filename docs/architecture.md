# Sugarota Desktop — Technical Architecture & Documentation

Sugarota Desktop is an ultra-lightweight, hardware-accelerated Windows 11 taskbar widget utility for real-time continuous glucose monitoring (CGM). It connects directly to personal Nightscout instances, polling blood glucose readings and displaying them in a sleek, non-intrusive floating taskbar widget pill with a detailed rich flyout card.

---

## 1. System Architecture Overview

```
                      +-----------------------------+
                      |   Nightscout Cloud / API    |
                      |  (/api/v1/entries.json)     |
                      +--------------+--------------+
                                     | HTTPS (WinHTTP)
                                     v
                       +---------------------------+
                       |  NightscoutClient Worker  |
                       |  (Background std::thread) |
                       +-------------+-------------+
                                     | WM_USER + 201
                                     v
                       +---------------------------+
                       |   AppManager (main.cpp)   |
                       |   - System Tray Icon      |
                       |   - Window Coordination   |
                       |   - Config & State Bus    |
                       +-------+-----------+-------+
                               |           |
            +------------------+           +------------------+
            |                                                 |
            v                                                 v
+-----------------------+                         +-----------------------+
|  PillWindow (1 to 5)  |                         |     FlyoutWindow      |
|  - Direct2D Pill      |                         |  - Direct2D Card      |
|  - Harvey Time Arc    |                         |  - Line Trend Chart   |
|  - Right Trend Arrow  |                         |  - Range Corridor     |
|  - Drag / Topmost     |                         |  - Stats & Telemetry  |
+-----------------------+                         +-----------------------+
            |
            | On Click (WM_USER + 101)
            +---------------------------------------------> Toggle Flyout
```

### Key Highlights
- **100% Native Win32 / C++17:** Zero runtime dependencies, no Chromium, and no Node.js/Electron overhead.
- **Resource Footprint:** Consumes ~5 to 8 MB of RAM (versus 180–260 MB in Electron implementations).
- **Direct2D & DirectWrite:** Hardware-accelerated vector rendering, sub-pixel ClearType text rendering, anti-aliased geometry, and smooth alpha blending.
- **Window Region Clipping:** Custom `SetWindowRgn` with `CreateRoundRectRgn` bounds to prevent Windows DWM square black corners on borderless rounded popup windows.
- **Taskbar Z-Order Immunity:** Periodic topmost assertion heartbeat to prevent the Windows 11 shell tray (`Shell_TrayWnd`) from permanently occluding the widget.

---

## 2. Component Reference

### 2.1 `AppManager` (`src-native/main.cpp`)
- Coordinates the application lifecycle, message pump, system tray icon (`NOTIFYICONDATAW`), and monitor enumeration.
- Enforces single-instance execution through a named Win32 mutex (`SugarotaDesktopNativeMutex`).
- Handles display resolution and topology changes (`WM_DISPLAYCHANGE`).
- Spawns `widgetCount` (1 to 5) pills on primary (and optionally secondary) displays with calculated horizontal spacing.

### 2.2 `PillWindow` (`src-native/pill_window.h`, `pill_window.cpp`)
- Taskbar widget pill (default 146 x 42 px) docked adjacent to the system tray notification area.
- **Left Indicator:** Harvey arc circle visualizing data age in minutes (0–5+ min fill) with multi-stage color thresholds:
  - Fresh (< 6 mins): Green (`#00FF44` dark / `#009933` light)
  - Stale (6–14 mins): Amber (`#FFAA00` dark / `#D97700` light)
  - Warning (≥ 15 mins): Red (`#FF4444` dark / `#CC0000` light)
  - Explicit relative elapsed time text beneath the circle (`now`, `3m`).
- **Center Telemetry:** Current blood glucose value (bold 16pt Segoe UI Black) + half-sized unit and delta line (`Δ -11 mg/dL`).
- **Trend Indicator:** Trend arrow (`DoubleUp`, `SingleUp`, `FortyFiveUp`, `Flat`, `FortyFiveDown`, `SingleDown`, `DoubleDown`) with left-margin alignment.
- **Mini Sparkline Preview:** Read-only 1-hour trend line with target corridor (70–180 mg/dL) and target-range color coding on the right side of the pill.
- **Interactions:**
  - Left click: Toggles the flyout card.
  - Left drag: Repositions the widget across the screen; position is automatically persisted to configuration.
  - Right click: Opens the Settings window.

### 2.3 `FlyoutWindow` (`src-native/flyout_window.h`, `flyout_window.cpp`)
- Windows 11-styled detailed flyout card (320 x 300 px) anchored automatically to the clicked widget pill.
- **Header Actions:** Title with quick-action pushpin (Always on Top toggle) and settings gear icon.
- **Hero Display:** Large 36pt current glucose reading, delta value (`Δ +27 mg/dL`), units label, and heavy-weight trend arrow.
- **Trend Graph:** 3-hour continuous trend line with discrete point nodes, highlighted latest reading, target-range color coding (Green inside target range, Orange outside), and target corridor (70–180 mg/dL).
- **Interactive Scrubber:** Hairline cursor guide and floating badge displaying exact glucose reading and timestamp (`HH:MM`).
- Automatically dismisses when clicking outside (`WM_ACTIVATE` losing focus), unless pinned via the top-right pushpin button.

### 2.4 `SettingsDialog` (`src-native/settings_dialog.h`, `settings_dialog.cpp`)
- Clean Win32 dialog styled with Segoe UI typography and structured Group Boxes:
  - Nightscout URL and API Secret / Token.
  - Glucose Unit selection (`mmol/L` vs `mg/dL`).
  - Theme selection (`Dark Mode` vs `Light Mode`).
  - Number of concurrent widgets on primary monitor (1 to 5).
  - Multi-monitor replication toggle.
  - Polling interval in seconds (default 60s, min 15s).
- Features live "Test Connection" validation and an explicit "Exit Sugarota" application termination flow.

### 2.5 `NightscoutClient` (`src-native/nightscout.h`, `nightscout.cpp`)
- Asynchronous polling loop executing on a detached background worker thread (`std::thread`).
- Built entirely on Windows HTTP Services (`winhttp.lib`), supporting HTTPS/TLS 1.2/1.3, custom port assignment, and authentication headers (`api-secret` or URL query tokens).
- Streamlined JSON/regex parser handling both numeric epoch timestamps (`"date": 1727271234000`) and ISO 8601 strings (`"dateString": "2026-09-25T13:20:00.000Z"`).
- Automatically calculates consecutive reading delta (`e[0].sgv - e[1].sgv`) if the server payload omits the field or reports 0.

### 2.6 `ConfigManager` (`src-native/config.h`, `config.cpp`)
- Persistent configuration management targeting `%APPDATA%\sugarota-desktop\config.json`.
- Safely handles atomic serialization and defaults recovery.

---

## 3. Data Schemas

### 3.1 Application Configuration (`config.json`)
Located at: `%APPDATA%\sugarota-desktop\config.json`

```json
{
  "nightscoutUrl": "https://cgm.example.com",
  "apiSecret": "your-api-secret-or-token",
  "unit": "mgdl",
  "theme": "dark",
  "refreshInterval": 60,
  "alarmLow": 70,
  "alarmHigh": 180,
  "pillX": -1,
  "pillY": -1,
  "showOnAllMonitors": false,
  "widgetCount": 1
}
```

| Property | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `nightscoutUrl` | string | `""` | Nightscout domain or base URL |
| `apiSecret` | string | `""` | SHA-1 hashed secret or plain read token |
| `unit` | string | `"mgdl"` | Display unit: `"mgdl"` or `"mmol"` |
| `theme` | string | `"dark"` | Color palette: `"dark"` or `"light"` |
| `refreshInterval` | integer | `60` | Network polling frequency in seconds (≥15) |
| `alarmLow` | integer | `70` | Lower bound for target corridor and color shift |
| `alarmHigh` | integer | `180` | Upper bound for target corridor and color shift |
| `pillX` | integer | `-1` | Saved screen X coordinate (-1 for auto taskbar placement) |
| `pillY` | integer | `-1` | Saved screen Y coordinate (-1 for auto taskbar placement) |
| `showOnAllMonitors` | boolean | `false` | If true, duplicates widgets across all active monitors |
| `widgetCount` | integer | `1` | Number of side-by-side widget pills (1 to 5) |

### 3.2 In-Memory Glucose Reading (`GlucoseEntry`)
```cpp
struct GlucoseEntry {
    double sgv = 0.0;             // Sensor Glucose Value in mg/dL (e.g. 115.0)
    std::string direction;        // Trend slope: "Flat", "SingleUp", "DoubleDown", etc.
    long long date = 0;           // Unix epoch timestamp in milliseconds
    double delta = 0.0;           // Difference relative to previous reading
};
```

---

## 4. Build, Packaging & Distribution

### 4.1 Prerequisites
- Microsoft Visual Studio 2022 Build Tools (MSVC `cl.exe` x64, Windows 10/11 SDK).
- NSIS (Nullsoft Scriptable Install System) v3.x.

### 4.2 Building the Binary
Run the automated build script from the project root:
```bat
build_native.bat
```
This produces `bin\SugarotaDesktop.exe` (~450 KB).

### 4.3 Packaging the Installer
Compile the NSIS script:
```bat
makensis.exe installer_native.nsi
```
This produces `dist\SugarotaDesktop-Native-Setup-1.0.0.exe` targeting `C:\Program Files\Sugarota Desktop\`, featuring UAC elevation, Start Menu shortcuts, and an uninstaller option to erase user credentials on removal.
