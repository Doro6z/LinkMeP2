---
description: Create Git Snapshot (Backup before refactoring)
---
1. Check current git status
// turbo
2. git status

3. Create a timestamped snapshot branch
// turbo
4. $timestamp = Get-Date -Format "yyyy-MM-dd_HHmmss"; git checkout -b "snapshot/$timestamp"

5. Stage all source code changes (excluding Docs/)
// turbo
6. git add Source/ Config/ Content/ *.uproject

7. Commit the snapshot
8. git commit -m "Snapshot before refactoring - $(Get-Date -Format 'yyyy-MM-dd HH:mm')"

9. Return to main development branch
// turbo
10. git checkout main
