#pragma once

#include "Components/SlateWrapperTypes.h"
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
// The canonical /Game package path of a Widget Blueprint (/Game/UI/WBP_Menu), which is what every
// widgetPath a reply carries must name. UObject::GetPathName() answers the object path
// (/Game/UI/WBP_Menu.WBP_Menu), so a receipt listed the same asset twice beside assetPath.
FString WidgetBlueprintPackagePath(const UWidgetBlueprint* WidgetBP);
// Loads the widget blueprint, creating an empty UUserWidget-based asset at the path when missing.
// Marks the Widget Blueprint structurally modified and saves it through the safe wrapper, so
// authoring edits survive an editor restart (dogfood c27: widgets added via MCP vanished).
// Returns whether the save landed.
bool MarkWidgetBlueprintModifiedAndSave(UWidgetBlueprint* WidgetBP);
// A widget added, renamed or copied since the last compile has no property on the Widget Blueprint's generated
// class, so a graph step naming it (a VariableGet, an event bind) failed as if it were not a variable. Compiles
// to make the class current, except while Play-In-Editor runs: a compile then reinstances the live widget, which
// vanishes from the viewport. The outcome is not a failure of the edit that came before (a Blueprint with an
// unrelated compile error must not fail a widget add); the return says whether it compiled clean.
bool RefreshWidgetBlueprintClass(UWidgetBlueprint* WidgetBP);

// Case-insensitive lookups shared by the animation and layout handlers; nullptr when absent.
UWidgetAnimation* FindWidgetAnimation(UWidgetBlueprint* WidgetBP, const FString& AnimationName);
UWidget* FindWidgetByName(UWidgetTree* Tree, const FString& WidgetName);

// widgetPath + animationName from the payload -> the blueprint and its animation. On any miss the
// refusal (MISSING_PARAMETER / NOT_FOUND / ANIMATION_NOT_FOUND) is already sent and null returned.
UWidgetAnimation* ResolveWidgetAnimation(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId,
                                         TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload,
                                         UWidgetBlueprint*& OutWidgetBP);

// widgetPath + slotName -> the widget. On any miss the refusal (MISSING_PARAMETER, NOT_FOUND,
// WIDGET_NOT_FOUND) is already sent and null returned.
UWidget* ResolveWidgetTarget(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId,
                             TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload,
                             UWidgetBlueprint*& OutWidgetBP);

// What the widget and its slot hold now: visibility, render transform, and the slot's
// canvas geometry or box padding and alignment.
TSharedPtr<FJsonObject> McpDescribeWidgetLayout(const UWidget* Widget);
const TCHAR* McpVisibilityName(ESlateVisibility Visibility);

// Saves, then replies success with the layout read back under "applied" and "saved".
void ReplyWidgetLayout(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                       TSharedPtr<FJsonObject> ResultJson, UWidgetBlueprint* WidgetBP, UWidget* Widget, const FString& Message);
}
