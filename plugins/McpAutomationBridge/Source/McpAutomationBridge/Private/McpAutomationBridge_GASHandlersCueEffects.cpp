#include "McpAutomationBridge_GASPayloadFields.h"
#include "McpAutomationBridge_GASRequestContext.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpHandlerUtils.h"

#if WITH_EDITOR && MCP_HAS_GAS
#include "Dom/JsonValue.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#endif

#if WITH_EDITOR && MCP_HAS_GAS
namespace McpGASHandlers
{
bool HandleGASCueEffects(const FGASRequestContext& Context, const FString& SubAction)
{
    UMcpAutomationBridgeSubsystem* Bridge = Context.Subsystem;
    const FString& RequestId = Context.RequestId;
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket = Context.RequestingSocket;
    const TSharedPtr<FJsonObject>& Payload = Context.Payload;
    const FString& Name = Context.Name;
    const FString& Path = Context.Path;
    const FString& BlueprintPath = Context.BlueprintPath;
    const FString& AssetPath = Context.AssetPath;

    if (SubAction == TEXT("set_cue_effects"))
    {
        if (BlueprintPath.IsEmpty())
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        FString ParticleSystem = GetGASStringFieldWithFallback(Payload, TEXT("particleSystem"), TEXT("particleSystemPath"));
        FString Sound = GetGASStringFieldWithFallback(Payload, TEXT("sound"), TEXT("soundPath"));
        FString CameraShake = GetGASStringFieldWithFallback(Payload, TEXT("cameraShake"), TEXT("cameraShakePath"));
        FString Decal = GetStringFieldGAS(Payload, TEXT("decalPath"));

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint || !Blueprint->GeneratedClass)
        {
            Bridge->SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintPath), TEXT("NOT_FOUND"));
            return true;
        }

        TArray<FString> VariablesAdded;

        // Add soft object reference variables for each effect type
        // Using SoftObjectPath for asset references that can be loaded on demand

        if (!ParticleSystem.IsEmpty())
        {
            // Add Niagara/Particle system soft reference
            FEdGraphPinType ParticlePinType;
            ParticlePinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
            ParticlePinType.PinSubCategoryObject = UObject::StaticClass();

            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CueParticleSystem"), ParticlePinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("CueParticleSystem"), nullptr, FText::FromString(TEXT("Cue Effects")));

            // Also add a string path variable for easy configuration
            FEdGraphPinType StringPinType;
            StringPinType.PinCategory = UEdGraphSchema_K2::PC_String;
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("ParticleSystemPath"), StringPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("ParticleSystemPath"), nullptr, FText::FromString(TEXT("Cue Effects")));

            VariablesAdded.Add(TEXT("CueParticleSystem"));
            VariablesAdded.Add(TEXT("ParticleSystemPath"));
        }

        if (!Sound.IsEmpty())
        {
            // Add sound cue/wave soft reference
            FEdGraphPinType SoundPinType;
            SoundPinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
            SoundPinType.PinSubCategoryObject = UObject::StaticClass();

            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CueSound"), SoundPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("CueSound"), nullptr, FText::FromString(TEXT("Cue Effects")));

            // String path for configuration
            FEdGraphPinType StringPinType;
            StringPinType.PinCategory = UEdGraphSchema_K2::PC_String;
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("SoundPath"), StringPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("SoundPath"), nullptr, FText::FromString(TEXT("Cue Effects")));

            // Volume and pitch multipliers
            FEdGraphPinType FloatPinType;
            FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
            FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;

            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("SoundVolumeMultiplier"), FloatPinType);
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("SoundPitchMultiplier"), FloatPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("SoundVolumeMultiplier"), nullptr, FText::FromString(TEXT("Cue Effects")));
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("SoundPitchMultiplier"), nullptr, FText::FromString(TEXT("Cue Effects")));

            VariablesAdded.Add(TEXT("CueSound"));
            VariablesAdded.Add(TEXT("SoundPath"));
            VariablesAdded.Add(TEXT("SoundVolumeMultiplier"));
            VariablesAdded.Add(TEXT("SoundPitchMultiplier"));
        }

        if (!CameraShake.IsEmpty())
        {
            // Add camera shake class reference
            FEdGraphPinType ShakePinType;
            ShakePinType.PinCategory = UEdGraphSchema_K2::PC_SoftClass;
            ShakePinType.PinSubCategoryObject = UObject::StaticClass();

            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CueCameraShakeClass"), ShakePinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("CueCameraShakeClass"), nullptr, FText::FromString(TEXT("Cue Effects")));

            // String path for configuration
            FEdGraphPinType StringPinType;
            StringPinType.PinCategory = UEdGraphSchema_K2::PC_String;
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CameraShakePath"), StringPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("CameraShakePath"), nullptr, FText::FromString(TEXT("Cue Effects")));

            // Shake scale
            FEdGraphPinType FloatPinType;
            FloatPinType.PinCategory = UEdGraphSchema_K2::PC_Real;
            FloatPinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;

            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CameraShakeScale"), FloatPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("CameraShakeScale"), nullptr, FText::FromString(TEXT("Cue Effects")));

            VariablesAdded.Add(TEXT("CueCameraShakeClass"));
            VariablesAdded.Add(TEXT("CameraShakePath"));
            VariablesAdded.Add(TEXT("CameraShakeScale"));
        }

        if (!Decal.IsEmpty())
        {
            FEdGraphPinType DecalPinType;
            DecalPinType.PinCategory = UEdGraphSchema_K2::PC_SoftObject;
            DecalPinType.PinSubCategoryObject = UObject::StaticClass();
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("CueDecal"), DecalPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("CueDecal"), nullptr, FText::FromString(TEXT("Cue Effects")));

            FEdGraphPinType StringPinType;
            StringPinType.PinCategory = UEdGraphSchema_K2::PC_String;
            FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("DecalPath"), StringPinType);
            FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("DecalPath"), nullptr, FText::FromString(TEXT("Cue Effects")));

            VariablesAdded.Add(TEXT("CueDecal"));
            VariablesAdded.Add(TEXT("DecalPath"));
        }

        // Add a master enable flag
        FEdGraphPinType BoolPinType;
        BoolPinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        FBlueprintEditorUtils::AddMemberVariable(Blueprint, TEXT("bCueEffectsEnabled"), BoolPinType);
        FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, TEXT("bCueEffectsEnabled"), nullptr, FText::FromString(TEXT("Cue Effects")));
        VariablesAdded.Add(TEXT("bCueEffectsEnabled"));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), BlueprintPath);
        if (!ParticleSystem.IsEmpty()) Result->SetStringField(TEXT("particleSystem"), ParticleSystem);
        if (!Sound.IsEmpty()) Result->SetStringField(TEXT("sound"), Sound);
        if (!CameraShake.IsEmpty()) Result->SetStringField(TEXT("cameraShake"), CameraShake);
        if (!Decal.IsEmpty()) Result->SetStringField(TEXT("decalPath"), Decal);

        TArray<TSharedPtr<FJsonValue>> VarsArray;
        for (const FString& VarName : VariablesAdded)
        {
            VarsArray.Add(MakeShared<FJsonValueString>(VarName));
        }
        Result->SetArrayField(TEXT("variablesAdded"), VarsArray);
        Result->SetNumberField(TEXT("variableCount"), VariablesAdded.Num());

        Bridge->SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Cue effect variables added to blueprint"), Result);
        return true;
    }


    return false;
}
}
#endif
