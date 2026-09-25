@echo off
setlocal

echo Setting up Visual Studio 64-bit environment...
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if not exist bin mkdir bin

echo Compiling Sugarota Desktop (Native C++ / Direct2D)...
cl /nologo /O2 /MT /std:c++17 /EHsc ^
   /Fo:bin\ ^
   src-native\main.cpp ^
   src-native\config.cpp ^
   src-native\nightscout.cpp ^
   src-native\d2d_context.cpp ^
   src-native\pill_window.cpp ^
   src-native\flyout_window.cpp ^
   src-native\settings_dialog.cpp ^
   /Fe:bin\SugarotaDesktop.exe ^
   /link /SUBSYSTEM:WINDOWS ^
   user32.lib gdi32.lib shell32.lib ole32.lib comctl32.lib d2d1.lib dwrite.lib winhttp.lib dxgi.lib

if %ERRORLEVEL% equ 0 (
    echo.
    echo =======================================================
    echo Build successful! Binary created: bin\SugarotaDesktop.exe
    echo =======================================================
) else (
    echo.
    echo Compilation failed!
)
