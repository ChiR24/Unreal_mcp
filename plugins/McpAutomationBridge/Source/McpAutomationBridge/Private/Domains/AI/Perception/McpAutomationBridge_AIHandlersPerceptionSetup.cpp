#include "Domains/AI/McpAutomationBridge_AIHandlerContext.h"

#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"

namespace McpAIHandlers
{
// Five perception actions (configure_sight_config, configure_hearing_config,
// configure_damage_sense_config, setup_perception, set_ai_perception) opened
// with the same twenty-five lines: reject a Blueprint with no SCS, scan the
// SCS for an existing UAIPerceptionComponent, create one when there is none,
// and reject a null template. Changing any of those three refusals meant
// editing all five. Returns nullptr once a refusal has been sent.
UAIPerceptionComponent* FindOrCreatePerceptionComponent(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket,
    UBlueprint* Blueprint,
    bool* OutCreated)
{
    if (OutCreated)
    {
        *OutCreated = false;
    }
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_STATE"));
        return nullptr;
    }

    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->ComponentTemplate)
        {
            if (UAIPerceptionComponent* Comp = Cast<UAIPerceptionComponent>(Node->ComponentTemplate))
            {
                return Comp;
            }
        }
    }

    USCS_Node* PerceptionNode = Blueprint->SimpleConstructionScript->CreateNode(
        UAIPerceptionComponent::StaticClass(), TEXT("AIPerceptionComponent"));
    if (!PerceptionNode)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create perception component node"), TEXT("CREATION_FAILED"));
        return nullptr;
    }
    Blueprint->SimpleConstructionScript->AddNode(PerceptionNode);

    UAIPerceptionComponent* Created = Cast<UAIPerceptionComponent>(PerceptionNode->ComponentTemplate);
    if (!Created)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Perception component is null"), TEXT("NULL_COMPONENT"));
        return nullptr;
    }
    if (OutCreated)
    {
        *OutCreated = true;
    }
    return Created;
}
namespace
{
// setup_perception / set_ai_perception: find or add the controller Blueprint's AIPerceptionComponent and configure
// the requested senses (sight/hearing/damage) and dominant sense.
bool ConfigurePerception(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
                         TSharedPtr<FMcpBridgeWebSocket> RequestingSocket, const TCHAR* SuccessMessage)
{
    const FString ControllerPath = McpGetFirstStringField(Payload, {TEXT("blueprintPath"), TEXT("controllerPath")});
    if (ControllerPath.IsEmpty())
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath or controllerPath"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    UBlueprint* ControllerBP = LoadObject<UBlueprint>(nullptr, *ControllerPath);
    if (!ControllerBP)
    {
        Self->SendAutomationError(RequestingSocket, RequestId,
            FString::Printf(TEXT("Blueprint not found: %s"), *ControllerPath), TEXT("NOT_FOUND"));
        return true;
    }
    bool bCreatedNew = false;
    UAIPerceptionComponent* PerceptionComp =
        FindOrCreatePerceptionComponent(Self, RequestId, RequestingSocket, ControllerBP, &bCreatedNew);
    if (!PerceptionComp)
    {
        return true;
    }

    TArray<TSharedPtr<FJsonValue>> SensesConfigured;
    if (GetJsonBoolField(Payload, TEXT("enableSight")))
    {
        const float SightRadius = GetJsonNumberField(Payload, TEXT("sightRadius"), 3000.0f);
        UAISenseConfig_Sight* SightConfig = NewObject<UAISenseConfig_Sight>(PerceptionComp);
        SightConfig->SightRadius = SightRadius;
        SightConfig->LoseSightRadius = GetJsonNumberField(Payload, TEXT("loseSightRadius"), SightRadius + 500.0f);
        SightConfig->PeripheralVisionAngleDegrees = GetJsonNumberField(Payload, TEXT("peripheralVisionAngle"), 90.0f);
        SightConfig->DetectionByAffiliation.bDetectEnemies = true;
        SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
        SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
        SightConfig->SetMaxAge(5.0f);
        PerceptionComp->ConfigureSense(*SightConfig);
        SensesConfigured.Add(MakeShared<FJsonValueString>(TEXT("Sight")));
    }
    if (GetJsonBoolField(Payload, TEXT("enableHearing")))
    {
        UAISenseConfig_Hearing* HearingConfig = NewObject<UAISenseConfig_Hearing>(PerceptionComp);
        HearingConfig->HearingRange = GetJsonNumberField(Payload, TEXT("hearingRange"), 3000.0f);
        HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
        HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
        HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
        HearingConfig->SetMaxAge(5.0f);
        PerceptionComp->ConfigureSense(*HearingConfig);
        SensesConfigured.Add(MakeShared<FJsonValueString>(TEXT("Hearing")));
    }
    if (GetJsonBoolField(Payload, TEXT("enableDamage")))
    {
        UAISenseConfig_Damage* DamageConfig = NewObject<UAISenseConfig_Damage>(PerceptionComp);
        DamageConfig->SetMaxAge(10.0f);
        PerceptionComp->ConfigureSense(*DamageConfig);
        SensesConfigured.Add(MakeShared<FJsonValueString>(TEXT("Damage")));
    }

    const FString DominantSense = GetJsonStringField(Payload, TEXT("dominantSense"));
    static const TMap<FString, UClass*> Senses = {
        {TEXT("Sight"), UAISense_Sight::StaticClass()},
        {TEXT("Hearing"), UAISense_Hearing::StaticClass()},
        {TEXT("Damage"), UAISense_Damage::StaticClass()}};
    if (UClass* const* Sense = Senses.Find(DominantSense))
    {
        PerceptionComp->SetDominantSense(*Sense);
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(ControllerBP);
    McpSafeAssetSave(ControllerBP);

    TSharedPtr<FJsonObject> PerceptionResult = McpHandlerUtils::CreateResultObject();
    PerceptionResult->SetStringField(TEXT("controllerPath"), ControllerPath);
    PerceptionResult->SetBoolField(TEXT("createdNew"), bCreatedNew);
    PerceptionResult->SetArrayField(TEXT("sensesConfigured"), SensesConfigured);
    if (!DominantSense.IsEmpty())
    {
        PerceptionResult->SetStringField(TEXT("dominantSense"), DominantSense);
    }
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, SuccessMessage, PerceptionResult);
    return true;
}
}

bool HandleSetupPerception(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return ConfigurePerception(Self, RequestId, Payload, RequestingSocket, TEXT("AI perception configured via setup_perception"));
}

// Implements the "set_ai_perception" action.
bool HandleSetAIPerception(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return ConfigurePerception(Self, RequestId, Payload, RequestingSocket, TEXT("AI perception configured"));
}
}
