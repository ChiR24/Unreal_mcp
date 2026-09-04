#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Effect/McpAutomationBridge_EffectHandlersPrivate.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "NiagaraSystem.h"
#include "Particles/Emitter.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#endif

namespace McpEffectHandlers
{
#if WITH_EDITOR
namespace
{
// Finds a UNiagaraSystem whose asset name contains the preset (an exact or NS_-prefixed
// name wins over a substring hit) under the engine, plugin and project content roots.
FString FindNiagaraSystemForPreset(const FString& Preset, TArray<FString>& OutCandidates)
{
    FAssetRegistryModule& Registry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FARFilter Filter;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    Filter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
#else
    Filter.ClassNames.Add(UNiagaraSystem::StaticClass()->GetFName());
#endif
    Filter.bRecursivePaths = true;
    Filter.PackagePaths.Add(TEXT("/Niagara"));
    Filter.PackagePaths.Add(TEXT("/Engine"));
    Filter.PackagePaths.Add(TEXT("/Game"));
    TArray<FAssetData> Assets;
    Registry.Get().GetAssets(Filter, Assets);

    FString Exact;
    FString Partial;
    for (const FAssetData& Asset : Assets)
    {
        const FString AssetName = Asset.AssetName.ToString();
        if (!AssetName.Contains(Preset, ESearchCase::IgnoreCase))
        {
            continue;
        }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
        const FString ObjectPath = Asset.GetObjectPathString();
#else
        const FString ObjectPath = Asset.ObjectPath.ToString();
#endif
        OutCandidates.Add(ObjectPath);
        const bool bExact = AssetName.Equals(Preset, ESearchCase::IgnoreCase) ||
            AssetName.Equals(TEXT("NS_") + Preset, ESearchCase::IgnoreCase);
        if (bExact && Exact.IsEmpty())
        {
            Exact = ObjectPath;
        }
        else if (!bExact && Partial.IsEmpty())
        {
            Partial = ObjectPath;
        }
    }
    if (OutCandidates.IsEmpty())
    {
        // Nothing matched the preset name: still name a few shipped systems the caller can pass as systemPath (dogfood #102).
        for (const FAssetData& Asset : Assets)
        {
            if (OutCandidates.Num() >= 8) { break; }
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
            OutCandidates.Add(Asset.GetObjectPathString());
#else
            OutCandidates.Add(Asset.ObjectPath.ToString());
#endif
        }
    }
    return Exact.IsEmpty() ? Partial : Exact;
}

bool SpawnCascadeEmitter(const FEffectActionContext& Context, UParticleSystem* Template, const FString& TemplatePath)
{
    AEmitter* Emitter = Cast<AEmitter>(SpawnActorInActiveWorld<AActor>(
        AEmitter::StaticClass(),
        ReadVectorField(Context.Payload, TEXT("location")),
        ReadRotatorField(Context.Payload, TEXT("rotation"))));
    if (!Emitter)
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("Failed to spawn Emitter actor"), nullptr, TEXT("SPAWN_FAILED"));
        return true;
    }
    Emitter->SetTemplate(Template);
    if (UParticleSystemComponent* Component = Emitter->FindComponentByClass<UParticleSystemComponent>())
    {
        Component->Activate(true);
    }
    FString Name;
    Context.Payload->TryGetStringField(TEXT("name"), Name);
    if (Name.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("actorName"), Name);
    }
    Emitter->SetActorLabel(
        !Name.IsEmpty() ? Name : FString::Printf(TEXT("Particle_%lld"), FDateTime::Now().ToUnixTimestamp()));

    TSharedPtr<FJsonObject> Response = McpHandlerUtils::CreateResultObject();
    Response->SetBoolField(TEXT("success"), true);
    Response->SetStringField(TEXT("effectType"), TEXT("particle"));
    Response->SetStringField(TEXT("backend"), TEXT("cascade"));
    Response->SetStringField(TEXT("systemPath"), TemplatePath);
    Response->SetStringField(TEXT("actorName"), Emitter->GetActorLabel());
    McpHandlerUtils::AddVerification(Response, Emitter);
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, true, TEXT("Particle effect created"), Response);
    return true;
}
}
#endif

bool HandleParticleEffect(const FEffectActionContext& Context)
{
    FString Preset;
    Context.Payload->TryGetStringField(TEXT("preset"), Preset);
    FString SystemPath = ReadNiagaraSystemPathField(Context.Payload);
    if (SystemPath.IsEmpty() && Preset.StartsWith(TEXT("/")))
    {
        // The stdio layer rewrites named presets into asset paths before dispatch.
        SystemPath = Preset;
    }
    if (SystemPath.IsEmpty() && Preset.IsEmpty())
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("preset or systemPath is required for particle"), nullptr, TEXT("INVALID_ARGUMENT"));
        return true;
    }
#if WITH_EDITOR
    if (!GEditor)
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            TEXT("Editor not available"), nullptr, TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }
    TSharedPtr<FJsonObject> Details = McpHandlerUtils::CreateResultObject();
    if (!Preset.IsEmpty())
    {
        Details->SetStringField(TEXT("preset"), Preset);
    }
    if (SystemPath.IsEmpty())
    {
        // A bare preset name has no built-in template of its own: resolve it against the
        // Niagara systems the engine and project actually ship (dogfood #102).
        TArray<FString> Candidates;
        SystemPath = FindNiagaraSystemForPreset(Preset, Candidates);
        if (SystemPath.IsEmpty())
        {
            Details->SetBoolField(TEXT("success"), false);
            Context.Bridge.SendAutomationResponse(
                Context.Socket, Context.RequestId, false,
                FString::Printf(TEXT("preset '%s' has no built-in Niagara template; pass systemPath or one of the shipped systems: %s"), *Preset,
                                Candidates.Num() > 0 ? *FString::Join(TArray<FString>(Candidates.GetData(), FMath::Min(Candidates.Num(), 8)), TEXT(", ")) : TEXT("(none found)")),
                Details, TEXT("NOT_SUPPORTED"));
            return true;
        }
        TArray<TSharedPtr<FJsonValue>> CandidateValues;
        for (int32 Index = 0; Index < Candidates.Num() && Index < 10; ++Index)
        {
            CandidateValues.Add(MakeShared<FJsonValueString>(Candidates[Index]));
        }
        Details->SetStringField(TEXT("resolvedSystemPath"), SystemPath);
        Details->SetStringField(TEXT("presetResolvedBy"), TEXT("asset-registry-name-match"));
        Details->SetArrayField(TEXT("presetCandidates"), CandidateValues);
    }

    const FString CanonicalPath = FPackageName::ObjectPathToPackageName(SystemPath);
    UObject* Asset = UEditorAssetLibrary::DoesAssetExist(CanonicalPath)
        ? UEditorAssetLibrary::LoadAsset(CanonicalPath)
        : nullptr;
    if (!Asset)
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            FString::Printf(TEXT("Particle system asset not found: %s"), *SystemPath),
            Details, TEXT("SYSTEM_NOT_FOUND"));
        return true;
    }
    if (UParticleSystem* Cascade = Cast<UParticleSystem>(Asset))
    {
        return SpawnCascadeEmitter(Context, Cascade, SystemPath);
    }
    if (!Asset->IsA<UNiagaraSystem>())
    {
        Context.Bridge.SendAutomationResponse(
            Context.Socket, Context.RequestId, false,
            FString::Printf(TEXT("%s is a %s, not a Niagara or Cascade particle system"),
                *SystemPath, *Asset->GetClass()->GetName()),
            Details, TEXT("ASSET_TYPE_MISMATCH"));
        return true;
    }
    Details->SetStringField(TEXT("backend"), TEXT("niagara"));
    return CreateNiagaraEffectFromPayload(Context, TEXT("particle"), SystemPath, Details);
#else
    Context.Bridge.SendAutomationResponse(
        Context.Socket, Context.RequestId, false,
        TEXT("particle requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
}
