#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FMcpBridgeWebSocket;
class UMcpAutomationBridgeSubsystem;
class UTexture2D;
class UWidget;
class UWidgetAnimation;
class UWidgetBlueprint;

// A widget subtree written as data, so the HUD elements and screen templates are
// layouts rather than 60-line ConstructWidget ladders. A node is
//   {type, name, children[], slot{}, ...widget props}
// type:     UMG class short name (TextBlock, Border, HorizontalBox, ...).
// name:     widget name; "{slot}" becomes the caller's slotName.
// props:    text, fontSize, color [r,g,b,a], justify (left|center|right), autoWrap,
//           percent, visibility, opacity, padding (number or [l,t,r,b]), radius,
//           imageSize [w,h], value, checked, width, height, options [strings].
// slot:     canvas: anchors [4], alignment [2], position [2], size [2], offsets [4],
//           autoSize, z; boxes: padding, hAlign, vAlign, fill; grids: row, column.
namespace WidgetAuthoringHelpers
{
TSharedPtr<FJsonObject> McpParseWidgetSpec(const TCHAR* Json);

// The first node (depth-first) whose name, after "{slot}" substitution, equals Name.
TSharedPtr<FJsonObject> McpFindSpecNode(const TSharedPtr<FJsonObject>& Spec, const FString& Name);

// Rewrites "{slot}" in every name of the spec, for composing several pieces into one tree.
void McpBindSpecSlot(const TSharedPtr<FJsonObject>& Spec, const FString& SlotName);

// Every name the spec would create; used to refuse a collision before anything is built.
void McpCollectSpecNames(const TSharedPtr<FJsonObject>& Spec, const FString& SlotName, TArray<FString>& OutNames);

// Builds the subtree with its root unparented. Every widget constructed is appended to
// OutCreated so a failure can be rolled back whole. Null with OutError on a bad node.
UWidget* McpBuildWidgetSpec(UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject>& Spec,
                            const FString& SlotName, TArray<UWidget*>& OutCreated, FString& OutError);

// Applies a node's "slot" object to a widget that is already seated in a panel.
void McpApplySpecSlot(UWidget* Widget, const TSharedPtr<FJsonObject>& SlotSpec);

// Takes everything McpBuildWidgetSpec created back out of the tree and frees the names.
void McpRollbackWidgetSpec(UWidgetBlueprint* WidgetBP, const TArray<UWidget*>& Created);

// Seats a built spec under ParentSlot (the root panel when empty; a canvas root is made
// for an empty tree), applies its default placement, then the caller's
// positionX/positionY/sizeX/sizeY. Sends the refusal itself and returns false on failure.
bool McpAddSpecToWidget(UMcpAutomationBridgeSubsystem& Subsystem, const FString& RequestId,
                        TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload,
                        UWidgetBlueprint* WidgetBP, const TSharedPtr<FJsonObject>& Spec,
                        const FString& SlotName, TArray<UWidget*>& OutCreated);

// name + path (folder, default /Game/UI) -> a new, empty Widget Blueprint (UserWidget
// when ParentClass is null). Refuses an existing asset rather than rebuilding someone's
// authored tree. Sends the refusal itself and returns null.
UWidgetBlueprint* McpCreateTemplateWidgetBlueprint(UMcpAutomationBridgeSubsystem& Subsystem,
                                                   const FString& RequestId,
                                                   TSharedPtr<FMcpBridgeWebSocket> Socket,
                                                   const TSharedPtr<FJsonObject>& Payload,
                                                   const TCHAR* DefaultName, UClass* ParentClass = nullptr);

// add_<kind> HUD pieces: the spec with the payload's knobs written in, its default slot
// name and label. False for an action that is not a HUD piece; OutError names a bad knob.
bool McpResolveHudElement(const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FJsonObject>& OutSpec,
                          FString& OutDefaultSlot, FString& OutLabel, FString& OutError);

// Post-build extras of a HUD piece (the damage indicator's flash animation): its name, or empty.
FString McpFinishHudElement(UWidgetBlueprint* WidgetBP, const FString& Action, const FString& SlotName,
                            const TSharedPtr<FJsonObject>& Payload);

// A texture a spec's "texture" prop may name; null when the path is empty or does not load.
UTexture2D* McpLoadSpecTexture(const FString& TexturePath);

// Adds (or replaces the keys of) a RenderOpacity animation on Target: Keys are {time, opacity}.
// Returns the animation, or null when the name is taken by something else.
UWidgetAnimation* McpAddOpacityAnimation(UWidgetBlueprint* WidgetBP, const FString& AnimationName, UWidget* Target,
                                         const TArray<FVector2D>& Keys);

// Reads an optional [r,g,b,a] or {r,g,b,a} payload color into a spec array.
void McpCopyPayloadColor(const TSharedPtr<FJsonObject>& Payload, const TCHAR* Field,
                         const TSharedPtr<FJsonObject>& Node, const TCHAR* SpecField = TEXT("color"));
}
