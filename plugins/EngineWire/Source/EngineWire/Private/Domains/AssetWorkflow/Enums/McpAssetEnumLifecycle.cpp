#include "Domains/AssetWorkflow/Enums/McpAssetEnumShared.h"
#include "Domains/AssetWorkflow/Operations/McpAssetBulkSelection.h"
#include "Kismet2/EnumEditorUtils.h"
#include "Safety/McpSafeOperationsAssetDelete.h"


bool HandleEnumAction(
    FString Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult)
{
    return HandleEnumLifecycleActions(Action, Params, OutResult) || HandleEnumValueActions(Action, Params, OutResult);
}

bool HandleEnumLifecycleActions(
    const FString& Action,
    const TSharedPtr<FJsonObject>& Params,
    TSharedPtr<FJsonObject>& OutResult)
{
    if (Action == TEXT("create_enum"))
    {
        FString EnumPath = GetJsonStringField(Params, TEXT("enumPath"));
        FString Name = GetJsonStringField(Params, TEXT("name"));
        FString Path = GetJsonStringField(Params, TEXT("path"), TEXT("/Game/Enums"));
        bool bSave = EnumSaveRequested(Params);

        if (Name.IsEmpty() && !EnumPath.IsEmpty())
        {
            if (LoadObject<UUserDefinedEnum>(nullptr, *EnumPath))
            {
                SetEnumResultFields(OutResult, false, FString::Printf(TEXT("Enum already exists: %s"), *EnumPath), TEXT("ASSET_ALREADY_EXISTS"));
                return true;
            }
            if (!EnumPath.Split(TEXT("/"), &Path, &Name, ESearchCase::CaseSensitive, ESearchDir::FromEnd)) { Name = EnumPath; }
        }

        if (Name.IsEmpty())
        {
            SetEnumResultFields(OutResult, false, TEXT("Missing required parameter: name or enumPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        // The initial values (`values`, names or {name} objects), checked before anything is made: two entries of one
        // name break the enum.
        TArray<FString> ValueNames;
        TSet<FString> Named; // the duplicate check, by hash: a search of the list per value grew with its square
        const TArray<TSharedPtr<FJsonValue>>* InitValues = nullptr;
        if (Params->TryGetArrayField(TEXT("values"), InitValues) && InitValues)
        {
            for (const TSharedPtr<FJsonValue>& V : *InitValues)
            {
                FString ValName;
                const TSharedPtr<FJsonObject>* Obj = nullptr;
                if (V->Type == EJson::Object && V->TryGetObject(Obj) && *Obj) { (*Obj)->TryGetStringField(TEXT("name"), ValName); }
                else if (V->Type != EJson::Object) { ValName = V->AsString(); }
                if (ValName.IsEmpty()) { continue; }
                bool bTwice = false;
                Named.Add(ValName, &bTwice);
                if (bTwice)
                {
                    SetEnumResultFields(OutResult, false, FString::Printf(TEXT("values names '%s' twice; nothing was created."), *ValName), TEXT("INVALID_ARGUMENT"));
                    return true;
                }
                ValueNames.Add(ValName);
            }
        }

        FString PathError;
        FString SanitizedName = SanitizeAssetName(Name);
        FString PackageName;
        if (!ValidateAssetCreationPath(Path, SanitizedName, PackageName, PathError))
        {
            SetEnumResultFields(OutResult, false, PathError);
            return true;
        }

        // Refused when any asset already holds the path: a bare CreatePackage handed back that asset's package, and the
        // enum made in it replaced an enum in place or, over an asset of another class, was a fatal error.
        FString CreateError, CreateCode;
        UPackage* Package = McpCreatePackageForNewAsset(PackageName, CreateError, CreateCode);
        if (!Package)
        {
            SetEnumResultFields(OutResult, false, CreateError, *CreateCode);
            return true;
        }

        // Transactional, as the editor's own factory makes an enum (and as every enum is once loaded): without it the
        // enum editor's edits of a new enum kept no undo step until the editor restarted.
        UUserDefinedEnum* Enum = Cast<UUserDefinedEnum>(FEnumEditorUtils::CreateUserDefinedEnum(
            Package, FName(*SanitizedName), RF_Public | RF_Standalone | RF_Transactional));
        if (!Enum)
        {
            SetEnumResultFields(OutResult, false, TEXT("Failed to create user defined enum"));
            return true;
        }
        if (ValueNames.Num() > 0)
        {
            TArray<TPair<FName, int64>> Names;
            for (int32 i = 0; i < ValueNames.Num(); ++i) { Names.Emplace(*Enum->GenerateFullEnumName(*ValueNames[i]), i); }
            MCP_SET_ENUMS(Enum, Names, Enum->GetCppForm());
        }

        Package->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(Enum);
        const bool bSaved = FinalizeEnum(Enum, bSave);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("enumPath"), McpToObjectPath(PackageName));
        Result->SetStringField(TEXT("enumName"), SanitizedName);
        Result->SetBoolField(TEXT("saved"), bSaved);
        OutResult = Result;
        return true;
    }

    if (Action == TEXT("get_enum"))
    {
        bool bHandled = false;
        UUserDefinedEnum* Enum = RequireEnum(Params, OutResult, bHandled);
        if (bHandled) { return true; }

        // name is the internal name stored values use; displayName what the editor shows, which rename_enum_value changes.
        TArray<TSharedPtr<FJsonValue>> ValuesArr;
        for (int32 i = 0; i < Enum->NumEnums(); ++i)
        {
            const FString EntryName = Enum->GetNameStringByIndex(i);
            if (McpIsEnumMaxEntry(Enum, i)) { continue; }
            TSharedPtr<FJsonObject> V = MakeShared<FJsonObject>();
            V->SetStringField(TEXT("name"), EntryName);
            V->SetStringField(TEXT("displayName"), Enum->GetDisplayNameTextByIndex(i).ToString());
            V->SetNumberField(TEXT("value"), static_cast<double>(Enum->GetValueByIndex(i)));
            ValuesArr.Add(MakeShared<FJsonValueObject>(V));
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("enumPath"), Enum->GetPathName());
        Result->SetArrayField(TEXT("values"), ValuesArr);
        Result->SetNumberField(TEXT("valueCount"), ValuesArr.Num());
        OutResult = Result;
        return true;
    }

    if (Action == TEXT("delete_enum"))
    {
        bool bHandled = false;
        UUserDefinedEnum* Enum = RequireEnum(Params, OutResult, bHandled);
        if (bHandled) { return true; }

        // As delete_struct deletes: refused while other assets use it unless force says to break them, then the safe
        // delete (the engine's DeleteObjects without its dialog deleted nothing while anything referenced the enum, so
        // there was no way through), and the loaded users recompile. The path is captured first: the pointer dangles
        // once the delete succeeds.
        const FString EnumPath = Enum->GetPathName();
        const FString EnumPackage = Enum->GetOutermost()->GetName();
        const TArray<FString> Users = McpOutsideReferencers(EnumPackage, {EnumPackage});
        const bool bForce = GetJsonBoolField(Params, TEXT("force"), false);
        if ((Users.Num() > 0 && !bForce) || !McpSafeOperations::McpDeleteAssetAndFile(EnumPath))
        {
            SetEnumResultFields(OutResult, false, Users.Num() > 0 && !bForce
                ? FString::Printf(TEXT("Enum %s was not deleted: assets still use it (referencedBy); change or delete them first, or pass force true to delete it anyway, breaking them."), *EnumPath)
                : FString::Printf(TEXT("Enum %s was not deleted: the asset or its file is still there."), *EnumPath),
                TEXT("DELETE_FAILED"));
            OutResult->SetStringField(TEXT("enumPath"), EnumPath);
            OutResult->SetBoolField(TEXT("deleted"), false);
            OutResult->SetArrayField(TEXT("referencedBy"), McpHandlerUtils::ToJsonStringArray(Users));
            return true;
        }
        McpRecompileLoadedBlueprints(Users);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("enumPath"), EnumPath);
        Result->SetBoolField(TEXT("deleted"), true);
        OutResult = Result;
        return true;
    }

    return false;
}

