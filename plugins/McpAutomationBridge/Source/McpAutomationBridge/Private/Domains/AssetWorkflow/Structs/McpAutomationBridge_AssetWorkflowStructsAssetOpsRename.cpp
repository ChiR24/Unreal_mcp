#pragma once

#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"
#include "UObject/ObjectRedirector.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/ScopedEvent.h"

#if WITH_EDITOR

#ifdef MCP_ASSETWORKFLOW_STRUCTS_ASSETOPS_IMPL

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

// rename_struct (reuses this) + duplicate_struct
static bool HandleStructAssetAction_Rename(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
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
        // Auto-trigger dependent refresh after the duplicate mutation (issue #510).
        McpRefreshStructDependents(Dst);
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

    // rename_struct  (supported AssetTools rename with automatic redirector)
    if (Lower == TEXT("rename_struct"))
    {
        FString StructPath = GetPayloadString(Payload, TEXT("structPath"));
        if (StructPath.IsEmpty())
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: structPath"), TEXT("MISSING_PARAMETER"));
            return true;
        }

        // Resolve the destination object path. The canonical new parameter is
        // newStructPath (a full object path, e.g. /Game/Folder/NewName.NewName).
        // Fall back to the legacy newName (+ optional destinationFolder) style.
        FString NewStructPath = GetPayloadString(Payload, TEXT("newStructPath"));
        FString NewName = GetPayloadString(Payload, TEXT("newName"));
        FString DestFolder = GetPayloadString(Payload, TEXT("destinationFolder"));

        FString FinalObjectPath;
        FString NewNameOnly;
        if (!NewStructPath.IsEmpty())
        {
            FinalObjectPath = NewStructPath;
            NewNameOnly = FPaths::GetBaseFilename(FinalObjectPath);
        }
        else if (!NewName.IsEmpty())
        {
            NewName = SanitizeAssetName(NewName);
            NewNameOnly = NewName;
            const FString ParentFolder = DestFolder.IsEmpty()
                ? FPaths::GetPath(StructPath)
                : DestFolder;
            FinalObjectPath = FString::Printf(TEXT("%s/%s.%s"), *ParentFolder, *NewNameOnly, *NewNameOnly);
        }
        else
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Missing required parameter: structPath and (newStructPath | newName)"),
                TEXT("MISSING_PARAMETER"));
            return true;
        }

        UUserDefinedStruct* S = LoadObject<UUserDefinedStruct>(nullptr, *StructPath);
        if (!S)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Struct not found: ") + StructPath, TEXT("NOT_FOUND"));
            return true;
        }

        // Reject a no-op rename.
        if (FinalObjectPath == StructPath)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("newStructPath equals the current struct path; no rename performed"),
                TEXT("NOOP"));
            return true;
        }

        // Reject an explicit collision rather than silently overwriting an
        // existing asset (issue #510: explicit diagnostics for failures).
        // Only check for an actual struct at the destination. The older
        // FindPackage check produced false positives in UE 5.7 (the package
        // path resolution matched the parent /Game package or a stale
        // redirector), blocking valid renames of brand-new names.
        if (LoadObject<UUserDefinedStruct>(nullptr, *FinalObjectPath))
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Destination already exists: ") + FinalObjectPath, TEXT("ALREADY_EXISTS"));
            return true;
        }

        const FString NewPackagePath = FPaths::GetPath(FinalObjectPath);

        // Supported, editor-side rename. AssetTools::RenameAssets moves the
        // asset, leaves a UObjectRedirector at the old path automatically when
        // the struct is referenced, fixes up soft references, and saves the
        // affected packages for us. It must run on the game thread or it
        // deadlocks. When the handler already runs on the game thread (native
        // MCP path), call directly; otherwise bounce via AsyncTask+Wait.
        auto DoRename = [&S, &NewPackagePath, &NewNameOnly]()
        {
            FAssetToolsModule& AssetToolsModule =
                FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
            IAssetTools& AssetTools = AssetToolsModule.Get();

            TArray<FAssetRenameData> AssetsToRename;
            AssetsToRename.Emplace(S, NewPackagePath, NewNameOnly);
            AssetTools.RenameAssets(AssetsToRename);
        };

        if (IsInGameThread())
        {
            DoRename();
        }
        else
        {
            FScopedEvent Event;
            AsyncTask(ENamedThreads::GameThread, [&Event, &DoRename]()
            {
                DoRename();
                Event.Trigger();
            });
            Event.Get()->Wait();  // pure wait, NO Pump -- pumping deadlocks
        }

        UUserDefinedStruct* Renamed = LoadObject<UUserDefinedStruct>(nullptr, *FinalObjectPath);
        if (!Renamed)
        {
            Bridge.SendAutomationError(RequestingSocket, RequestId,
                TEXT("Rename failed: struct not found at destination ") + FinalObjectPath,
                TEXT("OPERATION_FAILED"));
            return true;
        }

        // Did the supported rename leave a redirector at the old path?
        const bool bLeftRedirector = (LoadObject<UObjectRedirector>(nullptr, *StructPath) != nullptr);

        // Recompile every referencing Blueprint and notify matching DataTables
        // via the shared helper (redirector detection above happens first).
        McpRefreshStructDependents(S);

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("oldPath"), StructPath);
        Result->SetStringField(TEXT("newPath"), FinalObjectPath);
        Result->SetStringField(TEXT("newName"), NewNameOnly);
        Result->SetStringField(TEXT("assetPath"), FinalObjectPath);
        Result->SetBoolField(TEXT("renamed"), true);
        Result->SetBoolField(TEXT("leftRedirector"), bLeftRedirector);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct renamed"), Result);
        return true;
    }

    return false;
}

#endif // MCP_ASSETWORKFLOW_STRUCTS_ASSETOPS_IMPL
#endif // WITH_EDITOR
