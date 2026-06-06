#include "McpAutomationBridge_CharacterHandlers.h"

#if WITH_EDITOR
namespace McpCharacterHandlers
{
bool HandleSetupSliding(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    const float SlideSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("slideSpeed"), 800.0));
    const float SlideDuration = static_cast<float>(GetJsonNumberField(Payload, TEXT("slideDuration"), 1.0));
    const float SlideCooldown = static_cast<float>(GetJsonNumberField(Payload, TEXT("slideCooldown"), 0.5));
    AddBlueprintVariable(Blueprint, TEXT("bIsSliding"), BoolPinType(), TEXT("Sliding"));
    AddBlueprintVariable(Blueprint, TEXT("bCanSlide"), BoolPinType(), TEXT("Sliding"));
    AddBlueprintVariable(Blueprint, TEXT("SlideSpeed"), FloatPinType(), TEXT("Sliding"));
    AddBlueprintVariable(Blueprint, TEXT("SlideDuration"), FloatPinType(), TEXT("Sliding"));
    AddBlueprintVariable(Blueprint, TEXT("SlideCooldown"), FloatPinType(), TEXT("Sliding"));
    AddBlueprintVariable(Blueprint, TEXT("SlideTimeRemaining"), FloatPinType(), TEXT("Sliding"));
    AddBlueprintVariable(Blueprint, TEXT("SlideCooldownRemaining"), FloatPinType(), TEXT("Sliding"));
    SetBPVarDefaultValue(Blueprint, FName(TEXT("bCanSlide")), TEXT("true"));
    SetBPVarDefaultValue(Blueprint, FName(TEXT("SlideSpeed")), FString::SanitizeFloat(SlideSpeed));
    SetBPVarDefaultValue(Blueprint, FName(TEXT("SlideDuration")), FString::SanitizeFloat(SlideDuration));
    SetBPVarDefaultValue(Blueprint, FName(TEXT("SlideCooldown")), FString::SanitizeFloat(SlideCooldown));

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("slideSpeed"), SlideSpeed);
    Result->SetNumberField(TEXT("slideDuration"), SlideDuration);
    Result->SetNumberField(TEXT("slideCooldown"), SlideCooldown);
    Result->SetStringField(TEXT("stateVariable"), TEXT("bIsSliding"));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Sliding system configured with state and timing variables"), Result);
    return true;
}

bool HandleSetupWallRunning(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    const float WallRunSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("wallRunSpeed"), 600.0));
    const float WallRunDuration = static_cast<float>(GetJsonNumberField(Payload, TEXT("wallRunDuration"), 2.0));
    const float WallRunGravity = static_cast<float>(GetJsonNumberField(Payload, TEXT("wallRunGravityScale"), 0.25));
    AddBlueprintVariable(Blueprint, TEXT("bIsWallRunning"), BoolPinType(), TEXT("Wall Running"));
    AddBlueprintVariable(Blueprint, TEXT("bIsWallRunningLeft"), BoolPinType(), TEXT("Wall Running"));
    AddBlueprintVariable(Blueprint, TEXT("bIsWallRunningRight"), BoolPinType(), TEXT("Wall Running"));
    AddBlueprintVariable(Blueprint, TEXT("WallRunSpeed"), FloatPinType(), TEXT("Wall Running"));
    AddBlueprintVariable(Blueprint, TEXT("WallRunDuration"), FloatPinType(), TEXT("Wall Running"));
    AddBlueprintVariable(Blueprint, TEXT("WallRunGravityScale"), FloatPinType(), TEXT("Wall Running"));
    AddBlueprintVariable(Blueprint, TEXT("WallRunTimeRemaining"), FloatPinType(), TEXT("Wall Running"));
    AddBlueprintVariable(Blueprint, TEXT("WallRunNormal"), VectorPinType(), TEXT("Wall Running"));

    ACharacter* CharCDO = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (CharCDO && CharCDO->GetCharacterMovement())
    {
        CharCDO->GetCharacterMovement()->MaxCustomMovementSpeed = WallRunSpeed;
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("wallRunSpeed"), WallRunSpeed);
    Result->SetNumberField(TEXT("wallRunDuration"), WallRunDuration);
    Result->SetNumberField(TEXT("wallRunGravityScale"), WallRunGravity);
    Result->SetStringField(TEXT("stateVariable"), TEXT("bIsWallRunning"));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Wall running system configured with state variables"), Result);
    return true;
}

bool HandleSetupGrappling(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    const float GrappleRange = static_cast<float>(GetJsonNumberField(Payload, TEXT("grappleRange"), 2000.0));
    const float GrappleSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("grappleSpeed"), 1500.0));
    const FString GrappleTarget = GetJsonStringField(Payload, TEXT("grappleTargetTag"), TEXT("Grapple"));
    AddBlueprintVariable(Blueprint, TEXT("bIsGrappling"), BoolPinType(), TEXT("Grappling"));
    AddBlueprintVariable(Blueprint, TEXT("bHasGrappleTarget"), BoolPinType(), TEXT("Grappling"));
    AddBlueprintVariable(Blueprint, TEXT("GrappleRange"), FloatPinType(), TEXT("Grappling"));
    AddBlueprintVariable(Blueprint, TEXT("GrappleSpeed"), FloatPinType(), TEXT("Grappling"));
    AddBlueprintVariable(Blueprint, TEXT("GrappleTargetTag"), NamePinType(), TEXT("Grappling"));
    AddBlueprintVariable(Blueprint, TEXT("GrappleTargetLocation"), VectorPinType(), TEXT("Grappling"));

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("grappleRange"), GrappleRange);
    Result->SetNumberField(TEXT("grappleSpeed"), GrappleSpeed);
    Result->SetStringField(TEXT("grappleTargetTag"), GrappleTarget);
    Result->SetStringField(TEXT("stateVariable"), TEXT("bIsGrappling"));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Grappling system configured with state variables"), Result);
    return true;
}
}
#endif
