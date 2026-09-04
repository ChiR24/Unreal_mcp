#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

#include "Domains/Combat/McpAutomationBridge_CombatHandlersBlueprintHelpers.h"

namespace McpCombatHandlers
{
enum class ECombatEffectAssetKind : uint8
{
    Particle,
    Sound,
    Decal
};

// One effect parameter of a weapon-effect action: the request field, the path
// it carried and the asset that path resolved to (nullptr when the field was
// omitted). A supplied path that loads nothing is a request error rather than
// a value to persist, so callers refuse before the Blueprint is touched.
struct FCombatEffectAssetRef
{
    FString ParamName;
    FString Path;
    UObject* Asset = nullptr;

    bool WasSupplied() const { return !Path.IsEmpty(); }
    bool Loaded() const { return Asset != nullptr; }
};

inline const TCHAR* CombatEffectAssetKindName(ECombatEffectAssetKind Kind)
{
    switch (Kind)
    {
    case ECombatEffectAssetKind::Sound:
        return TEXT("USoundBase (SoundCue/SoundWave)");
    case ECombatEffectAssetKind::Decal:
        return TEXT("UMaterialInterface");
    default:
        return TEXT("UNiagaraSystem or UParticleSystem");
    }
}

// Reads ParamName from the payload and loads the asset it names. Returns false
// only when a path was supplied and nothing of the expected kind loads; the
// error names the parameter so the caller can answer ASSET_NOT_FOUND.
inline bool ResolveCombatEffectAsset(
    const TSharedPtr<FJsonObject>& Payload,
    const TCHAR* ParamName,
    ECombatEffectAssetKind Kind,
    FCombatEffectAssetRef& OutRef,
    FString& OutError)
{
    OutRef.ParamName = ParamName;
    OutRef.Path = GetJsonStringField(Payload, ParamName);
    OutRef.Asset = nullptr;
    if (OutRef.Path.IsEmpty())
    {
        return true;
    }

    switch (Kind)
    {
    case ECombatEffectAssetKind::Particle:
        OutRef.Asset = LoadObject<UNiagaraSystem>(nullptr, *OutRef.Path);
        if (!OutRef.Asset)
        {
            OutRef.Asset = LoadObject<UParticleSystem>(nullptr, *OutRef.Path);
        }
        break;
    case ECombatEffectAssetKind::Sound:
        OutRef.Asset = LoadObject<USoundBase>(nullptr, *OutRef.Path);
        break;
    case ECombatEffectAssetKind::Decal:
        OutRef.Asset = LoadObject<UMaterialInterface>(nullptr, *OutRef.Path);
        break;
    }

    if (!OutRef.Asset)
    {
        OutError = FString::Printf(TEXT("%s did not resolve to a %s asset: %s"),
            ParamName, CombatEffectAssetKindName(Kind), *OutRef.Path);
        return false;
    }
    return true;
}

// Adds the string path variable (always) and, when the asset resolved, a typed
// object variable. Particle assets get the object variable suffixed "Niagara"
// or "Particle" after the system type. Returns the object variable name, or
// empty when no asset resolved.
inline FString AddCombatEffectVariables(
    UBlueprint* Blueprint,
    const FCombatEffectAssetRef& Ref,
    const TCHAR* PathVarName,
    const TCHAR* ObjectVarName)
{
    AddBlueprintVariableCombat(Blueprint, PathVarName, MakeStringPinType());
    if (!Ref.Asset)
    {
        return FString();
    }

    FString VarName = ObjectVarName;
    UClass* PinClass = UMaterialInterface::StaticClass();
    if (Ref.Asset->IsA<UNiagaraSystem>())
    {
        VarName += TEXT("Niagara");
        PinClass = UNiagaraSystem::StaticClass();
    }
    else if (Ref.Asset->IsA<UParticleSystem>())
    {
        VarName += TEXT("Particle");
        PinClass = UParticleSystem::StaticClass();
    }
    else if (Ref.Asset->IsA<USoundBase>())
    {
        PinClass = USoundBase::StaticClass();
    }
    AddBlueprintVariableCombat(Blueprint, FName(*VarName), MakeObjectPinType(PinClass));
    return VarName;
}

// Writes the resolved path and object reference onto the compiled CDO. Only a
// resolved asset is stored: an omitted parameter leaves both defaults as they
// were, and the object default is skipped when a pre-existing variable of an
// incompatible class would otherwise receive the asset.
inline void AssignCombatEffectDefaults(
    UBlueprintGeneratedClass* BPGC,
    UObject* CDO,
    const FCombatEffectAssetRef& Ref,
    const TCHAR* PathVarName,
    const FString& ObjectVarName)
{
    if (!BPGC || !CDO || !Ref.Asset)
    {
        return;
    }
    if (FStrProperty* PathProp = FindFProperty<FStrProperty>(BPGC, PathVarName))
    {
        PathProp->SetPropertyValue_InContainer(CDO, Ref.Asset->GetPathName());
    }
    if (ObjectVarName.IsEmpty())
    {
        return;
    }
    FObjectProperty* ObjectProp = FindFProperty<FObjectProperty>(BPGC, *ObjectVarName);
    if (ObjectProp && (!ObjectProp->PropertyClass || Ref.Asset->IsA(ObjectProp->PropertyClass)))
    {
        ObjectProp->SetObjectPropertyValue_InContainer(CDO, Ref.Asset);
    }
}

inline void DescribeCombatEffectAsset(
    const TSharedPtr<FJsonObject>& Result,
    const TCHAR* PathField,
    const TCHAR* LoadedField,
    const FCombatEffectAssetRef& Ref)
{
    Result->SetStringField(PathField, Ref.Asset ? Ref.Asset->GetPathName() : FString());
    Result->SetBoolField(LoadedField, Ref.Asset != nullptr);
    if (Ref.Asset)
    {
        Result->SetStringField(FString(PathField) + TEXT("Class"), Ref.Asset->GetClass()->GetName());
    }
}
}
#endif
