@echo off
cls

echo ================================
echo ESP32 SPIFFS Uploader
echo ================================

:: ====== CONFIG ======
set PORT=COM11
set BAUD=921600
set OFFSET=0x290000
set SIZE=0x140000


:: ====== KILL SERIAL USERS ======
echo.
echo [1/4] Releasing COM port...

taskkill /f /im arduino.exe >nul 2>&1
taskkill /f /im arduino-ide.exe >nul 2>&1
taskkill /f /im putty.exe >nul 2>&1
taskkill /f /im python.exe >nul 2>&1

timeout /t 2 >nul


:: ====== CHECK FILES ======
if not exist mkSPIFFS.exe (
  echo ERROR: mkSPIFFS.exe not found!
  pause
  exit /b
)

if not exist data (
  echo ERROR: data folder not found!
  pause
  exit /b
)

:: ====== BUILD IMAGE ======
echo.
echo [2/4] Building SPIFFS image...

mkspiffs.exe -c data -b 4096 -p 256 -s %SIZE% spiffs.bin

if errorlevel 1 (
  echo ERROR: Failed to build filesystem!
  pause
  exit /b
)

echo OK: Image created

:: ====== UPLOAD ======
echo.
echo [3/4] Uploading to ESP32...

esptool --chip esp32 --port %PORT% --baud %BAUD% write-flash %OFFSET% spiffs.bin

if errorlevel 1 (
  echo ERROR: Upload failed!
  pause
  exit /b
)

echo OK: Upload complete

:: ====== DONE ======
echo.
echo [4/4] Done!
echo You can now open ESP IP in browser.
echo.

pause
