# Private/Core — request queue, game-thread dispatch, handler registration

Core owns the request queue, the game-thread drain and the per-tool handler map: the routing spine between the transports (`../Transport/`, `../MCP/`) and the domain implementations (`../Domains/`). Hazardous editor ops go through `../Safety/`.

## STRUCTURE
- `Subsystem/` (11): `UMcpAutomationBridgeSubsystem` (declared in `../../Public/McpAutomationBridgeSubsystem.h`) split into `...Subsystem.cpp`, `...RequestQueue.cpp`, `...RequestQueueCancellation.cpp`, `...Lifecycle.cpp`, `...HandlerRegistration.cpp`, `...Responses.cpp` + sanitization/enrichment headers, `...ErrorCapture.cpp`, `...EditorCommands.cpp`, `...RequestQueueTests.cpp`.
- `Requests/` (5): `McpAutomationBridge_ProcessRequest.cpp` (`AutomationHandlers.Find(Action)`, else `UNKNOWN_ACTION`), `McpRequestOriginRegistry` (request id → originating transport), `McpResponseCaptureRegistry`.
- `Security/` (3): `McpPrequeueGate` + `McpPrequeueDemand` — scope/consent authorization before anything is enqueued, on both transports.
- `Module/` (4), `Settings/` (1), `Compatibility/` (1), `Errors/` (1).

## REQUEST LIFECYCLE
1. A transport authorizes the request through the pre-queue gate, then calls `QueueAutomationRequest()`.
2. `QueueAutomationRequest()` locks `PendingAutomationRequestsMutex` and appends; it rejects with `EAutomationQueueRejection::{NotAccepting, AlreadyCanceled, QueueFull}` (cap `MaxPendingAutomationRequests`).
3. `Tick()` → `ProcessPendingAutomationRequests()` drains 16 per tick on the game thread (re-posts itself when called off it).
4. `ProcessRequest` looks up the tool name in `AutomationHandlers` and invokes the handler; the handler replies through its socket.

## HANDLER REGISTRATION
The wire contract is the 23 parent tools plus `console_command`. `InitializeHandlers()` in `...HandlerRegistration.cpp` is one table:
- direct parents map to one handler method;
- `Route(Parent, Fallback, { FSubRoute... })` sends the sub-actions a sibling domain claims (`IsLightingAction`, `IsWidgetAuthoringAction`, ...) to that domain under its own action name, everything else to the fallback;
- `manage_asset` routes texture and material-authoring sub-actions to their domains.

`RegisterHandler()` is defined in the same file and refuses an invalid identifier or a duplicate. Bare action names are not registered.

## ADD A HANDLER
1. Implement it in the matching `../Domains/<Domain>/` file and declare it on the subsystem.
2. Reach it from its parent's handler, or add an `FSubRoute` when it is a sibling domain.
3. Add the capability record on the TS side (`src/tools/catalog/capabilities/records/`) and regenerate.
4. Defer editor work during package save, GC, async load or unsafe map transitions: always go through the queue.

## ANTI-PATTERNS
- Editor APIs off the game thread, or invoking a handler around `QueueAutomationRequest()`.
- A second drain path (see `../../Public/McpQueueFairness.h`: exactly one game-thread dequeuer).
- Handler logic in Core: Core routes, Domains implement.
- More than 250 pure lines per file or 25 files per folder.
