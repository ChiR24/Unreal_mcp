#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"
#include "UObject/ObjectRedirector.h"

#if WITH_EDITOR

// Copy all variable descriptions from Src into a freshly created (empty) Dst
// struct. Avoids DuplicateObject (which triggers a UDS reinstancing cascade
// that deadlocks synchronous native requests).
static void CopyStructMembers(UUserDefinedStruct* Dst, UUserDefinedStruct* Src)
{
    for (const FStructVariableDescription& Var : FStructureEditorUtils::GetVarDesc(Src))
    {
        // Skip the engine-seeded default variable (MemberVar_0) that every
        // UserDefinedStruct carries; it is not a real user member.
        if (Var.FriendlyName == TEXT("MemberVar_0"))
        {
            continue;
        }
        FEdGraphPinType Pin = Var.ToPinType();
        if (FStructureEditorUtils::AddVariable(Dst, Pin))
        {
            if (FStructVariableDescription* NewVar =
                    FStructureEditorUtils::GetVarDescByGuid(Dst,
                        FStructureEditorUtils::GetVarDesc(Dst).Last().VarGuid))
            {
                NewVar->FriendlyName = Var.FriendlyName;
                NewVar->DefaultValue = Var.DefaultValue;
                NewVar->ToolTip = Var.ToolTip;
                NewVar->MetaData = Var.MetaData;
            }
        }
    }
}

bool HandleStructAssetActions(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();

    // duplicate_struct  (rename_struct reuses this)
    auto DuplicateStructTo = [&](const FString& SrcPath, const FString& DestPath,
                                 const FString& DestName, FString& OutError) -> UUserDefinedStruct*
    {
        UUserDefinedStruct* Src = LoadObject<UUserDefinedStruct>(nullptr, *SrcPath);
        if (!Src)
        {
            OutError = FString::Printf(TEXT("Struct not found: %s"), *SrcPath);
            return nullptr;
        }

        UPackage* DestPkg = CreatePackage(*DestPath);
        if (!DestPkg)
        {
            OutError = TEXT("Failed to create destination package");
            return nullptr;
        }

        UUserDefinedStruct* Dst = FStructureEditorUtils::CreateUserDefinedStruct(
            DestPkg, FName(*DestName), RF_Public | RF_Standalone);
        if (!Dst)
        {
            OutError = TEXT("Failed to create duplicate struct");
            return nullptr;
        }

        // Strip the seeded default var, then copy real members.
        TArray<FGuid> Seeded;
        for (const FStructVariableDescription& V : FStructureEditorUtils::GetVarDesc(Dst))
            Seeded.Add(V.VarGuid);
        for (const FGuid& G : Seeded) FStructureEditorUtils::RemoveVariable(Dst, G);

        CopyStructMembers(Dst, Src);
        FStructureEditorUtils::CompileStructure(Dst);
        Dst->MarkPackageDirty();
        McpSafeAssetSave(Dst);
        return Dst;
    };

    if (Lower == TEXT("duplicate_struct"))
    {
        FString StructPath = GetPayloadString(Payload, TEXT("structPath"));
        FString DestName = GetPayloadString(Payload, TEXT("destinationName"));
        FString DestPath = GetPayloadString(Payload, TEXT("destinationPath"));
        if (StructPath.IsEmpty() || (DestName.IsEmpty() && DestPath.IsEmpty()))
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: structPath and (destinationName|destinationPath)"),
                TEXT("MISSING_PARAMETER"));
            return true;
        }

        FString FinalDest;
        FString FinalName;
        if (!DestPath.IsEmpty())
        {
            FinalDest = DestPath;
            FinalName = FPaths::GetBaseFilename(DestPath);
        }
        else
        {
            FString Parent = FPaths::GetPath(StructPath);
            FinalName = SanitizeAssetName(DestName);
            FinalDest = FString::Printf(TEXT("%s/%s"), *Parent, *FinalName);
        }

        FString Err;
        UUserDefinedStruct* Dup = DuplicateStructTo(StructPath, FinalDest, FinalName, Err);
        if (!Dup)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId, Err, TEXT("OPERATION_FAILED"));
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("sourcePath"), StructPath);
        const FString DupObjectPath = FinalDest + TEXT(".") + FinalName;
        Result->SetStringField(TEXT("duplicatedPath"), DupObjectPath);
        Result->SetBoolField(TEXT("duplicated"), true);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct duplicated"), Result);
        return true;
    }

    // rename_struct  (duplicate to new path, then delete old)
    if (Lower == TEXT("rename_struct"))
    {
        FString StructPath = GetPayloadString(Payload, TEXT("structPath"));
        FString NewName = GetPayloadString(Payload, TEXT("newName"));
        if (StructPath.IsEmpty() || NewName.IsEmpty())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: structPath, newName"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        NewName = SanitizeAssetName(NewName);
        FString Parent = FPaths::GetPath(StructPath);
        FString DestPath = FString::Printf(TEXT("%s/%s"), *Parent, *NewName);

        FString Err;
        UUserDefinedStruct* Moved = DuplicateStructTo(StructPath, DestPath, NewName, Err);
        if (!Moved)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId, Err, TEXT("OPERATION_FAILED"));
            return true;
        }

        // Leave a redirector at the old path instead of deleting it, so soft
        // references to the old struct (in other Blueprints, data assets, etc.)
        // keep resolving to the renamed struct. Deleting the old package would
        // orphan those references.
        UUserDefinedStruct* OldS = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
        if (OldS)
        {
            UPackage* OldPkg = OldS->GetOutermost();
            const FString OldObjectName = FPaths::GetBaseFilename(StructPath);

            // Detach the stale struct from the package so the redirector can
            // claim the original object name without a collision.
            OldS->ClearFlags(RF_Standalone | RF_Public);
            OldS->Rename(TEXT(""), nullptr, REN_DontCreateRedirectors | REN_NonTransactional);
            OldS->MarkAsGarbage();

            UObjectRedirector* Redirector = NewObject<UObjectRedirector>(OldPkg, *OldObjectName);
            Redirector->DestinationObject = Moved;
            Redirector->SetFlags(RF_Standalone | RF_Public);
            OldPkg->MarkPackageDirty();
            McpSafeAssetSave(OldPkg);
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("oldPath"), StructPath);
        const FString NewObjectPath = DestPath + TEXT(".") + NewName;
        Result->SetStringField(TEXT("newPath"), NewObjectPath);
        Result->SetStringField(TEXT("assetPath"), NewObjectPath);
        Result->SetBoolField(TEXT("renamed"), true);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct renamed"), Result);
        return true;
    }

    // delete_struct
    if (Lower == TEXT("delete_struct"))
    {
        FString StructPath = GetPayloadString(Payload, TEXT("structPath"));
        if (StructPath.IsEmpty())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: structPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UUserDefinedStruct* S = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
        if (!S)
        {
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("deletedPath"), StructPath);
            Result->SetBoolField(TEXT("deleted"), false);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
                TEXT("Struct deleted (or did not exist)"), Result);
            return true;
        }

        // Non-blocking delete: unload the package from memory and remove the
        // asset file. ObjectTools::DeleteObjects deadlocks the synchronous
        // native request thread (it waits on a save that needs the game thread),
        // so we delete the file directly and let GC reclaim the package.
        UPackage* Pkg = S->GetOutermost();
        if (Pkg)
        {
            Pkg->ClearFlags(RF_Standalone | RF_Public);
            Pkg->RemoveFromRoot();
            Pkg->MarkAsGarbage();
        }
        FString PackageName = FPackageName::ObjectPathToPackageName(StructPath);
        FString FilePath = FPackageName::LongPackageNameToFilename(
            PackageName, FPackageName::GetAssetPackageExtension());
        IFileManager::Get().Delete(*FilePath, /*bRequireExists=*/false, /*bEvenReadOnly=*/true, /*bQuiet=*/true);
        FString UexpPath = FPackageName::LongPackageNameToFilename(
            PackageName, TEXT(".uexp"));
        if (IFileManager::Get().FileExists(*UexpPath))
        {
            IFileManager::Get().Delete(*UexpPath, /*bRequireExists=*/false, /*bEvenReadOnly=*/true, /*bQuiet=*/true);
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("deletedPath"), StructPath);
        Result->SetBoolField(TEXT("deleted"), true);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct deleted"), Result);
        return true;
    }

    // refresh_struct_dependencies
    if (Lower == TEXT("refresh_struct_dependencies"))
    {
        FString StructPath = GetPayloadString(Payload, TEXT("structPath"));
        if (StructPath.IsEmpty())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: structPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        UUserDefinedStruct* S = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
        if (!S)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                FString::Printf(TEXT("Struct not found: %s"), *StructPath), TEXT("ASSET_NOT_FOUND"));
            return true;
        }

        FStructureEditorUtils::CompileStructure(S);

        TArray<FString> Refreshed;
        ForEachReferencingBlueprint(S, [&](UBlueprint* BP)
        {
            if (BP)
            {
                FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None);
                Refreshed.Add(BP->GetPathName());
            }
        });

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("structPath"), StructPath);
        TArray<TSharedPtr<FJsonValue>> R;
        for (const FString& P : Refreshed) R.Add(MakeShared<FJsonValueString>(P));
        Result->SetArrayField(TEXT("refreshedBlueprints"), R);
        Result->SetNumberField(TEXT("refreshedCount"), Refreshed.Num());
        Result->SetBoolField(TEXT("refreshed"), true);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct dependencies refreshed"), Result);
        return true;
    }

    return false;
}

#endif // WITH_EDITOR
