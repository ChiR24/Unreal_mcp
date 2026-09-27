#include "Domains/Character/McpAutomationBridge_CharacterHandlers.h"

namespace McpCharacterHandlers
{
static bool HandleMovementScalar(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FCharacterSocket Socket,
    const TCHAR* InputField,
    const TCHAR* ResponseField,
    const FString& Message,
    float UCharacterMovementComponent::* Property)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    // An omitted value used to write the engine default over whatever the
    // Blueprint had, and a non-Character Blueprint changed nothing; both
    // answered success.
    double Value = 0.0;
    if (!Payload->TryGetNumberField(FString(InputField), Value))
    {
        Self->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("%s is required"), InputField), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    ACharacter* CharCDO = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (!CharCDO || !CharCDO->GetCharacterMovement())
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("%s is not a Character Blueprint with a movement component"), *BlueprintPath), TEXT("NOT_A_CHARACTER"));
        return true;
    }
    (CharCDO->GetCharacterMovement()->*Property) = static_cast<float>(Value);

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint); // compile so the added variables are usable (dogfood #39)
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(FString(ResponseField), Value);
    Self->SendAutomationResponse(Socket, RequestId, true, Message, Result);
    return true;
}

bool HandleSetWalkSpeed(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    return HandleMovementScalar(Self, RequestId, Payload, Socket, TEXT("walkSpeed"), TEXT("walkSpeed"), TEXT("Walk speed set"), &UCharacterMovementComponent::MaxWalkSpeed);
}

bool HandleSetJumpHeight(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    return HandleMovementScalar(Self, RequestId, Payload, Socket, TEXT("jumpHeight"), TEXT("jumpHeight"), TEXT("Jump height set"), &UCharacterMovementComponent::JumpZVelocity);
}

bool HandleSetGravityScale(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    return HandleMovementScalar(Self, RequestId, Payload, Socket, TEXT("gravityScale"), TEXT("gravityScale"), TEXT("Gravity scale set"), &UCharacterMovementComponent::GravityScale);
}

bool HandleSetGroundFriction(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    return HandleMovementScalar(Self, RequestId, Payload, Socket, TEXT("groundFriction"), TEXT("groundFriction"), TEXT("Ground friction set"), &UCharacterMovementComponent::GroundFriction);
}

bool HandleSetBrakingDeceleration(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    return HandleMovementScalar(Self, RequestId, Payload, Socket, TEXT("brakingDeceleration"), TEXT("brakingDeceleration"), TEXT("Braking deceleration set"), &UCharacterMovementComponent::BrakingDecelerationWalking);
}

bool HandleConfigureCrouch(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    // Each field changes only when sent: setting canCrouch alone used to reset
    // the crouch speed to 300 and the crouched half-height to 44.
    ACharacter* CharCDO = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    UCharacterMovementComponent* Movement = CharCDO ? CharCDO->GetCharacterMovement() : nullptr;
    if (!Movement)
    {
        Self->SendAutomationError(Socket, RequestId,
            FString::Printf(TEXT("%s is not a Character Blueprint with a movement component"), *BlueprintPath), TEXT("NOT_A_CHARACTER"));
        return true;
    }
    if (Payload->HasField(TEXT("crouchSpeed")))
    {
        Movement->MaxWalkSpeedCrouched = static_cast<float>(GetJsonNumberField(Payload, TEXT("crouchSpeed"), 300.0));
    }
    if (Payload->HasField(TEXT("crouchedHalfHeight")))
    {
        Movement->SetCrouchedHalfHeight(static_cast<float>(GetJsonNumberField(Payload, TEXT("crouchedHalfHeight"), 44.0)));
    }
    if (Payload->HasField(TEXT("canCrouch")))
    {
        Movement->NavAgentProps.bCanCrouch = GetJsonBoolField(Payload, TEXT("canCrouch"), true);
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint); // compile so the added variables are usable (dogfood #39)
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("crouchSpeed"), Movement->MaxWalkSpeedCrouched);
    Result->SetNumberField(TEXT("crouchedHalfHeight"), Movement->GetCrouchedHalfHeight());
    Result->SetBoolField(TEXT("canCrouch"), Movement->NavAgentProps.bCanCrouch);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Crouch configured"), Result);
    return true;
}

}
