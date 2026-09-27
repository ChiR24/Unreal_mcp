#pragma once

#include "Core/Compatibility/McpVersionCompatibility.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Dom/JsonObject.h"
#include "McpAutomationBridgeSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Factories/BlueprintFactory.h"
#include "GameFramework/Actor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Interface.h"

namespace McpInteractionHandlers
{
bool HandleInteractionComponentAuthoringAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);


bool HandleInteractableInterfaceAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleDoorAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleSwitchAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleChestAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleLeverAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);


bool HandleTriggerAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

bool HandleInteractionInfoAction(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const FString& SubAction,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);

// A member variable an interactable carries; a null Value only adds the variable.
enum class EInteractionVarType : uint8 { Bool, Float, Name, SoftObject };
struct FInteractionVar
{
    const TCHAR* Name;
    EInteractionVarType Type;
    TSharedPtr<FJsonValue> Value;
};

// Adds each missing variable, compiles (the members exist on GeneratedClass only after that, so an earlier
// CDO write resolved nothing), then writes each non-null Value to the class default object. Returns the
// number of writes that failed.
int32 ApplyInteractionVars(UBlueprint* Blueprint, std::initializer_list<FInteractionVar> Vars);

// One SCS node of an interactable. Parent names an earlier node (null: under the first node, the root).
// A shape node with TriggerSize > 0 becomes an overlap-all volume: sphere radius, box half-extent, or
// capsule radius with twice that as half-height.
struct FInteractionNode
{
    UClass* Class;
    const TCHAR* Name;
    const TCHAR* Parent;
    float TriggerSize;
};

// Makes Template (a sphere, box or capsule) an overlap-all volume of Size; nothing for Size <= 0.
void ConfigureInteractionShape(UObject* Template, float Size);

// A new Actor blueprint from the payload's name and folder (DefaultFolder when absent), path-validated,
// with Nodes as its SCS tree. Replies and returns null on failure, including an existing asset.
UBlueprint* CreateInteractableBlueprint(
    UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* DefaultFolder, const TCHAR* Noun, std::initializer_list<FInteractionNode> Nodes);

// The payload's PathField blueprint when its SCS carries every RequiredNodes name (so a configure_* call
// never authors door variables onto a chest); replies and returns null otherwise.
UBlueprint* LoadInteractableBlueprint(
    UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket, const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* PathField, const TCHAR* Noun, std::initializer_list<const TCHAR*> RequiredNodes);

// Marks Blueprint structurally modified, saves it, adds verification and mutation evidence (Changes,
// plus "saved" only when the save succeeded) to Result and replies success with Message.
void SendInteractableResult(
    UMcpAutomationBridgeSubsystem* Subsystem, const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> Socket, UBlueprint* Blueprint,
    TSharedPtr<FJsonObject> Result, TArray<FString> Changes, const FString& Message);
}
