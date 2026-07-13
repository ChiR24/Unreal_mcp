#pragma once

#include "Domains/AssetWorkflow/Structs/McpAutomationBridge_AssetWorkflowStructsShared.h"

// Deadlock-free tracked delete of a UserDefinedStruct (delete_struct).
// ObjectTools / UPackageTools must run on the game thread; called directly
// from the synchronous native MCP request thread they deadlock (they wait on
// a save that needs the game thread). We dispatch to the game thread via
// AsyncTask and wait (pure wait, NO Pump — pumping deadlocks) for completion.
#include "Misc/ScopedEvent.h"
#include "ObjectTools.h"

#if WITH_EDITOR

#ifdef MCP_ASSETWORKFLOW_STRUCTS_ASSETOPS_IMPL

// delete_struct
static bool HandleStructAssetAction_Delete(UMcpAutomationBridgeSubsystem& Bridge, const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();

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
        const bool bForce = GetPayloadBool(Payload, TEXT("force"), false);
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

        // Deadlock-free tracked delete. ObjectTools::DeleteObjects is the
        // supported editor API: it performs the file deletion, redirector
        // creation, garbage collection and source-control bookkeeping for us —
        // we must NOT delete .uasset/.uexp files directly (issue #510).
        //
        // When invoked from the request queue (native MCP path), the handler
        // already runs on the game thread. The AsyncTask+Wait pattern below
        // would deadlock because the queued task cannot execute until the
        // current game-thread work returns, but the handler blocks on Wait.
        // Detect the game-thread case and call ObjectTools directly instead.
        TArray<UObject*> ObjectsToDelete = { S };

        auto DoDelete = [&ObjectsToDelete, &PreDeleteReferencers]()
        {
            ObjectTools::DeleteObjects(ObjectsToDelete, /*bShowConfirmation=*/false);

            IAssetRegistry& AR = FAssetRegistryModule::GetRegistry();
            for (const FAssetIdentifier& Ref : PreDeleteReferencers)
            {
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
                const FAssetData AssetData = AR.GetAssetByObjectPath(FSoftObjectPath(Ref.ToString()));
#else
                const FAssetData AssetData = AR.GetAssetByObjectPath(FName(*Ref.ToString()));
#endif
                if (AssetData.IsValid() && AssetData.IsAssetLoaded())
                {
                    if (UBlueprint* BP = Cast<UBlueprint>(AssetData.GetAsset()))
                    {
                        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None);
                    }
                }
            }
        };

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
            Event.Get()->Wait();  // pure wait, NO Pump — pumping deadlocks
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
#endif // WITH_EDITOR
