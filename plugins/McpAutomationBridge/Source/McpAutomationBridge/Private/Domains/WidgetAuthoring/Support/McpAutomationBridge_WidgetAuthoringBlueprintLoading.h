#pragma once

#include "CoreMinimal.h"

class UWidget;
class UWidgetAnimation;
class UWidgetBlueprint;
class UWidgetTree;

namespace WidgetAuthoringHelpers
{
UWidgetBlueprint* LoadWidgetBlueprint(const FString& WidgetPath);
// Loads the widget blueprint, creating an empty UUserWidget-based asset at the path when missing.
UWidgetBlueprint* LoadOrCreateWidgetBlueprint(const FString& WidgetPath, bool* bOutCreated = nullptr);
// Marks the Widget Blueprint structurally modified and saves it through the safe wrapper, so
// authoring edits survive an editor restart (dogfood c27: widgets added via MCP vanished).
void MarkWidgetBlueprintModifiedAndSave(UWidgetBlueprint* WidgetBP);

// Case-insensitive lookups shared by the animation and layout handlers; nullptr when absent.
UWidgetAnimation* FindWidgetAnimation(UWidgetBlueprint* WidgetBP, const FString& AnimationName);
UWidget* FindWidgetByName(UWidgetTree* Tree, const FString& WidgetName);
}
