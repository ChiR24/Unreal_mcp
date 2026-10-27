#include "Domains/AI/McpAIContext.h"
#include "Domains/AI/Perception/McpAIPerceptionConfigs.h"

#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISense_Touch.h"
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
    UEngineWireSubsystem* Self,
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
bool ConfigurePerception(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload,
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
    // An unknown dominant sense was echoed back as if applied.
    const FString DominantSense = GetJsonStringField(Payload, TEXT("dominantSense"));
    static const TMap<FString, UClass*> Senses = {
        {TEXT("Sight"), UAISense_Sight::StaticClass()},
        {TEXT("Hearing"), UAISense_Hearing::StaticClass()},
        {TEXT("Damage"), UAISense_Damage::StaticClass()},
        {TEXT("Touch"), UAISense_Touch::StaticClass()},
        {TEXT("None"), nullptr}};
    UClass* const* DominantClass = DominantSense.IsEmpty() ? nullptr : Senses.Find(DominantSense);
    if (!DominantSense.IsEmpty() && !DominantClass)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("dominantSense must be Sight, Hearing, Damage, Touch or None."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    if (!DominantClass && !GetJsonBoolField(Payload, TEXT("enableSight")) && !GetJsonBoolField(Payload, TEXT("enableHearing"))
        && !GetJsonBoolField(Payload, TEXT("enableDamage")))
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Nothing to set: send enableSight, enableHearing, enableDamage or dominantSense."), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    bool bCreatedNew = false;
    UAIPerceptionComponent* PerceptionComp =
        FindOrCreatePerceptionComponent(Self, RequestId, RequestingSocket, ControllerBP, &bCreatedNew);
    if (!PerceptionComp)
    {
        return true;
    }

    // A sense already set up keeps the settings this call does not send; each call used to replace it with defaults.
    TArray<TSharedPtr<FJsonValue>> SensesConfigured;
    bool bSenseCreated = false;
    PerceptionComp->Modify();
    if (GetJsonBoolField(Payload, TEXT("enableSight")))
    {
        UAISenseConfig_Sight* SightConfig = FindOrAddSenseConfig<UAISenseConfig_Sight>(PerceptionComp, bSenseCreated);
        ApplySightSettings(SightConfig, MergedSenseSettings(Payload, TEXT("sightConfig")), bSenseCreated);
        PerceptionComp->ConfigureSense(*SightConfig);
        SensesConfigured.Add(MakeShared<FJsonValueString>(TEXT("Sight")));
    }
    if (GetJsonBoolField(Payload, TEXT("enableHearing")))
    {
        UAISenseConfig_Hearing* HearingConfig = FindOrAddSenseConfig<UAISenseConfig_Hearing>(PerceptionComp, bSenseCreated);
        ApplyHearingSettings(HearingConfig, MergedSenseSettings(Payload, TEXT("hearingConfig")), bSenseCreated);
        PerceptionComp->ConfigureSense(*HearingConfig);
        SensesConfigured.Add(MakeShared<FJsonValueString>(TEXT("Hearing")));
    }
    if (GetJsonBoolField(Payload, TEXT("enableDamage")))
    {
        UAISenseConfig_Damage* DamageConfig = FindOrAddSenseConfig<UAISenseConfig_Damage>(PerceptionComp, bSenseCreated);
        if (bSenseCreated)
        {
            DamageConfig->SetMaxAge(10.0f);
        }
        PerceptionComp->ConfigureSense(*DamageConfig);
        SensesConfigured.Add(MakeShared<FJsonValueString>(TEXT("Damage")));
    }

    if (DominantClass)
    {
        PerceptionComp->SetDominantSense(*DominantClass);
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(ControllerBP);

    TSharedPtr<FJsonObject> PerceptionResult = McpHandlerUtils::CreateResultObject();
    PerceptionResult->SetStringField(TEXT("controllerPath"), ControllerPath);
    PerceptionResult->SetBoolField(TEXT("createdNew"), bCreatedNew);
    PerceptionResult->SetArrayField(TEXT("sensesConfigured"), SensesConfigured);
    if (!DominantSense.IsEmpty())
    {
        PerceptionResult->SetStringField(TEXT("dominantSense"), DominantSense);
    }
    PerceptionResult->SetBoolField(TEXT("saved"), McpSafeAssetSave(ControllerBP));
    McpHandlerUtils::AddVerification(PerceptionResult, ControllerBP);
    Self->SendAutomationResponse(RequestingSocket, RequestId, true, SuccessMessage, PerceptionResult);
    return true;
}
}

bool HandleSetupPerception(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return ConfigurePerception(Self, RequestId, Payload, RequestingSocket, TEXT("AI perception configured via setup_perception"));
}

// Implements the "set_ai_perception" action.
bool HandleSetAIPerception(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    return ConfigurePerception(Self, RequestId, Payload, RequestingSocket, TEXT("AI perception configured"));
}
}
