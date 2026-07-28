# Private/Core — request queue, game-thread dispatch, registration shards

Core owns the request queue, the game-thread drain, and the per-action handler map. It is the routing spine between the WebSocket bridge (`../Transport/AGENTS.md`) and the domain implementations (`../Domains/AGENTS.md`). Hazardous editor ops go through `../Safety/AGENTS.md`.

## STRUCTURE

- `Subsystem/` (20): `UMcpAutomationBridgeSubsystem` definition shards. Declared in `../../Public/McpAutomationBridgeSubsystem.h` (`class UMcpAutomationBridgeSubsystem : public UEditorSubsystem`); split across `...Subsystem.cpp`, `...RequestQueue.cpp`, `...Lifecycle.cpp`, `...HandlerRegistration.cpp`, and per-domain registration shards.
- `Requests/` (3): `McpAutomationBridge_ProcessRequest.cpp` does per-request O(1) `AutomationHandlers.Find(Action)`; unmatched actions fall to `McpProcessRequestDispatch::DispatchFallbackAutomationRequest` (`...ProcessRequestDispatch.h/.cpp`).
- `Module/` (4): `McpAutomationBridgeModule.cpp` startup, `McpAutomationBridgeGlobals.{h,cpp}`, `McpAutomationBridgePCH.h`.
- `Settings/` (1): plugin Project Settings UObject.
- `Compatibility/` (1): version/engine compatibility shims.
- `Errors/` (1): error-code catalog.

## REQUEST LIFECYCLE

1. Socket receives a framed request, hands it to `QueueAutomationRequest()`.
2. `QueueAutomationRequest()` (`...RequestQueue.cpp`) locks `PendingAutomationRequestsMutex`, pushes `FPendingAutomationRequest` into `TArray<FPendingAutomationRequest> PendingAutomationRequests`. Rejects with `EAutomationQueueRejection::{NotAccepting, AlreadyCanceled, QueueFull}`; cap is `MaxPendingAutomationRequests`.
3. `Tick()` (`...Lifecycle.cpp`) calls `ProcessPendingAutomationRequests()`.
4. `ProcessPendingAutomationRequests()` returns early via `AsyncTask(ENamedThreads::GameThread, ...)` if `!IsInGameThread()`; otherwise drains a batch of 16/tick under lock.
5. Each request hits `ProcessRequest`, which looks up `AutomationHandlers.Find(Action)` and invokes the handler (receives `ReqId, Action, Payload, Socket`).
6. Handler writes the response back through `Socket` (`../Transport/AGENTS.md`).

## HANDLER REGISTRATION (shard pattern)

Handler map: `TMap<FString, FAutomationHandler> AutomationHandlers;` where
`FAutomationHandler = TFunction<bool(const FString& ReqId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)>`.
Registered via `RegisterHandler(Action, Handler)` (defined in `...CustomHandlerAliases.cpp`).

`InitializeHandlers()` (`...HandlerRegistration.cpp`) calls one `Register*Handlers()` per shard:
`RegisterCoreAndAssetHandlers` (`...CoreAndAssetRegistration.cpp`),
`RegisterEnvironmentMediaHandlers` (`...EnvironmentMediaRegistration.cpp`),
`RegisterSystemAndEditorHandlers` (`...SystemEditorRegistration.cpp`),
`RegisterAssetRoutingHandlers` (`...AssetRoutingRegistration.cpp`),
`RegisterBlueprintAndDomainHandlers` (`...BlueprintDomainRegistration.cpp`),
`RegisterAudioAnimationHandlers` (`...AudioAnimationRegistration.cpp`),
`RegisterWorldAndMiscHandlers` (`...WorldMiscRegistration.cpp`).
Alias map: `...CustomHandlerAliases.cpp` / `...CustomHandlerAliasConfig.cpp`. Support: `...ErrorCapture.cpp`, `...Responses.cpp`, `...ResponseSanitization.h`, `...Lifecycle.cpp`.

Real macro shape (reproduced from `...CoreAndAssetRegistration.cpp`):
```cpp
#define MCP_REGISTER_DIRECT(ActionName, MethodName) \
    RegisterHandler(TEXT(ActionName), [this](const FString& R, const FString& A, \
        const TSharedPtr<FJsonObject>& P, TSharedPtr<FMcpBridgeWebSocket> S) { \
        return MethodName(R, A, P, S); })
// ... MCP_REGISTER_DIRECT("execute_editor_function", HandleExecuteEditorFunction); ...
#undef MCP_REGISTER_DIRECT
```
Why sharded: `tests/unit/plugin/source_structure*.test.ts` enforce a 250 pure-line ceiling per file and ≤25 files per folder. Splitting registration keeps each shard under the line cap.

## ADD A NEW DOMAIN HANDLER

1. Implement the handler method in the matching `../Domains/<Domain>/` file (see `../Domains/AGENTS.md` for the dispatch-macro contract).
2. Declare the method on `UMcpAutomationBridgeSubsystem` in `../../Public/McpAutomationBridgeSubsystem.h`.
3. Pick or add a registration shard; add a `MCP_REGISTER_DIRECT("your_action", HandleYourAction)` line (or `RegisterHandler(...)` for a custom lambda).
4. If it is a new shard, add a `Register*Handlers()` declaration, define it, and call it from `InitializeHandlers()` in `...HandlerRegistration.cpp`.
5. Defer any editor work during package save, GC, async load, or unsafe map transitions (use the queue; never run editor API off the game thread).
6. Add a unit test beside `...RequestQueueTests.cpp`; cover acceptance, alias, and rejection (`EAutomationQueueRejection`).

## CONVENTIONS

- Every action is a string key in `AutomationHandlers`; lookup is O(1).
- `RegisterHandler` is callable at runtime (custom aliases map to existing actions).
- Cancellation lives in `...RequestQueueCancellation.cpp` via `AutomationRequestCancellationCallbacks`.
- Responses are sanitized in `...Responses.cpp` / `...ResponseSanitization.h` before send.

## ANTI-PATTERNS

- Do NOT call editor APIs off the game thread. If `!IsInGameThread()`, let `ProcessPendingAutomationRequests()` re-post.
- Do NOT bypass `QueueAutomationRequest()` and invoke handlers directly.
- Do NOT hand-author per-tool native `/mcp` registration here (see `../MCP/AGENTS.md`).
- Do NOT push handler logic into Core; Core routes, Domains implement.
- Do NOT exceed 250 pure lines per file or 25 files per folder; shard instead.
