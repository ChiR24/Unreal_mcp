#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
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
bool HandleSetAIPerception(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("set_ai_perception");
    if (SubAction == TEXT("set_ai_perception"))
    {
        FString ControllerPath = GetStringFieldAI(Payload, TEXT("controllerPath"));
        if (ControllerPath.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing controllerPath"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* ControllerBP = LoadObject<UBlueprint>(nullptr, *ControllerPath);
        if (!ControllerBP)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Controller blueprint not found: %s"), *ControllerPath), TEXT("NOT_FOUND"));
            return true;
        }

        if (!ControllerBP->SimpleConstructionScript)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_STATE"));
            return true;
        }

        // Find or create AIPerceptionComponent
        UAIPerceptionComponent* PerceptionComp = nullptr;
        USCS_Node* PerceptionNode = nullptr;

        for (USCS_Node* Node : ControllerBP->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->ComponentTemplate)
            {
                if (UAIPerceptionComponent* Comp = Cast<UAIPerceptionComponent>(Node->ComponentTemplate))
                {
                    PerceptionComp = Comp;
                    PerceptionNode = Node;
                    break;
                }
            }
        }

        bool bCreatedNew = false;
        if (!PerceptionComp)
        {
            // Create new perception component
            PerceptionNode = ControllerBP->SimpleConstructionScript->CreateNode(
                UAIPerceptionComponent::StaticClass(), TEXT("AIPerceptionComponent"));
            if (!PerceptionNode)
            {
                Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create perception component node"), TEXT("CREATION_FAILED"));
                return true;
            }
            ControllerBP->SimpleConstructionScript->AddNode(PerceptionNode);
            PerceptionComp = Cast<UAIPerceptionComponent>(PerceptionNode->ComponentTemplate);
            if (!PerceptionComp)
            {
                Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to cast perception component"), TEXT("CAST_FAILED"));
                return true;
            }
            bCreatedNew = true;
        }

        if (!PerceptionComp)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Perception component is null"), TEXT("NULL_COMPONENT"));
            return true;
        }

        TArray<FString> SensesConfigured;

        // Configure sight sense
        bool bEnableSight = GetBoolFieldAI(Payload, TEXT("enableSight"));
        if (bEnableSight)
        {
            float SightRadius = GetNumberFieldAI(Payload, TEXT("sightRadius"), 3000.0f);
            float LoseSightRadius = GetNumberFieldAI(Payload, TEXT("loseSightRadius"), SightRadius + 500.0f);
            float PeripheralVisionAngle = GetNumberFieldAI(Payload, TEXT("peripheralVisionAngle"), 90.0f);

            UAISenseConfig_Sight* SightConfig = NewObject<UAISenseConfig_Sight>(PerceptionComp);
            SightConfig->SightRadius = SightRadius;
            SightConfig->LoseSightRadius = LoseSightRadius;
            SightConfig->PeripheralVisionAngleDegrees = PeripheralVisionAngle;
            SightConfig->DetectionByAffiliation.bDetectEnemies = true;
            SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
            SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
            SightConfig->SetMaxAge(5.0f);

            PerceptionComp->ConfigureSense(*SightConfig);
            SensesConfigured.Add(TEXT("Sight"));
        }

        // Configure hearing sense
        bool bEnableHearing = GetBoolFieldAI(Payload, TEXT("enableHearing"));
        if (bEnableHearing)
        {
            float HearingRange = GetNumberFieldAI(Payload, TEXT("hearingRange"), 3000.0f);

            UAISenseConfig_Hearing* HearingConfig = NewObject<UAISenseConfig_Hearing>(PerceptionComp);
            HearingConfig->HearingRange = HearingRange;
            HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
            HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
            HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
            HearingConfig->SetMaxAge(5.0f);

            PerceptionComp->ConfigureSense(*HearingConfig);
            SensesConfigured.Add(TEXT("Hearing"));
        }

        // Configure damage sense
        bool bEnableDamage = GetBoolFieldAI(Payload, TEXT("enableDamage"));
        if (bEnableDamage)
        {
            UAISenseConfig_Damage* DamageConfig = NewObject<UAISenseConfig_Damage>(PerceptionComp);
            DamageConfig->SetMaxAge(10.0f);

            PerceptionComp->ConfigureSense(*DamageConfig);
            SensesConfigured.Add(TEXT("Damage"));
        }

        // Set dominant sense if specified
        FString DominantSense = GetStringFieldAI(Payload, TEXT("dominantSense"));
        if (!DominantSense.IsEmpty())
        {
            if (DominantSense.Equals(TEXT("Sight"), ESearchCase::IgnoreCase))
            {
                PerceptionComp->SetDominantSense(UAISense_Sight::StaticClass());
            }
            else if (DominantSense.Equals(TEXT("Hearing"), ESearchCase::IgnoreCase))
            {
                PerceptionComp->SetDominantSense(UAISense_Hearing::StaticClass());
            }
            else if (DominantSense.Equals(TEXT("Damage"), ESearchCase::IgnoreCase))
            {
                PerceptionComp->SetDominantSense(UAISense_Damage::StaticClass());
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(ControllerBP);
        McpSafeAssetSave(ControllerBP);

        TSharedPtr<FJsonObject> PerceptionResult = McpHandlerUtils::CreateResultObject();
        PerceptionResult->SetStringField(TEXT("controllerPath"), ControllerPath);
        PerceptionResult->SetBoolField(TEXT("createdNew"), bCreatedNew);

        TArray<TSharedPtr<FJsonValue>> SensesArray;
        for (const FString& Sense : SensesConfigured)
        {
            SensesArray.Add(MakeShared<FJsonValueString>(Sense));
        }
        PerceptionResult->SetArrayField(TEXT("sensesConfigured"), SensesArray);

        if (!DominantSense.IsEmpty())
        {
            PerceptionResult->SetStringField(TEXT("dominantSense"), DominantSense);
        }

        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("AI perception configured"), PerceptionResult);
        return true;
    }

    // create_nav_modifier - Create navigation modifier component on actor
    return true;
}
}
#endif
