#pragma once

#include "Components/SlateWrapperTypes.h"
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UEngineWireSubsystem;
class UPanelSlot;
struct FMargin;

class UUserWidget;
class UWidget;
class UWidgetAnimation;
class UWidgetBlueprint;
class UWidgetTree;

namespace WidgetAuthoringHelpers
{
UWidgetBlueprint* LoadWidgetBlueprint(const FString& WidgetPath);
// The FMargin Padding a slot declares (fifteen UMG slot classes do), by reflection; null when its class has none.
FMargin* FindSlotPadding(UPanelSlot* Slot);
// LoadWidgetBlueprint, or null after refusing "Widget blueprint not found" (NOT_FOUND).
UWidgetBlueprint* LoadWidgetBlueprintOrReply(UEngineWireSubsystem& Subsystem, const FString& RequestId,
                                             const TSharedPtr<FMcpBridgeWebSocket>& Socket, const FString& WidgetPath);
// Draws WidgetBP offscreen the way the game would at Size (the project's DPI curve applied, Scale), with the designer
// flags of its thumbnail: Construct graphs do not run, and each widget keeps the visibility it is saved with.
// OutPixels are the raw pixels (alpha 0 where nothing drew); Inspect sees the laid-out widget before it is dropped.
bool McpDrawWidget(UWidgetBlueprint* WidgetBP, FIntPoint Size, TArray<FColor>& OutPixels, FString& OutError,
                   TFunctionRef<void(UUserWidget& Widget, float Scale)> Inspect);
// A pixel of that picture over the preview's dark backdrop (24, 24, 28), by its alpha: white text on nothing still reads.
inline FColor McpOverPreviewBackdrop(const FColor& Pixel)
{
    const float Alpha = Pixel.A / 255.f;
    return FColor(FMath::RoundToInt(Pixel.R * Alpha + 24 * (1.f - Alpha)), FMath::RoundToInt(Pixel.G * Alpha + 24 * (1.f - Alpha)),
                  FMath::RoundToInt(Pixel.B * Alpha + 28 * (1.f - Alpha)), 255);
}
// The canonical /Game package path of a Widget Blueprint (/Game/UI/WBP_Menu), which is what every
// widgetPath a reply carries must name. UObject::GetPathName() answers the object path
// (/Game/UI/WBP_Menu.WBP_Menu), so a receipt listed the same asset twice beside assetPath.
FString WidgetBlueprintPackagePath(const UWidgetBlueprint* WidgetBP);
// Marks the Widget Blueprint structurally modified and saves it through the safe wrapper, so
// authoring edits survive an editor restart (widgets added via MCP vanished).
// Returns whether the save landed.
bool MarkWidgetBlueprintModifiedAndSave(UWidgetBlueprint* WidgetBP);
// A widget added, renamed or copied since the last compile has no property on the Widget Blueprint's generated
// class, so a graph step naming it (a VariableGet, an event bind) failed as if it were not a variable. Compiles
// to make the class current, except while Play-In-Editor runs: a compile then reinstances the live widget, which
// vanishes from the viewport. The outcome is not a failure of the edit that came before (a Blueprint with an
// unrelated compile error must not fail a widget add); the return says whether it compiled clean.
bool RefreshWidgetBlueprintClass(UWidgetBlueprint* WidgetBP);
// The end of an edit of a Widget Blueprint its caller marked structurally modified: compiled first
// (RefreshWidgetBlueprintClass), so the saved asset holds the generated class with the edit, then saved once; Result
// answers saved. Saved first, the file held the class from before the edit. Returns whether the save landed.
bool CompileAndSaveWidgetBlueprint(UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject>& Result);

// The animation of that name, case ignored; nullptr when absent. A widget by name is UWidgetTree::FindWidget(FName).
UWidgetAnimation* FindWidgetAnimation(UWidgetBlueprint* WidgetBP, const FString& AnimationName);

// widgetPath + animationName from the payload -> the blueprint and its animation. On any miss the
// refusal (MISSING_PARAMETER / NOT_FOUND / ANIMATION_NOT_FOUND) is already sent and null returned.
UWidgetAnimation* ResolveWidgetAnimation(UEngineWireSubsystem& Subsystem, const FString& RequestId,
                                         TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload,
                                         UWidgetBlueprint*& OutWidgetBP);

// widgetPath + slotName -> the widget. On any miss the refusal (MISSING_PARAMETER, NOT_FOUND,
// WIDGET_NOT_FOUND) is already sent and null returned.
UWidget* ResolveWidgetTarget(UEngineWireSubsystem& Subsystem, const FString& RequestId,
                             TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload,
                             UWidgetBlueprint*& OutWidgetBP);

// What the widget and its slot hold now: visibility, render transform, and the slot's
// canvas geometry or box padding and alignment.
TSharedPtr<FJsonObject> McpDescribeWidgetLayout(const UWidget* Widget);
const TCHAR* McpVisibilityName(ESlateVisibility Visibility);

// Saves, then replies success with the layout read back under "applied" and "saved".
void ReplyWidgetLayout(UEngineWireSubsystem& Subsystem, const FString& RequestId, TSharedPtr<FMcpBridgeWebSocket> Socket,
                       TSharedPtr<FJsonObject> ResultJson, UWidgetBlueprint* WidgetBP, UWidget* Widget, const FString& Message);
}
