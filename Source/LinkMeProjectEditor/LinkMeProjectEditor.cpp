// LinkMeProjectEditor.cpp

#include "LinkMeProjectEditor.h"
#include "Framework/Docking/TabManager.h"
#include "LevelEditor.h"
#include "QuickSettings/SQuickSettingsPanel.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "FLinkMeProjectEditorModule"

// Tab identifier
static const FName QuickSettingsTabName("LinkMeQuickSettings");

void FLinkMeProjectEditorModule::StartupModule() {
  // Register tab spawners after Level Editor is loaded
  FLevelEditorModule &LevelEditorModule =
      FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

  RegisterTabSpawners();

  UE_LOG(LogTemp, Log,
         TEXT("LinkMeProjectEditor: Module loaded successfully."));
}

void FLinkMeProjectEditorModule::ShutdownModule() {
  UnregisterTabSpawners();

  UE_LOG(LogTemp, Log, TEXT("LinkMeProjectEditor: Module unloaded."));
}

void FLinkMeProjectEditorModule::RegisterTabSpawners() {
  FGlobalTabmanager::Get()
      ->RegisterNomadTabSpawner(
          QuickSettingsTabName,
          FOnSpawnTab::CreateRaw(
              this, &FLinkMeProjectEditorModule::SpawnQuickSettingsTab))
      .SetDisplayName(LOCTEXT("QuickSettingsTabTitle", "Quick Settings"))
      .SetMenuType(ETabSpawnerMenuType::Enabled);
}

void FLinkMeProjectEditorModule::UnregisterTabSpawners() {
  FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(QuickSettingsTabName);
}

TSharedRef<SDockTab> FLinkMeProjectEditorModule::SpawnQuickSettingsTab(
    const FSpawnTabArgs &SpawnTabArgs) {
  return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SQuickSettingsPanel)];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FLinkMeProjectEditorModule, LinkMeProjectEditor)
