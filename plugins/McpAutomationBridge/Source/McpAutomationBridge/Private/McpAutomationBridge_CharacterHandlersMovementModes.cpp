#include "McpAutomationBridge_CharacterHandlers.h"

#if WITH_EDITOR
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
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(FString(ResponseField), Value);
    Self->SendAutomationResponse(Socket, RequestId, true, Message, Result);
    return true;
}

bool HandleAddCustomMovementMode(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    const FString ModeName = GetJsonStringField(Payload, TEXT("modeName"), TEXT("Custom"));
    const int32 ModeId = static_cast<int32>(GetJsonNumberField(Payload, TEXT("modeId"), 0));
    const float CustomSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("customSpeed"), 600.0));
    const FString StateVarName = FString::Printf(TEXT("bIsIn%sMode"), *ModeName);
    const FString ModeIdVarName = FString::Printf(TEXT("CustomModeId_%s"), *ModeName);
    const FString SpeedVarName = FString::Printf(TEXT("%sSpeed"), *ModeName);

    AddBlueprintVariable(Blueprint, StateVarName, BoolPinType(), TEXT("Movement States"));
    AddBlueprintVariable(Blueprint, ModeIdVarName, IntPinType(), TEXT("Movement States"));
    AddBlueprintVariable(Blueprint, SpeedVarName, FloatPinType(), TEXT("Movement States"));
    SetBPVarDefaultValue(Blueprint, FName(*ModeIdVarName), FString::FromInt(ModeId));
    SetBPVarDefaultValue(Blueprint, FName(*SpeedVarName), FString::SanitizeFloat(CustomSpeed));

    ACharacter* CharCDO = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (CharCDO && CharCDO->GetCharacterMovement())
    {
        CharCDO->GetCharacterMovement()->MaxCustomMovementSpeed = CustomSpeed;
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetStringField(TEXT("modeName"), ModeName);
    Result->SetNumberField(TEXT("modeId"), ModeId);
    Result->SetStringField(TEXT("stateVariable"), StateVarName);
    Result->SetStringField(TEXT("speedVariable"), SpeedVarName);
    Result->SetNumberField(TEXT("customSpeed"), CustomSpeed);
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Custom movement mode added with state tracking variables"), Result);
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
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("crouchSpeed"), CrouchSpeed);
    Result->SetNumberField(TEXT("crouchedHalfHeight"), CrouchedHalfHeight);
    Result->SetBoolField(TEXT("canCrouch"), CanCrouch);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Crouch configured"), Result);
    return true;
}

bool HandleConfigureSprint(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    const double SprintSpeed = GetJsonNumberField(Payload, TEXT("sprintSpeed"), 900.0);
    AddBlueprintVariable(Blueprint, TEXT("bIsSprinting"), BoolPinType(), TEXT("Sprint"));
    AddBlueprintVariable(Blueprint, TEXT("SprintSpeed"), FloatPinType(), TEXT("Sprint"));

    ACharacter* CharCDO = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (CharCDO && CharCDO->GetCharacterMovement())
    {
        CharCDO->GetCharacterMovement()->MaxCustomMovementSpeed = static_cast<float>(SprintSpeed);
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("sprintSpeed"), SprintSpeed);
    Result->SetStringField(TEXT("stateVariable"), TEXT("bIsSprinting"));
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Sprint configured with state variables"), Result);
    return true;
}
}
#endif
