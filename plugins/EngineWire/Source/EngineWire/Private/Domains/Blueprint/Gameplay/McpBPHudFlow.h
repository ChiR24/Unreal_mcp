#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Domains/WidgetAuthoring/McpWidgetActions.h"

class UBlueprint;

// The flow the screen behaviours share (health_hud, counter_hud, game_over and hint in McpBPAddHud.cpp,
// McpBPAddGameOver.cpp and McpBPAddHint.cpp): the Blueprint loaded and checked, its widget made or loaded and given the element the recipe writes, the recipe
// authored, and one reply naming the widget.
namespace McpBlueprintHandlers {
struct FBlueprintActionContext;

// One screen behaviour: its recipe (Resources/Recipes/Gameplay) and values, the widget made next to the Blueprint under
// UI/ when no widgetPath is given, the element the pawn writes, and the widget step that adds that element to a widget
// without it. An element the caller named must be there already (bElementNamed).
struct FHud {
  const TCHAR *Recipe = nullptr;
  FString WidgetName;
  FString Element;
  const UClass *ElementClass = nullptr;
  bool bElementNamed = false;
  WidgetAuthoringHandlers::FWidgetAuthoringActionHandler AddHandler = nullptr;
  const TCHAR *AddSubAction = nullptr;
  TSharedPtr<FJsonObject> AddPayload = MakeShared<FJsonObject>();
  TMap<FString, TSharedPtr<FJsonValue>> Values; // WidgetClass and WidgetType are filled in by AuthorHud
};

// The Blueprint at blueprintPath, compiled when dirty, as a Base class (null: a pawn, whose player sees the screen);
// else the refusal is sent and null returned. What names the behaviour in that refusal ("A health bar").
UBlueprint *LoadScreenBlueprint(const FBlueprintActionContext &Context, const TCHAR *What, FString &OutPath,
                                const UClass *Base = nullptr);

// Runs the shared flow once the caller's own checks passed and sends the reply. Message is the applied reply's text,
// where {widget} stands for the widget's path.
void AuthorHud(const FBlueprintActionContext &Context, UBlueprint *Blueprint, const FString &BlueprintPath, FHud &Hud,
               const TCHAR *Message);

inline void SetHudString(FHud &Hud, const TCHAR *Name, const FString &Value) {
  Hud.Values.Add(Name, MakeShared<FJsonValueString>(Value));
}
} // namespace McpBlueprintHandlers
