#include "Domains/AssetWorkflow/Enums/Shared.h"
#include "Kismet2/EnumEditorUtils.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"

#if WITH_EDITOR

bool HandleEnumAction(
    FString Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult)
{
    const FString Lower = Action.ToLower();

    if (HandleEnumLifecycleActions(Lower, Params, OutResult))
    {
        return true;
    }
    if (HandleEnumValueActions(Lower, Params, OutResult))
    {
        return true;
    }

    SetEnumResultFields(OutResult, false, TEXT("Unknown enum action"));
    return false;
}

bool HandleEnumLifecycleActions(
    const FString& Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult)
{
    if (Action == TEXT("create_enum"))
    {
        FString EnumPath = GetPayloadString(Params, TEXT("enumPath"));
        FString Name = GetPayloadString(Params, TEXT("name"));
        FString Path = GetPayloadString(Params, TEXT("path"), TEXT("/Game/Enums"));
        bool bSave = GetPayloadBool(Params, TEXT("save"), false);

        if (Name.IsEmpty() && !EnumPath.IsEmpty())
        {
            if (LoadObject<UUserDefinedEnum>(nullptr, *EnumPath))
            {
                SetEnumResultFields(OutResult, false, FString::Printf(TEXT("Enum already exists: %s"), *EnumPath));
                return true;
            }
            int32 Slash = INDEX_NONE;
            EnumPath.FindLastChar('/', Slash);
            Name = EnumPath.Mid(Slash + 1);
            if (Slash != INDEX_NONE) { Path = EnumPath.Left(Slash); }
        }

        if (Name.IsEmpty())
        {
            SetEnumResultFields(OutResult, false, TEXT("Missing required parameter: name or enumPath"));
            return true;
        }

        FString PathError;
        FString SanitizedName = SanitizeAssetName(Name);
        FString PackageName;
        if (!ValidateAssetCreationPath(Path, SanitizedName, PackageName, PathError))
        {
            SetEnumResultFields(OutResult, false, PathError);
            return true;
        }

        UPackage* Package = CreatePackage(*PackageName);
        if (!Package)
        {
            SetEnumResultFields(OutResult, false, TEXT("Failed to create package"));
            return true;
        }

        UUserDefinedEnum* Enum = Cast<UUserDefinedEnum>(FEnumEditorUtils::CreateUserDefinedEnum(
            Package, FName(*SanitizedName), RF_Public | RF_Standalone));
        if (!Enum)
        {
            SetEnumResultFields(OutResult, false, TEXT("Failed to create user defined enum"));
            return true;
        }

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(Enum);
        FinalizeEnum(Enum, bSave);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("enumPath"), PackageName + TEXT(".") + SanitizedName);
        Result->SetStringField(TEXT("enumName"), SanitizedName);
        Result->SetBoolField(TEXT("saved"), bSave);
        OutResult = Result;
        return true;
    }

    if (Action == TEXT("get_enum"))
    {
        UUserDefinedEnum* Enum = ResolveUserDefinedEnum(Params);
        if (!Enum)
        {
            SetEnumResultFields(OutResult, false, TEXT("Enum not found"));
            return true;
        }

        TArray<TPair<FName, int64>> Names;
        const int32 NumEnumEntries = Enum->NumEnums();
        for (int32 i = 0; i < NumEnumEntries; ++i)
        {
            const FString EntryName = Enum->GetNameStringByIndex(i);
            if (EntryName.EndsWith(TEXT("_MAX"))) { continue; }
            Names.Add(TPair<FName, int64>(FName(*EntryName), Enum->GetValueByIndex(i)));
        }

        TArray<TSharedPtr<FJsonValue>> ValuesArr;
        for (const TPair<FName, int64>& Pair : Names)
        {
            TSharedPtr<FJsonObject> V = MakeShared<FJsonObject>();
            V->SetStringField(TEXT("name"), Pair.Key.ToString());
            V->SetNumberField(TEXT("value"), static_cast<double>(Pair.Value));
            ValuesArr.Add(MakeShared<FJsonValueObject>(V));
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("enumPath"), Enum->GetPathName());
        Result->SetArrayField(TEXT("values"), ValuesArr);
        Result->SetNumberField(TEXT("valueCount"), Names.Num());
        OutResult = Result;
        return true;
    }

    if (Action == TEXT("delete_enum"))
    {
        UUserDefinedEnum* Enum = ResolveUserDefinedEnum(Params);
        if (!Enum)
        {
            SetEnumResultFields(OutResult, false, TEXT("Enum not found"));
            return true;
        }

        // Non-blocking delete: unload the package from memory and remove the
        // asset file. Direct file delete + GC avoids the synchronous-request
        // deadlock that ObjectTools::DeleteObjects would cause on this thread.
        UPackage* Pkg = Enum->GetOutermost();
        if (Pkg)
        {
            Pkg->ClearFlags(RF_Standalone | RF_Public);
            Pkg->RemoveFromRoot();
            Pkg->MarkAsGarbage();
        }

        const FString PackageName = FPackageName::ObjectPathToPackageName(Enum->GetPathName());
        const FString FilePath = FPackageName::LongPackageNameToFilename(
            PackageName, FPackageName::GetAssetPackageExtension());
        IFileManager::Get().Delete(*FilePath, /*bRequireExists=*/false, /*bEvenReadOnly=*/true, /*bQuiet=*/true);

        const FString UexpPath = FPackageName::LongPackageNameToFilename(
            PackageName, TEXT(".uexp"));
        if (IFileManager::Get().FileExists(*UexpPath))
        {
            IFileManager::Get().Delete(*UexpPath, /*bRequireExists=*/false, /*bEvenReadOnly=*/true, /*bQuiet=*/true);
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("enumPath"), Enum->GetPathName());
        Result->SetBoolField(TEXT("deleted"), true);
        OutResult = Result;
        return true;
    }

    return false;
}

#endif // WITH_EDITOR
