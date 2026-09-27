#include "Domains/Character/McpAutomationBridge_CharacterHandlers.h"

namespace McpCharacterHandlers
{
static bool HandleMovementScalar(
    UMcpAutomationBridgeSubsystem* Self,
    const FString& RequestId,
    const TSharedPtr<FJsonObject>& Payload,
    FCharacterSocket Socket,
    const TCHAR* InputField,
    double DefaultValue,
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

    const double Value = GetJsonNumberField(Payload, FString(InputField), DefaultValue);
    ACharacter* CharCDO = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (CharCDO && CharCDO->GetCharacterMovement())
    {
        UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();
        (Movement->*Property) = static_cast<float>(Value);
    }

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
    return HandleMovementScalar(Self, RequestId, Payload, Socket, TEXT("walkSpeed"), 600.0, TEXT("walkSpeed"), TEXT("Walk speed set"), &UCharacterMovementComponent::MaxWalkSpeed);
}

bool HandleSetJumpHeight(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    return HandleMovementScalar(Self, RequestId, Payload, Socket, TEXT("jumpHeight"), 600.0, TEXT("jumpHeight"), TEXT("Jump height set"), &UCharacterMovementComponent::JumpZVelocity);
}

bool HandleSetGravityScale(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    return HandleMovementScalar(Self, RequestId, Payload, Socket, TEXT("gravityScale"), 1.0, TEXT("gravityScale"), TEXT("Gravity scale set"), &UCharacterMovementComponent::GravityScale);
}

bool HandleSetGroundFriction(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    return HandleMovementScalar(Self, RequestId, Payload, Socket, TEXT("groundFriction"), 8.0, TEXT("groundFriction"), TEXT("Ground friction set"), &UCharacterMovementComponent::GroundFriction);
}

bool HandleSetBrakingDeceleration(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    return HandleMovementScalar(Self, RequestId, Payload, Socket, TEXT("brakingDeceleration"), 2048.0, TEXT("brakingDeceleration"), TEXT("Braking deceleration set"), &UCharacterMovementComponent::BrakingDecelerationWalking);
}

bool HandleConfigureCrouch(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    const double CrouchSpeed = GetJsonNumberField(Payload, TEXT("crouchSpeed"), 300.0);
    const double CrouchedHalfHeight = GetJsonNumberField(Payload, TEXT("crouchedHalfHeight"), 44.0);
    const bool CanCrouch = GetJsonBoolField(Payload, TEXT("canCrouch"), true);
    ACharacter* CharCDO = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (CharCDO && CharCDO->GetCharacterMovement())
    {
        UCharacterMovementComponent* Movement = CharCDO->GetCharacterMovement();
        Movement->MaxWalkSpeedCrouched = static_cast<float>(CrouchSpeed);
        Movement->SetCrouchedHalfHeight(static_cast<float>(CrouchedHalfHeight));
        Movement->NavAgentProps.bCanCrouch = CanCrouch;
    }

    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    McpSafeCompileBlueprint(Blueprint); // compile so the added variables are usable (dogfood #39)
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("crouchSpeed"), CrouchSpeed);
    Result->SetNumberField(TEXT("crouchedHalfHeight"), CrouchedHalfHeight);
    Result->SetBoolField(TEXT("canCrouch"), CanCrouch);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Crouch configured"), Result);
    return true;
}

}
