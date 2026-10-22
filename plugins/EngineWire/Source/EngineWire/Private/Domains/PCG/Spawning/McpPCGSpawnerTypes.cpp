#include "Domains/PCG/Spawning/McpPCGSpawnerTypes.h"
#include "Domains/PCG/McpPCGPrivate.h"

#if MCP_HAS_PCG
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7)
#include "InstanceDataPackers/PCGInstanceDataPackerByAttribute.h"
#include "MeshSelectors/PCGMeshSelectorByAttribute.h"
#include "Subsystems/PCGSubsystem.h"
#include "Subsystems/PCGEngineSubsystem.h"

namespace McpPCGHandlers
{
namespace
{
bool ReadSpawnerClass(const TSharedPtr<FJsonValue>& Value, bool bSelector, UClass*& OutClass, FString& OutError)
{
    OutClass = nullptr;
    const bool bClear = Value.IsValid() && (Value->Type == EJson::Null || (Value->Type == EJson::String &&
        (Value->AsString().IsEmpty() || Value->AsString().Equals(TEXT("None"), ESearchCase::IgnoreCase))));
    if (bClear && !bSelector) return true;
    if (!Value.IsValid() || Value->Type != EJson::String || bClear || Value->AsString().Len() > 512)
    {
        OutError = TEXT("Spawner type requires a supported native class name/path; only the instance packer may be cleared.");
        return false;
    }
    const TArray<UClass*> Allowed = bSelector ? TArray<UClass*>{UPCGMeshSelectorWeighted::StaticClass(), UPCGMeshSelectorByAttribute::StaticClass()} :
        TArray<UClass*>{UPCGInstanceDataPackerByAttribute::StaticClass()};
    for (UClass* Class : Allowed)
    {
        if (Value->AsString().Equals(Class->GetPathName(), ESearchCase::IgnoreCase) ||
            Value->AsString().Equals(Class->GetName(), ESearchCase::IgnoreCase))
        {
            OutClass = Class;
            return true;
        }
    }
    OutError = bSelector ? TEXT("Supported native selector types are PCGMeshSelectorWeighted and PCGMeshSelectorByAttribute; no class load was attempted.") :
        TEXT("The supported native packer type is PCGInstanceDataPackerByAttribute; no class load was attempted.");
    return false;
}

bool CheckNativeIdle(FString& OutError)
{
    int32 Visited = 0;
    for (TObjectIterator<UPCGSubsystem> It; It; ++It)
    {
        if (++Visited > 64 || It->IsAnyGraphCurrentlyExecuting())
        {
            OutError = TEXT("Spawner type changes require native PCG tasks to be idle within the bounded subsystem inventory.");
            return false;
        }
    }
    for (TObjectIterator<UPCGEngineSubsystem> It; It; ++It)
    {
        if (++Visited > 64 || It->IsAnyGraphCurrentlyExecuting())
        {
            OutError = TEXT("Spawner type changes require native PCG tasks to be idle within the bounded subsystem inventory.");
            return false;
        }
    }
    return true;
}

bool CheckOwnedParameters(UObject* Object, UPCGSettings* Settings, bool bSelector, FString& OutError)
{
    if (!Object) return true;
    if (!IsValid(Object) || Object->HasAnyFlags(RF_ClassDefaultObject) || Object->GetOuter() != Settings ||
        Object->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
    {
        OutError = TEXT("Spawner type changes require existing privately owned native parameters, not shared objects, CDOs or Blueprint callbacks.");
        return false;
    }
    const UClass* Class = Object->GetClass();
    if (bSelector ? (Class != UPCGMeshSelectorWeighted::StaticClass() && Class != UPCGMeshSelectorByAttribute::StaticClass()) :
        Class != UPCGInstanceDataPackerByAttribute::StaticClass())
    {
        OutError = TEXT("Existing parameter classes must also belong to the proven native selector/packer set; custom callbacks are not sandboxed.");
        return false;
    }
    return true;
}

template<typename T>
T* CopyParameters(T* Source, UPCGSettings* Settings)
{
    T* Copy = Source ? DuplicateObject<T>(Source, Settings, MakeUniqueObjectName(Settings, Source->GetClass())) : nullptr;
    if (Copy)
    {
        Copy->ClearFlags(RF_Transient);
        Copy->SetFlags(Settings->GetMaskedFlags(RF_PropagateToSubObjects));
    }
    return Copy;
}
}

bool ApplyPCGSpawnerTypes(UPCGSettings* Settings, const TSharedPtr<FJsonObject>& Fields,
    TSet<FString>& OutHandledFields, FString& OutError)
{
    OutHandledFields.Reset();
    OutError.Empty();
    UPCGStaticMeshSpawnerSettings* Spawner = Cast<UPCGStaticMeshSpawnerSettings>(Settings);
    if (!Spawner || !Fields.IsValid()) return true;
    UClass* SelectorClass = nullptr;
    UClass* PackerClass = nullptr;
    bool bSelector = false, bPacker = false;
    for (const auto& Field : Fields->Values)
    {
        TArray<FString> Segments;
        const FString FieldName(Field.Key.Len(), *Field.Key);
        FieldName.ParseIntoArray(Segments, TEXT("."), true);
        if (Segments.Num() != 1) continue;
        const FString& Name = Segments[0];
        if (Name.Equals(TEXT("MeshSelectorParameters"), ESearchCase::IgnoreCase) ||
            Name.Equals(TEXT("InstanceDataPackerParameters"), ESearchCase::IgnoreCase))
        {
            OutError = TEXT("Spawner parameter pointers are read-only; choose a supported type and configure its owned nested properties instead.");
            return false;
        }
        const bool bMeshField = Name.Equals(TEXT("MeshSelectorType"), ESearchCase::IgnoreCase);
        const bool bPackerField = Name.Equals(TEXT("InstanceDataPackerType"), ESearchCase::IgnoreCase);
        if (!bMeshField && !bPackerField) continue;
        bool& Seen = bMeshField ? bSelector : bPacker;
        if (Seen || Spawner->HasAnyFlags(RF_ClassDefaultObject) || Spawner->GetClass() != UPCGStaticMeshSpawnerSettings::StaticClass())
        {
            OutError = TEXT("Type changes require an exact native static mesh spawner and one unambiguous value per type field.");
            return false;
        }
        Seen = true;
        if (!ReadSpawnerClass(Field.Value, bMeshField, bMeshField ? SelectorClass : PackerClass, OutError)) return false;
        OutHandledFields.Add(FieldName);
    }
    if (!bSelector && !bPacker) return true;
    if (!IsInGameThread() || !CheckNativeIdle(OutError))
    {
        if (OutError.IsEmpty()) OutError = TEXT("Spawner type changes require the game thread.");
        return false;
    }
    if (!CheckOwnedParameters(Spawner->MeshSelectorParameters, Settings, true, OutError) ||
        !CheckOwnedParameters(Spawner->InstanceDataPackerParameters, Settings, false, OutError)) return false;
    const bool bNewSelector = bSelector && (!Spawner->MeshSelectorParameters || Spawner->MeshSelectorType.Get() != SelectorClass ||
        Spawner->MeshSelectorParameters->GetClass() != SelectorClass);
    const bool bNewPacker = bPacker && (Spawner->InstanceDataPackerType.Get() != PackerClass ||
        (PackerClass && (!Spawner->InstanceDataPackerParameters || Spawner->InstanceDataPackerParameters->GetClass() != PackerClass)));
    TStrongObjectPtr<UPCGStaticMeshSpawnerSettings> Prototype;
    TStrongObjectPtr<UPCGMeshSelectorBase> Selector;
    TStrongObjectPtr<UPCGInstanceDataPackerBase> Packer;
    if (bNewSelector || (bNewPacker && PackerClass))
        Prototype.Reset(NewObject<UPCGStaticMeshSpawnerSettings>(GetTransientPackage(), NAME_None, RF_Transient));
    // Native refresh garbage-marks old parameters. Initialize off-graph so recovery retains the originals.
    if (bNewSelector)
    {
        Prototype->SetMeshSelectorType(TSubclassOf<UPCGMeshSelectorBase>());
        Prototype->SetMeshSelectorType(SelectorClass);
        Selector.Reset(CopyParameters(Prototype->MeshSelectorParameters.Get(), Settings));
        if (!Selector.IsValid())
        {
            OutError = TEXT("Could not initialize owned native selector parameters.");
            return false;
        }
    }
    if (bNewPacker && PackerClass)
    {
        Prototype->SetInstancePackerType(PackerClass);
        Packer.Reset(CopyParameters(Prototype->InstanceDataPackerParameters.Get(), Settings));
        if (!Packer.IsValid())
        {
            if (Selector.IsValid()) Selector->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
            OutError = TEXT("Could not initialize owned native instance packer parameters.");
            return false;
        }
    }
    if ((bNewSelector && Spawner->MeshSelectorParameters &&
        !Spawner->MeshSelectorParameters->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional)) ||
        ((bNewPacker || (bPacker && !PackerClass)) && Spawner->InstanceDataPackerParameters &&
        !Spawner->InstanceDataPackerParameters->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional)))
    {
        OutError = TEXT("Could not detach old native parameters without destroying them; authored recovery is required.");
        return false;
    }
    if (bSelector)
    {
        Spawner->MeshSelectorType = SelectorClass;
        if (bNewSelector) Spawner->MeshSelectorParameters = Selector.Get();
    }
    if (bPacker)
    {
        Spawner->InstanceDataPackerType = PackerClass;
        if (bNewPacker || !PackerClass) Spawner->InstanceDataPackerParameters = Packer.Get();
    }
    return true;
}
}
#else
namespace McpPCGHandlers
{
bool ApplyPCGSpawnerTypes(UPCGSettings*, const TSharedPtr<FJsonObject>&, TSet<FString>& OutHandledFields, FString& OutError)
{
    OutHandledFields.Reset();
    OutError.Empty();
    return true;
}
}
#endif
#endif
