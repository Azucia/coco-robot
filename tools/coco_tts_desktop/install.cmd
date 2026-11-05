@echo off
cd /d "%~dp0"
py -3 -m venv .venv
if errorlevel 1 goto failed
".venv\Scripts\python.exe" -m pip install -r requirements.txt
if errorlevel 1 goto failed
echo Ready. Double-click start.cmd to open Coco TTS.
pause
exit /b 0
:failed
echo Setup failed. Python 3.11+ with Tkinter and internet access are required.
pause
exit /b 1
