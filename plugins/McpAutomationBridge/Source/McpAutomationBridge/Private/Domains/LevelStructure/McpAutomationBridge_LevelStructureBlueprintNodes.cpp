#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureActions.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructureEditorWorld.h"
#include "Domains/LevelStructure/McpAutomationBridge_LevelStructurePayload.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Foundation/BridgeHelpers/McpAutomationBridgeHelpers.h"
#include "Foundation/GraphLayout/McpGraphNodeExtent.h"
#include "McpAutomationBridgeSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

#if WITH_EDITOR
namespace McpLevelStructure
{

bool HandleAddLevelBlueprintNode(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString NodeClass = GetJsonStringField(Payload, TEXT("nodeClass"), TEXT(""));
    FString NodeName = GetJsonStringField(Payload, TEXT("nodeName"), TEXT(""));
    TSharedPtr<FJsonObject> PositionJson = GetObjectField(Payload, TEXT("nodePosition"));
    int32 PosX = PositionJson.IsValid() ? static_cast<int32>(GetJsonNumberField(PositionJson, TEXT("x"))) : 0;
    int32 PosY = PositionJson.IsValid() ? static_cast<int32>(GetJsonNumberField(PositionJson, TEXT("y"))) : 0;
    const TArray<TSharedPtr<FJsonValue>>* PositionArray = nullptr;
    if (Payload->TryGetArrayField(TEXT("position"), PositionArray) && PositionArray && PositionArray->Num() >= 2)
    {
        PosX = static_cast<int32>((*PositionArray)[0]->AsNumber());
        PosY = static_cast<int32>((*PositionArray)[1]->AsNumber());
    }
    // Friendly names such as EventBeginPlay / PrintString (dogfood #160).
    FString AliasEventName;
    FString AliasFunctionName;
    ResolveLevelBlueprintNodeAlias(NodeClass, AliasEventName, AliasFunctionName);
    if (AliasFunctionName.IsEmpty())
    {
        AliasFunctionName = GetJsonStringField(Payload, TEXT("functionName"), TEXT("")); // explicit function for a raw K2Node_CallFunction
    }

    if (NodeClass.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("nodeClass is required"), nullptr);
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    // Honor levelPath: edit the level the caller named, not whatever level is
    // open. A transient /Temp/ level is refused because its blueprint cannot be
    // saved (previously such a call reported success and silently discarded the
    // node — see ResolveTargetLevelForBlueprintRequest).
    FString TargetLevelError;
    ULevel* CurrentLevel = ResolveTargetLevelForBlueprintRequest(World, Payload, TargetLevelError);
    if (!CurrentLevel)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TargetLevelError, nullptr, TEXT("LEVEL_NOT_OPEN"));
        return true;
    }

    // Pass false to allow creation of Level Blueprint if it doesn't exist
    ULevelScriptBlueprint* LevelBP = CurrentLevel->GetLevelScriptBlueprint(false);
    if (!LevelBP)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to get or create Level Blueprint"), nullptr);
        return true;
    }

    UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(LevelBP);
    if (!EventGraph)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Failed to find event graph in Level Blueprint"), nullptr);
        return true;
    }

    // Find the node class - try multiple lookup paths
    FString TriedPaths;
    UClass* NodeClassObj = FindObject<UClass>(nullptr, *NodeClass);
    TriedPaths = NodeClass;

    for (const TCHAR* Prefix : { TEXT("/Script/BlueprintGraph."), TEXT("/Script/Engine."), TEXT("/Script/UnrealEd.") })
    {
        if (NodeClassObj) { break; }
        const FString Candidate = FString(Prefix) + NodeClass;
        NodeClassObj = FindObject<UClass>(nullptr, *Candidate);
        TriedPaths += TEXT(", ") + Candidate;
    }

    FString CreatedNodeName;
    if (NodeClassObj && NodeClassObj->IsChildOf(UK2Node::StaticClass()))
    {
        UK2Node* NewNode = NewObject<UK2Node>(EventGraph, NodeClassObj);
        FString AliasError;
        if (NewNode && !ApplyLevelBlueprintNodeAlias(NewNode, AliasEventName, AliasFunctionName, AliasError))
        {
            Subsystem->SendAutomationResponse(Socket, RequestId, false, AliasError, nullptr, TEXT("NOT_SUPPORTED"));
            return true;
        }
        // An unbound call node compiles as "Could not find a function named None" and breaks the level blueprint.
        if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(NewNode))
        {
            if (!CallNode->GetTargetFunction())
            {
                Subsystem->SendAutomationResponse(Socket, RequestId, false,
                    TEXT("K2Node_CallFunction needs a function: pass the function name as nodeClass (PrintString, Delay, ...) or functionName"),
                    nullptr, TEXT("INVALID_ARGUMENT"));
                return true;
            }
        }
        if (NewNode)
        {
            if (!NodeName.IsEmpty())
            {
                FString SafeNodeName = NodeName.TrimStartAndEnd();
                for (const TCHAR* Bad : { TEXT(" "), TEXT("/"), TEXT("\\"), TEXT(":"), TEXT("."), TEXT("'"), TEXT("\"") }) { SafeNodeName.ReplaceInline(Bad, TEXT("_")); }
                if (!SafeNodeName.IsEmpty())
                {
                    FName UniqueNodeName = MakeUniqueObjectName(EventGraph, NodeClassObj, FName(*SafeNodeName));
                    NewNode->Rename(*UniqueNodeName.ToString(), EventGraph, REN_DontCreateRedirectors | REN_NonTransactional);
                }
            }
            NewNode->CreateNewGuid();
            // ROOT-CAUSE FIX: allocate pins BEFORE PostPlacedNewNode(). Node families
            // such as UK2Node_SpawnActorFromClass read checked pin accessors inside
            // PostPlacedNewNode() (GetScaleMethodPin() => FindPinChecked()), which
            // check()-asserts the editor when the pin does not exist yet. The editor
            // palette survives because its cached template node already carries pins;
            // creating from scratch via NewObject does not, so it crashed here
            // (EdGraphNode.h:586). Allocating first makes the checked accessor safe.
            if (NewNode->Pins.Num() == 0) { NewNode->AllocateDefaultPins(); }
            NewNode->PostPlacedNewNode();
            NewNode->NodePosX = PosX;
            NewNode->NodePosY = PosY;
            // Refuse stacked placements BEFORE the node joins the graph: estimate
            // its extent from the pins just allocated and test it against every
            // occupant. A refusal names each overlapping node with its full
            // coordinates plus two concrete free slots, so the caller can re-run
            // with a position that lands instead of guessing.
            {
                float NewWidth = 0.0f;
                float NewHeight = 0.0f;
                McpGraphLayout::EstimateNodeExtent(*NewNode, NewWidth, NewHeight);
                TArray<McpGraphLayout::FGraphNodeOccupant> Overlapping;
                if (McpGraphLayout::CheckGraphNodeOverlap(
                        EventGraph, static_cast<float>(PosX), static_cast<float>(PosY),
                        NewWidth, NewHeight, Overlapping,
                        McpGraphLayout::NodeOverlapPadding, NewNode))
                {
                    FString OverlapMessage;
                    TSharedPtr<FJsonObject> OverlapDetails =
                        McpGraphLayout::BuildNodeOverlapDetails(
                            static_cast<float>(PosX), static_cast<float>(PosY),
                            NewWidth, NewHeight, Overlapping, OverlapMessage);
                    NewNode->MarkAsGarbage();
                    Subsystem->SendAutomationResponse(Socket, RequestId, false,
                        OverlapMessage, OverlapDetails, TEXT("NODE_OVERLAP"));
                    return true;
                }
            }
            EventGraph->AddNode(NewNode, true, false);
            CreatedNodeName = NewNode->GetName();
        }
    }

    // Check if node creation actually succeeded
    if (CreatedNodeName.IsEmpty())
    {
        FString ErrorMsg;
        if (!NodeClassObj)
        {
            ErrorMsg = FString::Printf(TEXT("Node class not found. Tried paths: [%s]"), *TriedPaths);
        }
        else if (!NodeClassObj->IsChildOf(UK2Node::StaticClass()))
        {
            ErrorMsg = FString::Printf(TEXT("Class '%s' found but is not a K2Node subclass"), *NodeClass);
        }
        else
        {
            ErrorMsg = FString::Printf(TEXT("Failed to create node instance of class: %s"), *NodeClass);
        }
        Subsystem->SendAutomationResponse(Socket, RequestId, false, ErrorMsg, nullptr);
        return true;
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(LevelBP);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, LevelBP);
    ResponseJson->SetStringField(TEXT("nodeClass"), NodeClass);
    ResponseJson->SetStringField(TEXT("nodeName"), CreatedNodeName);
    if (UEdGraphNode* CreatedGraphNode = FindObject<UEdGraphNode>(EventGraph, *CreatedNodeName))
    {
        ResponseJson->SetStringField(TEXT("nodeTitle"), CreatedGraphNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
        ResponseJson->SetStringField(TEXT("nodeId"), CreatedGraphNode->NodeGuid.ToString());
    }
    ResponseJson->SetNumberField(TEXT("posX"), PosX);
    ResponseJson->SetNumberField(TEXT("posY"), PosY);
    ResponseJson->SetBoolField(TEXT("nodeCreated"), true);

    FString Message = FString::Printf(TEXT("Added node to Level Blueprint: %s"), *CreatedNodeName);
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

}
#endif
