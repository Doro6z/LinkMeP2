@echo off
set PROJECT="C:\Users\Corentin\Documents\Unreal Projects\LinkMeProject\LinkMeProject.uproject"
set MAP="/Game/Maps/Prototypes/L_Proto_Forest"

REM --- Attempt to find Unreal Engine ---
set "UE_ROOT=C:\Program Files\Epic Games"

REM Check 5.7 (Target)
set "EDITOR=%UE_ROOT%\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe"
if exist "%EDITOR%" goto FOUND

REM Check 5.5
set "EDITOR=%UE_ROOT%\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe"
if exist "%EDITOR%" goto FOUND

REM Check 5.4
set "EDITOR=%UE_ROOT%\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe"
if exist "%EDITOR%" goto FOUND

REM Check 5.3
set "EDITOR=%UE_ROOT%\UE_5.3\Engine\Binaries\Win64\UnrealEditor.exe"
if exist "%EDITOR%" goto FOUND

echo [ERROR] Could not find UnrealEditor.exe.
pause
exit /b

:FOUND
echo Found Editor: %EDITOR%
echo Launching Forest Prototype...
start "" %EDITOR% %PROJECT% %MAP%
exit

