#include "Domains/AssetWorkflow/Enums/Shared.h"
#include "Domains/AssetWorkflow/Operations/McpAssetBulkSelection.h"
#include "Async/Async.h"
#include "Kismet2/EnumEditorUtils.h"
#include "Misc/ScopedEvent.h"
#include "ObjectTools.h"


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
                if (ValueNames.Contains(ValName))
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
        Result->SetStringField(TEXT("enumPath"), PackageName + TEXT(".") + SanitizedName);
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
            if (EntryName.EndsWith(TEXT("_MAX"))) { continue; }
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

        // Editor-tracked delete via ObjectTools::DeleteObjects (issue
        // #struct-ecosystem [27]). The previous implementation unloaded the
        // package and deleted the .uasset/.uexp files directly, which bypassed
        // the editor deletion flow (asset registry, source control, redirectors,
        // loaded references) and could leave a referenced enum half-unregistered.
        // ObjectTools::DeleteObjects performs the file deletion, redirector
        // creation, GC and source-control bookkeeping for us and must run on the
        // game thread. Capture the path BEFORE deletion: the pointer is dangling
        // once the delete succeeds.
        const FString EnumPath = Enum->GetPathName();
        const FString EnumPackage = Enum->GetOutermost()->GetName();
        TArray<UObject*> ObjectsToDelete = { Enum };

        // ObjectTools::DeleteObjects returns the number of objects actually removed.
        // A cancelled or failed delete returns 0; we must NOT claim success.
        const int32 DeletedCount = ObjectTools::DeleteObjects(ObjectsToDelete, /*bShowConfirmation=*/false);

        if (DeletedCount == 0)
        {
            // The engine refuses while anything references the enum; name what does, so the caller can fix it.
            const TArray<FString> Users = McpOutsideReferencers(EnumPackage, {EnumPackage});
            const TCHAR* Why = Users.Num() > 0 ? TEXT("assets still use it (referencedBy); change or delete them first")
                                               : TEXT("something loaded still holds it");
            SetEnumResultFields(OutResult, false, FString::Printf(TEXT("Enum %s was not deleted: %s."), *EnumPath, Why), TEXT("DELETE_FAILED"));
            OutResult->SetStringField(TEXT("enumPath"), EnumPath);
            OutResult->SetBoolField(TEXT("deleted"), false);
            OutResult->SetArrayField(TEXT("referencedBy"), McpHandlerUtils::ToJsonStringArray(Users));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("enumPath"), EnumPath);
        Result->SetBoolField(TEXT("deleted"), true);
        OutResult = Result;
        return true;
    }

    return false;
}

