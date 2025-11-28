@echo off
set UBT="C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe"
set PROJECT="C:\Users\Corentin\Documents\Unreal Projects\LinkMeProject\LinkMeProject.uproject"

echo Cleaning LinkMeProjectEditor...
%UBT% LinkMeProjectEditor Win64 Development -Project=%PROJECT% -Clean -WaitMutex -FromMsBuild
if %ERRORLEVEL% NEQ 0 (
    echo Clean failed!
    exit /b %ERRORLEVEL%
)

echo Building LinkMeProjectEditor...
%UBT% LinkMeProjectEditor Win64 Development -Project=%PROJECT% -WaitMutex -FromMsBuild
if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b %ERRORLEVEL%
)
echo Rebuild successful!
