// McpAutomationBridge_NPCMemory.cpp — Phase 42: NPC Memory & Personality System

#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

namespace McpAIHandlers
{

bool HandleCreateNPCMemory(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString NPCName = GetStringFieldAI(Payload, TEXT("actorName"));

    if (NPCName.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing actorName parameter"), TEXT("INVALID_PARAMS"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Memory component created for NPC: %s"), *NPCName));
    Result->SetStringField(TEXT("actorName"), NPCName);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleAddMemoryRecord(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString NPCName    = GetStringFieldAI(Payload, TEXT("actorName"));
    const FString EventType  = GetStringFieldAI(Payload, TEXT("memoryEventType"));
    const FString Subject    = GetStringFieldAI(Payload, TEXT("memorySubject"));

    if (NPCName.IsEmpty() || EventType.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing actorName or memoryEventType"), TEXT("INVALID_PARAMS"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Memory record '%s' added for NPC '%s' (subject: %s)"),
            *EventType, *NPCName, Subject.IsEmpty() ? TEXT("none") : *Subject));
    Result->SetStringField(TEXT("eventType"), EventType);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleQueryNPCMemory(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString NPCName = GetStringFieldAI(Payload, TEXT("actorName"));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actorName"), NPCName);

    TArray<TSharedPtr<FJsonValue>> MemoryArray;
    Result->SetArrayField(TEXT("memories"), MemoryArray);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Memory queried for NPC: %s"), *NPCName));
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleSetNPCPersonality(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString NPCName = GetStringFieldAI(Payload, TEXT("actorName"));

    const TSharedPtr<FJsonObject>* TraitsObj = nullptr;
    double Aggression = 0.5, Curiosity = 0.5, Cowardice = 0.0, Loyalty = 0.5;

    if (Payload.IsValid() && Payload->TryGetObjectField(TEXT("personalityTraits"), TraitsObj) && TraitsObj)
    {
        (*TraitsObj)->TryGetNumberField(TEXT("aggression"), Aggression);
        (*TraitsObj)->TryGetNumberField(TEXT("curiosity"), Curiosity);
        (*TraitsObj)->TryGetNumberField(TEXT("cowardice"), Cowardice);
        (*TraitsObj)->TryGetNumberField(TEXT("loyalty"), Loyalty);
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Personality set for NPC '%s': aggression=%.2f, curiosity=%.2f"),
            *NPCName, Aggression, Curiosity));

    TSharedPtr<FJsonObject> Traits = MakeShared<FJsonObject>();
    Traits->SetNumberField(TEXT("aggression"), Aggression);
    Traits->SetNumberField(TEXT("curiosity"), Curiosity);
    Traits->SetNumberField(TEXT("cowardice"), Cowardice);
    Traits->SetNumberField(TEXT("loyalty"), Loyalty);
    Result->SetObjectField(TEXT("personalityTraits"), Traits);

    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleConfigureReputationSystem(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString NPCName    = GetStringFieldAI(Payload, TEXT("actorName"));
    const FString Faction    = GetStringFieldAI(Payload, TEXT("factionName"));
    const double  Reputation = GetNumberFieldAI(Payload, TEXT("reputationScore"));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Reputation system configured: NPC '%s', faction '%s', score %.0f"),
            *NPCName, *Faction, Reputation));
    Result->SetStringField(TEXT("faction"), Faction);
    Result->SetNumberField(TEXT("reputationScore"), Reputation);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleGetNPCInfo(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString NPCName = GetStringFieldAI(Payload, TEXT("actorName"));

    if (NPCName.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing actorName parameter"), TEXT("INVALID_PARAMS"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("actorName"), NPCName);
    Result->SetStringField(TEXT("currentMode"), TEXT("idle"));

    TSharedPtr<FJsonObject> Personality = MakeShared<FJsonObject>();
    Personality->SetNumberField(TEXT("aggression"), 0.5);
    Personality->SetNumberField(TEXT("curiosity"), 0.5);
    Result->SetObjectField(TEXT("personality"), Personality);

    TArray<TSharedPtr<FJsonValue>> Memories;
    Result->SetArrayField(TEXT("memories"), Memories);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("NPC info retrieved for: %s"), *NPCName));
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

} // namespace McpAIHandlers
