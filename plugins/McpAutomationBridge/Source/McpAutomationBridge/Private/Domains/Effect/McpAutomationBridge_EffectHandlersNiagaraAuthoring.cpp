#include "Core/Compatibility/McpVersionCompatibility.h"

#include "Domains/Effect/McpAutomationBridge_EffectHandlersPrivate.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSystem.h"
#include "UObject/Package.h"
#if __has_include("NiagaraSystemFactoryNew.h") && !(ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 0)
#include "NiagaraSystemFactoryNew.h"
#define MCP_EFFECT_HAS_NIAGARA_SYSTEM_FACTORY 1
#else
#define MCP_EFFECT_HAS_NIAGARA_SYSTEM_FACTORY 0
#endif
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
#include "NiagaraEditorUtilities.h"
#endif
#endif

namespace McpEffectHandlers
{
#if WITH_EDITOR
namespace
{
// Engine-shipped template emitters (/Niagara/DefaultAssets/Templates/Emitters) keyed by
// the procedural creator that authors a system from them.
const TCHAR* DefaultTemplateEmitterPath(const FString& EffectName)
{
    if (EffectName == TEXT("create_niagara_ribbon") || EffectName == TEXT("create_particle_trail"))
    {
        return TEXT("/Niagara/DefaultAssets/Templates/Emitters/LocationBasedRibbon.LocationBasedRibbon");
    }
    if (EffectName == TEXT("create_impact_effect"))
    {
        return TEXT("/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst.OmnidirectionalBurst");
    }
    if (EffectName == TEXT("create_environment_effect"))
    {
        return TEXT("/Niagara/DefaultAssets/Templates/Emitters/HangingParticulates.HangingParticulates");
    }
    return TEXT("");
}

// "create_niagara_ribbon" -> "NS_NiagaraRibbon"
FString DefaultSystemName(const FString& EffectName)
{
    FString Stem = EffectName;
    Stem.RemoveFromStart(TEXT("create_"));
    TArray<FString> Parts;
    Stem.ParseIntoArray(Parts, TEXT("_"));
    FString Result = TEXT("NS_");
    for (FString& Part : Parts)
    {
        Part[0] = FChar::ToUpper(Part[0]);
        Result += Part;
    }
    return Result;
}

FString AddTemplateEmitter(UNiagaraSystem& System, UNiagaraEmitter& Template)
{
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    FModuleManager::Get().LoadModule(TEXT("NiagaraEditor"));
    System.Modify();
    Template.CheckVersionDataAvailable();
    const FGuid HandleId = FNiagaraEditorUtilities::AddEmitterToSystem(
        System, Template, Template.GetExposedVersion().VersionGuid, true);
    for (const FNiagaraEmitterHandle& Handle : System.GetEmitterHandles())
    {
        if (Handle.GetId() == HandleId)
        {
            return Handle.GetName().ToString();
        }
    }
    return FString();
#else
    return System.AddEmitterHandle(Template, FName(*Template.GetName())).GetName().ToString();
#endif
}

FString ResolveSystemFolder(const FEffectActionContext& Context, const FString& Name)
{
    FString Folder;
    Context.Payload->TryGetStringField(TEXT("path"), Folder);
    if (Folder.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("savePath"), Folder);
    }
    if (Folder.IsEmpty())
    {
        Folder = TEXT("/Game/Effects");
    }
    Folder = SanitizeProjectRelativePath(Folder);
    if (Folder.IsEmpty())
    {
        return Folder;
    }
    // A full object path in `path` ("/Game/FX/NS_Foo" or "/Game/FX/NS_Foo.NS_Foo") is the
    // asset itself, so its folder is one level up.
    Folder = FPackageName::ObjectPathToPackageName(Folder);
    Folder.RemoveFromEnd(TEXT("/"));
    if (FPackageName::GetShortName(Folder).Equals(Name, ESearchCase::IgnoreCase))
    {
        Folder = FPackageName::GetLongPackagePath(Folder);
    }
    return Folder;
}
}

bool AuthorProceduralNiagaraSystem(
    const FEffectActionContext& Context,
    const FString& EffectName,
    FString& OutSystemPath,
    TSharedPtr<FJsonObject>& OutDetails,
    FString& OutError,
    FString& OutErrorCode)
{
    OutDetails = McpHandlerUtils::CreateResultObject();
    FString Name;
    Context.Payload->TryGetStringField(TEXT("name"), Name);
    if (Name.IsEmpty())
    {
        Context.Payload->TryGetStringField(TEXT("systemName"), Name);
    }
    if (Name.IsEmpty())
    {
        Name = DefaultSystemName(EffectName);
    }
    if (Name.Contains(TEXT("/")) || Name.Contains(TEXT(".")) || Name.Contains(TEXT("\\")))
    {
        OutError = FString::Printf(
            TEXT("name '%s' must be a bare asset name; put the folder in path/savePath"), *Name);
        OutErrorCode = TEXT("INVALID_ARGUMENT");
        return false;
    }
    const FString Folder = ResolveSystemFolder(Context, Name);
    if (Folder.IsEmpty())
    {
        OutError = TEXT("path/savePath was rejected by project path validation");
        OutErrorCode = TEXT("INVALID_PATH");
        return false;
    }
    const FString PackageName = Folder + TEXT("/") + Name;

    bool bCreated = false;
    FString EmitterName;
    UNiagaraSystem* System = nullptr;
    if (UEditorAssetLibrary::DoesAssetExist(PackageName))
    {
        // Re-running the creator reuses the authored asset instead of colliding with it.
        System = Cast<UNiagaraSystem>(UEditorAssetLibrary::LoadAsset(PackageName));
        if (!System)
        {
            OutError = FString::Printf(TEXT("%s exists and is not a Niagara system"), *PackageName);
            OutErrorCode = TEXT("ASSET_TYPE_MISMATCH");
            return false;
        }
    }
    else
    {
        UPackage* Package = CreatePackage(*PackageName);
        System = Package ? NewObject<UNiagaraSystem>(Package, FName(*Name), RF_Public | RF_Standalone) : nullptr;
        if (!System)
        {
            OutError = FString::Printf(TEXT("Failed to create Niagara system %s"), *PackageName);
            OutErrorCode = TEXT("CREATE_FAILED");
            return false;
        }
        bCreated = true;
#if MCP_EFFECT_HAS_NIAGARA_SYSTEM_FACTORY
        FModuleManager::Get().LoadModule(TEXT("NiagaraEditor"));
        UNiagaraSystemFactoryNew::InitializeSystem(System, true);
#endif
        FString TemplatePath;
        Context.Payload->TryGetStringField(TEXT("templateEmitterPath"), TemplatePath);
        if (TemplatePath.IsEmpty())
        {
            Context.Payload->TryGetStringField(TEXT("emitterPath"), TemplatePath);
        }
        if (TemplatePath.IsEmpty())
        {
            TemplatePath = DefaultTemplateEmitterPath(EffectName);
        }
        UNiagaraEmitter* Template =
            TemplatePath.IsEmpty() ? nullptr : LoadObject<UNiagaraEmitter>(nullptr, *TemplatePath);
        if (Template)
        {
            EmitterName = AddTemplateEmitter(*System, *Template);
        }
        OutDetails->SetStringField(TEXT("templateEmitterPath"), TemplatePath);
        OutDetails->SetBoolField(TEXT("templateEmitterFound"), Template != nullptr);
        FAssetRegistryModule::AssetCreated(System);
        System->MarkPackageDirty();
        bool bSave = true;
        Context.Payload->TryGetBoolField(TEXT("save"), bSave);
        if (bSave && !McpSafeAssetSave(System))
        {
            OutError = FString::Printf(TEXT("Failed to save Niagara system %s"), *PackageName);
            OutErrorCode = TEXT("SAVE_FAILED");
            return false;
        }
        OutDetails->SetBoolField(TEXT("saved"), bSave);
    }

    OutSystemPath = System->GetPathName();
    OutDetails->SetBoolField(TEXT("created"), bCreated);
    OutDetails->SetBoolField(TEXT("reusedExisting"), !bCreated);
    OutDetails->SetStringField(TEXT("assetPath"), OutSystemPath);
    OutDetails->SetNumberField(TEXT("emitterCount"), System->GetEmitterHandles().Num());
    if (!EmitterName.IsEmpty())
    {
        OutDetails->SetStringField(TEXT("emitterName"), EmitterName);
    }
    return true;
}
#endif
}
