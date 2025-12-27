// QuickSettings/SQuickSettingsPanel.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "ShowFlags.h" // For FEngineShowFlags
#include "Widgets/SCompoundWidget.h"

/**
 * Slate panel for quick editor settings access.
 * Provides shortcuts to common development settings like startup map,
 * game mode, and debug toggles.
 */
class SQuickSettingsPanel : public SCompoundWidget {
public:
  SLATE_BEGIN_ARGS(SQuickSettingsPanel) {}
  SLATE_END_ARGS()

  /** Constructs this widget */
  void Construct(const FArguments &InArgs);

private:
  // --- Map Settings ---

  /** Gets all available maps in the project */
  TArray<TSharedPtr<FString>> GetAvailableMaps() const;

  /** Called when startup map selection changes */
  void OnStartupMapChanged(TSharedPtr<FString> NewSelection,
                           ESelectInfo::Type SelectInfo);

  /** Gets the current startup map name */
  FText GetCurrentStartupMapName() const;

  // --- Quick Open Buttons ---

  /** Opens a specific map in the editor */
  FReply OnQuickOpenMapClicked(FString MapPath);

  /** Sets the viewport view mode */
  FReply OnSetViewMode(EViewModeIndex ViewMode);

  /** Sets global time dilation */
  void OnSetTimeDilation(float Value);

  /** Gets current time dilation as text */
  FText GetTimeDilationText() const;

  /** Toggles a show flag (collision, grid, etc) */
  FReply OnToggleShowFlag(FEngineShowFlags::EShowFlag ShowFlag);

  /** Checks if a show flag is enabled */
  bool IsShowFlagEnabled(FEngineShowFlags::EShowFlag ShowFlag) const;

  /** Toggles a stat command (e.g. stat fps) */
  FReply OnToggleStat(FString StatName);

private:
  /** ComboBox for startup map selection */
  TSharedPtr<SComboBox<TSharedPtr<FString>>> StartupMapComboBox;

  /** Cached list of available maps */
  TArray<TSharedPtr<FString>> AvailableMaps;
};
