#include "Domains/Character/McpAutomationBridge_CharacterHandlers.h"

#if WITH_EDITOR
namespace McpCharacterHandlers
{
bool HandleSetupFootstepSystem(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = RequireCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    const bool Enabled = GetJsonBoolField(Payload, TEXT("footstepEnabled"), true);
    const FString SocketLeft = GetJsonStringField(Payload, TEXT("footstepSocketLeft"), TEXT("foot_l"));
    const FString SocketRight = GetJsonStringField(Payload, TEXT("footstepSocketRight"), TEXT("foot_r"));
    const float TraceDistance = static_cast<float>(GetJsonNumberField(Payload, TEXT("footstepTraceDistance"), 50.0));
    AddBlueprintVariable(Blueprint, TEXT("bFootstepSystemEnabled"), BoolPinType(), TEXT("Footsteps"));
    AddBlueprintVariable(Blueprint, TEXT("FootstepSocketLeft"), NamePinType(), TEXT("Footsteps"));
    AddBlueprintVariable(Blueprint, TEXT("FootstepSocketRight"), NamePinType(), TEXT("Footsteps"));
    AddBlueprintVariable(Blueprint, TEXT("FootstepTraceDistance"), FloatPinType(), TEXT("Footsteps"));
    // Every one of these was read from the payload, echoed back as applied, and
    // never written, so the graph saw false/None/0 whatever the caller asked for.
    SetBPVarDefaultValue(Blueprint, FName(TEXT("bFootstepSystemEnabled")), Enabled ? TEXT("true") : TEXT("false"));
    SetBPVarDefaultValue(Blueprint, FName(TEXT("FootstepSocketLeft")), SocketLeft);
    SetBPVarDefaultValue(Blueprint, FName(TEXT("FootstepSocketRight")), SocketRight);
    SetBPVarDefaultValue(Blueprint, FName(TEXT("FootstepTraceDistance")), FString::SanitizeFloat(TraceDistance));

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetBoolField(TEXT("enabled"), Enabled);
    Result->SetStringField(TEXT("socketLeft"), SocketLeft);
    Result->SetStringField(TEXT("socketRight"), SocketRight);
    Result->SetNumberField(TEXT("traceDistance"), TraceDistance);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Footstep system configured with tracking variables"), Result);
    return true;
}

bool HandleMapSurfaceToSound(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    const FString SurfaceType = GetJsonStringField(Payload, TEXT("surfaceType"));
    FEdGraphPinType MapPinType;
    MapPinType.PinCategory = UEdGraphSchema_K2::PC_Name;
    MapPinType.ContainerType = EPinContainerType::Map;
    MapPinType.PinValueType.TerminalCategory = UEdGraphSchema_K2::PC_SoftObject;
    AddBlueprintVariable(Blueprint, TEXT("FootstepSoundMap"), MapPinType, TEXT("Footsteps"));

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetStringField(TEXT("surfaceType"), SurfaceType);
    Result->SetStringField(TEXT("mapVariable"), TEXT("FootstepSoundMap"));
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Surface mapping configured with map variable"), Result);
    return true;
}

bool HandleConfigureFootstepFx(UMcpAutomationBridgeSubsystem* Self, const FString& RequestId, const TSharedPtr<FJsonObject>& Payload, FCharacterSocket Socket)
{
    const FString BlueprintPath = GetJsonStringField(Payload, TEXT("blueprintPath"));
    UBlueprint* Blueprint = LoadCharacterBlueprint(Self, RequestId, BlueprintPath, Socket);
    if (!Blueprint)
    {
        return true;
    }

    const float VolumeMultiplier = static_cast<float>(GetJsonNumberField(Payload, TEXT("volumeMultiplier"), 1.0));
    const float ParticleScale = static_cast<float>(GetJsonNumberField(Payload, TEXT("particleScale"), 1.0));
    AddBlueprintVariable(Blueprint, TEXT("FootstepVolumeMultiplier"), FloatPinType(), TEXT("Footsteps"));
    AddBlueprintVariable(Blueprint, TEXT("FootstepParticleScale"), FloatPinType(), TEXT("Footsteps"));
    // Read, echoed, and dropped: the scale variables stayed at 0 while the reply
    // reported the requested multipliers.
    SetBPVarDefaultValue(Blueprint, FName(TEXT("FootstepVolumeMultiplier")), FString::SanitizeFloat(VolumeMultiplier));
    SetBPVarDefaultValue(Blueprint, FName(TEXT("FootstepParticleScale")), FString::SanitizeFloat(ParticleScale));

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
    Result->SetNumberField(TEXT("volumeMultiplier"), VolumeMultiplier);
    Result->SetNumberField(TEXT("particleScale"), ParticleScale);
    Self->SendAutomationResponse(Socket, RequestId, true, TEXT("Footstep FX configured with scale variables"), Result);
    return true;
}
}
#endif
