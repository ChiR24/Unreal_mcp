#include "McpAutomationBridge_CharacterHandlers.h"

#if WITH_EDITOR
namespace McpCharacterHandlers
{
bool HandleConfigureMovementSpeeds(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    bool bHasAppliedWalkSpeed = false;
    bool bRunSpeedApplied = false;
    double AppliedWalkSpeed = 0.0;
    ACharacter* CharCDO = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (CharCDO && CharCDO->GetCharacterMovement())
    {
        UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();
        if (Payload->HasField(TEXT("walkSpeed")))
        {
            Movement->MaxWalkSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("walkSpeed"), 600.0));
            if (Payload->HasField(TEXT("runSpeed")))
            {
                UE_LOG(LogMcpCharacterHandlers, Warning, TEXT("configure_movement_speeds: Both walkSpeed and runSpeed provided. runSpeed is ignored (both map to MaxWalkSpeed in UE). Use configure_sprint for run speed."));
            }
        }
        else if (Payload->HasField(TEXT("runSpeed")))
        {
            Movement->MaxWalkSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("runSpeed"), 600.0));
            bRunSpeedApplied = true;
        }
        if (Payload->HasField(TEXT("crouchSpeed"))) Movement->MaxWalkSpeedCrouched = static_cast<float>(GetJsonNumberField(Payload, TEXT("crouchSpeed"), 300.0));
        if (Payload->HasField(TEXT("swimSpeed"))) Movement->MaxSwimSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("swimSpeed"), 300.0));
        if (Payload->HasField(TEXT("flySpeed"))) Movement->MaxFlySpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("flySpeed"), 600.0));
        if (Payload->HasField(TEXT("acceleration"))) Movement->MaxAcceleration = static_cast<float>(GetJsonNumberField(Payload, TEXT("acceleration"), 2048.0));
        if (Payload->HasField(TEXT("deceleration"))) Movement->BrakingDecelerationWalking = static_cast<float>(GetJsonNumberField(Payload, TEXT("deceleration"), 2048.0));
        if (Payload->HasField(TEXT("groundFriction"))) Movement->GroundFriction = static_cast<float>(GetJsonNumberField(Payload, TEXT("groundFriction"), 8.0));
        AppliedWalkSpeed = Movement->MaxWalkSpeed;
        bHasAppliedWalkSpeed = true;
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    if (Payload->HasField(TEXT("runSpeed")))
    {
        Result->SetNumberField(TEXT("runSpeed"), GetJsonNumberField(Payload, TEXT("runSpeed"), 600.0));
        Result->SetBoolField(TEXT("runSpeedApplied"), bRunSpeedApplied);
    }
    if (bHasAppliedWalkSpeed)
    {
        Result->SetNumberField(TEXT("walkSpeed"), AppliedWalkSpeed);
    }
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Movement speeds configured"), Result);
    return true;
}

bool HandleConfigureJump(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    ACharacter* CharCDO = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (CharCDO && CharCDO->GetCharacterMovement())
    {
        UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();
        if (Payload->HasField(TEXT("jumpHeight"))) Movement->JumpZVelocity = static_cast<float>(GetJsonNumberField(Payload, TEXT("jumpHeight"), 600.0));
        if (Payload->HasField(TEXT("airControl"))) Movement->AirControl = static_cast<float>(GetJsonNumberField(Payload, TEXT("airControl"), 0.35));
        if (Payload->HasField(TEXT("gravityScale"))) Movement->GravityScale = static_cast<float>(GetJsonNumberField(Payload, TEXT("gravityScale"), 1.0));
        if (Payload->HasField(TEXT("fallingLateralFriction"))) Movement->FallingLateralFriction = static_cast<float>(GetJsonNumberField(Payload, TEXT("fallingLateralFriction"), 0.0));
        if (Payload->HasField(TEXT("maxJumpCount"))) CharCDO->JumpMaxCount = static_cast<int32>(GetJsonNumberField(Payload, TEXT("maxJumpCount"), 1));
        if (Payload->HasField(TEXT("jumpHoldTime"))) CharCDO->JumpMaxHoldTime = static_cast<float>(GetJsonNumberField(Payload, TEXT("jumpHoldTime"), 0.0));
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Jump configured"), Result);
    return true;
}

bool HandleConfigureRotation(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    ACharacter* CharCDO = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (CharCDO && CharCDO->GetCharacterMovement())
    {
        UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();
        if (Payload->HasField(TEXT("orientToMovement"))) Movement->bOrientRotationToMovement = GetJsonBoolField(Payload, TEXT("orientToMovement"), true);
        if (Payload->HasField(TEXT("useControllerRotationYaw"))) CharCDO->bUseControllerRotationYaw = GetJsonBoolField(Payload, TEXT("useControllerRotationYaw"), false);
        if (Payload->HasField(TEXT("useControllerRotationPitch"))) CharCDO->bUseControllerRotationPitch = GetJsonBoolField(Payload, TEXT("useControllerRotationPitch"), false);
        if (Payload->HasField(TEXT("useControllerRotationRoll"))) CharCDO->bUseControllerRotationRoll = GetJsonBoolField(Payload, TEXT("useControllerRotationRoll"), false);
        if (Payload->HasField(TEXT("rotationRate"))) Movement->RotationRate = FRotator(0.0, GetJsonNumberField(Payload, TEXT("rotationRate"), 540.0), 0.0);
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Rotation configured"), Result);
    return true;
}

bool HandleConfigureNavMovement(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    ACharacter* CharCDO = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (CharCDO && CharCDO->GetCharacterMovement())
    {
        UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();
        if (Payload->HasField(TEXT("navAgentRadius"))) Movement->NavAgentProps.AgentRadius = static_cast<float>(GetJsonNumberField(Payload, TEXT("navAgentRadius"), 42.0));
        if (Payload->HasField(TEXT("navAgentHeight"))) Movement->NavAgentProps.AgentHeight = static_cast<float>(GetJsonNumberField(Payload, TEXT("navAgentHeight"), 192.0));
        if (Payload->HasField(TEXT("avoidanceEnabled"))) Movement->bUseRVOAvoidance = GetJsonBoolField(Payload, TEXT("avoidanceEnabled"), false);
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Nav movement configured"), Result);
    return true;
}

bool HandleSetupMovement(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    ACharacter* CharCDO = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (CharCDO && CharCDO->GetCharacterMovement())
    {
        UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();
        if (Payload->HasField(TEXT("walkSpeed"))) Movement->MaxWalkSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("walkSpeed"), 600.0));
        else if (Payload->HasField(TEXT("runSpeed"))) Movement->MaxWalkSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("runSpeed"), 600.0));
        if (Payload->HasField(TEXT("acceleration"))) Movement->MaxAcceleration = static_cast<float>(GetJsonNumberField(Payload, TEXT("acceleration"), 2048.0));
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Movement configured"), Result);
    return true;
}
}
#endif
