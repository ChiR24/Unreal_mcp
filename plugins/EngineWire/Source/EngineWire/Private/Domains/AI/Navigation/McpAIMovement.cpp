#include "Domains/AI/McpAIContext.h"

#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace McpAIHandlers
{
// Implements the "set_ai_movement" action.
bool HandleSetAIMovement(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
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
            TEXT("No CharacterMovementComponent found in the Blueprint; set_ai_movement needs a Character Blueprint."), TEXT("COMPONENT_NOT_FOUND"));
        return true;
    }

    // A value of 0 (an AI that stands still) used to be dropped like a missing one, a negative one too, and a
    // call that set nothing was saved and answered success.
    static const TCHAR* const NumberFields[] = {TEXT("maxWalkSpeed"), TEXT("maxAcceleration"), TEXT("brakingDeceleration"),
        TEXT("rotationRate"), TEXT("avoidanceWeight"), TEXT("maxFlySpeed"), TEXT("jumpZVelocity")};
    bool bAnyField = Payload->HasField(TEXT("useAccelerationForPaths")) || Payload->HasField(TEXT("orientRotationToMovement"))
        || Payload->HasField(TEXT("useRVOAvoidance"));
    for (const TCHAR* Field : NumberFields)
    {
        double Value = 0.0;
        if (!Payload->TryGetNumberField(Field, Value))
        {
            continue;
        }
        bAnyField = true;
        if (Value < 0.0)
        {
            Self->SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("%s must be 0 or more. Nothing was changed."), Field), TEXT("INVALID_ARGUMENT"));
            return true;
        }
    }
    if (!bAnyField)
    {
        Self->SendAutomationError(RequestingSocket, RequestId, TEXT("Nothing to set: send maxWalkSpeed, maxAcceleration, brakingDeceleration, rotationRate, "
            "avoidanceWeight, maxFlySpeed, jumpZVelocity, useAccelerationForPaths, orientRotationToMovement or useRVOAvoidance."), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    MovementComp->Modify();
    TArray<FString> PropertiesSet;
    const auto SetNumber = [&](const TCHAR* Field, float& Target, const TCHAR* Name)
    {
        double Value = 0.0;
        if (Payload->TryGetNumberField(Field, Value))
        {
            Target = static_cast<float>(Value);
            PropertiesSet.Add(Name);
        }
    };
    SetNumber(TEXT("maxWalkSpeed"), MovementComp->MaxWalkSpeed, TEXT("MaxWalkSpeed"));
    SetNumber(TEXT("maxAcceleration"), MovementComp->MaxAcceleration, TEXT("MaxAcceleration"));
    SetNumber(TEXT("brakingDeceleration"), MovementComp->BrakingDecelerationWalking, TEXT("BrakingDecelerationWalking"));
    double RotationRate = 0.0;
    if (Payload->TryGetNumberField(TEXT("rotationRate"), RotationRate))
    {
        MovementComp->RotationRate.Yaw = RotationRate;
        PropertiesSet.Add(TEXT("RotationRate"));
    }

    // Use acceleration for paths
    // UE 5.7+: bUseAccelerationForPaths was removed from UNavMovementComponent
    // Use bRequestedMoveUseAcceleration in UCharacterMovementComponent instead
    bool bUseAcceleration = GetJsonBoolField(Payload, TEXT("useAccelerationForPaths"));
    if (Payload->HasField(TEXT("useAccelerationForPaths")))
    {
        MovementComp->bRequestedMoveUseAcceleration = bUseAcceleration;
        PropertiesSet.Add(TEXT("bRequestedMoveUseAcceleration"));
    }

    // Orient rotation to movement
    bool bOrientToMovement = GetJsonBoolField(Payload, TEXT("orientRotationToMovement"));
    if (Payload->HasField(TEXT("orientRotationToMovement")))
    {
        MovementComp->bOrientRotationToMovement = bOrientToMovement;
        PropertiesSet.Add(TEXT("bOrientRotationToMovement"));
    }

    // Use RVO avoidance
    bool bUseRVOAvoidance = GetJsonBoolField(Payload, TEXT("useRVOAvoidance"));
    if (Payload->HasField(TEXT("useRVOAvoidance")))
    {
        MovementComp->bUseRVOAvoidance = bUseRVOAvoidance;
        PropertiesSet.Add(TEXT("bUseRVOAvoidance"));
    }

    SetNumber(TEXT("avoidanceWeight"), MovementComp->AvoidanceWeight, TEXT("AvoidanceWeight"));
    SetNumber(TEXT("maxFlySpeed"), MovementComp->MaxFlySpeed, TEXT("MaxFlySpeed"));
    SetNumber(TEXT("jumpZVelocity"), MovementComp->JumpZVelocity, TEXT("JumpZVelocity"));

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

    TSharedPtr<FJsonObject> MovementResult = McpHandlerUtils::CreateResultObject();
    MovementResult->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    MovementResult->SetBoolField(TEXT("saved"), McpSafeAssetSave(Blueprint));
    McpHandlerUtils::AddVerification(MovementResult, Blueprint);

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
}
