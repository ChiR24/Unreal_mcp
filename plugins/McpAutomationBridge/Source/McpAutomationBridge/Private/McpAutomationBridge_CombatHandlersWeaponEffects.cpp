#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_CombatHandlersPrivate.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
bool FCombatActionContext::HandleWeaponEffects() const
{
    if (SubAction == TEXT("configure_muzzle_flash"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found."), TEXT("NOT_FOUND"));
            return true;
        }

        FString ParticlePath = GetStringFieldCombat(Payload, TEXT("muzzleFlashParticlePath"));
        double Scale = GetNumberFieldCombat(Payload, TEXT("muzzleFlashScale"), 1.0);
        FString SoundPath = GetStringFieldCombat(Payload, TEXT("muzzleSoundPath"));

        // Add variables for muzzle flash config
        AddBlueprintVariableCombat(Blueprint, TEXT("MuzzleFlashParticlePath"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("MuzzleFlashScale"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("MuzzleSoundPath"), MakeStringPinType());

        // Load and add object references if paths are valid
        bool bParticleLoaded = false;
        bool bSoundLoaded = false;
        if (!ParticlePath.IsEmpty())
        {
            UNiagaraSystem* NiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, *ParticlePath);
            if (NiagaraSystem)
            {
                AddBlueprintVariableCombat(Blueprint, TEXT("MuzzleFlashNiagara"), MakeObjectPinType(UNiagaraSystem::StaticClass()));
                bParticleLoaded = true;
            }
            else
            {
                UParticleSystem* ParticleSystem = LoadObject<UParticleSystem>(nullptr, *ParticlePath);
                if (ParticleSystem)
                {
                    AddBlueprintVariableCombat(Blueprint, TEXT("MuzzleFlashParticle"), MakeObjectPinType(UParticleSystem::StaticClass()));
                    bParticleLoaded = true;
                }
            }
        }
        if (!SoundPath.IsEmpty())
        {
            USoundCue* SoundCue = LoadObject<USoundCue>(nullptr, *SoundPath);
            if (SoundCue)
            {
                AddBlueprintVariableCombat(Blueprint, TEXT("MuzzleSound"), MakeObjectPinType(USoundCue::StaticClass()));
                bSoundLoaded = true;
            }
        }

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FStrProperty* PathProp = FindFProperty<FStrProperty>(BPGC, TEXT("MuzzleFlashParticlePath")))
                {
                    PathProp->SetPropertyValue_InContainer(CDO, ParticlePath);
                }
                if (FDoubleProperty* ScaleProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("MuzzleFlashScale")))
                {
                    ScaleProp->SetPropertyValue_InContainer(CDO, Scale);
                }
                if (FStrProperty* SoundProp = FindFProperty<FStrProperty>(BPGC, TEXT("MuzzleSoundPath")))
                {
                    SoundProp->SetPropertyValue_InContainer(CDO, SoundPath);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("particlePath"), ParticlePath);
        Result->SetStringField(TEXT("soundPath"), SoundPath);
        Result->SetNumberField(TEXT("scale"), Scale);
        Result->SetBoolField(TEXT("particleLoaded"), bParticleLoaded);
        Result->SetBoolField(TEXT("soundLoaded"), bSoundLoaded);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Muzzle flash configured."), Result);
        return true;
    }

    // configure_tracer

    if (SubAction == TEXT("configure_tracer"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found."), TEXT("NOT_FOUND"));
            return true;
        }

        FString TracerPath = GetStringFieldCombat(Payload, TEXT("tracerParticlePath"));
        double TracerSpeed = GetNumberFieldCombat(Payload, TEXT("tracerSpeed"), 10000.0);

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("TracerParticlePath"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("TracerSpeed"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bUseTracers"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FStrProperty* PathProp = FindFProperty<FStrProperty>(BPGC, TEXT("TracerParticlePath")))
                {
                    PathProp->SetPropertyValue_InContainer(CDO, TracerPath);
                }
                if (FDoubleProperty* SpeedProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("TracerSpeed")))
                {
                    SpeedProp->SetPropertyValue_InContainer(CDO, TracerSpeed);
                }
                if (FBoolProperty* UseProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bUseTracers")))
                {
                    UseProp->SetPropertyValue_InContainer(CDO, !TracerPath.IsEmpty());
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("tracerPath"), TracerPath);
        Result->SetNumberField(TEXT("tracerSpeed"), TracerSpeed);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Tracer configured."), Result);
        return true;
    }

    // configure_impact_effects

    if (SubAction == TEXT("configure_impact_effects"))
    {
        if (BlueprintPath.IsEmpty())
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
            return true;
        }

        UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *BlueprintPath);
        if (!Blueprint)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Blueprint not found."), TEXT("NOT_FOUND"));
            return true;
        }

        FString ParticlePath = GetStringFieldCombat(Payload, TEXT("impactParticlePath"));
        FString SoundPath = GetStringFieldCombat(Payload, TEXT("impactSoundPath"));
        FString DecalPath = GetStringFieldCombat(Payload, TEXT("impactDecalPath"));

        // Add variables
        AddBlueprintVariableCombat(Blueprint, TEXT("ImpactParticlePath"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ImpactSoundPath"), MakeStringPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("ImpactDecalPath"), MakeStringPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        // Set values
        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            if (UObject* CDO = BPGC->GetDefaultObject())
            {
                if (FStrProperty* ParticleProp = FindFProperty<FStrProperty>(BPGC, TEXT("ImpactParticlePath")))
                {
                    ParticleProp->SetPropertyValue_InContainer(CDO, ParticlePath);
                }
                if (FStrProperty* SoundProp = FindFProperty<FStrProperty>(BPGC, TEXT("ImpactSoundPath")))
                {
                    SoundProp->SetPropertyValue_InContainer(CDO, SoundPath);
                }
                if (FStrProperty* DecalProp = FindFProperty<FStrProperty>(BPGC, TEXT("ImpactDecalPath")))
                {
                    DecalProp->SetPropertyValue_InContainer(CDO, DecalPath);
                }
            }
        }

        McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        Result->SetStringField(TEXT("particlePath"), ParticlePath);
        Result->SetStringField(TEXT("soundPath"), SoundPath);
        Result->SetStringField(TEXT("decalPath"), DecalPath);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Impact effects configured."), Result);
        return true;
    }

    // configure_shell_ejection

    return false;
}
#endif
}
