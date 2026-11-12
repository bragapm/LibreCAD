call set-windows-env.bat

REM Deploy Qt runtime DLLs (incl. Qt6OpenGL / Qt6OpenGLWidgets) into windows\ before packaging
if "%MINGW_VER%"=="" set MINGW_VER=mingw_64
set "WINDEPLOYQT=%Qt6_Dir%\%MINGW_VER%\bin\windeployqt6.exe"
if not exist "%WINDEPLOYQT%" set "WINDEPLOYQT=windeployqt6.exe"
set "PATH=%Qt6_Dir%\%MINGW_VER%\bin;%Qt6_Dir%\..\Tools\mingw1310_64\bin;%PATH%"
"%WINDEPLOYQT%" "%~dp0..\windows\LibreCAD.exe" --compiler-runtime
if errorlevel 1 (
    echo "windeployqt failed!"
    exit /b 1
)

if _%LC_NSIS_FILE%==_ (
    set LC_NSIS_FILE=nsis-5.4.nsi
)
pushd postprocess-windows
makensis.exe /X"SetCompressor /FINAL lzma" /V4 %LC_NSIS_FILE%
popd
