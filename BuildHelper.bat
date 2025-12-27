@echo off
rem ============================================================================
rem  BuildHelper.bat - Agent-friendly wrapper for Unreal Engine builds
rem  Usage: BuildHelper.bat [options]
rem         Options are passed directly to UnrealBuildTool (e.g., -LiveCoding)
rem ============================================================================

set "PROJECT_PATH=%~dp0LinkMeProject.uproject"
set "ENGINE_PATH=C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat"

echo ========================================
echo [AGENT] Build starting...
echo [AGENT] Project: %PROJECT_PATH%
echo [AGENT] Arguments: %*
echo ========================================

call "%ENGINE_PATH%" LinkMeProjectEditor Win64 Development -Project="%PROJECT_PATH%" -WaitMutex -FromMsBuild %*

if %ERRORLEVEL% NEQ 0 (
    echo ========================================
    echo [AGENT] BUILD FAILED! ErrorLevel: %ERRORLEVEL%
    echo ========================================
    exit /b %ERRORLEVEL%
)

echo ========================================
echo [AGENT] BUILD SUCCESS!
echo ========================================
exit 0
