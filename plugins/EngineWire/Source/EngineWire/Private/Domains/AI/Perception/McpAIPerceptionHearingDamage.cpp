#include "Domains/AI/McpAIContext.h"
#include "Domains/AI/Perception/McpAIPerceptionConfigs.h"

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
namespace
{
// The Blueprint at blueprintPath with its perception component (added when missing), or null once a refusal went out.
UAIPerceptionComponent* LoadPerceptionOrReply(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, UBlueprint*& OutBlueprint)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    OutBlueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
    if (!OutBlueprint)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
        return nullptr;
    }
    UAIPerceptionComponent* Perception = FindOrCreatePerceptionComponent(Self, RequestId, RequestingSocket, OutBlueprint, nullptr);
    if (Perception)
    {
        Perception->Modify();
    }
    return Perception;
}
}

// Implements the "configure_hearing_config" action. Each call used to build a new config, resetting the affiliation
// and max age it was not sent, and read only hearingRange.
bool HandleConfigureHearingConfig(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    UBlueprint* Blueprint = nullptr;
    UAIPerceptionComponent* PerceptionComp = LoadPerceptionOrReply(Self, RequestId, Payload, RequestingSocket, Blueprint);
    if (!PerceptionComp)
    {
        return true;
    }

    bool bCreated = false;
    UAISenseConfig_Hearing* HearingConfig = FindOrAddSenseConfig<UAISenseConfig_Hearing>(PerceptionComp, bCreated);
    ApplyHearingSettings(HearingConfig, MergedSenseSettings(Payload, TEXT("hearingConfig")), bCreated);
    PerceptionComp->ConfigureSense(*HearingConfig);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("createdSense"), bCreated);
    AddSenseSettings(HearingConfig, Result);
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Blueprint));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Hearing config set"), Result);
    return true;
}

// Implements the "configure_damage_sense_config" action.
bool HandleConfigureDamageSenseConfig(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    UBlueprint* Blueprint = nullptr;
    UAIPerceptionComponent* PerceptionComp = LoadPerceptionOrReply(Self, RequestId, Payload, RequestingSocket, Blueprint);
    if (!PerceptionComp)
    {
        return true;
    }

    bool bCreated = false;
    UAISenseConfig_Damage* DamageConfig = FindOrAddSenseConfig<UAISenseConfig_Damage>(PerceptionComp, bCreated);
    double MaxAge = 10.0;
    if (MergedSenseSettings(Payload, TEXT("damageConfig"))->TryGetNumberField(TEXT("maxAge"), MaxAge) || bCreated)
    {
        DamageConfig->SetMaxAge(static_cast<float>(MaxAge));
    }
    PerceptionComp->ConfigureSense(*DamageConfig);

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetBoolField(TEXT("createdSense"), bCreated);
    AddSenseSettings(DamageConfig, Result);
    Result->SetBoolField(TEXT("saved"), McpSafeAssetSave(Blueprint));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Damage config set"), Result);
    return true;
}
}
