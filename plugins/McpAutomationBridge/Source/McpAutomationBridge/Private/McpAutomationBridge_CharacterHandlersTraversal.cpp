#include "McpAutomationBridge_CharacterHandlers.h"

#if WITH_EDITOR
namespace McpCharacterHandlers
{
bool HandleSetupMantling(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    const float MantleHeight = static_cast<float>(GetJsonNumberField(Payload, TEXT("mantleHeight"), 200.0));
    const float MantleReach = static_cast<float>(GetJsonNumberField(Payload, TEXT("mantleReachDistance"), 100.0));
    AddBlueprintVariable(Blueprint, TEXT("bIsMantling"), BoolPinType(), TEXT("Mantling"));
    AddBlueprintVariable(Blueprint, TEXT("bCanMantle"), BoolPinType(), TEXT("Mantling"));
    AddBlueprintVariable(Blueprint, TEXT("MantleHeight"), FloatPinType(), TEXT("Mantling"));
    AddBlueprintVariable(Blueprint, TEXT("MantleReachDistance"), FloatPinType(), TEXT("Mantling"));
    SetBPVarDefaultValue(Blueprint, FName(TEXT("bCanMantle")), TEXT("true"));
    SetBPVarDefaultValue(Blueprint, FName(TEXT("MantleHeight")), FString::SanitizeFloat(MantleHeight));
    SetBPVarDefaultValue(Blueprint, FName(TEXT("MantleReachDistance")), FString::SanitizeFloat(MantleReach));
    AddBlueprintVariable(Blueprint, TEXT("MantleTargetLocation"), VectorPinType(), TEXT("Mantling"));

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("mantleHeight"), MantleHeight);
    Result->SetNumberField(TEXT("mantleReachDistance"), MantleReach);
    Result->SetStringField(TEXT("stateVariable"), TEXT("bIsMantling"));
    Result->SetStringField(TEXT("targetVariable"), TEXT("MantleTargetLocation"));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Mantling system configured with state variables"), Result);
    return true;
}

bool HandleSetupVaulting(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    const float VaultHeight = static_cast<float>(GetJsonNumberField(Payload, TEXT("vaultHeight"), 100.0));
    const float VaultDepth = static_cast<float>(GetJsonNumberField(Payload, TEXT("vaultDepth"), 100.0));
    AddBlueprintVariable(Blueprint, TEXT("bIsVaulting"), BoolPinType(), TEXT("Vaulting"));
    AddBlueprintVariable(Blueprint, TEXT("bCanVault"), BoolPinType(), TEXT("Vaulting"));
    AddBlueprintVariable(Blueprint, TEXT("VaultHeight"), FloatPinType(), TEXT("Vaulting"));
    AddBlueprintVariable(Blueprint, TEXT("VaultDepth"), FloatPinType(), TEXT("Vaulting"));
    SetBPVarDefaultValue(Blueprint, FName(TEXT("bCanVault")), TEXT("true"));
    SetBPVarDefaultValue(Blueprint, FName(TEXT("VaultHeight")), FString::SanitizeFloat(VaultHeight));
    SetBPVarDefaultValue(Blueprint, FName(TEXT("VaultDepth")), FString::SanitizeFloat(VaultDepth));
    AddBlueprintVariable(Blueprint, TEXT("VaultStartLocation"), VectorPinType(), TEXT("Vaulting"));
    AddBlueprintVariable(Blueprint, TEXT("VaultEndLocation"), VectorPinType(), TEXT("Vaulting"));

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("vaultHeight"), VaultHeight);
    Result->SetNumberField(TEXT("vaultDepth"), VaultDepth);
    Result->SetStringField(TEXT("stateVariable"), TEXT("bIsVaulting"));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Vaulting system configured with state variables"), Result);
    return true;
}

bool HandleSetupClimbing(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    const float ClimbSpeed = static_cast<float>(GetJsonNumberField(Payload, TEXT("climbSpeed"), 300.0));
    const FString ClimbableTag = GetJsonStringField(Payload, TEXT("climbableTag"), TEXT("Climbable"));
    AddBlueprintVariable(Blueprint, TEXT("bIsClimbing"), BoolPinType(), TEXT("Climbing"));
    AddBlueprintVariable(Blueprint, TEXT("bCanClimb"), BoolPinType(), TEXT("Climbing"));
    AddBlueprintVariable(Blueprint, TEXT("ClimbSpeed"), FloatPinType(), TEXT("Climbing"));
    AddBlueprintVariable(Blueprint, TEXT("ClimbableTag"), NamePinType(), TEXT("Climbing"));
    AddBlueprintVariable(Blueprint, TEXT("ClimbSurfaceNormal"), VectorPinType(), TEXT("Climbing"));
    SetBPVarDefaultValue(Blueprint, FName(TEXT("bCanClimb")), TEXT("true"));
    SetBPVarDefaultValue(Blueprint, FName(TEXT("ClimbSpeed")), FString::SanitizeFloat(ClimbSpeed));
    SetBPVarDefaultValue(Blueprint, FName(TEXT("ClimbableTag")), ClimbableTag);

    ACharacter* CharCDO = Blueprint->GeneratedClass ? Cast<ACharacter>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
    if (CharCDO && CharCDO->GetCharacterMovement())
    {
        CharCDO->GetCharacterMovement()->MaxCustomMovementSpeed = ClimbSpeed;
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("climbSpeed"), ClimbSpeed);
    Result->SetStringField(TEXT("climbableTag"), ClimbableTag);
    Result->SetStringField(TEXT("stateVariable"), TEXT("bIsClimbing"));
    McpHandlerUtils::AddVerification(Result, Blueprint);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Climbing system configured with state variables"), Result);
    return true;
}
}
#endif
