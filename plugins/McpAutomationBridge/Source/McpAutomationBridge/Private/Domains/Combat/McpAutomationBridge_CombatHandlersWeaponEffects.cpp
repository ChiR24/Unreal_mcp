#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Combat/McpAutomationBridge_CombatHandlersPrivate.h"
#include "Domains/Combat/McpAutomationBridge_CombatHandlersEffectAssets.h"

namespace McpCombatHandlers
{
#if WITH_EDITOR
namespace
{
struct FWeaponEffectParam
{
    const TCHAR* Name;
    ECombatEffectAssetKind Kind;
};

UBlueprint* LoadWeaponEffectsBlueprint(const FCombatActionContext& Context)
{
    if (Context.BlueprintPath.IsEmpty())
    {
        Context.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Missing blueprintPath."), TEXT("INVALID_ARGUMENT"));
        return nullptr;
    }

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *Context.BlueprintPath);
    if (!Blueprint)
    {
        Context.SendAutomationError(Context.RequestingSocket, Context.RequestId, TEXT("Blueprint not found."), TEXT("NOT_FOUND"));
    }
    return Blueprint;
}

// Resolves every effect parameter before anything is mutated: a supplied path
// that loads nothing fails the whole request with ASSET_NOT_FOUND, so a bogus
// path is never written into the Blueprint.
bool ResolveWeaponEffectAssets(
    const FCombatActionContext& Context,
    std::initializer_list<FWeaponEffectParam> Params,
    TArray<FCombatEffectAssetRef>& OutRefs)
{
    OutRefs.Reset();
    for (const FWeaponEffectParam& Param : Params)
    {
        FCombatEffectAssetRef Ref;
        FString Error;
        if (!ResolveCombatEffectAsset(Context.Payload, Param.Name, Param.Kind, Ref, Error))
        {
            Context.SendAutomationError(Context.RequestingSocket, Context.RequestId, Error, TEXT("ASSET_NOT_FOUND"));
            return false;
        }
        OutRefs.Add(MoveTemp(Ref));
    }
    return true;
}
}

bool FCombatActionContext::HandleWeaponEffects() const
{
    if (SubAction == TEXT("configure_muzzle_flash"))
    {
        UBlueprint* Blueprint = LoadWeaponEffectsBlueprint(*this);
        if (!Blueprint)
        {
            return true;
        }

        TArray<FCombatEffectAssetRef> Refs;
        if (!ResolveWeaponEffectAssets(*this,
                {{TEXT("muzzleFlashParticlePath"), ECombatEffectAssetKind::Particle},
                 {TEXT("muzzleSoundPath"), ECombatEffectAssetKind::Sound}}, Refs))
        {
            return true;
        }
        const FCombatEffectAssetRef& Particle = Refs[0];
        const FCombatEffectAssetRef& Sound = Refs[1];
        const double Scale = GetJsonNumberField(Payload, TEXT("muzzleFlashScale"), 1.0);

        const FString ParticleVar = AddCombatEffectVariables(Blueprint, Particle, TEXT("MuzzleFlashParticlePath"), TEXT("MuzzleFlash"));
        const FString SoundVar = AddCombatEffectVariables(Blueprint, Sound, TEXT("MuzzleSoundPath"), TEXT("MuzzleSound"));
        AddBlueprintVariableCombat(Blueprint, TEXT("MuzzleFlashScale"), MakeFloatPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            UObject* CDO = BPGC->GetDefaultObject();
            AssignCombatEffectDefaults(BPGC, CDO, Particle, TEXT("MuzzleFlashParticlePath"), ParticleVar);
            AssignCombatEffectDefaults(BPGC, CDO, Sound, TEXT("MuzzleSoundPath"), SoundVar);
            if (FDoubleProperty* ScaleProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("MuzzleFlashScale")))
            {
                ScaleProp->SetPropertyValue_InContainer(CDO, Scale);
            }
        }

        const bool bSaved = McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        DescribeCombatEffectAsset(Result, TEXT("particlePath"), TEXT("particleLoaded"), Particle);
        DescribeCombatEffectAsset(Result, TEXT("soundPath"), TEXT("soundLoaded"), Sound);
        Result->SetNumberField(TEXT("scale"), Scale);
        Result->SetBoolField(TEXT("saved"), bSaved);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Muzzle flash configured."), Result);
        return true;
    }
    if (SubAction == TEXT("configure_tracer"))
    {
        UBlueprint* Blueprint = LoadWeaponEffectsBlueprint(*this);
        if (!Blueprint)
        {
            return true;
        }

        TArray<FCombatEffectAssetRef> Refs;
        if (!ResolveWeaponEffectAssets(*this, {{TEXT("tracerParticlePath"), ECombatEffectAssetKind::Particle}}, Refs))
        {
            return true;
        }
        const FCombatEffectAssetRef& Tracer = Refs[0];
        const double TracerSpeed = GetJsonNumberField(Payload, TEXT("tracerSpeed"), 10000.0);

        const FString TracerVar = AddCombatEffectVariables(Blueprint, Tracer, TEXT("TracerParticlePath"), TEXT("Tracer"));
        AddBlueprintVariableCombat(Blueprint, TEXT("TracerSpeed"), MakeFloatPinType());
        AddBlueprintVariableCombat(Blueprint, TEXT("bUseTracers"), MakeBoolPinType());

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            UObject* CDO = BPGC->GetDefaultObject();
            AssignCombatEffectDefaults(BPGC, CDO, Tracer, TEXT("TracerParticlePath"), TracerVar);
            if (FDoubleProperty* SpeedProp = FindFProperty<FDoubleProperty>(BPGC, TEXT("TracerSpeed")))
            {
                SpeedProp->SetPropertyValue_InContainer(CDO, TracerSpeed);
            }
            FBoolProperty* UseProp = FindFProperty<FBoolProperty>(BPGC, TEXT("bUseTracers"));
            if (UseProp && Tracer.Loaded())
            {
                UseProp->SetPropertyValue_InContainer(CDO, true);
            }
        }

        const bool bSaved = McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        DescribeCombatEffectAsset(Result, TEXT("tracerPath"), TEXT("tracerLoaded"), Tracer);
        Result->SetNumberField(TEXT("tracerSpeed"), TracerSpeed);
        Result->SetBoolField(TEXT("saved"), bSaved);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Tracer configured."), Result);
        return true;
    }
    if (SubAction == TEXT("configure_impact_effects"))
    {
        UBlueprint* Blueprint = LoadWeaponEffectsBlueprint(*this);
        if (!Blueprint)
        {
            return true;
        }

        TArray<FCombatEffectAssetRef> Refs;
        if (!ResolveWeaponEffectAssets(*this,
                {{TEXT("impactParticlePath"), ECombatEffectAssetKind::Particle},
                 {TEXT("impactSoundPath"), ECombatEffectAssetKind::Sound},
                 {TEXT("impactDecalPath"), ECombatEffectAssetKind::Decal}}, Refs))
        {
            return true;
        }
        const FCombatEffectAssetRef& Particle = Refs[0];
        const FCombatEffectAssetRef& Sound = Refs[1];
        const FCombatEffectAssetRef& Decal = Refs[2];

        const FString ParticleVar = AddCombatEffectVariables(Blueprint, Particle, TEXT("ImpactParticlePath"), TEXT("Impact"));
        const FString SoundVar = AddCombatEffectVariables(Blueprint, Sound, TEXT("ImpactSoundPath"), TEXT("ImpactSound"));
        const FString DecalVar = AddCombatEffectVariables(Blueprint, Decal, TEXT("ImpactDecalPath"), TEXT("ImpactDecalMaterial"));

        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        McpSafeCompileBlueprint(Blueprint);

        if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(Blueprint->GeneratedClass))
        {
            UObject* CDO = BPGC->GetDefaultObject();
            AssignCombatEffectDefaults(BPGC, CDO, Particle, TEXT("ImpactParticlePath"), ParticleVar);
            AssignCombatEffectDefaults(BPGC, CDO, Sound, TEXT("ImpactSoundPath"), SoundVar);
            AssignCombatEffectDefaults(BPGC, CDO, Decal, TEXT("ImpactDecalPath"), DecalVar);
        }

        const bool bSaved = McpSafeAssetSave(Blueprint);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetPathName());
        DescribeCombatEffectAsset(Result, TEXT("particlePath"), TEXT("particleLoaded"), Particle);
        DescribeCombatEffectAsset(Result, TEXT("soundPath"), TEXT("soundLoaded"), Sound);
        DescribeCombatEffectAsset(Result, TEXT("decalPath"), TEXT("decalLoaded"), Decal);
        Result->SetBoolField(TEXT("saved"), bSaved);

        SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Impact effects configured."), Result);
        return true;
    }
    return false;
}
#endif
}
