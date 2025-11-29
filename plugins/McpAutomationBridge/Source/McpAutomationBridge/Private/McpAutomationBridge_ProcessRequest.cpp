#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Async/Async.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "HAL/PlatformTime.h"

void UMcpAutomationBridgeSubsystem::ProcessAutomationRequest(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // This large implementation was extracted from the original subsystem
    // translation unit to keep the core file smaller and focused. It
    // contains the main dispatcher that delegates to specialized handler
    // functions (property/blueprint/sequence/asset handlers) and retains
    // the queuing/scope-exit safety logic expected by callers.

    // Ensure automation processing happens on the game thread
    // This trace is intentionally verbose — routine requests can be high
    // frequency and will otherwise flood the logs. Developers can enable
    // Verbose logging to see these messages when required.
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT(">>> ProcessAutomationRequest ENTRY: RequestId=%s action='%s' (thread=%s)"), *RequestId, *Action, IsInGameThread() ? TEXT("GameThread") : TEXT("SocketThread"));
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ProcessAutomationRequest invoked (thread=%s) RequestId=%s action=%s activeSockets=%d pendingQueue=%d"),
        IsInGameThread() ? TEXT("GameThread") : TEXT("SocketThread"), *RequestId, *Action, ActiveSockets.Num(), PendingAutomationRequests.Num());
    if (!IsInGameThread())
    {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Scheduling ProcessAutomationRequest on GameThread: RequestId=%s action=%s"), *RequestId, *Action);
        AsyncTask(ENamedThreads::GameThread, [WeakThis = TWeakObjectPtr<UMcpAutomationBridgeSubsystem>(this), RequestId, Action, Payload, RequestingSocket]()
        {
            if (UMcpAutomationBridgeSubsystem* Pinned = WeakThis.Get())
            {
                Pinned->ProcessAutomationRequest(RequestId, Action, Payload, RequestingSocket);
            }
        });
        return;
    }

    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Starting ProcessAutomationRequest on GameThread: RequestId=%s action=%s bProcessingAutomationRequest=%s"), *RequestId, *Action, bProcessingAutomationRequest ? TEXT("true") : TEXT("false"));

    const FString LowerAction = Action.ToLower();

    if (!ActiveRequestTelemetry.Contains(RequestId))
    {
        FAutomationRequestTelemetry Entry;
        Entry.Action = LowerAction.IsEmpty() ? Action : LowerAction;
        Entry.StartTimeSeconds = FPlatformTime::Seconds();
        ActiveRequestTelemetry.Add(RequestId, Entry);
    }

    // Reentrancy guard / enqueue
    if (bProcessingAutomationRequest)
    {
        FPendingAutomationRequest P;
        P.RequestId = RequestId;
        P.Action = Action;
        P.Payload = Payload;
        P.RequestingSocket = RequestingSocket;
        {
            FScopeLock Lock(&PendingAutomationRequestsMutex);
            PendingAutomationRequests.Add(MoveTemp(P));
            bPendingRequestsScheduled = true;
        }
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("Enqueued automation request %s for action %s (processing in progress)."), *RequestId, *Action);
        return;
    }

    bProcessingAutomationRequest = true;
    bool bDispatchHandled = false;
    FString ConsumedHandlerLabel = TEXT("unknown-handler");
    const double DispatchStartSeconds = FPlatformTime::Seconds();

    auto HandleAndLog = [&](const TCHAR* HandlerLabel, auto&& Callable) -> bool
    {
        const bool bResult = Callable();
        if (bResult)
        {
            bDispatchHandled = true;
            ConsumedHandlerLabel = HandlerLabel;
        }
        return bResult;
    };

    {
        ON_SCOPE_EXIT
        {
            bProcessingAutomationRequest = false;
            const double DispatchEndSeconds = FPlatformTime::Seconds();
            const double DurationMs = (DispatchEndSeconds - DispatchStartSeconds) * 1000.0;
            if (bDispatchHandled)
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ProcessAutomationRequest: Completed handler='%s' RequestId=%s action='%s' (%.3f ms)"),
                    *ConsumedHandlerLabel, *RequestId, *Action, DurationMs);
            }
            else
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("ProcessAutomationRequest: No handler consumed RequestId=%s action='%s' (%.3f ms)"),
                    *RequestId, *Action, DurationMs);
            }

            if (bPendingRequestsScheduled)
            {
                bPendingRequestsScheduled = false;
                ProcessPendingAutomationRequests();
            }
        };

        try
        {
            // Map this requestId to the requesting socket so responses can be delivered reliably
            if (!RequestId.IsEmpty() && RequestingSocket.IsValid())
            {
                PendingRequestsToSockets.Add(RequestId, RequestingSocket);
            }

            // ---------------------------------------------------------
            // Check Handler Registry (O(1) dispatch)
            // ---------------------------------------------------------
            if (const FAutomationHandler* Handler = AutomationHandlers.Find(Action))
            {
                 if (HandleAndLog(*Action, [&](){ return (*Handler)(RequestId, Action, Payload, RequestingSocket); }))
                 {
                     return;
                 }
            }

            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ProcessAutomationRequest: Starting handler dispatch for action='%s'"), *Action);

            // Prioritize blueprint actions early only for blueprint-like actions to avoid noisy prefix logs
            {
                FString LowerNormalized = LowerAction;
                LowerNormalized.ReplaceInline(TEXT("-"), TEXT("_"));
                LowerNormalized.ReplaceInline(TEXT(" "), TEXT("_"));
                const bool bLooksBlueprint = (
                    LowerNormalized.StartsWith(TEXT("blueprint_")) ||
                    LowerNormalized.StartsWith(TEXT("manage_blueprint")) ||
                    LowerNormalized.Contains(TEXT("_scs")) ||
                    LowerNormalized.Contains(TEXT("scs_")) ||
                    LowerNormalized.Contains(TEXT("scs"))
                );
                if (bLooksBlueprint)
                {
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ProcessAutomationRequest: Checking HandleBlueprintAction (early)"));
                    if (HandleAndLog(TEXT("HandleBlueprintAction (early)"), [&]() { return HandleBlueprintAction(RequestId, Action, Payload, RequestingSocket); }))
                    {
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleBlueprintAction (early) consumed request"));
                        return;
                    }
                }
            }

            // Allow small handlers to short-circuit fast (property/function)
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ProcessAutomationRequest: About to call HandleExecuteEditorFunction"));
            if (HandleAndLog(TEXT("HandleExecuteEditorFunction"), [&]() { return HandleExecuteEditorFunction(RequestId, Action, Payload, RequestingSocket); }))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleExecuteEditorFunction consumed request"));
                return;
            }
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ProcessAutomationRequest: HandleExecuteEditorFunction returned false"));

            // Level utilities (top-level aliases)
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ProcessAutomationRequest: Checking HandleLevelAction"));
            if (HandleAndLog(TEXT("HandleLevelAction"), [&]() { return HandleLevelAction(RequestId, Action, Payload, RequestingSocket); }))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleLevelAction consumed request"));
                return;
            }

            // Try asset actions early (materials, import, list, rename, etc.)
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ProcessAutomationRequest: Checking HandleAssetAction (early)"));
            if (HandleAndLog(TEXT("HandleAssetAction (early)"), [&]() { return HandleAssetAction(RequestId, Action, Payload, RequestingSocket); }))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleAssetAction (early) consumed request"));
                return;
            }
            if (HandleAndLog(TEXT("HandleSetObjectProperty"), [&]() { return HandleSetObjectProperty(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleGetObjectProperty"), [&]() { return HandleGetObjectProperty(RequestId, Action, Payload, RequestingSocket); })) return;
            // Array manipulation operations
            if (HandleAndLog(TEXT("HandleArrayAppend"), [&]() { return HandleArrayAppend(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleArrayRemove"), [&]() { return HandleArrayRemove(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleArrayInsert"), [&]() { return HandleArrayInsert(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleArrayGetElement"), [&]() { return HandleArrayGetElement(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleArraySetElement"), [&]() { return HandleArraySetElement(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleArrayClear"), [&]() { return HandleArrayClear(RequestId, Action, Payload, RequestingSocket); })) return;
            // Map manipulation operations
            if (HandleAndLog(TEXT("HandleMapSetValue"), [&]() { return HandleMapSetValue(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleMapGetValue"), [&]() { return HandleMapGetValue(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleMapRemoveKey"), [&]() { return HandleMapRemoveKey(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleMapHasKey"), [&]() { return HandleMapHasKey(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleMapGetKeys"), [&]() { return HandleMapGetKeys(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleMapClear"), [&]() { return HandleMapClear(RequestId, Action, Payload, RequestingSocket); })) return;
            // Set manipulation operations
            if (HandleAndLog(TEXT("HandleSetAdd"), [&]() { return HandleSetAdd(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleSetRemove"), [&]() { return HandleSetRemove(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleSetContains"), [&]() { return HandleSetContains(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleSetClear"), [&]() { return HandleSetClear(RequestId, Action, Payload, RequestingSocket); })) return;
            // Asset dependency graph traversal
            if (HandleAndLog(TEXT("HandleGetAssetReferences"), [&]() { return HandleGetAssetReferences(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleGetAssetDependencies"), [&]() { return HandleGetAssetDependencies(RequestId, Action, Payload, RequestingSocket); })) return;
            // Asset workflow handlers
            if (HandleAndLog(TEXT("HandleFixupRedirectors"), [&]() { return HandleFixupRedirectors(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleSourceControlCheckout"), [&]() { return HandleSourceControlCheckout(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleSourceControlSubmit"), [&]() { return HandleSourceControlSubmit(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleBulkRenameAssets"), [&]() { return HandleBulkRenameAssets(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleBulkDeleteAssets"), [&]() { return HandleBulkDeleteAssets(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleGenerateThumbnail"), [&]() { return HandleGenerateThumbnail(RequestId, Action, Payload, RequestingSocket); })) return;
            // Landscape operations
            if (HandleAndLog(TEXT("HandleCreateLandscape"), [&]() { return HandleCreateLandscape(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleCreateProceduralTerrain"), [&]() { return HandleCreateProceduralTerrain(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleCreateLandscapeGrassType"), [&]() { return HandleCreateLandscapeGrassType(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleSculptLandscape"), [&]() { return HandleSculptLandscape(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleSetLandscapeMaterial"), [&]() { return HandleSetLandscapeMaterial(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleEditLandscape"), [&]() { return HandleEditLandscape(RequestId, Action, Payload, RequestingSocket); })) return;
            // Foliage operations
            if (HandleAndLog(TEXT("HandleAddFoliageType"), [&]() { return HandleAddFoliageType(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleCreateProceduralFoliage"), [&]() { return HandleCreateProceduralFoliage(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandlePaintFoliage"), [&]() { return HandlePaintFoliage(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleAddFoliageInstances"), [&]() { return HandleAddFoliageInstances(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleRemoveFoliage"), [&]() { return HandleRemoveFoliage(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleGetFoliageInstances"), [&]() { return HandleGetFoliageInstances(RequestId, Action, Payload, RequestingSocket); })) return;
            // Niagara operations
            if (HandleAndLog(TEXT("HandleCreateNiagaraSystem"), [&]() { return HandleCreateNiagaraSystem(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleCreateNiagaraEmitter"), [&]() { return HandleCreateNiagaraEmitter(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleSpawnNiagaraActor"), [&]() { return HandleSpawnNiagaraActor(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleModifyNiagaraParameter"), [&]() { return HandleModifyNiagaraParameter(RequestId, Action, Payload, RequestingSocket); })) return;
            // Animation blueprint operations
            if (HandleAndLog(TEXT("HandleCreateAnimBlueprint"), [&]() { return HandleCreateAnimBlueprint(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandlePlayAnimMontage"), [&]() { return HandlePlayAnimMontage(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleSetupRagdoll"), [&]() { return HandleSetupRagdoll(RequestId, Action, Payload, RequestingSocket); })) return;
            // Material graph operations
            if (HandleAndLog(TEXT("HandleAddMaterialTextureSample"), [&]() { return HandleAddMaterialTextureSample(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleAddMaterialExpression"), [&]() { return HandleAddMaterialExpression(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleCreateMaterialNodes"), [&]() { return HandleCreateMaterialNodes(RequestId, Action, Payload, RequestingSocket); })) return;
            // Sequencer operations
            if (HandleAndLog(TEXT("HandleAddSequencerKeyframe"), [&]() { return HandleAddSequencerKeyframe(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleManageSequencerTrack"), [&]() { return HandleManageSequencerTrack(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleAddCameraTrack"), [&]() { return HandleAddCameraTrack(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleAddAnimationTrack"), [&]() { return HandleAddAnimationTrack(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleAddTransformTrack"), [&]() { return HandleAddTransformTrack(RequestId, Action, Payload, RequestingSocket); })) return;

            // Delegate asset/control/blueprint/sequence actions to their handlers
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ProcessAutomationRequest: Checking HandleAssetAction"));
            if (HandleAndLog(TEXT("HandleAssetAction"), [&]() { return HandleAssetAction(RequestId, Action, Payload, RequestingSocket); }))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleAssetAction consumed request"));
                return;
            }
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ProcessAutomationRequest: Checking HandleControlActorAction"));
            if (HandleAndLog(TEXT("HandleControlActorAction"), [&]() { return HandleControlActorAction(RequestId, Action, Payload, RequestingSocket); }))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleControlActorAction consumed request"));
                return;
            }
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ProcessAutomationRequest: Checking HandleControlEditorAction"));
            if (HandleAndLog(TEXT("HandleControlEditorAction"), [&]() { return HandleControlEditorAction(RequestId, Action, Payload, RequestingSocket); }))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleControlEditorAction consumed request"));
                return;
            }
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ProcessAutomationRequest: Checking HandleUiAction"));
            if (HandleAndLog(TEXT("HandleUiAction"), [&]() { return HandleUiAction(RequestId, Action, Payload, RequestingSocket); }))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleUiAction consumed request"));
                return;
            }
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ProcessAutomationRequest: Checking HandleBlueprintAction (late)"));
            if (HandleAndLog(TEXT("HandleBlueprintAction (late)"), [&]() { return HandleBlueprintAction(RequestId, Action, Payload, RequestingSocket); }))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleBlueprintAction (late) consumed request"));
                return;
            }
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ProcessAutomationRequest: Checking HandleSequenceAction"));
            if (HandleAndLog(TEXT("HandleSequenceAction"), [&]() { return HandleSequenceAction(RequestId, Action, Payload, RequestingSocket); }))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleSequenceAction consumed request"));
                return;
            }
            if (HandleAndLog(TEXT("HandleEffectAction"), [&]() { return HandleEffectAction(RequestId, Action, Payload, RequestingSocket); })) return;
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("ProcessAutomationRequest: Checking HandleAnimationPhysicsAction"));
            if (HandleAndLog(TEXT("HandleAnimationPhysicsAction"), [&]() { return HandleAnimationPhysicsAction(RequestId, Action, Payload, RequestingSocket); }))
            {
                UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose, TEXT("HandleAnimationPhysicsAction consumed request"));
                return;
            }
            if (HandleAndLog(TEXT("HandleAudioAction"), [&]() { return HandleAudioAction(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleLightingAction"), [&]() { return HandleLightingAction(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandlePerformanceAction"), [&]() { return HandlePerformanceAction(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleBuildEnvironmentAction"), [&]() { return HandleBuildEnvironmentAction(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleControlEnvironmentAction"), [&]() { return HandleControlEnvironmentAction(RequestId, Action, Payload, RequestingSocket); })) return;

            // Additional consolidated tool handlers
            if (HandleAndLog(TEXT("HandleSystemControlAction"), [&]() { return HandleSystemControlAction(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleConsoleCommandAction"), [&]() { return HandleConsoleCommandAction(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleInspectAction"), [&]() { return HandleInspectAction(RequestId, Action, Payload, RequestingSocket); })) return;

            // 1. Editor Authoring & Graph Editing
            if (HandleAndLog(TEXT("HandleBlueprintGraphAction"), [&]() { return HandleBlueprintGraphAction(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleNiagaraGraphAction"), [&]() { return HandleNiagaraGraphAction(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleMaterialGraphAction"), [&]() { return HandleMaterialGraphAction(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleBehaviorTreeAction"), [&]() { return HandleBehaviorTreeAction(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleWorldPartitionAction"), [&]() { return HandleWorldPartitionAction(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleRenderAction"), [&]() { return HandleRenderAction(RequestId, Action, Payload, RequestingSocket); })) return;

            // 2. Execution & Build / Test Pipeline
            if (HandleAndLog(TEXT("HandlePipelineAction"), [&]() { return HandlePipelineAction(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleTestAction"), [&]() { return HandleTestAction(RequestId, Action, Payload, RequestingSocket); })) return;

            // 3. Observability, Logs, Debugging & History
            if (HandleAndLog(TEXT("HandleLogAction"), [&]() { return HandleLogAction(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleDebugAction"), [&]() { return HandleDebugAction(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleAssetQueryAction"), [&]() { return HandleAssetQueryAction(RequestId, Action, Payload, RequestingSocket); })) return;
            if (HandleAndLog(TEXT("HandleInsightsAction"), [&]() { return HandleInsightsAction(RequestId, Action, Payload, RequestingSocket); })) return;

            // Unhandled action
            bDispatchHandled = true;
            ConsumedHandlerLabel = TEXT("SendAutomationError (unknown action)");
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown automation action: %s"), *Action), TEXT("UNKNOWN_ACTION"));
        }
        catch (const std::exception& E)
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Unhandled exception processing automation request %s: %s"), *RequestId, ANSI_TO_TCHAR(E.what()));
            bDispatchHandled = true;
            ConsumedHandlerLabel = TEXT("Exception handler");
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Internal error: %s"), ANSI_TO_TCHAR(E.what())), TEXT("INTERNAL_ERROR"));
        }
        catch (...)
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Unhandled unknown exception processing automation request %s"), *RequestId);
            bDispatchHandled = true;
            ConsumedHandlerLabel = TEXT("Exception handler (unknown)");
            SendAutomationError(RequestingSocket, RequestId, TEXT("Internal error (unknown)."), TEXT("INTERNAL_ERROR"));
        }

    }
}

// ProcessPendingAutomationRequests() intentionally implemented in the
// primary subsystem translation unit (McpAutomationBridgeSubsystem.cpp)
// to ensure the linker emits the symbol into the module's object file.
