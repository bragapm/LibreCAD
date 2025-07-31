@echo off

if "%Qt6_Dir%"=="" goto SetEnv
if "%NSIS_DIR%"=="" goto SetEnv
goto Exit

:SetEnv
set Qt6_Dir=D:\Work\Braga\Qt\6.9.1
set NSIS_DIR=C:\Program Files (x86)\NSIS

if exist custom-windows.bat call custom-windows.bat
set PATH=%Qt6_Dir%\%MINGW_VER%\bin;%Qt6_Dir%\..\Tools\%MINGW_VER%\bin;%NSIS_DIR%;%PATH%

:Exit
echo on
