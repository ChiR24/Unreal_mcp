#include "Domains/AssetWorkflow/Enums/Shared.h"
#include "Async/Async.h"
#include "Kismet2/EnumEditorUtils.h"
#include "Misc/ScopedEvent.h"
#include "ObjectTools.h"

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

        // Apply initial enum values when provided. Both the TS and native MCP
        // schemas advertise a `values` array for create_enum; previously the
        // handler created an empty enum and silently ignored it, leaving the
        // advertised parameter non-functional. Reuses the same SetEnums pattern
        // as add_enum_value.
        const TArray<TSharedPtr<FJsonValue>>* InitValues = nullptr;
        if (Params->TryGetArrayField(TEXT("values"), InitValues) && InitValues && InitValues->Num() > 0)
        {
            TArray<TPair<FName, int64>> Names;
            Names.Reserve(InitValues->Num());
            for (const TSharedPtr<FJsonValue>& V : *InitValues)
            {
                FString ValName;
                if (V->Type == EJson::Object)
                {
                    const TSharedPtr<FJsonObject>* Obj = nullptr;
                    if (V->TryGetObject(Obj) && *Obj)
                    {
                        (*Obj)->TryGetStringField(TEXT("name"), ValName);
                    }
                }
                else
                {
                    ValName = V->AsString();
                }
                if (ValName.IsEmpty())
                {
                    continue;
                }
                Names.Emplace(*Enum->GenerateFullEnumName(*ValName), 0);
            }
            for (int32 i = 0; i < Names.Num(); ++i)
            {
                Names[i].Value = i;
            }
            Enum->SetEnums(Names, Enum->GetCppForm());
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
        TArray<UObject*> ObjectsToDelete = { Enum };

        // ObjectTools::DeleteObjects returns the number of objects actually removed.
        // A cancelled or failed delete returns 0; we must NOT claim success.
        int32 DeletedCount = 0;
        auto DoDelete = [&ObjectsToDelete, &DeletedCount]()
        {
            DeletedCount = ObjectTools::DeleteObjects(ObjectsToDelete, /*bShowConfirmation=*/false);
        };

        // When invoked from the request queue (native MCP path) the handler already
        // runs on the game thread; dispatching to the game thread via AsyncTask +
        // Wait there would deadlock. Detect the game-thread case and call
        // ObjectTools directly. Otherwise run on the game thread and wait.
        if (IsInGameThread())
        {
            DoDelete();
        }
        else
        {
            FScopedEvent Event;
            AsyncTask(ENamedThreads::GameThread, [&Event, &DoDelete]()
            {
                DoDelete();
                Event.Trigger();
            });
            Event.Get()->Wait(); // pure wait, NO Pump — pumping deadlocks
        }

        if (DeletedCount == 0)
        {
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("enumPath"), EnumPath);
            Result->SetBoolField(TEXT("deleted"), false);
            OutResult = Result;
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

#endif // WITH_EDITOR
