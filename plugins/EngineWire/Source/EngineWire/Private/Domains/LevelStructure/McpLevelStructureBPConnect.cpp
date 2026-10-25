#include "Domains/LevelStructure/McpLevelStructureActions.h"
#include "Domains/LevelStructure/McpLevelStructureEditorWorld.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
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

bool HandleConnectLevelBlueprintNodes(
    UEngineWireSubsystem* Subsystem,
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

    // Same levelPath honoring as add_node: connect must act on the named level,
    // never on whatever level happens to be open.
    ULevel* CurrentLevel = nullptr;
    ULevelScriptBlueprint* LevelBP = nullptr;
    UEdGraph* EventGraph = nullptr;
    if (!ResolveLevelBlueprintGraph(Subsystem, RequestId, Socket, Payload, /*bCreate=*/false, CurrentLevel, LevelBP, EventGraph))
    {
        return true;
    }

    UEdGraphNode* SourceNode = FindLevelBlueprintNode(EventGraph, SourceNodeName);
    UEdGraphNode* TargetNode = FindLevelBlueprintNode(EventGraph, TargetNodeName);

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

    // A missing pin, or a link the schema refused (wrong direction, incompatible
    // types), used to come back as success:true with "Nodes prepared for
    // connection (manual pin connection may be required)" -- the wiring the
    // caller asked for did not exist and nothing said so. Name the pins that
    // are actually on each node so the retry can be right.
    if (!SourcePin || !TargetPin)
    {
        auto PinNames = [](const UEdGraphNode* Node)
        {
            TArray<FString> Names;
            for (const UEdGraphPin* Pin : Node->Pins) { Names.Add(Pin->PinName.ToString()); }
            return FString::Join(Names, TEXT(", "));
        };
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(
                TEXT("Pin not found: %s has no pin '%s'. Pins on %s: [%s]. Pins on %s: [%s]."),
                SourcePin ? *TargetNodeName : *SourceNodeName,
                SourcePin ? *TargetPinName : *SourcePinName,
                *SourceNodeName, *PinNames(SourceNode),
                *TargetNodeName, *PinNames(TargetNode)),
            nullptr, TEXT("PIN_NOT_FOUND"));
        return true;
    }

    // MakeLinkTo skipped the schema, so any two pins linked (exec into data, a float into an object) and the level
    // Blueprint stopped compiling; the schema checks types and puts in the conversion node a cast needs.
    const UEdGraphSchema* Schema = EventGraph->GetSchema();
    const FPinConnectionResponse Response = Schema ? Schema->CanCreateConnection(SourcePin, TargetPin) : FPinConnectionResponse();
    const bool bConnected = Schema && Response.Response != CONNECT_RESPONSE_DISALLOW && Schema->TryCreateConnection(SourcePin, TargetPin);
    if (!bConnected)
    {
        Subsystem->SendAutomationResponse(Socket, RequestId, false,
            FString::Printf(TEXT("Connection refused by the graph schema: %s.%s -> %s.%s (%s). Nothing was linked."),
                *SourceNodeName, *SourcePinName, *TargetNodeName, *TargetPinName,
                Response.Message.IsEmpty() ? TEXT("incompatible pin types or directions") : *Response.Message.ToString()),
            nullptr, TEXT("CONNECTION_REFUSED"));
        return true;
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(LevelBP);

    TSharedPtr<FJsonObject> ResponseJson = McpHandlerUtils::CreateResultObject();
    McpHandlerUtils::AddVerification(ResponseJson, LevelBP);
    ResponseJson->SetStringField(TEXT("sourceNode"), SourceNodeName);
    ResponseJson->SetStringField(TEXT("sourcePin"), SourcePinName);
    ResponseJson->SetStringField(TEXT("targetNode"), TargetNodeName);
    ResponseJson->SetStringField(TEXT("targetPin"), TargetPinName);
    ResponseJson->SetBoolField(TEXT("connected"), bConnected);

    SendLevelEditResult(Subsystem, RequestId, Socket, Payload, CurrentLevel,
        FString::Printf(TEXT("Connected %s.%s -> %s.%s"), *SourceNodeName, *SourcePinName, *TargetNodeName, *TargetPinName),
        ResponseJson);
    return true;
}

}
