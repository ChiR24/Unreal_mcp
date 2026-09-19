#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Hearing.h"

namespace McpAIHandlers
{
// Implements the "configure_hearing_config" action.
bool HandleConfigureHearingConfig(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    // CRITICAL: Explicitly check if asset exists before LoadObject
    // LoadObject may return non-null for invalid paths due to UE's path resolution behavior
    if (!UEditorAssetLibrary::DoesAssetExist(BlueprintPath))
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    double HearingRange = GetJsonNumberField(Payload, TEXT("hearingRange"), 3000.0);
    const TSharedPtr<FJsonObject>* HearingConfigObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("hearingConfig"), HearingConfigObj) && HearingConfigObj->IsValid())
    {
        HearingRange = GetJsonNumberField(*HearingConfigObj, TEXT("hearingRange"), HearingRange);
    }

    UAIPerceptionComponent* PerceptionComp = FindOrCreatePerceptionComponent(
        Self, RequestId, RequestingSocket, Blueprint, nullptr);
    if (!PerceptionComp)
    {
        return true;
    }

    UAISenseConfig_Hearing* HearingConfig = NewObject<UAISenseConfig_Hearing>(PerceptionComp);
    HearingConfig->HearingRange = HearingRange;
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
    HearingConfig->SetMaxAge(5.0f);
    PerceptionComp->ConfigureSense(*HearingConfig);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeAssetSave(Blueprint);
    Result->SetNumberField(TEXT("hearingRange"), HearingRange);
    Result->SetStringField(TEXT("message"), TEXT("Hearing sense configured"));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hearing config set"), Result);
    return true;
}

// Implements the "configure_damage_sense_config" action.
bool HandleConfigureDamageSenseConfig(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));

    // CRITICAL: Explicitly check if asset exists before LoadObject
    // LoadObject may return non-null for invalid paths due to UE's path resolution behavior
    if (!UEditorAssetLibrary::DoesAssetExist(BlueprintPath))
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
        return true;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!Blueprint)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
                            FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath),
                            TEXT("NOT_FOUND"));
        return true;
    }

    double MaxAge = 10.0;
    const TSharedPtr<FJsonObject>* DamageConfigObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("damageConfig"), DamageConfigObj) && DamageConfigObj->IsValid())
    {
        MaxAge = GetJsonNumberField(*DamageConfigObj, TEXT("maxAge"), MaxAge);
    }

    UAIPerceptionComponent* PerceptionComp = FindOrCreatePerceptionComponent(
        Self, RequestId, RequestingSocket, Blueprint, nullptr);
    if (!PerceptionComp)
    {
        return true;
    }

    UAISenseConfig_Damage* DamageConfig = NewObject<UAISenseConfig_Damage>(PerceptionComp);
    DamageConfig->SetMaxAge(MaxAge);
    PerceptionComp->ConfigureSense(*DamageConfig);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    McpSafeAssetSave(Blueprint);
    Result->SetNumberField(TEXT("maxAge"), MaxAge);
    Result->SetStringField(TEXT("message"), TEXT("Damage sense configured"));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Damage config set"), Result);
    return true;
}
}
#endif
