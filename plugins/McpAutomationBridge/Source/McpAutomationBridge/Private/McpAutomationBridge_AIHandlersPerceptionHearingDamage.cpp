#include "McpAutomationBridge_AIHandlerContext.h"

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
bool HandleConfigureHearingConfig(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("configure_hearing_config");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("configure_hearing_config"))
    {
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));

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

        double HearingRange = GetNumberFieldAI(Payload, TEXT("hearingRange"), 3000.0);
        const TSharedPtr<FJsonObject>* HearingConfigObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("hearingConfig"), HearingConfigObj) && HearingConfigObj->IsValid())
        {
            HearingRange = GetNumberFieldAI(*HearingConfigObj, TEXT("hearingRange"), HearingRange);
        }

        if (!Blueprint->SimpleConstructionScript)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_STATE"));
            return true;
        }

        UAIPerceptionComponent* PerceptionComp = nullptr;
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->ComponentTemplate)
            {
                if (UAIPerceptionComponent* Comp = Cast<UAIPerceptionComponent>(Node->ComponentTemplate))
                {
                    PerceptionComp = Comp;
                    break;
                }
            }
        }

        if (!PerceptionComp)
        {
            USCS_Node* PerceptionNode = Blueprint->SimpleConstructionScript->CreateNode(
                UAIPerceptionComponent::StaticClass(), TEXT("AIPerceptionComponent"));
            if (!PerceptionNode)
            {
                Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create perception component node"), TEXT("CREATION_FAILED"));
                return true;
            }
            Blueprint->SimpleConstructionScript->AddNode(PerceptionNode);
            PerceptionComp = Cast<UAIPerceptionComponent>(PerceptionNode->ComponentTemplate);
        }

        if (!PerceptionComp)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Perception component is null"), TEXT("NULL_COMPONENT"));
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

    return true;
}

bool HandleConfigureDamageSenseConfig(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("configure_damage_sense_config");
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    if (SubAction == TEXT("configure_damage_sense_config"))
    {
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));

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
            MaxAge = GetNumberFieldAI(*DamageConfigObj, TEXT("maxAge"), MaxAge);
        }

        if (!Blueprint->SimpleConstructionScript)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_STATE"));
            return true;
        }

        UAIPerceptionComponent* PerceptionComp = nullptr;
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->ComponentTemplate)
            {
                if (UAIPerceptionComponent* Comp = Cast<UAIPerceptionComponent>(Node->ComponentTemplate))
                {
                    PerceptionComp = Comp;
                    break;
                }
            }
        }

        if (!PerceptionComp)
        {
            USCS_Node* PerceptionNode = Blueprint->SimpleConstructionScript->CreateNode(
                UAIPerceptionComponent::StaticClass(), TEXT("AIPerceptionComponent"));
            if (!PerceptionNode)
            {
                Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create perception component node"), TEXT("CREATION_FAILED"));
                return true;
            }
            Blueprint->SimpleConstructionScript->AddNode(PerceptionNode);
            PerceptionComp = Cast<UAIPerceptionComponent>(PerceptionNode->ComponentTemplate);
        }

        if (!PerceptionComp)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Perception component is null"), TEXT("NULL_COMPONENT"));
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

    return true;
}
}
#endif
