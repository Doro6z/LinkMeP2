---
description: Clean Rebuild LinkMeProject
---
1. Clean the project intermediate files
// turbo
2. Remove-Item -Recurse -Force "c:\Users\Corentin\Documents\Unreal Projects\LinkMeProject\Intermediate" -ErrorAction SilentlyContinue; Remove-Item -Recurse -Force "c:\Users\Corentin\Documents\Unreal Projects\LinkMeProject\Binaries" -ErrorAction SilentlyContinue; Remove-Item -Recurse -Force "c:\Users\Corentin\Documents\Unreal Projects\LinkMeProject\Saved" -ErrorAction SilentlyContinue

3. Rebuild the project with Unreal Build Tool
// turbo
4. & "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" LinkMeProjectEditor Win64 Development -Project="c:\Users\Corentin\Documents\Unreal Projects\LinkMeProject\LinkMeProject.uproject" -WaitMutex -FromMsBuild
