@echo off
rem
rem Root of Visual Developer Studio Common files.
set VSCommonDir=C:\PROGRA~1\MICROS~4\Common

rem
rem Root of Visual Developer Studio installed files.
rem
set MSDevDir=C:\PROGRA~1\MICROS~4\Common\msdev98

rem
rem Root of Visual C++ installed files.
rem
set MSVCDir=C:\PROGRA~1\MICROS~4\VC98

rem
rem VcOsDir is used to help create either a Windows 95 or Windows NT specific path.
rem
set VcOsDir=WIN95
if "%OS%" == "Windows_NT" set VcOsDir=WINNT

rem
rem LabVIEW CINTools
rem
set CINTools=C:\PROGRA~1\NATION~1\LABVIE~1.1\cintools

rem
echo Setting environment for using Microsoft Visual C++ tools.
rem

if "%OS%" == "Windows_NT" set PATH=%MSDevDir%\BIN;%MSVCDir%\BIN;%VSCommonDir%\TOOLS\%VcOsDir%;%VSCommonDir%\TOOLS;%PATH%
if "%OS%" == "" set PATH="%MSDevDir%\BIN";"%MSVCDir%\BIN";"%VSCommonDir%\TOOLS\%VcOsDir%";"%VSCommonDir%\TOOLS";"%windir%\SYSTEM";"%PATH%"
set INCLUDE=%MSVCDir%\ATL\INCLUDE;%MSVCDir%\INCLUDE;%MSVCDir%\MFC\INCLUDE;%INCLUDE%
set LIB=%MSVCDir%\LIB;%MSVCDir%\MFC\LIB;%CINTools%;%LIB%

set VcOsDir=
set VSCommonDir=

rem --------------------- Setup Environment to edit GFTSIO -----------------------------
rem set PROMPT=gfitsio$G
rem ------------------------------------------------------------------------------------

