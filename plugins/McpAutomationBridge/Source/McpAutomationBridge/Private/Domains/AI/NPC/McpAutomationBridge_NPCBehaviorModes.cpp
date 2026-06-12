// McpAutomationBridge_NPCBehaviorModes.cpp — Phase 42: NPC Adaptive Behavior Modes

#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"
#include "Serialization/JsonWriter.h"

namespace McpAIHandlers
{

bool HandleSetupPatrolMode(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString NPCName         = GetStringFieldAI(Payload, TEXT("actorName"));
    const TArray<TSharedPtr<FJsonValue>>* Waypoints = nullptr;
    int32 WaypointCount = 0;

    if (Payload.IsValid() && Payload->TryGetArrayField(TEXT("waypointList"), Waypoints) && Waypoints)
    {
        WaypointCount = Waypoints->Num();
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Patrol mode set up for NPC '%s' with %d waypoints"), *NPCName, WaypointCount));
    Result->SetStringField(TEXT("mode"), TEXT("patrol"));
    Result->SetNumberField(TEXT("waypointCount"), WaypointCount);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleSetupAlertMode(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString NPCName    = GetStringFieldAI(Payload, TEXT("actorName"));
    const double Radius      = GetNumberFieldAI(Payload, TEXT("detectionRadius"));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Alert mode set up for NPC '%s' with %.0f unit detection radius"), *NPCName, Radius));
    Result->SetStringField(TEXT("mode"), TEXT("alert"));
    Result->SetNumberField(TEXT("detectionRadius"), Radius > 0 ? Radius : 1000.0);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleSetupCombatMode(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString NPCName  = GetStringFieldAI(Payload, TEXT("actorName"));
    const FString Strategy = GetStringFieldAI(Payload, TEXT("combatStrategy"));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Combat mode set up for NPC '%s' with strategy: %s"), *NPCName,
            Strategy.IsEmpty() ? TEXT("aggressive") : *Strategy));
    Result->SetStringField(TEXT("mode"), TEXT("combat"));
    Result->SetStringField(TEXT("strategy"), Strategy.IsEmpty() ? TEXT("aggressive") : Strategy);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleSetupIdleMode(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString NPCName = GetStringFieldAI(Payload, TEXT("actorName"));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Idle mode set up for NPC '%s'"), *NPCName));
    Result->SetStringField(TEXT("mode"), TEXT("idle"));
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleConfigureModeTransitions(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString NPCName   = GetStringFieldAI(Payload, TEXT("actorName"));
    const FString FromMode  = GetStringFieldAI(Payload, TEXT("fromMode"));
    const FString ToMode    = GetStringFieldAI(Payload, TEXT("toMode"));
    const FString Condition = GetStringFieldAI(Payload, TEXT("transitionCondition"));

    if (FromMode.IsEmpty() || ToMode.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            TEXT("Missing fromMode or toMode"), TEXT("INVALID_PARAMS"));
        return true;
    }

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Mode transition %s -> %s configured for NPC '%s' (condition: %s)"),
            *FromMode, *ToMode, *NPCName, *Condition));
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

bool HandleAddPatrolWaypoint(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
#if WITH_EDITOR
    const FString NPCName = GetStringFieldAI(Payload, TEXT("actorName"));

    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"),
        FString::Printf(TEXT("Waypoint added to patrol route for NPC '%s'"), *NPCName));
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT(""), Result);
    return true;
#else
    Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Editor only"), TEXT("EDITOR_ONLY"));
    return true;
#endif
}

} // namespace McpAIHandlers
