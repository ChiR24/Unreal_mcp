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
    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT(">>> ProcessAutomationRequest ENTRY: RequestId=%s action='%s' (thread=%s)"), *RequestId, *Action, IsInGameThread() ? TEXT("GameThread") : TEXT("SocketThread"));
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
    {
        ON_SCOPE_EXIT
        {
            bProcessingAutomationRequest = false;
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

            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("ProcessAutomationRequest: Starting handler dispatch for action='%s'"), *Action);

            // Prioritize blueprint actions early to avoid accidental matches in other handlers
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("ProcessAutomationRequest: Checking HandleBlueprintAction (early)"));
            if (HandleBlueprintAction(RequestId, Action, Payload, RequestingSocket)) { UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction (early) consumed request")); return; }

            // Allow small handlers to short-circuit fast (property/function)
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("ProcessAutomationRequest: About to call HandleExecuteEditorFunction"));
            if (HandleExecuteEditorFunction(RequestId, Action, Payload, RequestingSocket)) { UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleExecuteEditorFunction consumed request")); return; }
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("ProcessAutomationRequest: HandleExecuteEditorFunction returned false"));
            if (HandleSetObjectProperty(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleGetObjectProperty(RequestId, Action, Payload, RequestingSocket)) return;
            // Array manipulation operations
            if (HandleArrayAppend(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleArrayRemove(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleArrayInsert(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleArrayGetElement(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleArraySetElement(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleArrayClear(RequestId, Action, Payload, RequestingSocket)) return;
            // Map manipulation operations
            if (HandleMapSetValue(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleMapGetValue(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleMapRemoveKey(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleMapHasKey(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleMapGetKeys(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleMapClear(RequestId, Action, Payload, RequestingSocket)) return;
            // Set manipulation operations
            if (HandleSetAdd(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleSetRemove(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleSetContains(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleSetClear(RequestId, Action, Payload, RequestingSocket)) return;
            // Asset dependency graph traversal
            if (HandleGetAssetReferences(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleGetAssetDependencies(RequestId, Action, Payload, RequestingSocket)) return;
            // Asset workflow handlers
            if (HandleFixupRedirectors(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleSourceControlCheckout(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleSourceControlSubmit(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleBulkRenameAssets(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleBulkDeleteAssets(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleGenerateThumbnail(RequestId, Action, Payload, RequestingSocket)) return;
            // Landscape operations
            if (HandleCreateLandscape(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleEditLandscape(RequestId, Action, Payload, RequestingSocket)) return;
            // Foliage operations
            if (HandleAddFoliageType(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandlePaintFoliage(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleRemoveFoliage(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleGetFoliageInstances(RequestId, Action, Payload, RequestingSocket)) return;
            // Niagara operations
            if (HandleCreateNiagaraSystem(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleCreateNiagaraEmitter(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleSpawnNiagaraActor(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleModifyNiagaraParameter(RequestId, Action, Payload, RequestingSocket)) return;
            // Animation blueprint operations
            if (HandleCreateAnimBlueprint(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandlePlayAnimMontage(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleSetupRagdoll(RequestId, Action, Payload, RequestingSocket)) return;
            // Material graph operations
            if (HandleAddMaterialTextureSample(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleAddMaterialExpression(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleCreateMaterialNodes(RequestId, Action, Payload, RequestingSocket)) return;
            // Sequencer operations
            if (HandleAddSequencerKeyframe(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleManageSequencerTrack(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleAddCameraTrack(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleAddAnimationTrack(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleAddTransformTrack(RequestId, Action, Payload, RequestingSocket)) return;

            // Delegate asset/control/blueprint/sequence actions to their handlers
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("ProcessAutomationRequest: Checking HandleAssetAction"));
            if (HandleAssetAction(RequestId, Action, Payload, RequestingSocket)) { UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleAssetAction consumed request")); return; }
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("ProcessAutomationRequest: Checking HandleControlActorAction"));
            if (HandleControlActorAction(RequestId, Action, Payload, RequestingSocket)) { UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleControlActorAction consumed request")); return; }
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("ProcessAutomationRequest: Checking HandleControlEditorAction"));
            if (HandleControlEditorAction(RequestId, Action, Payload, RequestingSocket)) { UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleControlEditorAction consumed request")); return; }
            UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("ProcessAutomationRequest: Checking HandleBlueprintAction (late)"));
            if (HandleBlueprintAction(RequestId, Action, Payload, RequestingSocket)) { UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("HandleBlueprintAction (late) consumed request")); return; }
            if (HandleSequenceAction(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleEffectAction(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleAnimationPhysicsAction(RequestId, Action, Payload, RequestingSocket)) return;
            if (HandleBuildEnvironmentAction(RequestId, Action, Payload, RequestingSocket)) return;

            // Unhandled action
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Unknown automation action: %s"), *Action), TEXT("UNKNOWN_ACTION"));
        }
        catch (const std::exception& E)
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Unhandled exception processing automation request %s: %s"), *RequestId, ANSI_TO_TCHAR(E.what()));
            SendAutomationError(RequestingSocket, RequestId, FString::Printf(TEXT("Internal error: %s"), ANSI_TO_TCHAR(E.what())), TEXT("INTERNAL_ERROR"));
        }
        catch (...)
        {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Error, TEXT("Unhandled unknown exception processing automation request %s"), *RequestId);
            SendAutomationError(RequestingSocket, RequestId, TEXT("Internal error (unknown)."), TEXT("INTERNAL_ERROR"));
        }

    }
}

// ProcessPendingAutomationRequests() intentionally implemented in the
// primary subsystem translation unit (McpAutomationBridgeSubsystem.cpp)
// to ensure the linker emits the symbol into the module's object file.
