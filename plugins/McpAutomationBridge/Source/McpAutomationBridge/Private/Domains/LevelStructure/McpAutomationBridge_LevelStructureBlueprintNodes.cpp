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

    ULevel* CurrentLevel = World->GetCurrentLevel();
    if (!CurrentLevel)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No current level available"), nullptr);
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
            NewNode->PostPlacedNewNode();
            // Guard against duplicate pins: some node types already allocate in
            // PostPlacedNewNode(), so only allocate when the node has no pins yet.
            if (NewNode->Pins.Num() == 0) { NewNode->AllocateDefaultPins(); }
            NewNode->NodePosX = PosX;
            NewNode->NodePosY = PosY;
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

bool HandleConnectLevelBlueprintNodes(
    UMcpAutomationBridgeSubsystem* Subsystem,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> Socket)
{
    using namespace LevelStructureHelpers;

    FString SourceNodeName = GetJsonStringField(Payload, TEXT("sourceNodeName"), TEXT(""));
    FString SourcePinName = GetJsonStringField(Payload, TEXT("sourcePinName"), TEXT(""));
    FString TargetNodeName = GetJsonStringField(Payload, TEXT("targetNodeName"), TEXT(""));
    FString TargetPinName = GetJsonStringField(Payload, TEXT("targetPinName"), TEXT(""));

    if (SourceNodeName.IsEmpty() || TargetNodeName.IsEmpty())
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("sourceNodeName and targetNodeName are required"), nullptr);
        return true;
    }

    UWorld* World = GetEditorWorld();
    if (!World)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("No editor world available"), nullptr);
        return true;
    }

    ULevel* CurrentLevel = World->GetCurrentLevel();
    ULevelScriptBlueprint* LevelBP = CurrentLevel ? CurrentLevel->GetLevelScriptBlueprint(false) : nullptr;
    if (!LevelBP)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Level Blueprint not available"), nullptr);
        return true;
    }

    UEdGraph* EventGraph = FBlueprintEditorUtils::FindEventGraph(LevelBP);
    if (!EventGraph)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            TEXT("Event graph not found"), nullptr);
        return true;
    }

    // Find source and target nodes
    UEdGraphNode* SourceNode = nullptr;
    UEdGraphNode* TargetNode = nullptr;

    for (UEdGraphNode* Node : EventGraph->Nodes)
    {
        FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
        if (NodeTitle.Contains(SourceNodeName) || Node->GetName().Contains(SourceNodeName))
        {
            SourceNode = Node;
        }
        if (NodeTitle.Contains(TargetNodeName) || Node->GetName().Contains(TargetNodeName))
        {
            TargetNode = Node;
        }
    }

    if (!SourceNode || !TargetNode)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Could not find nodes: source='%s' target='%s'"),
                *SourceNodeName, *TargetNodeName), nullptr);
        return true;
    }

    // Find pins and connect
    UEdGraphPin* SourcePin = nullptr;
    UEdGraphPin* TargetPin = nullptr;

    for (UEdGraphPin* Pin : SourceNode->Pins)
    {
        if (Pin->PinName.ToString() == SourcePinName || Pin->GetDisplayName().ToString() == SourcePinName)
        {
            SourcePin = Pin;
            break;
        }
    }

    for (UEdGraphPin* Pin : TargetNode->Pins)
    {
        if (Pin->PinName.ToString() == TargetPinName || Pin->GetDisplayName().ToString() == TargetPinName)
        {
            TargetPin = Pin;
            break;
        }
    }

    bool bConnected = false;
    if (SourcePin && TargetPin)
    {
        SourcePin->MakeLinkTo(TargetPin);
        bConnected = SourcePin->LinkedTo.Contains(TargetPin);
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(LevelBP);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, LevelBP);
    ResponseJson->SetStringField(TEXT("sourceNode"), SourceNodeName);
    ResponseJson->SetStringField(TEXT("sourcePin"), SourcePinName);
    ResponseJson->SetStringField(TEXT("targetNode"), TargetNodeName);
    ResponseJson->SetStringField(TEXT("targetPin"), TargetPinName);
    ResponseJson->SetBoolField(TEXT("connected"), bConnected);

    FString Message = bConnected
        ? FString::Printf(TEXT("Connected %s.%s -> %s.%s"), *SourceNodeName, *SourcePinName, *TargetNodeName, *TargetPinName)
        : TEXT("Nodes prepared for connection (manual pin connection may be required)");
    Subsystem->SendAutomationResponse(Socket, RequestId, true, Message, ResponseJson);
    return true;
}

}
#endif
