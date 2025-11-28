---
description: Build and Launch for Test Session
---
1. Build the project
// turbo
2. & "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" LinkMeProjectEditor Win64 Development -Project="c:\Users\Corentin\Documents\Unreal Projects\LinkMeProject\LinkMeProject.uproject" -WaitMutex -FromMsBuild

3. Launch Unreal Editor for testing
// turbo
4. & "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe" "c:\Users\Corentin\Documents\Unreal Projects\LinkMeProject\LinkMeProject.uproject"

5. Reminder to enable debug visualizations
// turbo
6. & echo ""; echo "=== TEST SESSION STARTED ==="; echo "Don't forget to run these console commands:"; echo "  rope.debug.all 1"; echo "  stat FPS"; echo ""; echo "Refer to TestPlan.md for test scenarios"; echo "Use TestSessionTemplate.md to document results"
