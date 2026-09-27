#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

// Tracked delete of a UserDefinedStruct (delete_struct), on the game thread like every handler.
#include "ObjectTools.h"

#ifdef MCP_ASSETWORKFLOW_STRUCTS_ASSETOPS_IMPL

// delete_struct
static bool HandleStructAssetAction_Delete(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();

    if (Lower == TEXT("delete_struct"))
    {
        FString StructPath = GetJsonStringField(Payload, TEXT("structPath"));
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

        // Capture referencers BEFORE any delete. GetReferencers is a synchronous
        // AssetRegistry query (safe on the native request thread). We need the
        // list both for the safety gate below and to recompile dependent
        // Blueprints after the struct is removed.
        IAssetRegistry& AR = FAssetRegistryModule::GetRegistry();
        TArray<FAssetIdentifier> PreDeleteReferencers;
        AR.GetReferencers(FAssetIdentifier(S->GetOutermost()->GetFName()), PreDeleteReferencers);

        // Safety gate: refuse to delete a struct that is still referenced by
        // other assets unless force:true is given. Deleting a referenced struct
        // orphans those references and can corrupt dependent Blueprints, data
        // assets, or other structs. See issue #510 — safely reject existing
        // references with an explicit diagnostic listing the referencers.
        const bool bForce = GetJsonBoolField(Payload, TEXT("force"), false);
        if (!bForce && PreDeleteReferencers.Num() > 0)
        {
            TArray<TSharedPtr<FJsonValue>> RefArr;
            for (const FAssetIdentifier& Ref : PreDeleteReferencers)
            {
                RefArr.Add(MakeShared<FJsonValueString>(Ref.ToString()));
            }
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("structPath"), StructPath);
            Result->SetNumberField(TEXT("referencerCount"), PreDeleteReferencers.Num());
            Result->SetArrayField(TEXT("referencers"), RefArr);
            Result->SetBoolField(TEXT("deleted"), false);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                FString::Printf(TEXT("Struct '%s' is referenced by %d asset(s); pass force:true to delete anyway"),
                    *StructPath, PreDeleteReferencers.Num()),
                Result);
            return true;
        }

        // ObjectTools::DeleteObjects is the supported editor API: it performs the
        // file deletion, redirector creation, garbage collection and
        // source-control bookkeeping for us — we must NOT delete .uasset/.uexp
        // files directly (issue #510).
        TArray<UObject*> ObjectsToDelete = { S };

        // ObjectTools::DeleteObjects returns the number of objects actually removed.
        // A cancelled or failed delete returns 0; we must NOT claim success or
        // recompile referencers in that case.
        int32 DeletedCount = 0;

        auto DoDelete = [&ObjectsToDelete, &PreDeleteReferencers, &DeletedCount]()
        {
            DeletedCount = ObjectTools::DeleteObjects(ObjectsToDelete, /*bShowConfirmation=*/false);
            if (DeletedCount == 0)
            {
                return;
            }

            // Recompile referencers snapshotted before the delete so dependent
            // Blueprints rebuild against the now-removed struct. Resolve by package
            // name: Ref.ToString() may be a package path (e.g. /Game/Foo), not an
            // object path (e.g. /Game/Foo.Foo) that GetAssetByObjectPath expects.
            IAssetRegistry& AR = FAssetRegistryModule::GetRegistry();
            for (const FAssetIdentifier& Ref : PreDeleteReferencers)
            {
                TArray<FAssetData> PackageAssets;
                AR.GetAssetsByPackageName(Ref.PackageName, PackageAssets);
                for (const FAssetData& AssetData : PackageAssets)
                {
                    if (AssetData.IsValid() && AssetData.IsAssetLoaded())
                    {
                        if (UBlueprint* BP = Cast<UBlueprint>(AssetData.GetAsset()))
                        {
                            FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None);
                        }
                    }
                }
            }
        };

        DoDelete();

        if (DeletedCount == 0)
        {
            TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
            Result->SetStringField(TEXT("deletedPath"), StructPath);
            Result->SetBoolField(TEXT("deleted"), false);
            Bridge.SendAutomationResponse(RequestingSocket, RequestId, false,
                TEXT("Struct delete failed (0 objects removed)"), Result);
            return true;
        }

        TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
        Result->SetStringField(TEXT("deletedPath"), StructPath);
        Result->SetBoolField(TEXT("deleted"), true);
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
            TEXT("Struct deleted"), Result);
        return true;
    }

    return false;
}

#endif // MCP_ASSETWORKFLOW_STRUCTS_ASSETOPS_IMPL
