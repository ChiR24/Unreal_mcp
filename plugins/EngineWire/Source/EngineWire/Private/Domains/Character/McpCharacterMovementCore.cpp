#include "Domains/Character/McpCharacterHandlers.h"

namespace McpCharacterHandlers
{
bool HandleConfigureMovementSpeeds(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
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
        // Jump params are documented on configure_character and callers pass them
        // alongside the movement speeds. Previously they were accepted and silently
        // dropped here (only configure_jump applied them), so a batch call looked
        // successful while jumpZVelocity stayed at the default.
        if (Payload->HasField(TEXT("jumpHeight"))) Movement->JumpZVelocity = static_cast<float>(GetJsonNumberField(Payload, TEXT("jumpHeight"), 420.0));
        if (Payload->HasField(TEXT("airControl"))) Movement->AirControl = static_cast<float>(GetJsonNumberField(Payload, TEXT("airControl"), 0.05));
        if (Payload->HasField(TEXT("gravityScale"))) Movement->GravityScale = static_cast<float>(GetJsonNumberField(Payload, TEXT("gravityScale"), 1.0));
        if (Payload->HasField(TEXT("fallingLateralFriction"))) Movement->FallingLateralFriction = static_cast<float>(GetJsonNumberField(Payload, TEXT("fallingLateralFriction"), 0.0));
        if (Payload->HasField(TEXT("maxJumpCount"))) CharCDO->JumpMaxCount = static_cast<int32>(GetJsonNumberField(Payload, TEXT("maxJumpCount"), 1));
        if (Payload->HasField(TEXT("jumpHoldTime"))) CharCDO->JumpMaxHoldTime = static_cast<float>(GetJsonNumberField(Payload, TEXT("jumpHoldTime"), 0.0));
        AppliedWalkSpeed = Movement->MaxWalkSpeed;
        bHasAppliedWalkSpeed = true;
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    if (!CommitCharacterEdit(Self, RequestId, Blueprint, Socket)) return true;
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    if (CharCDO && CharCDO->GetCharacterMovement())
    {
        // Echo the values now on the CDO (dogfood #39).
        const UCharacterMovementComponent* Applied = CharCDO->GetCharacterMovement();
        Result->SetNumberField(TEXT("jumpZVelocity"), Applied->JumpZVelocity);
        Result->SetNumberField(TEXT("airControl"), Applied->AirControl);
        Result->SetNumberField(TEXT("gravityScale"), Applied->GravityScale);
        Result->SetNumberField(TEXT("fallingLateralFriction"), Applied->FallingLateralFriction);
        Result->SetNumberField(TEXT("maxJumpCount"), CharCDO->JumpMaxCount);
        Result->SetNumberField(TEXT("jumpHoldTime"), CharCDO->JumpMaxHoldTime);
        // crouch/swim/fly/accel/braking/friction were applied above but never
        // echoed, so a caller batching them could not tell them apart from the
        // ones this action ignores. Report every speed it writes.
        Result->SetNumberField(TEXT("crouchSpeed"), Applied->MaxWalkSpeedCrouched);
        Result->SetNumberField(TEXT("swimSpeed"), Applied->MaxSwimSpeed);
        Result->SetNumberField(TEXT("flySpeed"), Applied->MaxFlySpeed);
        Result->SetNumberField(TEXT("acceleration"), Applied->MaxAcceleration);
        Result->SetNumberField(TEXT("deceleration"), Applied->BrakingDecelerationWalking);
        Result->SetNumberField(TEXT("groundFriction"), Applied->GroundFriction);
    }
    if (Payload->HasField(TEXT("runSpeed")))
    {
        Result->SetBoolField(TEXT("runSpeedApplied"), bRunSpeedApplied);
        // The old log line sent callers to a sprint action that does not exist.
        if (!bRunSpeedApplied)
        {
            TArray<TSharedPtr<FJsonValue>> Warnings;
            Warnings.Add(MakeShared<FJsonValueString>(TEXT("runSpeed was left out: a Character has one walk speed, and walkSpeed "
                "set it. A sprint is game logic that raises MaxWalkSpeed while a key is held (manage_blueprint edit_graph).")));
            Result->SetArrayField(TEXT("warnings"), Warnings);
        }
    }
    if (bHasAppliedWalkSpeed)
    {
        Result->SetNumberField(TEXT("walkSpeed"), AppliedWalkSpeed);
    }
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Movement speeds configured"), Result);
    return true;
}

bool HandleConfigureJump(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
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
    if (!CommitCharacterEdit(Self, RequestId, Blueprint, Socket)) return true;
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    if (CharCDO && CharCDO->GetCharacterMovement())
    {
        // Echo the values now on the CDO (dogfood #39).
        const UCharacterMovementComponent* Applied = CharCDO->GetCharacterMovement();
        Result->SetNumberField(TEXT("jumpZVelocity"), Applied->JumpZVelocity);
        Result->SetNumberField(TEXT("airControl"), Applied->AirControl);
        Result->SetNumberField(TEXT("gravityScale"), Applied->GravityScale);
        Result->SetNumberField(TEXT("fallingLateralFriction"), Applied->FallingLateralFriction);
        Result->SetNumberField(TEXT("maxJumpCount"), CharCDO->JumpMaxCount);
        Result->SetNumberField(TEXT("jumpHoldTime"), CharCDO->JumpMaxHoldTime);
    }
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Jump configured"), Result);
    return true;
}

bool HandleConfigureRotation(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
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
    if (!CommitCharacterEdit(Self, RequestId, Blueprint, Socket)) return true;
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    if (CharCDO && CharCDO->GetCharacterMovement())
    {
        // This answered "Rotation configured" with no evidence at all, so a
        // caller could not tell an applied flag from a silently dropped one.
        const UCharacterMovementComponent* Applied = CharCDO->GetCharacterMovement();
        Result->SetBoolField(TEXT("orientToMovement"), Applied->bOrientRotationToMovement);
        Result->SetNumberField(TEXT("rotationRate"), Applied->RotationRate.Yaw);
        Result->SetBoolField(TEXT("useControllerRotationYaw"), CharCDO->bUseControllerRotationYaw);
        Result->SetBoolField(TEXT("useControllerRotationPitch"), CharCDO->bUseControllerRotationPitch);
        Result->SetBoolField(TEXT("useControllerRotationRoll"), CharCDO->bUseControllerRotationRoll);
    }
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Rotation configured"), Result);
    return true;
}

bool HandleConfigureNavMovement(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
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
        // By default the spawned character copies its capsule size over the nav agent size, so a size set here was
        // gone before any path was found; an explicit size stops that copy.
        if (Payload->HasField(TEXT("navAgentRadius")) || Payload->HasField(TEXT("navAgentHeight"))) Movement->SetUpdateNavAgentWithOwnersCollisions(false);
        if (Payload->HasField(TEXT("avoidanceEnabled"))) Movement->bUseRVOAvoidance = GetJsonBoolField(Payload, TEXT("avoidanceEnabled"), false);
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    if (!CommitCharacterEdit(Self, RequestId, Blueprint, Socket)) return true;
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    // Read back from the defaults: it answered "configured" with no value at all.
    if (const UCharacterMovementComponent* Applied = CharCDO ? CharCDO->GetCharacterMovement() : nullptr)
    {
        Result->SetNumberField(TEXT("navAgentRadius"), Applied->NavAgentProps.AgentRadius);
        Result->SetNumberField(TEXT("navAgentHeight"), Applied->NavAgentProps.AgentHeight);
        Result->SetBoolField(TEXT("avoidanceEnabled"), Applied->bUseRVOAvoidance);
        Result->SetBoolField(TEXT("navAgentFollowsCapsule"), Applied->ShouldUpdateNavAgentWithOwnersCollision());
    }
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Nav movement configured"), Result);
    return true;
}

bool HandleSetupMovement(UEngineWireSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    // LoadCharacterBlueprint already refused a Blueprint without CharacterMovement (NOT_A_CHARACTER).
    UCharacterMovementComponent* Movement = Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject())->GetCharacterMovement();
    if (Payload->HasField(TEXT("walkSpeed"))) Movement->MaxWalkSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("walkSpeed"), 600.0));
    else if (Payload->HasField(TEXT("runSpeed"))) Movement->MaxWalkSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("runSpeed"), 600.0));
    if (Payload->HasField(TEXT("acceleration"))) Movement->MaxAcceleration = static_cast<float>(GetJsonNumberField(Payload, TEXT("acceleration"), 2048.0));

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    if (!CommitCharacterEdit(Self, RequestId, Blueprint, Socket)) return true;
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("maxWalkSpeed"), Movement->MaxWalkSpeed);
    Result->SetNumberField(TEXT("maxAcceleration"), Movement->MaxAcceleration);
    if (Payload->HasField(TEXT("walkSpeed")) && Payload->HasField(TEXT("runSpeed")))
    {
        TArray<TSharedPtr<FJsonValue>> Warnings;
        Warnings.Add(MakeShared<FJsonValueString>(TEXT("runSpeed was left out: a Character has one walk speed, and walkSpeed "
            "set it. A sprint is game logic that raises MaxWalkSpeed while a key is held (manage_blueprint edit_graph).")));
        Result->SetArrayField(TEXT("warnings"), Warnings);
    }
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Movement configured"), Result);
    return true;
}
}
