@echo off
setlocal

rem Build the LinkMeProject using UnrealBuildTool
rem Adjust paths if necessary

set UE_ROOT=C:\Program Files\Epic Games\UE_5.7
set PROJECT_ROOT=%~dp0
set PROJECT_NAME=LinkMeProject
set PROJECT_FILE=%PROJECT_ROOT%%PROJECT_NAME%.uproject

rem Generate project files (if needed)
"%UE_ROOT%\Engine\Build\BatchFiles\GenerateProjectFiles.bat" -project="%PROJECT_FILE%" -game -engine

rem Build the project
"%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" %PROJECT_NAME% Win64 Development "%PROJECT_FILE%"

if %errorlevel% neq 0 (
    echo Build failed.
    exit /b %errorlevel%
) else (
    echo Build succeeded.
)

endlocal
