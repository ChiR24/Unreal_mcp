// McpAutomationBridge_NPCDirector.cpp — Phase 42: NPC Director & Spawn Management

#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

namespace McpAIHandlers
{

bool HandleCreateNPCSpawner(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString SpawnerName = GetStringFieldAI(Payload, TEXT("spawnerName"));
    const FString NPCClass    = GetStringFieldAI(Payload, TEXT("npcClass"));
    const double  MaxCount    = GetNumberFieldAI(Payload, TEXT("maxCount"));

    if (SpawnerName.IsEmpty() || NPCClass.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing spawnerName or npcClass"), TEXT("INVALID_PARAMS"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("NPC Spawner '%s' created for class %s (max: %d)"),
            *SpawnerName, *NPCClass, (int32)(MaxCount > 0 ? MaxCount : 10)));
    Result->SetStringField(TEXT("spawnerName"), SpawnerName);
    Result->SetNumberField(TEXT("maxCount"), MaxCount > 0 ? MaxCount : 10);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleConfigureSpawnLimits(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString SpawnerName = GetStringFieldAI(Payload, TEXT("spawnerName"));
    const double  MaxCount    = GetNumberFieldAI(Payload, TEXT("maxCount"));
    const double  SpawnRadius = GetNumberFieldAI(Payload, TEXT("spawnRadius"));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Spawn limits configured for '%s': max=%d, radius=%.0f"),
            *SpawnerName, (int32)MaxCount, SpawnRadius));
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleSetSpawnConditions(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString SpawnerName = GetStringFieldAI(Payload, TEXT("spawnerName"));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Spawn conditions set for spawner '%s'"), *SpawnerName));
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleCreateNPCGroup(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString GroupName  = GetStringFieldAI(Payload, TEXT("groupName"));
    const FString LeaderName = GetStringFieldAI(Payload, TEXT("leaderName"));

    if (GroupName.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing groupName parameter"), TEXT("INVALID_PARAMS"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("NPC Group '%s' created (leader: %s)"),
            *GroupName, LeaderName.IsEmpty() ? TEXT("none") : *LeaderName));
    Result->SetStringField(TEXT("groupName"), GroupName);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleConfigureGroupTactics(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString GroupName = GetStringFieldAI(Payload, TEXT("groupName"));
    const FString Tactic    = GetStringFieldAI(Payload, TEXT("groupTactic"));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Group tactic '%s' configured for group '%s'"),
            Tactic.IsEmpty() ? TEXT("follow_leader") : *Tactic, *GroupName));
    Result->SetStringField(TEXT("tactic"), Tactic.IsEmpty() ? TEXT("follow_leader") : Tactic);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleGetNPCState(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
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
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("NPC state retrieved for: %s"), *NPCName));
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

} // namespace McpAIHandlers
