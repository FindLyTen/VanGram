@echo off
chcp 65001 >nul
setlocal
title VanGram - Portable Backup

set "SRC=%~dp0"
set "OUT=%TEMP%\vangram_portable"

if not exist "%SRC%AyuGram.exe" if not exist "%SRC%Vangram.exe" (
  echo [!] AyuGram.exe / Vangram.exe not found in this folder.
  echo     Put MakePortable.bat into your VanGram folder and run it again.
  pause
  exit /b 1
)

echo Collecting VanGram folder...
if exist "%OUT%" rmdir /s /q "%OUT%"
xcopy /e /i /h /y /q "%SRC%*" "%OUT%\" >nul

echo Cleaning cache, temp and logs...
if exist "%OUT%\tdata\cache" rmdir /s /q "%OUT%\tdata\cache"
if exist "%OUT%\tdata\temp" rmdir /s /q "%OUT%\tdata\temp"
if exist "%OUT%\tdata\user_data\cache" rmdir /s /q "%OUT%\tdata\user_data\cache"
if exist "%OUT%\tdata\user_data\media_cache" rmdir /s /q "%OUT%\tdata\user_data\media_cache"
del /s /q "%OUT%\*.log" >nul 2>&1
del /s /q "%OUT%\*.pdb" >nul 2>&1
if exist "%OUT%\VanGram-portable.zip" del /q "%OUT%\VanGram-portable.zip"
if exist "%OUT%\MakePortable.bat" del /q "%OUT%\MakePortable.bat"

echo Packing into VanGram-portable.zip ...
if exist "%SRC%VanGram-portable.zip" del /q "%SRC%VanGram-portable.zip"
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -Path '%OUT%\*' -DestinationPath '%SRC%VanGram-portable.zip' -Force -CompressionLevel Optimal"

rmdir /s /q "%OUT%"

echo.
echo ============================================
echo  DONE!  VanGram-portable.zip created here.
echo ============================================
echo Move this zip to the other device, extract it
echo and run the .exe. All accounts, tags and
echo settings will be exactly the same.
echo.
echo IMPORTANT: do NOT run VanGram with these
echo accounts on the OLD device anymore - Telegram
echo will kill the sessions (one account = one
echo device at a time).
echo.
pause
