# DOMAINS — Automation Implementation Layer

66 top-level domain directories (135 dirs incl. nested), 1097 files. The single hottest area in the repo (1203 file-changes in 60 days). Every editor action an MCP client can trigger is implemented here.

Cross-link, never duplicate: scope map in `../AGENTS.md`, registration in `../Core/AGENTS.md`, shared helpers in `../Foundation/AGENTS.md`, hazardous-op wrappers in `../Safety/AGENTS.md`, native MCP in `../MCP/AGENTS.md`.

## STRUCTURE

Each domain is a folder `Domains/<Domain>/` holding one thin `*Dispatch.cpp` plus sibling behavior `.cpp` files split by responsibility (e.g. `ControlActor/McpAutomationBridge_ControlActorSpawn.cpp`).

Grouped index (heavy sub-trees first; the rest are discoverable by folder name):

- Cinematics: `Sequence` (~100), `Sequencer`, `Animation` (~63), `AnimationAuthoring` (~24), `Skeleton` (~26)
- Asset/Blueprint: `AssetWorkflow` (~54), `MaterialAuthoring` (~51), `Blueprint` (~50), `BlueprintCreation`, `BlueprintGraph`, `WidgetAuthoring` (~47), `SCS`, `StructProperty`
- World: `Level` (~41), `LevelStructure` (~23), `Geometry` (~37), `Environment` (~33), `Landscape`, `Foliage`, `Volume` (~23), `WorldPartition`, `Spline`, `Navigation`
- VFX: `Niagara`, `NiagaraActor`, `NiagaraAuthoring`, `NiagaraEmitter`, `NiagaraGraph`, `NiagaraParameter`, `NiagaraRibbon`, `NiagaraSystem`
- Gameplay: `AI` (~31), `GAS` (~24), `Character`, `Combat`, `Interaction`, `Inventory`, `BehaviorTree`, `GameFramework`, `Input`
- Editor/System: `ControlActor`, `ControlEditor`, `ConsoleCommand`, `Debug`, `EditorFunction`, `Inspect`, `Log`, `Property`, `SystemControl`, `Test`, `Misc`, `Networking`, `Sessions`, `Performance`, `Pipeline`, `PCG`, `Render`, `Texture`, `Ui`, `Audio`, `AudioAuthoring`, `Lighting`, `Effect`, `Insights`, `MaterialGraph`, `AssetQuery`

## DISPATCH CONTRACT

Every domain exposes one entry point on `UMcpAutomationBridgeSubsystem`:

```cpp
bool UMcpAutomationBridgeSubsystem::Handle<Domain>Action(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
```

The dispatcher reads `payload.action` (sub-action) and routes to concrete `Handle<Domain>Xxx` methods in sibling `.cpp` files via the macros in `Foundation/HandlerUtils/McpHandlerUtils.h`:

- `MCP_DISPATCH_ACTION(ActionVar, "name", HandlerCall)` — top-level action match.
- `MCP_DISPATCH_SUBACTION(ActionVar, Payload, "sub", HandlerCall)` — resolves and normalizes the sub-action.

Real shape (from `ControlActor/McpAutomationBridge_ControlActorDispatch.cpp`):

```cpp
if (LowerSub == TEXT("spawn") || LowerSub == TEXT("spawn_actor"))
    return HandleControlActorSpawn(RequestId, Payload, RequestingSocket);
...
if (LowerSub == TEXT("set_transform") || LowerSub == TEXT("set_actor_transform") ...)
    return HandleControlActorSetTransform(RequestId, Payload, RequestingSocket);
```

**Keep dispatchers thin.** Validation of the sub-action string and a single unknown-action error belong in the dispatcher. All real work lives in the responsibility `.cpp`. Leaf handlers must NOT emit their own "Unknown <X> subAction" — return `false` and let the dispatcher own it.

Error macros (from `McpHandlerUtils.h`): `MCP_ERROR_INVALID_PAYLOAD(Send, ReqId, Msg)`, `MCP_ERROR_MISSING_PARAM(Send, ReqId, Param)`, `MCP_ERROR_NOT_FOUND(Send, ReqId, Item, Id)`.

## ADDING AN ACTION

1. Pick or create the domain folder `Domains/<Domain>/`.
2. Add a `Handle<Domain>Xxx(RequestId, Payload, Socket)` declaration to the domain support header and the body in a responsibility `.cpp`.
3. Add the route in `<Domain>Dispatch.cpp` via `MCP_DISPATCH_ACTION` / `MCP_DISPATCH_SUBACTION`.
4. Register the action in the matching `../Core/Subsystem/McpAutomationBridgeSubsystem<Area>Registration.cpp` shard using `MCP_REGISTER_DIRECT(action, Method)` -> `RegisterHandler()`, wired from `InitializeHandlers()`. See `../Core/AGENTS.md` for the full procedure.
5. Add a unit/contract test under `tests/unit/plugin/` or `tests/`.

## FILE-SIZE + STRUCTURE CEILINGS

Enforced by Vitest source-contract tests that read C++ text and fail CI (`tests/unit/plugin/source_structure_contracts.test.ts`, `source_structure.test.ts`):

- **250 pure lines per file** (pure = non-blank, non-`#`, non-`//`). Split oversized files into responsibility `.cpp`.
- **≤25 files per folder.**
- **No split artifacts**: never name files `Common*`, `Part\d+`, `\d+.cpp`, or `.incl`.
- **Every local `Mcp*` include must resolve on disk** — dangling includes fail CI.

## CONVENTIONS

- Reuse `../Foundation/` for reflection, path, Blueprint, JSON, response, and object-resolution helpers. Do not grow domain-local copies.
- Editor work runs on the game thread via the Core queue. Never call editor APIs from a socket thread.
- Optional engine features must compile away or fail clearly when their module is missing, via the `MCP_HAS_*` defines emitted by `McpAutomationBridge.Build.cs` (e.g. `MCP_HAS_PCG`, `MCP_HAS_MOVIE_RENDER_PIPELINE`, `MCP_HAS_TAKE_RECORDER`). A missing module must never break the build.

## ANTI-PATTERNS (forbidden -> alternative)

| Forbidden | Use instead |
|-----------|-------------|
| `UPackage::SavePackage()` | `McpSafeAssetSave` / `McpSafeLevelSave` / `McpSafeLoadMap` (`../Safety/AGENTS.md`) |
| `FEditorFileUtils::SaveMap` | `McpSafeLevelSave` |
| `UEditorAssetLibrary::DeleteDirectory` | `McpSafeDeleteFolder` |
| `ANY_PACKAGE` | modern `nullptr` / project lookup |
| `TargetGraph->RemoveNode` | `RemoveNiagaraGraphNodeSafely` |
| `GenerateFullEnumName` | `GetIndexByValue` + `HasMetaData("Hidden")` |
| `SendRawMessage` / `SendControlMessage` for automation events | `BroadcastAutomationEvent` / `SendAutomationResponse` |
| Leaf handler emits "Unknown <X> subAction" | return `false`; let the dispatcher own it |
