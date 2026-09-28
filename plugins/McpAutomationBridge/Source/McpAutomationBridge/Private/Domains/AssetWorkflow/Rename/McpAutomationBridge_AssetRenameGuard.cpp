// Copyright (c) 2024 MCP Automation Bridge Contributors

#include "Domains/AssetWorkflow/Rename/McpAutomationBridge_AssetRenameGuard.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Domains/AssetWorkflow/Rename/McpAutomationBridge_AssetRenameFollowSupport.h"
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

    bool RemapFile(const FString& Package, FString& OutNew) const
    {
        const FMcpRenameTarget* Target = Package.Len() < NAME_SIZE ? ByPackage.Find(FName(*Package)) : nullptr;
        return Target && Target->NewObjectPrefix.Split(TEXT("."), &OutNew, nullptr);
    }

    // A folder follows only a whole-folder move: every package under it renamed, keeping the layout below it.
    bool RemapFolder(const FString& Folder, FString& OutNew) const
    {
        TArray<FAssetData> Assets;
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetsByPath(FName(*Folder), Assets, true);
        for (const FAssetData& Asset : Assets)
        {
            FString NewPackage;
            const FString Tail = Asset.PackageName.ToString().Mid(Folder.Len());
            if (!RemapFile(Asset.PackageName.ToString(), NewPackage) || !NewPackage.EndsWith(Tail) ||
                (!OutNew.IsEmpty() && OutNew != NewPackage.LeftChop(Tail.Len())))
            {
                return false;
            }
            OutNew = NewPackage.LeftChop(Tail.Len());
        }
        return !OutNew.IsEmpty() && OutNew != Folder;
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
            // FFilePath and FDirectoryPath hold long package names as plain text (ProjectPackagingSettings.MapsToCook,
            // DirectoriesToAlwaysCook): a moved map or folder left there would silently drop out of every packaged build.
            const bool bFile = Struct->Struct->GetFName() == TEXT("FilePath");
            if (bFile || Struct->Struct->GetFName() == TEXT("DirectoryPath"))
            {
                const FStrProperty* Text = FindFProperty<FStrProperty>(Struct->Struct, bFile ? TEXT("FilePath") : TEXT("Path"));
                FString* Value = Text ? Text->ContainerPtrToValuePtr<FString>(Data) : nullptr;
                FString NewValue;
                if (Value && Value->StartsWith(TEXT("/")) && (bFile ? RemapFile(*Value, NewValue) : RemapFolder(*Value, NewValue)))
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
    SetRenameReportStrings(Report, TEXT("settingsUpdated"), bRenamed ? Follow.Moved : TArray<FString>());
    if (Follow.Blocking.Num() > 0)
    {
        SetRenameReportStrings(Report, TEXT("blockingReferences"), Follow.Blocking);
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
