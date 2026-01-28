#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "McpAutomationBridgeGlobals.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeSubsystem.h"
#include "McpConnectionManager.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"

void UMcpAutomationBridgeSubsystem::ProcessAutomationRequest(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket) {
  // This large implementation was extracted from the original subsystem
  // translation unit to keep the core file smaller and focused. It
  // contains the main dispatcher that delegates to specialized handler
  // functions (property/blueprint/sequence/asset handlers) and retains
  // the queuing/scope-exit safety logic expected by callers.

  // Ensure automation processing happens on the game thread
  // This trace is intentionally verbose — routine requests can be high
  // frequency and will otherwise flood the logs. Developers can enable
  // Verbose logging to see these messages when required.
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT(">>> ProcessAutomationRequest ENTRY: RequestId=%s action='%s' "
              "(thread=%s)"),
         *RequestId, *Action,
         IsInGameThread() ? TEXT("GameThread") : TEXT("SocketThread"));
  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("ProcessAutomationRequest invoked (thread=%s) RequestId=%s "
              "action=%s activeSockets=%d pendingQueue=%d"),
         IsInGameThread() ? TEXT("GameThread") : TEXT("SocketThread"),
         *RequestId, *Action,
         ConnectionManager.IsValid() ? ConnectionManager->GetActiveSocketCount()
                                     : 0,
         PendingAutomationRequests.Num());
  if (!IsInGameThread()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Scheduling ProcessAutomationRequest on GameThread: "
                "RequestId=%s action=%s"),
           *RequestId, *Action);
    AsyncTask(ENamedThreads::GameThread,
              [WeakThis = TWeakObjectPtr<UMcpAutomationBridgeSubsystem>(this),
               RequestId, Action, Payload, RequestingSocket]() {
                if (UMcpAutomationBridgeSubsystem *Pinned = WeakThis.Get()) {
                  Pinned->ProcessAutomationRequest(RequestId, Action, Payload,
                                                   RequestingSocket);
                }
              });
    return;
  }

  // Guard against unsafe engine states (Saving, GC, Async Loading)
  // Calling StaticFindObject (via ResolveClassByName) during these states can
  // cause crashes.
  if (GIsSavingPackage || IsGarbageCollecting() || IsAsyncLoading()) {
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Deferring ProcessAutomationRequest due to active "
                "Serialization/GC/Loading: RequestId=%s Action=%s"),
           *RequestId, *Action);

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
    return;
  }

  UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
         TEXT("Starting ProcessAutomationRequest on GameThread: RequestId=%s "
              "action=%s bProcessingAutomationRequest=%s"),
         *RequestId, *Action,
         bProcessingAutomationRequest ? TEXT("true") : TEXT("false"));

  const FString LowerAction = Action.ToLower();

  if (ConnectionManager.IsValid()) {
    ConnectionManager->StartRequestTelemetry(RequestId, Action);
  }

  // Reentrancy guard / enqueue
  if (bProcessingAutomationRequest) {
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
    UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
           TEXT("Enqueued automation request %s for action %s (processing in "
                "progress)."),
           *RequestId, *Action);
    return;
  }

  bProcessingAutomationRequest = true;
  bool bDispatchHandled = false;
  FString ConsumedHandlerLabel = TEXT("unknown-handler");
  const double DispatchStartSeconds = FPlatformTime::Seconds();

  auto HandleAndLog = [&](const TCHAR *HandlerLabel, auto &&Callable) -> bool {
    const bool bResult = Callable();
    if (bResult) {
      bDispatchHandled = true;
      ConsumedHandlerLabel = HandlerLabel;
    }
    return bResult;
  };

  {
    ON_SCOPE_EXIT {
      bProcessingAutomationRequest = false;
      const double DispatchEndSeconds = FPlatformTime::Seconds();
      const double DurationMs =
          (DispatchEndSeconds - DispatchStartSeconds) * 1000.0;
      if (bDispatchHandled) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("ProcessAutomationRequest: Completed handler='%s' "
                    "RequestId=%s action='%s' (%.3f ms)"),
               *ConsumedHandlerLabel, *RequestId, *Action, DurationMs);
      } else {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Warning,
               TEXT("ProcessAutomationRequest: No handler consumed "
                    "RequestId=%s action='%s' (%.3f ms)"),
               *RequestId, *Action, DurationMs);
      }

      if (bPendingRequestsScheduled) {
        bPendingRequestsScheduled = false;
        ProcessPendingAutomationRequests();
      }
    };

    try {
      // Map this requestId to the requesting socket so responses can be
      // delivered reliably
      if (!RequestId.IsEmpty() && RequestingSocket.IsValid() &&
          ConnectionManager.IsValid()) {
        ConnectionManager->RegisterRequestSocket(RequestId, RequestingSocket);
      }

      // ---------------------------------------------------------
      // Check Handler Registry (O(1) dispatch)
      // ---------------------------------------------------------
      if (const FAutomationHandler *Handler = AutomationHandlers.Find(Action)) {
        if (HandleAndLog(*Action, [&]() {
              return (*Handler)(RequestId, Action, Payload, RequestingSocket);
            })) {
          return;
        }
      }

      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("ProcessAutomationRequest: Starting handler dispatch for "
                  "action='%s'"),
             *Action);

      // Prioritize blueprint actions early only for blueprint-like actions to
      // avoid noisy prefix logs
      {
        FString LowerNormalized = LowerAction;
        LowerNormalized.ReplaceInline(TEXT("-"), TEXT("_"));
        LowerNormalized.ReplaceInline(TEXT(" "), TEXT("_"));
        const bool bLooksBlueprint =
            (LowerNormalized.StartsWith(TEXT("blueprint_")) ||
             LowerNormalized.StartsWith(TEXT("manage_blueprint")) ||
             LowerNormalized.Contains(TEXT("_scs")) ||
             LowerNormalized.Contains(TEXT("scs_")) ||
             LowerNormalized.Contains(TEXT("scs")));
        if (bLooksBlueprint) {
          UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
                 TEXT("ProcessAutomationRequest: Checking "
                      "HandleBlueprintAction (early)"));
          if (HandleAndLog(TEXT("HandleBlueprintAction (early)"), [&]() {
                return HandleBlueprintAction(RequestId, Action, Payload,
                                             RequestingSocket);
              })) {
            UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
                   TEXT("HandleBlueprintAction (early) consumed request"));
            return;
          }
        }
      }

      // Allow small handlers to short-circuit fast (property/function)
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("ProcessAutomationRequest: About to call "
                  "HandleExecuteEditorFunction"));
      if (HandleAndLog(TEXT("HandleExecuteEditorFunction"), [&]() {
            return HandleExecuteEditorFunction(RequestId, Action, Payload,
                                               RequestingSocket);
          })) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("HandleExecuteEditorFunction consumed request"));
        return;
      }
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("ProcessAutomationRequest: HandleExecuteEditorFunction "
                  "returned false"));

      // Level utilities (top-level aliases)
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("ProcessAutomationRequest: Checking HandleLevelAction"));
      if (HandleAndLog(TEXT("HandleLevelAction"), [&]() {
            return HandleLevelAction(RequestId, Action, Payload,
                                     RequestingSocket);
          })) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("HandleLevelAction consumed request"));
        return;
      }

      // Try asset actions early (materials, import, list, rename, etc.)
      UE_LOG(
          LogMcpAutomationBridgeSubsystem, Verbose,
          TEXT("ProcessAutomationRequest: Checking HandleAssetAction (early)"));
      if (HandleAndLog(TEXT("HandleAssetAction (early)"), [&]() {
            return HandleAssetAction(RequestId, Action, Payload,
                                     RequestingSocket);
          })) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("HandleAssetAction (early) consumed request"));
        return;
      }
      if (HandleAndLog(TEXT("HandleSetObjectProperty"), [&]() {
            return HandleSetObjectProperty(RequestId, Action, Payload,
                                           RequestingSocket);
          }))
        return;
      if (HandleAndLog(TEXT("HandleGetObjectProperty"), [&]() {
            return HandleGetObjectProperty(RequestId, Action, Payload,
                                           RequestingSocket);
          }))
        return;

      // Specialized actions (Array, Map, Set, Landscape, Foliage, Niagara,
      // Animation, Sequencer, etc.) are now handled by the O(1)
      // AutomationHandlers registry check at the top of this function. The
      // linear checks have been removed for performance.

      // Delegate asset/control/blueprint/sequence actions to their handlers
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("ProcessAutomationRequest: Checking HandleAssetAction"));
      if (HandleAndLog(TEXT("HandleAssetAction"), [&]() {
            return HandleAssetAction(RequestId, Action, Payload,
                                     RequestingSocket);
          })) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("HandleAssetAction consumed request"));
        return;
      }
      UE_LOG(
          LogMcpAutomationBridgeSubsystem, Verbose,
          TEXT("ProcessAutomationRequest: Checking HandleControlActorAction"));
      if (HandleAndLog(TEXT("HandleControlActorAction"), [&]() {
            return HandleControlActorAction(RequestId, Action, Payload,
                                            RequestingSocket);
          })) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("HandleControlActorAction consumed request"));
        return;
      }
      UE_LOG(
          LogMcpAutomationBridgeSubsystem, Verbose,
          TEXT("ProcessAutomationRequest: Checking HandleControlEditorAction"));
      if (HandleAndLog(TEXT("HandleControlEditorAction"), [&]() {
            return HandleControlEditorAction(RequestId, Action, Payload,
                                             RequestingSocket);
          })) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("HandleControlEditorAction consumed request"));
        return;
      }
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("ProcessAutomationRequest: Checking HandleUiAction"));
      if (HandleAndLog(TEXT("HandleUiAction"), [&]() {
            return HandleUiAction(RequestId, Action, Payload, RequestingSocket);
          })) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("HandleUiAction consumed request"));
        return;
      }
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("ProcessAutomationRequest: Checking HandleBlueprintAction "
                  "(late)"));
      if (HandleAndLog(TEXT("HandleBlueprintAction (late)"), [&]() {
            return HandleBlueprintAction(RequestId, Action, Payload,
                                         RequestingSocket);
          })) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("HandleBlueprintAction (late) consumed request"));
        return;
      }
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("ProcessAutomationRequest: Checking HandleSequenceAction"));
      if (HandleAndLog(TEXT("HandleSequenceAction"), [&]() {
            return HandleSequenceAction(RequestId, Action, Payload,
                                        RequestingSocket);
          })) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("HandleSequenceAction consumed request"));
        return;
      }
      if (HandleAndLog(TEXT("HandleEffectAction"), [&]() {
            return HandleEffectAction(RequestId, Action, Payload,
                                      RequestingSocket);
          }))
        return;
      UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
             TEXT("ProcessAutomationRequest: Checking "
                  "HandleAnimationPhysicsAction"));
      if (HandleAndLog(TEXT("HandleAnimationPhysicsAction"), [&]() {
            return HandleAnimationPhysicsAction(RequestId, Action, Payload,
                                                RequestingSocket);
          })) {
        UE_LOG(LogMcpAutomationBridgeSubsystem, Verbose,
               TEXT("HandleAnimationPhysicsAction consumed request"));
        return;
      }
      if (HandleAndLog(TEXT("HandleAudioAction"), [&]() {
            return HandleAudioAction(RequestId, Action, Payload,
                                     RequestingSocket);
          }))
        return;
      if (HandleAndLog(TEXT("HandleLightingAction"), [&]() {
            return HandleLightingAction(RequestId, Action, Payload,
                                        RequestingSocket);
          }))
        return;
      if (HandleAndLog(TEXT("HandlePerformanceAction"), [&]() {
            return HandlePerformanceAction(RequestId, Action, Payload,
                                           RequestingSocket);
          }))
        return;
      if (HandleAndLog(TEXT("HandleBuildEnvironmentAction"), [&]() {
            return HandleBuildEnvironmentAction(RequestId, Action, Payload,
                                                RequestingSocket);
          }))
        return;
      if (HandleAndLog(TEXT("HandleControlEnvironmentAction"), [&]() {
            return HandleControlEnvironmentAction(RequestId, Action, Payload,
                                                  RequestingSocket);
          }))
        return;

      // Additional consolidated tool handlers
      if (HandleAndLog(TEXT("HandleSystemControlAction"), [&]() {
            return HandleSystemControlAction(RequestId, Action, Payload,
                                             RequestingSocket);
          }))
        return;
      if (HandleAndLog(TEXT("HandleConsoleCommandAction"), [&]() {
            return HandleConsoleCommandAction(RequestId, Action, Payload,
                                              RequestingSocket);
          }))
        return;
      if (HandleAndLog(TEXT("HandleInspectAction"), [&]() {
            return HandleInspectAction(RequestId, Action, Payload,
                                       RequestingSocket);
          }))
        return;

      // 1. Editor Authoring & Graph Editing
      if (HandleAndLog(TEXT("HandleBlueprintGraphAction"), [&]() {
            return HandleBlueprintGraphAction(RequestId, Action, Payload,
                                              RequestingSocket);
          }))
        return;
      if (HandleAndLog(TEXT("HandleNiagaraGraphAction"), [&]() {
            return HandleNiagaraGraphAction(RequestId, Action, Payload,
                                            RequestingSocket);
          }))
        return;
      if (HandleAndLog(TEXT("HandleMaterialGraphAction"), [&]() {
            return HandleMaterialGraphAction(RequestId, Action, Payload,
                                             RequestingSocket);
          }))
        return;
      if (HandleAndLog(TEXT("HandleBehaviorTreeAction"), [&]() {
            return HandleBehaviorTreeAction(RequestId, Action, Payload,
                                            RequestingSocket);
          }))
        return;
      if (HandleAndLog(TEXT("HandleWorldPartitionAction"), [&]() {
            return HandleWorldPartitionAction(RequestId, Action, Payload,
                                              RequestingSocket);
          }))
        return;
      if (HandleAndLog(TEXT("HandleRenderAction"), [&]() {
            return HandleRenderAction(RequestId, Action, Payload,
                                      RequestingSocket);
          }))
        return;

      // Phase 6: Geometry Script
      if (HandleAndLog(TEXT("HandleGeometryAction"), [&]() {
            return HandleGeometryAction(RequestId, Action, Payload,
                                        RequestingSocket);
          }))
        return;

      // Phase 7: Skeleton & Rigging
      if (HandleAndLog(TEXT("HandleManageSkeleton"), [&]() {
            return HandleManageSkeleton(RequestId, Action, Payload, RequestingSocket);
          }))
        return;

      // Phase 8: Material Authoring
      if (HandleAndLog(TEXT("HandleManageMaterialAuthoringAction"), [&]() {
            return HandleManageMaterialAuthoringAction(RequestId, Action, Payload,
                                                        RequestingSocket);
          }))
        return;

      // Phase 9: Texture Management
      if (HandleAndLog(TEXT("HandleManageTextureAction"), [&]() {
            return HandleManageTextureAction(RequestId, Action, Payload,
                                             RequestingSocket);
          }))
        return;

      // Phase 10: Animation Authoring
      if (HandleAndLog(TEXT("HandleManageAnimationAuthoringAction"), [&]() {
            return HandleManageAnimationAuthoringAction(RequestId, Action, Payload,
                                                         RequestingSocket);
          }))
        return;

      // Phase 11: Audio Authoring
      if (HandleAndLog(TEXT("HandleManageAudioAuthoringAction"), [&]() {
            return HandleManageAudioAuthoringAction(RequestId, Action, Payload,
                                                    RequestingSocket);
          }))
        return;

      // Phase 12: Niagara Authoring
      if (HandleAndLog(TEXT("HandleManageNiagaraAuthoringAction"), [&]() {
            return HandleManageNiagaraAuthoringAction(RequestId, Action, Payload,
                                                       RequestingSocket);
          }))
        return;

      // Phase 13: GAS (Gameplay Ability System)
      if (HandleAndLog(TEXT("HandleManageGASAction"), [&]() {
            return HandleManageGASAction(RequestId, Action, Payload,
                                         RequestingSocket);
          }))
        return;

      // Phase 14: Character & Movement System
      if (HandleAndLog(TEXT("HandleManageCharacterAction"), [&]() {
            return HandleManageCharacterAction(RequestId, Action, Payload,
                                               RequestingSocket);
          }))
        return;

      // Phase 15: Combat & Weapons System
      if (HandleAndLog(TEXT("HandleManageCombatAction"), [&]() {
            return HandleManageCombatAction(RequestId, Action, Payload,
                                            RequestingSocket);
          }))
        return;

      // Phase 16: AI System
      if (HandleAndLog(TEXT("HandleManageAIAction"), [&]() {
            return HandleManageAIAction(RequestId, Action, Payload,
                                        RequestingSocket);
          }))
        return;

      // Phase 17: Inventory & Items System
      if (HandleAndLog(TEXT("HandleManageInventoryAction"), [&]() {
            return HandleManageInventoryAction(RequestId, Action, Payload,
                                               RequestingSocket);
          }))
        return;

      // Phase 18: Interaction System
      if (HandleAndLog(TEXT("HandleManageInteractionAction"), [&]() {
            return HandleManageInteractionAction(RequestId, Action, Payload,
                                                 RequestingSocket);
          }))
        return;

      // Phase 19: Widget Authoring System
      if (HandleAndLog(TEXT("HandleManageWidgetAuthoringAction"), [&]() {
            return HandleManageWidgetAuthoringAction(RequestId, Action, Payload,
                                                     RequestingSocket);
          }))
        return;

      // Phase 20: Networking & Multiplayer
      if (HandleAndLog(TEXT("HandleManageNetworkingAction"), [&]() {
            return HandleManageNetworkingAction(RequestId, Action, Payload,
                                                RequestingSocket);
          }))
        return;

      // Phase 21: Game Framework
      if (HandleAndLog(TEXT("HandleManageGameFrameworkAction"), [&]() {
            return HandleManageGameFrameworkAction(RequestId, Action, Payload,
                                                   RequestingSocket);
          }))
        return;

      // Phase 22: Sessions & Local Multiplayer
      if (HandleAndLog(TEXT("HandleManageSessionsAction"), [&]() {
            return HandleManageSessionsAction(RequestId, Action, Payload,
                                              RequestingSocket);
          }))
        return;

      // Phase 23: Level Structure
      if (HandleAndLog(TEXT("HandleManageLevelStructureAction"), [&]() {
            return HandleManageLevelStructureAction(RequestId, Action, Payload,
                                                    RequestingSocket);
          }))
        return;

      // Phase 24: Volumes & Zones
      if (HandleAndLog(TEXT("HandleManageVolumesAction"), [&]() {
            return HandleManageVolumesAction(RequestId, Action, Payload,
                                             RequestingSocket);
          }))
        return;

      // Phase 25: Navigation System
      if (HandleAndLog(TEXT("HandleManageNavigationAction"), [&]() {
            return HandleManageNavigationAction(RequestId, Action, Payload,
                                                RequestingSocket);
          }))
        return;

      // Phase 26: Spline System
      if (HandleAndLog(TEXT("HandleManageSplinesAction"), [&]() {
            return HandleManageSplinesAction(RequestId, Action, Payload,
                                             RequestingSocket);
          }))
        return;

      // 2. Execution & Build / Test Pipeline
      if (HandleAndLog(TEXT("HandlePipelineAction"), [&]() {
            return HandlePipelineAction(RequestId, Action, Payload,
                                        RequestingSocket);
          }))
        return;
      if (HandleAndLog(TEXT("HandleTestAction"), [&]() {
            return HandleTestAction(RequestId, Action, Payload,
                                    RequestingSocket);
          }))
        return;

      // 3. Observability, Logs, Debugging & History
      if (HandleAndLog(TEXT("HandleLogAction"), [&]() {
            return HandleLogAction(RequestId, Action, Payload,
                                   RequestingSocket);
          }))
        return;
      if (HandleAndLog(TEXT("HandleDebugAction"), [&]() {
            return HandleDebugAction(RequestId, Action, Payload,
                                     RequestingSocket);
          }))
        return;
      if (HandleAndLog(TEXT("HandleAssetQueryAction"), [&]() {
            return HandleAssetQueryAction(RequestId, Action, Payload,
                                          RequestingSocket);
          }))
        return;
      if (HandleAndLog(TEXT("HandleInsightsAction"), [&]() {
            return HandleInsightsAction(RequestId, Action, Payload,
                                        RequestingSocket);
          }))
        return;

      // Unhandled action
      bDispatchHandled = true;
      ConsumedHandlerLabel = TEXT("SendAutomationError (unknown action)");
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Unknown automation action: %s"), *Action),
          TEXT("UNKNOWN_ACTION"));
    } catch (const std::exception &E) {
      UE_LOG(LogMcpAutomationBridgeSubsystem, Error,
             TEXT("Unhandled exception processing automation request %s: %s"),
             *RequestId, ANSI_TO_TCHAR(E.what()));
      bDispatchHandled = true;
      ConsumedHandlerLabel = TEXT("Exception handler");
      SendAutomationError(
          RequestingSocket, RequestId,
          FString::Printf(TEXT("Internal error: %s"), ANSI_TO_TCHAR(E.what())),
          TEXT("INTERNAL_ERROR"));
    } catch (...) {
      UE_LOG(
          LogMcpAutomationBridgeSubsystem, Error,
          TEXT("Unhandled unknown exception processing automation request %s"),
          *RequestId);
      bDispatchHandled = true;
      ConsumedHandlerLabel = TEXT("Exception handler (unknown)");
      SendAutomationError(RequestingSocket, RequestId,
                          TEXT("Internal error (unknown)."),
                          TEXT("INTERNAL_ERROR"));
    }
  }
}

// ProcessPendingAutomationRequests() intentionally implemented in the
// primary subsystem translation unit (McpAutomationBridgeSubsystem.cpp)
// to ensure the linker emits the symbol into the module's object file.
