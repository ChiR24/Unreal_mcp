#include "McpAutomationBridge_AIHandlerContext.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpAIHandlers
{
bool HandleSetAIMovement(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString SubAction = TEXT("set_ai_movement");
    if (SubAction == TEXT("set_ai_movement"))
    {
        FString BlueprintPath = GetStringFieldAI(Payload, TEXT("blueprintPath"));
        if (BlueprintPath.IsEmpty())
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath"), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        if (!Blueprint->SimpleConstructionScript)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint has no SimpleConstructionScript"), TEXT("INVALID_STATE"));
            return true;
        }

        // Find CharacterMovementComponent
        UCharacterMovementComponent* MovementComp = nullptr;
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->ComponentTemplate)
            {
                if (UCharacterMovementComponent* Comp = Cast<UCharacterMovementComponent>(Node->ComponentTemplate))
                {
                    MovementComp = Comp;
                    break;
                }
            }
        }

        if (!MovementComp)
        {
            // Check CDO for native component
            if (Blueprint->GeneratedClass)
            {
                if (AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()))
                {
                    MovementComp = CDO->FindComponentByClass<UCharacterMovementComponent>();
                }
            }
        }

        if (!MovementComp)
        {
            Self->SendAutomationError(RequestingSocket, RequestId,
                TEXT("No CharacterMovementComponent found in blueprint"), TEXT("COMPONENT_NOT_FOUND"));
            return true;
        }

        TArray<FString> PropertiesSet;

        // Walking speed
        float MaxWalkSpeed = GetNumberFieldAI(Payload, TEXT("maxWalkSpeed"), -1.0f);
        if (MaxWalkSpeed > 0.0f)
        {
            MovementComp->MaxWalkSpeed = MaxWalkSpeed;
            PropertiesSet.Add(TEXT("MaxWalkSpeed"));
        }

        // Max acceleration
        float MaxAcceleration = GetNumberFieldAI(Payload, TEXT("maxAcceleration"), -1.0f);
        if (MaxAcceleration > 0.0f)
        {
            MovementComp->MaxAcceleration = MaxAcceleration;
            PropertiesSet.Add(TEXT("MaxAcceleration"));
        }

        // Braking deceleration walking
        float BrakingDeceleration = GetNumberFieldAI(Payload, TEXT("brakingDeceleration"), -1.0f);
        if (BrakingDeceleration > 0.0f)
        {
            MovementComp->BrakingDecelerationWalking = BrakingDeceleration;
            PropertiesSet.Add(TEXT("BrakingDecelerationWalking"));
        }

        // Rotation rate
        float RotationRate = GetNumberFieldAI(Payload, TEXT("rotationRate"), -1.0f);
        if (RotationRate > 0.0f)
        {
            MovementComp->RotationRate = FRotator(0.0f, RotationRate, 0.0f);
            PropertiesSet.Add(TEXT("RotationRate"));
        }

        // Use acceleration for paths
        // UE 5.7+: bUseAccelerationForPaths was removed from UNavMovementComponent
        // Use bRequestedMoveUseAcceleration in UCharacterMovementComponent instead
        bool bUseAcceleration = GetBoolFieldAI(Payload, TEXT("useAccelerationForPaths"));
        if (Payload->HasField(TEXT("useAccelerationForPaths")))
        {
            MovementComp->bRequestedMoveUseAcceleration = bUseAcceleration;
            PropertiesSet.Add(TEXT("bRequestedMoveUseAcceleration"));
        }

        // Orient rotation to movement
        bool bOrientToMovement = GetBoolFieldAI(Payload, TEXT("orientRotationToMovement"));
        if (Payload->HasField(TEXT("orientRotationToMovement")))
        {
            MovementComp->bOrientRotationToMovement = bOrientToMovement;
            PropertiesSet.Add(TEXT("bOrientRotationToMovement"));
        }

        // Use RVO avoidance
        bool bUseRVOAvoidance = GetBoolFieldAI(Payload, TEXT("useRVOAvoidance"));
        if (Payload->HasField(TEXT("useRVOAvoidance")))
        {
            MovementComp->bUseRVOAvoidance = bUseRVOAvoidance;
            PropertiesSet.Add(TEXT("bUseRVOAvoidance"));
        }

        // Avoidance weight
        float AvoidanceWeight = GetNumberFieldAI(Payload, TEXT("avoidanceWeight"), -1.0f);
        if (AvoidanceWeight >= 0.0f)
        {
            MovementComp->AvoidanceWeight = AvoidanceWeight;
            PropertiesSet.Add(TEXT("AvoidanceWeight"));
        }

        // Max fly speed (for flying AI)
        float MaxFlySpeed = GetNumberFieldAI(Payload, TEXT("maxFlySpeed"), -1.0f);
        if (MaxFlySpeed > 0.0f)
        {
            MovementComp->MaxFlySpeed = MaxFlySpeed;
            PropertiesSet.Add(TEXT("MaxFlySpeed"));
        }

        // Jump Z velocity
        float JumpZVelocity = GetNumberFieldAI(Payload, TEXT("jumpZVelocity"), -1.0f);
        if (JumpZVelocity > 0.0f)
        {
            MovementComp->JumpZVelocity = JumpZVelocity;
            PropertiesSet.Add(TEXT("JumpZVelocity"));
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> MovementResult = McpHandlerUtils::CreateResultObject();
        MovementResult->SetStringField(TEXT("blueprintPath"), BlueprintPath);

        TArray<TSharedPtr<FJsonValue>> PropsArray;
        for (const FString& Prop : PropertiesSet)
        {
            PropsArray.Add(MakeShared<FJsonValueString>(Prop));
        }
        MovementResult->SetArrayField(TEXT("propertiesSet"), PropsArray);
        MovementResult->SetNumberField(TEXT("propertyCount"), PropertiesSet.Num());

        // Include current values
        TSharedPtr<FJsonObject> CurrentValues = McpHandlerUtils::CreateResultObject();
        CurrentValues->SetNumberField(TEXT("maxWalkSpeed"), MovementComp->MaxWalkSpeed);
        CurrentValues->SetNumberField(TEXT("maxAcceleration"), MovementComp->MaxAcceleration);
        CurrentValues->SetNumberField(TEXT("rotationRateYaw"), MovementComp->RotationRate.Yaw);
        CurrentValues->SetBoolField(TEXT("orientRotationToMovement"), MovementComp->bOrientRotationToMovement);
        CurrentValues->SetBoolField(TEXT("useRVOAvoidance"), MovementComp->bUseRVOAvoidance);
        MovementResult->SetObjectField(TEXT("currentValues"), CurrentValues);

        Self->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("AI movement configured"), MovementResult);
        return true;
    }

    // =========================================================================
    // Aliases & Convenience Actions
    // =========================================================================

    // Alias: create_blackboard -> create_blackboard_asset
    return true;
}
}
#endif
