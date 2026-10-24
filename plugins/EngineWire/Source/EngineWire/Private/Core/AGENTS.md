# Private/Core — request queue, game-thread dispatch, handler registration

Core owns the request queue, the game-thread drain and the per-tool handler map: the routing spine between the transports (`../Transport/`, `../MCP/`) and the domain implementations (`../Domains/`). Hazardous editor ops go through `../Safety/`.

## STRUCTURE
- `Subsystem/` (11): `UEngineWireSubsystem` (declared in `../../Public/EngineWireSubsystem.h`) split into `...Subsystem.cpp`, `...RequestQueue.cpp`, `...RequestQueueCancellation.cpp`, `...Lifecycle.cpp`, `...HandlerRegistration.cpp`, `...Responses.cpp` + sanitization/enrichment headers, `...ErrorCapture.cpp`, `...EditorCommands.cpp`, `...RequestQueueTests.cpp`.
- `Requests/` (9): `McpProcessRequest.cpp` (`AutomationHandlers.Find(Action)`, else `UNKNOWN_ACTION`), `McpRequestOriginRegistry` (request id → originating transport), `McpResponseCaptureRegistry`, `McpModalRefusal.cpp` (answers queued requests while a dialog runs its own loop), `McpUndoLedger` (the undo steps a request left, read from the transaction buffer, plus its in-editor test).
- `Security/` (3): `McpPrequeueGate` + `McpPrequeueDemand` — scope/consent authorization before anything is enqueued, on both transports.
- `Journal/` (6): `McpChangeJournal` — what changed in the editor and who changed it (mcp, user, editor), read by `inspect changes_since`: the recorder (object and actor events), its package events and Start/Stop, the reader, the shared state and an in-editor attribution test.
- `Module/` (4), `Settings/` (1), `Compatibility/` (1), `Errors/` (1).

## REQUEST LIFECYCLE
1. A transport authorizes the request through the pre-queue gate, then calls `QueueAutomationRequest()`.
2. `QueueAutomationRequest()` locks `PendingAutomationRequestsMutex` and appends; it rejects with `EAutomationQueueRejection::{NotAccepting, AlreadyCanceled, QueueFull}` (cap `MaxPendingAutomationRequests`).
3. `Tick()` → `ProcessPendingAutomationRequests()` drains 16 per tick on the game thread (re-posts itself when called off it).
4. `ProcessRequest` looks up the tool name in `AutomationHandlers` and invokes the handler; the handler replies through its socket.
5. While the handler runs, `McpChangeWatch` (`../Domains/ControlActor/Placement/`) notes the editor-level actors it spawns or modifies, and `SendAutomationResponse` adds their placement and content checks to a successful reply's `warnings`. A reply sent after the handler returned carries none. Core only calls it: the checks live in the domain. Beside it, `McpUndoLedger` records the transactions the request closes and gives the reply its `undo` block from the buffer: the steps it left, a handler claim the buffer dropped (`NOTHING_CHANGED`), or a cleared history (`UNDO_HISTORY_CLEARED`). `McpChangeJournal::BeginRequest`/`EndRequest` bracket the same span, so the journal marks what changes inside it as this server's, and a reply gains one warning when the user changed something in the editor since the previous request.

## CHANGE JOURNAL
- Hot path: `OnObjectModified` fires on every Modify, thousands of times in one compile. Reject on pointers and flags first and judge each package once by its pointer; no string work for a package already judged.
- Attribution: inside a request = mcp; inside an editor transaction (`GUndo` or `GIsTransacting`) no request opened = user; else editor. The editor's own actor and object edits are not kept (loading, compiling, a new level's actors); a level that opens forgets the rows of its actors once the editor's `LoadMap` is done (`OnMapOpened` comes last).
- Never name a reply field "token": the receipt redaction masks it. The cursor is `<session>.<seq>`.
- The journal is game-thread state. The transports' threads read only what it publishes: `LatestSeq()` (atomic) and `LatestCursor()` (the session under a lock), which back `ue://changes`, the native subscription sweep and the bridge's `changes_moved`.

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
- A second dispatching drain (see `../../Public/McpQueueFairness.h`: exactly one game-thread path RUNS queued requests). Paths that only remove queued requests without running them are allowed: cancellation, and the modal-loop answer (`Requests/McpModalRefusal.cpp`, EDITOR_MODAL_ACTIVE while a dialog's own loop stops the ticker).
- Handler logic in Core: Core routes, Domains implement.
- More than 250 pure lines per file or 25 files per folder.
