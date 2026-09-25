!include "MUI2.nsh"
!include "FileFunc.nsh"

Name "Sugarota Desktop"
OutFile "dist\SugarotaDesktop-Native-Setup-1.0.0.exe"
InstallDir "$PROGRAMFILES64\Sugarota Desktop"
InstallDirRegKey HKLM "Software\SugarotaDesktop" "InstallDir"
RequestExecutionLevel admin

!define MUI_ICON "assets\icon.ico"
!define MUI_UNICON "assets\icon.ico"

!define MUI_ABORTWARNING

; Welcome / Directory / Instfiles pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

; Finish page with launch option
!define MUI_FINISHPAGE_RUN "$INSTDIR\SugarotaDesktop.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch Sugarota Desktop"
!insertmacro MUI_PAGE_FINISH

; Uninstaller pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Section "Install"
  SetOutPath "$INSTDIR"
  
  ; Stop existing instance if running
  nsExec::Exec 'taskkill /F /IM SugarotaDesktop.exe'
  Sleep 500

  File "bin\SugarotaDesktop.exe"
  File "assets\icon.ico"

  ; Create shortcuts
  CreateDirectory "$SMPROGRAMS\Sugarota Desktop"
  CreateShortcut "$SMPROGRAMS\Sugarota Desktop\Sugarota Desktop.lnk" "$INSTDIR\SugarotaDesktop.exe" "" "$INSTDIR\icon.ico"
  CreateShortcut "$DESKTOP\Sugarota Desktop.lnk" "$INSTDIR\SugarotaDesktop.exe" "" "$INSTDIR\icon.ico"

  ; Write registry for uninstaller
  WriteRegStr HKLM "Software\SugarotaDesktop" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SugarotaDesktop" "DisplayName" "Sugarota Desktop"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SugarotaDesktop" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SugarotaDesktop" "DisplayIcon" '"$INSTDIR\icon.ico"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SugarotaDesktop" "Publisher" "Leonid Ardaev"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SugarotaDesktop" "DisplayVersion" "1.0.0"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Uninstall"
  nsExec::Exec 'taskkill /F /IM SugarotaDesktop.exe'
  Sleep 500

  Delete "$DESKTOP\Sugarota Desktop.lnk"
  Delete "$SMPROGRAMS\Sugarota Desktop\Sugarota Desktop.lnk"
  RMDir "$SMPROGRAMS\Sugarota Desktop"

  Delete "$INSTDIR\SugarotaDesktop.exe"
  Delete "$INSTDIR\icon.ico"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\SugarotaDesktop"
  DeleteRegKey HKLM "Software\SugarotaDesktop"

  ; Prompt user to clean settings (default YES)
  MessageBox MB_YESNO|MB_ICONQUESTION "Do you want to delete your saved Nightscout credentials and settings?" /SD IDYES IDNO skipSettings
    RMDir /r "$APPDATA\sugarota-desktop"
  skipSettings:
SectionEnd
