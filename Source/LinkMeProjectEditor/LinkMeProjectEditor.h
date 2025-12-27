// LinkMeProjectEditor.h

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Editor module for LinkMe development tools.
 * Provides quick-access panels for common editor settings.
 */
class FLinkMeProjectEditorModule : public IModuleInterface {
public:
  /** IModuleInterface implementation */
  virtual void StartupModule() override;
  virtual void ShutdownModule() override;

private:
  /** Registers the Quick Settings tab spawner */
  void RegisterTabSpawners();

  /** Unregisters the Quick Settings tab spawner */
  void UnregisterTabSpawners();

  /** Spawns the Quick Settings tab */
  TSharedRef<class SDockTab>
  SpawnQuickSettingsTab(const class FSpawnTabArgs &SpawnTabArgs);
};
