# Sugarota Desktop — Developer & Operations Guide

## 1. Directory Structure

```
sugarota-desktop/
├── assets/                  # Application icon assets (.ico, .png)
├── bin/                     # Compiler output binaries (.exe, intermediate .obj)
├── dist/                    # Packaged NSIS installer executables
├── docs/                    # Architecture and technical documentation
│   ├── architecture.md      # Core architecture, component reference & data schemas
│   └── guide.md             # Developer build commands and troubleshooting guide
├── extras/                  # Supplementary reference designs & web previews
├── src-native/              # Pure C++ Win32 & Direct2D implementation
│   ├── config.h / .cpp      # JSON configuration management
│   ├── d2d_context.h / .cpp # Direct2D & DirectWrite engine
│   ├── flyout_window.h / .cpp # Acrylic flyout card & trend chart
│   ├── main.cpp             # App coordinator, tray icon, WinMain
│   ├── nightscout.h / .cpp  # Asynchronous WinHTTP client & parser
│   ├── pill_window.h / .cpp # Taskbar widget pill, Harvey circle & gesture handling
│   └── settings_dialog.h / .cpp # Modern Win32 settings modal
├── build_native.bat         # Single-command MSVC 64-bit compilation script
├── installer_native.nsi     # NSIS installer packaging script
└── README.md                # Project landing overview
```

---

## 2. Compilation and Build Pipeline

### Compiling `SugarotaDesktop.exe`
Open a command prompt and run:
```bat
build_native.bat
```
This script calls `vcvars64.bat` to configure the MSVC 64-bit toolchain and invokes:
```bat
cl /nologo /O2 /MT /std:c++17 /EHsc ^
   /Fo:bin\ ^
   src-native\*.cpp ^
   /Fe:bin\SugarotaDesktop.exe ^
   /link /SUBSYSTEM:WINDOWS ^
   user32.lib gdi32.lib shell32.lib ole32.lib comctl32.lib d2d1.lib dwrite.lib winhttp.lib dxgi.lib
```
- `/O2`: Maximum speed optimization.
- `/MT`: Statically links the C/C++ runtime library (no Visual C++ redistributable installer needed on the target machine).
- `/SUBSYSTEM:WINDOWS`: Suppresses any background console window on launch.
- `/Fo:bin\`: Ensures compiler object files (`*.obj`) stay segregated inside `bin/`.

### Packaging with NSIS
To compile the installer:
```bat
makensis.exe installer_native.nsi
```
- Sets the destination directory default to `C:\Program Files\Sugarota Desktop\`.
- Requests `admin` execution level to install into Program Files and create system-wide shortcuts.
- Registers uninstallation entry in Windows "Installed apps" with an option to wipe `%APPDATA%\sugarota-desktop` credentials.

---

## 3. Windows 11 Compatibility Notes

### Taskbar Z-Ordering
Windows 11 periodically reasserts `Shell_TrayWnd` to the foreground when clicked. Sugarota Desktop counters this by:
1. Creating windows with `WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE`.
2. Setting an internal 500ms heartbeat timer (`TIMER_TOPMOST`) that reaffirms `HWND_TOPMOST` via `SetWindowPos`.
3. Intercepting `WM_ACTIVATE` and `WM_KILLFOCUS` to ensure the pill remains above the taskbar.

### Corner Artifacting Prevention
Windows DWM can draw unwanted dark or black corners on borderless popups when Direct2D clears to transparent (`0, 0, 0, 0`). Sugarota Desktop resolves this by passing a matched `CreateRoundRectRgn` into `SetWindowRgn`, ensuring Windows clips the physical window geometry to the Direct2D rounded rectangle curvature.
