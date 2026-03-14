; DirektDSP PedalSuite NSIS Installer
; VERSION is passed via /DVERSION=x.x.x on the makensis command line
; ARTIFACTS_DIR is passed via /DARTIFACTS_DIR=path

!include "MUI2.nsh"

!ifndef VERSION
  !define VERSION "0.0.1"
!endif

!ifndef ARTIFACTS_DIR
  !define ARTIFACTS_DIR "artifacts\windows"
!endif

Name "DirektDSP PedalSuite ${VERSION}"
OutFile "artifacts\DirektDSP-PedalSuite-${VERSION}-Windows.exe"
InstallDir "$PROGRAMFILES64\DirektDSP\PedalSuite"
RequestExecutionLevel admin
Unicode True

!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "DirektDSP PedalSuite ${VERSION}"
!define MUI_WELCOMEPAGE_TEXT "This will install the DirektDSP PedalSuite audio plugins on your computer.$\r$\n$\r$\nPlugins included:$\r$\n  - Mechanica (Distortion)$\r$\n  - Hydraulica (Compressor)$\r$\n  - Pneumatica (High-End Tweaker)$\r$\n  - Electrica (Synth Pedal)$\r$\n$\r$\nClick Next to continue."

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

;----------------------------------------------------------------------
; VST3 Plugins
;----------------------------------------------------------------------

SectionGroup "VST3 Plugins" SecVST3

Section "Mechanica VST3" SecMechanicaVST3
  SetOutPath "$COMMONFILES64\VST3\DirektDSP"
  File /nonfatal /r "${ARTIFACTS_DIR}\Mechanica.vst3"
SectionEnd

Section "Hydraulica VST3" SecHydraulicaVST3
  SetOutPath "$COMMONFILES64\VST3\DirektDSP"
  File /nonfatal /r "${ARTIFACTS_DIR}\Hydraulica.vst3"
SectionEnd

Section "Pneumatica VST3" SecPneumaticaVST3
  SetOutPath "$COMMONFILES64\VST3\DirektDSP"
  File /nonfatal /r "${ARTIFACTS_DIR}\Pneumatica.vst3"
SectionEnd

Section "Electrica VST3" SecElectricaVST3
  SetOutPath "$COMMONFILES64\VST3\DirektDSP"
  File /nonfatal /r "${ARTIFACTS_DIR}\Electrica.vst3"
SectionEnd

SectionGroupEnd

;----------------------------------------------------------------------
; CLAP Plugins
;----------------------------------------------------------------------

SectionGroup "CLAP Plugins" SecCLAP

Section "Mechanica CLAP" SecMechanicaCLAP
  SetOutPath "$COMMONFILES64\CLAP\DirektDSP"
  File /nonfatal "${ARTIFACTS_DIR}\Mechanica.clap"
SectionEnd

Section "Hydraulica CLAP" SecHydraulicaCLAP
  SetOutPath "$COMMONFILES64\CLAP\DirektDSP"
  File /nonfatal "${ARTIFACTS_DIR}\Hydraulica.clap"
SectionEnd

Section "Pneumatica CLAP" SecPneumaticaCLAP
  SetOutPath "$COMMONFILES64\CLAP\DirektDSP"
  File /nonfatal "${ARTIFACTS_DIR}\Pneumatica.clap"
SectionEnd

Section "Electrica CLAP" SecElectricaCLAP
  SetOutPath "$COMMONFILES64\CLAP\DirektDSP"
  File /nonfatal "${ARTIFACTS_DIR}\Electrica.clap"
SectionEnd

SectionGroupEnd

;----------------------------------------------------------------------
; Standalone Applications
;----------------------------------------------------------------------

SectionGroup "Standalone Apps" SecStandalone

Section "Mechanica Standalone" SecMechanicaSA
  SetOutPath "$INSTDIR"
  File /nonfatal "${ARTIFACTS_DIR}\Mechanica.exe"
SectionEnd

Section "Hydraulica Standalone" SecHydraulicaSA
  SetOutPath "$INSTDIR"
  File /nonfatal "${ARTIFACTS_DIR}\Hydraulica.exe"
SectionEnd

Section "Pneumatica Standalone" SecPneumaticaSA
  SetOutPath "$INSTDIR"
  File /nonfatal "${ARTIFACTS_DIR}\Pneumatica.exe"
SectionEnd

Section "Electrica Standalone" SecElectricaSA
  SetOutPath "$INSTDIR"
  File /nonfatal "${ARTIFACTS_DIR}\Electrica.exe"
SectionEnd

SectionGroupEnd

;----------------------------------------------------------------------
; Uninstaller
;----------------------------------------------------------------------

Section "-Uninstaller"
  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\Uninstall-PedalSuite.exe"

  ; Add/Remove Programs registry
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DirektDSP-PedalSuite" \
    "DisplayName" "DirektDSP PedalSuite ${VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DirektDSP-PedalSuite" \
    "UninstallString" "$\"$INSTDIR\Uninstall-PedalSuite.exe$\""
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DirektDSP-PedalSuite" \
    "Publisher" "DirektDSP"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DirektDSP-PedalSuite" \
    "DisplayVersion" "${VERSION}"
SectionEnd

;----------------------------------------------------------------------
; Component descriptions
;----------------------------------------------------------------------

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecVST3} "VST3 plugins — installed to the system VST3 folder."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecCLAP} "CLAP plugins — installed to the system CLAP folder."
  !insertmacro MUI_DESCRIPTION_TEXT ${SecStandalone} "Standalone applications — installed to the program folder."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;----------------------------------------------------------------------
; Uninstall section
;----------------------------------------------------------------------

Section "Uninstall"
  ; VST3
  RMDir /r "$COMMONFILES64\VST3\DirektDSP\Mechanica.vst3"
  RMDir /r "$COMMONFILES64\VST3\DirektDSP\Hydraulica.vst3"
  RMDir /r "$COMMONFILES64\VST3\DirektDSP\Pneumatica.vst3"
  RMDir /r "$COMMONFILES64\VST3\DirektDSP\Electrica.vst3"
  RMDir "$COMMONFILES64\VST3\DirektDSP"

  ; CLAP
  Delete "$COMMONFILES64\CLAP\DirektDSP\Mechanica.clap"
  Delete "$COMMONFILES64\CLAP\DirektDSP\Hydraulica.clap"
  Delete "$COMMONFILES64\CLAP\DirektDSP\Pneumatica.clap"
  Delete "$COMMONFILES64\CLAP\DirektDSP\Electrica.clap"
  RMDir "$COMMONFILES64\CLAP\DirektDSP"

  ; Standalone + uninstaller
  Delete "$INSTDIR\Mechanica.exe"
  Delete "$INSTDIR\Hydraulica.exe"
  Delete "$INSTDIR\Pneumatica.exe"
  Delete "$INSTDIR\Electrica.exe"
  Delete "$INSTDIR\Uninstall-PedalSuite.exe"
  RMDir "$INSTDIR"
  RMDir "$PROGRAMFILES64\DirektDSP"

  ; Registry
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\DirektDSP-PedalSuite"
SectionEnd
