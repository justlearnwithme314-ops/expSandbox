@echo off
cd /d "%~dp0"
if not exist "build\native\bin\Release\voxel-lab.exe" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1"
    if errorlevel 1 exit /b 1
)
start "Voxel Lab" "build\native\bin\Release\voxel-lab.exe" --data-root "%~dp0."
