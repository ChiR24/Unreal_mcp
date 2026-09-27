#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;

class UWidget;
class UWidgetAnimation;
class UWidgetBlueprint;
class UWidgetTree;

namespace WidgetAuthoringHelpers
{
UWidgetBlueprint* LoadWidgetBlueprint(const FString& WidgetPath);
// Loads the widget blueprint, creating an empty UUserWidget-based asset at the path when missing.
// Marks the Widget Blueprint structurally modified and saves it through the safe wrapper, so
// authoring edits survive an editor restart (dogfood c27: widgets added via MCP vanished).
void MarkWidgetBlueprintModifiedAndSave(UWidgetBlueprint* WidgetBP);

// Case-insensitive lookups shared by the animation and layout handlers; nullptr when absent.
UWidgetAnimation* FindWidgetAnimation(UWidgetBlueprint* WidgetBP, const FString& AnimationName);
UWidget* FindWidgetByName(UWidgetTree* Tree, const FString& WidgetName);

// widgetPath + animationName from the payload -> the blueprint and its animation. On any miss the
// refusal (MISSING_PARAMETER / NOT_FOUND / ANIMATION_NOT_FOUND) is already sent and null returned.
UWidgetAnimation* ResolveWidgetAnimation(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId,
                                         TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload,
                                         UWidgetBlueprint*& OutWidgetBP);
}
