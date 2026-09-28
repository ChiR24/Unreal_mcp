// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/AssetWorkflow/Rename/McpAutomationBridge_AssetRenameGuard.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace McpAssetRename
{
namespace
{
struct FMcpRenameTarget
{
    FString OldAssetName;
    FString NewObjectPrefix; // "/Game/New/Folder/Name.Name"
};

// Re-keys one entry of a soft-path-keyed map, keeping its value.
void RekeySoftMapEntry(const FMapProperty* Map, void* Data, const FSoftObjectPath& From, const FSoftObjectPath& To)
{
    FScriptMapHelper Helper(Map, Data);
    FSoftObjectPtr FromKey(From);
    const int32 Index = Helper.FindMapIndexWithKey(&FromKey);
    if (Index == INDEX_NONE)
    {
        return;
    }
    const FProperty* ValueProp = Map->ValueProp;
    void* Value = FMemory::Malloc(ValueProp->GetSize(), ValueProp->GetMinAlignment());
    ValueProp->InitializeValue(Value);
    ValueProp->CopyCompleteValue(Value, Helper.GetValuePtr(Index));
    Helper.RemoveAt(Index);
    FSoftObjectPtr ToKey(To);
    Helper.AddPair(&ToKey, Value);
    ValueProp->DestroyValue(Value);
    FMemory::Free(Value);
}

struct FMcpSettingsFollow
{
    TMap<FName, FMcpRenameTarget> ByPackage;
    TArray<TFunction<void(bool)>> Swaps; // true points at the new path, false back at the old one
    TSet<UObject*> Touched;
    TArray<FString> Moved;
    TArray<FString> Blocking;

    bool Remap(const FSoftObjectPath& Path, FSoftObjectPath& OutNew) const
    {
        const FMcpRenameTarget* Target = Path.IsNull() ? nullptr : ByPackage.Find(FName(*Path.GetLongPackageName()));
        const FString AssetName = Path.GetAssetName();
        if (!Target || !AssetName.StartsWith(Target->OldAssetName, ESearchCase::CaseSensitive))
        {
            return false;
        }
        // BP_X_C follows BP_X: whatever comes after the asset name is kept.
        FString NewPath = Target->NewObjectPrefix + AssetName.Mid(Target->OldAssetName.Len());
        if (!Path.GetSubPathString().IsEmpty())
        {
            NewPath += TEXT(":") + Path.GetSubPathString();
        }
        OutNew = FSoftObjectPath(NewPath);
        return true;
    }

    void Note(UObject* Owner, const FString& Label, TFunction<void(bool)> Swap)
    {
        Swap(true);
        Swaps.Add(MoveTemp(Swap));
        Touched.Add(Owner);
        Moved.AddUnique(Label);
    }

    void WalkMapKeys(UObject* Owner, const FString& Label, const FMapProperty* Map, void* Data)
    {
        if (!CastField<FSoftObjectProperty>(Map->KeyProp))
        {
            return;
        }
        TArray<TPair<FSoftObjectPath, FSoftObjectPath>> Rekeys;
        FScriptMapHelper Helper(Map, Data);
        for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
        {
            FSoftObjectPath NewPath;
            const FSoftObjectPath OldPath = Helper.IsValidIndex(Index)
                ? static_cast<const FSoftObjectPtr*>(static_cast<const void*>(Helper.GetKeyPtr(Index)))->ToSoftObjectPath()
                : FSoftObjectPath();
            if (Remap(OldPath, NewPath))
            {
                Rekeys.Emplace(OldPath, NewPath);
            }
        }
        for (const TPair<FSoftObjectPath, FSoftObjectPath>& Rekey : Rekeys)
        {
            const FSoftObjectPath OldPath = Rekey.Key;
            const FSoftObjectPath NewPath = Rekey.Value;
            Note(Owner, Label + TEXT(" entry ") + OldPath.GetAssetName(), [Map, Data, OldPath, NewPath](bool bNew)
            {
                RekeySoftMapEntry(Map, Data, bNew ? OldPath : NewPath, bNew ? NewPath : OldPath);
            });
        }
    }

    void Walk(UObject* Owner, const FString& Label, const FProperty* Property, void* Data, int32 Depth)
    {
        if (Depth > 6)
        {
            return;
        }
        if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
        {
            if (Struct->Struct->IsChildOf(TBaseStructure<FSoftObjectPath>::Get()))
            {
                FSoftObjectPath* Path = static_cast<FSoftObjectPath*>(Data);
                FSoftObjectPath NewPath;
                if (Remap(*Path, NewPath))
                {
                    const FSoftObjectPath OldPath = *Path;
                    Note(Owner, Label, [Path, OldPath, NewPath](bool bNew) { *Path = bNew ? NewPath : OldPath; });
                }
                return;
            }
            // FFilePath holds a long package name as plain text (ProjectPackagingSettings.MapsToCook): a moved map
            // left there would silently drop out of every packaged build.
            if (Struct->Struct->GetFName() == TEXT("FilePath"))
            {
                const FStrProperty* Text = FindFProperty<FStrProperty>(Struct->Struct, TEXT("FilePath"));
                FString* Value = Text ? Text->ContainerPtrToValuePtr<FString>(Data) : nullptr;
                const FMcpRenameTarget* Target = Value && Value->StartsWith(TEXT("/")) && Value->Len() < NAME_SIZE
                    ? ByPackage.Find(FName(**Value)) : nullptr;
                FString NewValue;
                if (Target && Target->NewObjectPrefix.Split(TEXT("."), &NewValue, nullptr))
                {
                    const FString OldValue = *Value;
                    Note(Owner, Label, [Value, OldValue, NewValue](bool bNew) { *Value = bNew ? NewValue : OldValue; });
                }
                return;
            }
            for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
            {
                for (int32 Index = 0; Index < It->ArrayDim; ++Index)
                {
                    Walk(Owner, Label + TEXT(".") + It->GetName(), *It, It->ContainerPtrToValuePtr<void>(Data, Index), Depth + 1);
                }
            }
        }
        else if (CastField<FSoftObjectProperty>(Property))
        {
            FSoftObjectPtr* Ptr = static_cast<FSoftObjectPtr*>(Data);
            FSoftObjectPath NewPath;
            if (Remap(Ptr->ToSoftObjectPath(), NewPath))
            {
                const FSoftObjectPath OldPath = Ptr->ToSoftObjectPath();
                Note(Owner, Label, [Ptr, OldPath, NewPath](bool bNew) { *Ptr = FSoftObjectPtr(bNew ? NewPath : OldPath); });
            }
        }
        else if (const FArrayProperty* Array = CastField<FArrayProperty>(Property))
        {
            FScriptArrayHelper Helper(Array, Data);
            for (int32 Index = 0; Index < Helper.Num(); ++Index)
            {
                Walk(Owner, FString::Printf(TEXT("%s[%d]"), *Label, Index), Array->Inner, Helper.GetRawPtr(Index), Depth + 1);
            }
        }
        else if (const FMapProperty* Map = CastField<FMapProperty>(Property))
        {
            WalkMapKeys(Owner, Label, Map, Data);
        }
        else if (const FObjectProperty* Hard = CastField<FObjectProperty>(Property))
        {
            const UObject* Object = Hard->GetObjectPropertyValue(Data);
            if (Object && ByPackage.Contains(Object->GetOutermost()->GetFName()))
            {
                Blocking.AddUnique(Label);
            }
        }
    }
};

void SetStrings(const TSharedPtr<FJsonObject>& Report, const TCHAR* Field, const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Array;
    for (const FString& Value : Values)
    {
        Array.Add(MakeShared<FJsonValueString>(Value));
    }
    Report->SetArrayField(Field, Array);
}
} // namespace

bool RenameWithSettingsFollow(const TArray<FAssetRenameData>& RenameData, const TSharedPtr<FJsonObject>& Report,
                              FString& OutFailure)
{
    FMcpSettingsFollow Follow;
    for (const FAssetRenameData& Data : RenameData)
    {
        if (const UObject* Asset = Data.Asset.Get())
        {
            Follow.ByPackage.Add(Asset->GetOutermost()->GetFName(),
                                 FMcpRenameTarget{Asset->GetName(), Data.NewPackagePath / Data.NewName + TEXT(".") + Data.NewName});
        }
    }
    // The same set AssetRenameManager's FindCDOReferences checks: native class defaults only.
    for (TObjectIterator<UClass> It; It; ++It)
    {
        UClass* Class = *It;
        UObject* Defaults = (Class->ClassGeneratedBy || Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
            ? nullptr : Class->GetDefaultObject(false);
        for (TFieldIterator<FProperty> Prop(Class); Defaults && Prop; ++Prop)
        {
            for (int32 Index = 0; Index < Prop->ArrayDim; ++Index)
            {
                Follow.Walk(Defaults, Class->GetName() + TEXT(".") + Prop->GetName(), *Prop,
                            Prop->ContainerPtrToValuePtr<void>(Defaults, Index), 0);
            }
        }
    }

    // Nobody can answer a modal or watch a progress window during an MCP call.
    TGuardValue<bool> NoSlowTaskWindows(GIsSilent, true);
    TGuardValue<bool> NoModals(GIsRunningUnattendedScript, true);
    // RenameAssets refuses outright while the registry is still discovering assets (just after startup).
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    if (Registry.IsLoadingAssets())
    {
        Registry.WaitForCompletion();
    }
    const bool bRenamed = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().RenameAssets(RenameData);
    for (int32 Index = Follow.Swaps.Num() - 1; Index >= 0 && !bRenamed; --Index)
    {
        Follow.Swaps[Index](false);
    }
    for (UObject* Defaults : Follow.Touched)
    {
        const UClass* Class = Defaults->GetClass();
        if (!bRenamed)
        {
            continue;
        }
        if (Class->HasAnyClassFlags(CLASS_DefaultConfig))
        {
            Defaults->TryUpdateDefaultConfigFile(FString(), false);
        }
        else if (Class->HasAnyClassFlags(CLASS_Config))
        {
            Defaults->SaveConfig();
        }
    }
    SetStrings(Report, TEXT("settingsUpdated"), bRenamed ? Follow.Moved : TArray<FString>());
    if (Follow.Blocking.Num() > 0)
    {
        SetStrings(Report, TEXT("blockingReferences"), Follow.Blocking);
    }
    if (!bRenamed)
    {
        OutFailure = Follow.Blocking.Num() > 0
            ? FString::Printf(TEXT("Nothing was renamed: native class defaults hold hard references to these assets (%s)."),
                              *FString::Join(Follow.Blocking, TEXT(", ")))
            : TEXT("Nothing was renamed: a destination is taken, a file is read-only, or a package is checked out elsewhere.");
    }
    return bRenamed;
}

} // namespace McpAssetRename
