// QuickSettings/SQuickSettingsPanel.cpp
// Simplified version - Debug Tools removed for stability

#include "SQuickSettingsPanel.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorStyleSet.h"
#include "FileHelpers.h"
#include "GameMapsSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SQuickSettingsPanel"

void SQuickSettingsPanel::Construct(const FArguments &InArgs) {
  AvailableMaps = GetAvailableMaps();

  TSharedRef<SVerticalBox> MainContent = SNew(SVerticalBox);

  // Header
  MainContent->AddSlot().AutoHeight().Padding(
      0, 0, 0, 8)[SNew(STextBlock)
                      .Text(LOCTEXT("PanelTitle", "Quick Settings"))
                      .TextStyle(FAppStyle::Get(), "LargeText")];

  MainContent->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SSeparator)];

  // Startup Map Section
  MainContent->AddSlot().AutoHeight().Padding(
      0, 0, 0, 4)[SNew(STextBlock)
                      .Text(LOCTEXT("StartupMapLabel", "Editor Startup Map"))
                      .TextStyle(FAppStyle::Get(), "NormalText")];

  MainContent->AddSlot().AutoHeight().Padding(0, 0, 0, 16)
      [SAssignNew(StartupMapComboBox, SComboBox<TSharedPtr<FString>>)
           .OptionsSource(&AvailableMaps)
           .OnSelectionChanged(this, &SQuickSettingsPanel::OnStartupMapChanged)
           .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) {
             return SNew(STextBlock).Text(FText::FromString(*Item));
           })[SNew(STextBlock)
                  .Text(this, &SQuickSettingsPanel::GetCurrentStartupMapName)]];

  MainContent->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SSeparator)];

  // Quick Open Section
  MainContent->AddSlot().AutoHeight().Padding(
      0, 0, 0, 4)[SNew(STextBlock)
                      .Text(LOCTEXT("QuickOpenLabel", "Quick Open Map"))
                      .TextStyle(FAppStyle::Get(), "NormalText")];

  // Quick Open Buttons
  {
    TSharedRef<SHorizontalBox> QuickOpenRow = SNew(SHorizontalBox);

    QuickOpenRow->AddSlot().AutoWidth().Padding(
        0, 0, 4, 0)[SNew(SButton)
                        .Text(LOCTEXT("MainLevel", "Main"))
                        .OnClicked_Lambda([this]() {
                          return OnQuickOpenMapClicked(
                              FString("/Game/Maps/LEVELS/Lvl_ThirdPerson"));
                        })];

    QuickOpenRow->AddSlot().AutoWidth().Padding(
        0, 0, 4, 0)[SNew(SButton)
                        .Text(LOCTEXT("ForestLevel", "Forest"))
                        .OnClicked_Lambda([this]() {
                          return OnQuickOpenMapClicked(
                              FString("/Game/Maps/Prototypes/L_Proto_Forest"));
                        })];

    QuickOpenRow->AddSlot()
        .AutoWidth()[SNew(SButton)
                         .Text(LOCTEXT("AnimLevel", "Anim"))
                         .OnClicked_Lambda([this]() {
                           return OnQuickOpenMapClicked(
                               FString("/Game/Maps/AnimMap/Level_AnimMap"));
                         })];

    MainContent->AddSlot().AutoHeight().Padding(0, 0, 0, 4)[QuickOpenRow];
  }

  // Final Assignment
  ChildSlot[SNew(SScrollBox) + SScrollBox::Slot().Padding(8.0f)[MainContent]];
}

TArray<TSharedPtr<FString>> SQuickSettingsPanel::GetAvailableMaps() const {
  TArray<TSharedPtr<FString>> Maps;

  FAssetRegistryModule &AssetRegistryModule =
      FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
  TArray<FAssetData> AssetDataList;
  AssetRegistryModule.Get().GetAssetsByClass(
      FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("World")), AssetDataList);

  for (const FAssetData &AssetData : AssetDataList) {
    FString PackageName = AssetData.PackageName.ToString();
    if (!PackageName.StartsWith(TEXT("/Engine"))) {
      Maps.Add(MakeShared<FString>(PackageName));
    }
  }

  Maps.Sort([](const TSharedPtr<FString> &A, const TSharedPtr<FString> &B) {
    return *A < *B;
  });

  return Maps;
}

void SQuickSettingsPanel::OnStartupMapChanged(TSharedPtr<FString> NewSelection,
                                              ESelectInfo::Type SelectInfo) {
  if (!NewSelection.IsValid())
    return;

  UGameMapsSettings *GameMapsSettings = GetMutableDefault<UGameMapsSettings>();
  if (GameMapsSettings) {
    GameMapsSettings->EditorStartupMap = FSoftObjectPath(*NewSelection);
    GameMapsSettings->SaveConfig();

    GConfig->SetString(TEXT("/Script/EngineSettings.GameMapsSettings"),
                       TEXT("EditorStartupMap"), **NewSelection, GEngineIni);
    GConfig->Flush(false, GEngineIni);

    UE_LOG(LogTemp, Log, TEXT("QuickSettings: Set Editor Startup Map to %s"),
           **NewSelection);
  }
}

FText SQuickSettingsPanel::GetCurrentStartupMapName() const {
  FString CurrentMap;
  GConfig->GetString(TEXT("/Script/EngineSettings.GameMapsSettings"),
                     TEXT("EditorStartupMap"), CurrentMap, GEngineIni);

  if (CurrentMap.IsEmpty()) {
    return LOCTEXT("NoMapSelected", "(None)");
  }

  return FText::FromString(CurrentMap);
}

FReply SQuickSettingsPanel::OnQuickOpenMapClicked(FString MapPath) {
  FEditorFileUtils::LoadMap(MapPath);
  return FReply::Handled();
}

// Stub implementations for Debug Tools (disabled for now)
void SQuickSettingsPanel::OnSetTimeDilation(float Value) {}
FText SQuickSettingsPanel::GetTimeDilationText() const {
  return FText::GetEmpty();
}
FReply
SQuickSettingsPanel::OnToggleShowFlag(FEngineShowFlags::EShowFlag ShowFlag) {
  return FReply::Handled();
}
bool SQuickSettingsPanel::IsShowFlagEnabled(
    FEngineShowFlags::EShowFlag ShowFlag) const {
  return false;
}
FReply SQuickSettingsPanel::OnToggleStat(FString StatName) {
  return FReply::Handled();
}
FReply SQuickSettingsPanel::OnSetViewMode(EViewModeIndex ViewMode) {
  return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
