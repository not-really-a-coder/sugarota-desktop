# Sugarota Desktop

An ultra-lightweight Windows 11 Taskbar Glucose Utility & Widget for Nightscout built with pure native C++ / Win32 API and Direct2D.

Designed to match the aesthetics of the Windows 11 taskbar weather widget, Sugarota displays live glucose readings, trend arrows, deltas, and multi-hour sparkline charts directly on your desktop with minimal resource consumption (~5 to 8 MB RAM).

---

## Features

- **Windows 11 Weather-Style Pill Widget:**
  - Compact two-line taskbar overlay (`Value` & `Units + Delta`).
  - **Harvey Arc Circle:** Left indicator showing real-time data age with 0–5+ minute quadrant fill and multi-tier color coding (Green < 6m, Amber 6–14m, Red ≥ 15m), plus dedicated elapsed time string directly below the ball (`now`, `3m`, etc.).
  - **Trend Direction Indicator:** Direction arrow (`DoubleUp`, `SingleUp`, `FortyFiveUp`, `Flat`, `FortyFiveDown`, `SingleDown`, `DoubleDown`) aligned consistently to the value.
  - **Integrated 1-Hour Sparkline Preview:** Read-only mini-chart on the right side showing the target corridor and a color-coded smoothed trend line of the past hour's data.
  - **Draggable & Dockable:** Drag anywhere along the taskbar or screen edges; coordinates are persisted automatically.
- **Configurable Multi-Widget & Multi-Monitor Support:**
  - Configurable count of widgets (1 to 5) side-by-side on the primary monitor.
  - Optional multi-monitor replication across secondary displays.
- **Hardware-Accelerated Direct2D Flyout:**
  - Left-clicking toggles a Windows 11-styled flyout card above the taskbar.
  - **3-Hour History Trend Chart:** Continuous smoothed curve and point nodes with target-range color coding (Green inside 70–180 mg/dL target corridor, Orange outside).
  - **Interactive Scrubber Tooltip:** Hovering over the chart displays a vertical hairline guide and a floating badge showing the exact glucose value and timestamp (`HH:MM`).
  - **Top-Right Quick Actions:**
    - **Pushpin Button:** Toggles "Always on Top" pin mode so the flyout remains open and pinned until unpinned.
    - **Settings Gear Button:** Directly opens the Settings configuration dialog.
- **Settings & Management:**
  - Nightscout URL & API Secret / Token configuration with a live "Test Connection" validation.
  - Toggle between `mg/dL` and `mmol/L`.
  - Dark Mode and Light Mode styling.
  - Polling interval configuration (default 60s, min 15s).
  - "Exit Sugarota" button and system tray context menu.

---

## Pre-built Distributables

The compiled Windows binaries are located in:
1. **Windows Installer (`NSIS`):**  
   [`dist/SugarotaDesktop-Native-Setup-1.0.0.exe`](dist/SugarotaDesktop-Native-Setup-1.0.0.exe)  
   Installs to `C:\Program Files\Sugarota Desktop\`, creates Start Menu and Desktop shortcuts, and includes an uninstaller with an option to erase user data.
2. **Direct Executable:**  
   [`bin/SugarotaDesktop.exe`](bin/SugarotaDesktop.exe) (~460 KB, self-contained standalone executable).

---

## Technical Documentation & Security

- [Technical Architecture & Component Specifications](docs/architecture.md)
- [Developer & Operations Guide](docs/guide.md)
- [Security Model & Threat Mitigations](SECURITY.md)

---

## Building from Source

### Requirements
- Windows 10 / 11 (x64)
- Microsoft Visual Studio 2022 Build Tools (`cl.exe`, Windows SDK)
- NSIS v3.x (for packaging the installer)

### Commands
```bat
# Compile the native C++ binary to bin\SugarotaDesktop.exe
build_native.bat

# Package the NSIS installer to dist\SugarotaDesktop-Native-Setup-1.0.0.exe
makensis.exe installer_native.nsi
```

