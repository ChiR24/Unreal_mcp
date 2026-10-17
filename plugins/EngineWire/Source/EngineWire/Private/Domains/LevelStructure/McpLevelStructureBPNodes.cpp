#include "Domains/LevelStructure/McpLevelStructureActions.h"
#include "Domains/LevelStructure/McpLevelStructureEditorWorld.h"

#include "Domains/BlueprintGraph/McpBPGraphPrivate.h"
#include "Foundation/BridgeHelpers/Blueprints/McpBPActorBinding.h"
#include "K2Node_ActorBoundEvent.h"
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
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/GraphLayout/McpGraphNodeExtent.h"
#include "EngineWireSubsystem.h"
#include "Transport/WebSocket/McpBridgeWebSocket.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"

namespace McpLevelStructure
{

bool HandleAddLevelBlueprintNode(
    UEngineWireSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString NodeClass = GetJsonStringField(Payload, TEXT("nodeClass"), TEXT(""));
    FString NodeName = GetJsonStringField(Payload, TEXT("nodeName"), TEXT(""));
    const TSharedPtr<FJsonObject>* PositionJson = nullptr;
    Payload->TryGetObjectField(TEXT("nodePosition"), PositionJson);
    int32 PosX = PositionJson ? static_cast<int32>(GetJsonNumberField(*PositionJson, TEXT("x"))) : 0;
    int32 PosY = PositionJson ? static_cast<int32>(GetJsonNumberField(*PositionJson, TEXT("y"))) : 0;
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

    // Honor levelPath: edit the level the caller named, not whatever level is
    // open. A transient /Temp/ level is refused because its blueprint cannot be
    // saved (previously such a call reported success and silently discarded the
    // node — see ResolveTargetLevelForBlueprintRequest). Adding the first node
    // creates the level Blueprint.
    ULevel* CurrentLevel = nullptr;
    ULevelScriptBlueprint* LevelBP = nullptr;
    UEdGraph* EventGraph = nullptr;
    if (!ResolveLevelBlueprintGraph(Subsystem, RequestId, Socket, Payload, /*bCreate=*/true, CurrentLevel, LevelBP, EventGraph))
    {
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
    // The catalog create_node uses also takes a name without its K2Node_ prefix (ActorBoundEvent) or a friendly one
    // (Literal), and refuses a node this graph cannot hold (an animation node) instead of placing it.
    if (!NodeClassObj)
    {
        NodeClassObj = McpBlueprintGraphHandlers::FindNodeClassByName(NodeClass);
        TriedPaths += TEXT(", the node catalog");
    }
    // Only for a graph node: it reads the class default as one, which check()s on any other class (nodeClass "Actor").
    const FString Mismatch = NodeClassObj && NodeClassObj->IsChildOf(UK2Node::StaticClass())
        ? McpBlueprintGraphHandlers::DescribeGraphMismatch(NodeClassObj, EventGraph) : FString();
    if (!Mismatch.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false, Mismatch, nullptr, TEXT("NODE_NOT_ALLOWED_IN_GRAPH"));
        return true;
    }

    FString CreatedNodeName;
    bool bStructural = false;
    if (NodeClassObj && NodeClassObj->IsChildOf(UK2Node::StaticClass()))
    {
        UK2Node* NewNode = NewObject<UK2Node>(EventGraph, NodeClassObj);
        FString AliasError;
        FString AliasCode;
        const FString MemberClass = GetJsonStringField(Payload, TEXT("memberClass"), TEXT(""));
        // Every refusal below drops the node it made, so no orphan stays in the graph to take a later node's name.
        if (NewNode && !ApplyLevelBlueprintNodeAlias(NewNode, AliasEventName, AliasFunctionName, MemberClass, AliasError, AliasCode))
        {
            NewNode->MarkAsGarbage();
            Subsystem->SendAutomationResponse(Socket, RequestId, false, AliasError, nullptr, AliasCode);
            return true;
        }
        // An unbound call node compiles as "Could not find a function named None" and breaks the level blueprint.
        if (UK2Node_CallFunction* CallNode = Cast<UK2Node_CallFunction>(NewNode))
        {
            if (!CallNode->GetTargetFunction())
            {
                NewNode->MarkAsGarbage();
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
            // After the rename: a bound event's function name carries the node name. Before the pins: they come
            // from the actor's class, the event's signature or the variable's property. eventName binds nothing
            // else here: BeginPlay, Tick and the other events are nodeClass aliases, and an ignored eventName left
            // an unbound event node.
            const FString EventName = GetJsonStringField(Payload, TEXT("eventName"), TEXT(""));
            FString BindError = TEXT("eventName applies to a K2Node_ActorBoundEvent only; pass BeginPlay, Tick and the "
                                     "other events as nodeClass (EventBeginPlay, EventTick).");
            FString BindCode = TEXT("INVALID_ARGUMENT");
            if ((!EventName.IsEmpty() && !NewNode->IsA<UK2Node_ActorBoundEvent>()) ||
                !McpBindLevelActorNode(NewNode, CurrentLevel, GetJsonStringField(Payload, TEXT("actorName"), TEXT("")),
                                       EventName, BindError, BindCode) ||
                !BindLevelBlueprintVariableNode(NewNode, LevelBP, GetJsonStringField(Payload, TEXT("memberName"), TEXT("")),
                                                MemberClass, BindError, BindCode))
            {
                NewNode->MarkAsGarbage();
                Subsystem->SendAutomationResponse(Socket, RequestId, false, BindError, nullptr, BindCode);
                return true;
            }
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
                            NewWidth, NewHeight, Overlapping, OverlapMessage, EventGraph, NewNode);
                    NewNode->MarkAsGarbage();
                    Subsystem->SendAutomationResponse(Socket, RequestId, false,
                        OverlapMessage, OverlapDetails, TEXT("NODE_OVERLAP"));
                    return true;
                }
            }
            EventGraph->AddNode(NewNode, true, false);
            CreatedNodeName = NewNode->GetName();
            bStructural = NewNode->NodeCausesStructuralBlueprintChange();
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

    // An event node adds a function to the class, as the editor's own placement marks it; a plain mark left the
    // skeleton class without it until something else recompiled the level blueprint.
    if (bStructural)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(LevelBP);
    }
    else
    {
        FBlueprintEditorUtils::MarkBlueprintAsModified(LevelBP);
    }

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, LevelBP);
    ResponseJson->SetStringField(TEXT("nodeClass"), NodeClass);
    ResponseJson->SetStringField(TEXT("nodeName"), CreatedNodeName);
    if (UEdGraphNode* CreatedGraphNode = FindObject<UEdGraphNode>(EventGraph, *CreatedNodeName))
    {
        ResponseJson->SetStringField(TEXT("nodeTitle"), CreatedGraphNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
        ResponseJson->SetStringField(TEXT("nodeId"), CreatedGraphNode->NodeGuid.ToString());
        // The names connect_level_blueprint_nodes takes (a bound event's OtherActor, an actor reference's output), in
        // the shape blueprint.edit_graph answers with.
        ResponseJson->SetStringField(TEXT("pins"), McpDescribeNodePins(CreatedGraphNode));
    }
    ResponseJson->SetNumberField(TEXT("posX"), PosX);
    ResponseJson->SetNumberField(TEXT("posY"), PosY);
    ResponseJson->SetBoolField(TEXT("nodeCreated"), true);

    FString Message = FString::Printf(TEXT("Added node to Level Blueprint: %s"), *CreatedNodeName);
    SendLevelEditResult(Subsystem, RequestId, Socket, Payload, CurrentLevel, Message, ResponseJson);
    return true;
}

}
