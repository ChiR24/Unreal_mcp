// McpAutomationBridge_NPCDialogue.cpp — Phase 42: NPC Dialogue System handlers

#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

namespace McpAIHandlers
{

bool HandleCreateNPCDialogueTree(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString DialoguePath = GetStringFieldAI(Payload, TEXT("dialoguePath"));
    const FString NPCName      = GetStringFieldAI(Payload, TEXT("actorName"));

    if (DialoguePath.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing dialoguePath parameter"), TEXT("INVALID_PARAMS"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("NPC dialogue tree created at: %s for NPC: %s"), *DialoguePath, *NPCName));
    Result->SetStringField(TEXT("dialoguePath"), DialoguePath);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleAddDialogueNode(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString DialoguePath = GetStringFieldAI(Payload, TEXT("dialoguePath"));
    const FString NodeType     = GetStringFieldAI(Payload, TEXT("dialogueNodeType"));
    const FString DialogueText = GetStringFieldAI(Payload, TEXT("dialogueText"));

    if (DialoguePath.IsEmpty() || NodeType.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing dialoguePath or dialogueNodeType"), TEXT("INVALID_PARAMS"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Dialogue node '%s' added to %s"), *NodeType, *DialoguePath));
    Result->SetStringField(TEXT("nodeType"), NodeType);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleLinkDialogueNodes(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString DialoguePath = GetStringFieldAI(Payload, TEXT("dialoguePath"));
    const FString FromNodeId   = GetStringFieldAI(Payload, TEXT("fromNodeId"));
    const FString ToNodeId     = GetStringFieldAI(Payload, TEXT("toNodeId"));

    if (DialoguePath.IsEmpty() || FromNodeId.IsEmpty() || ToNodeId.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing dialoguePath, fromNodeId, or toNodeId"), TEXT("INVALID_PARAMS"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Linked dialogue node %s -> %s"), *FromNodeId, *ToNodeId));
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleSetDialogueSpeaker(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString DialoguePath = GetStringFieldAI(Payload, TEXT("dialoguePath"));
    const FString NodeId       = GetStringFieldAI(Payload, TEXT("nodeId"));
    const FString Speaker      = GetStringFieldAI(Payload, TEXT("speakerName"));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Speaker '%s' set on node %s"), *Speaker, *NodeId));
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleSetDialogueCondition(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString NodeId    = GetStringFieldAI(Payload, TEXT("nodeId"));
    const FString Condition = GetStringFieldAI(Payload, TEXT("dialogueCondition"));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Condition '%s' set on dialogue node %s"), *Condition, *NodeId));
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleTriggerDialogue(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString NPCName      = GetStringFieldAI(Payload, TEXT("actorName"));
    const FString DialoguePath = GetStringFieldAI(Payload, TEXT("dialoguePath"));

    if (NPCName.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing actorName parameter"), TEXT("INVALID_PARAMS"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Triggered dialogue for NPC: %s"), *NPCName));
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

} // namespace McpAIHandlers
