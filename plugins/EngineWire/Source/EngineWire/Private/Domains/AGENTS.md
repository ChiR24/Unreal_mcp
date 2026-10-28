# DOMAINS — Automation Implementation Layer

58 top-level domain directories, ~1,300 source files. Every editor action an MCP client can trigger is implemented here.

Cross-link, never duplicate: scope map in `../AGENTS.md`, registration in `../Core/AGENTS.md`, shared helpers in `../Foundation/AGENTS.md`, hazardous-op wrappers in `../Safety/AGENTS.md`, native MCP in `../MCP/AGENTS.md`.

## STRUCTURE

Each domain is a folder `Domains/<Domain>/` holding one thin `*Dispatch.cpp` plus sibling behavior `.cpp` files split by responsibility (e.g. `ControlActor/McpActorSpawn.cpp`).

Grouped index (largest first; the rest are discoverable by folder name):

- Cinematics: `Sequence` (~129; `Planner/`: `frame_shot`, a camera placed by where its subjects must sit on screen, two subjects through toric space, refined by `McpNelderMead` over sight-line costs; `fly_through`, A* through free cells, cut short by sweeps, smoothed by `McpMinSnap` and keyed with a camera cut; `auto_edit`, segment x camera scores and continuity cut costs, cut by dynamic programming), `Animation` (~27), `AnimationAuthoring` (~24; `Analysis/`: `analyze_animation`, and `auto_blend_space`, each clip's ground velocity from `McpAnimLocomotion` placed by the pure `McpAnimBlendLayout`), `Skeleton` (~25)
- Asset/Blueprint: `AssetWorkflow` (~69; `DataRegistries/`: data registries by reflection, no DataRegistry link), `MaterialAuthoring` (~50), `Blueprint` (~49), `WidgetAuthoring` (~36; `Audit/`: `audit_widget`, the preview's offscreen draw read back as laid-out rectangles and pixels; `Arrange/`: `arrange_widgets`, relations solved by `McpCassowary`, anchors from what pins each widget), `BlueprintGraph` (`Lint/`: `lint_blueprint`'s rules on the exec flow and its dominators; `Lint/Interp/`: an abstract interpreter, intervals with bounds against array lengths, run to a fixpoint per event or function, for the value rules), `BlueprintCreation`, `SCS`, `StructProperty`, `Texture`, `AssetQuery` (`Retrieval/`: `thumbnail_sheet`, `caption_assets`, `find_assets_like`; an index of each asset's words, registry size and stored-thumbnail look under Saved/EngineWire, captions beside it, criteria fused by reciprocal rank)
- World: `Environment` (~42; `Roads/`: `McpRoadPlan`, the lattice search under `plan_road`; `Modules/`: `McpModuleKit`, the face reading and wave function collapse under `assemble_modules`), `Geometry` (~37; `Roof/`: `McpRoofSkeleton`, the straight skeleton a hip roof follows, its faces through `AppendPolygonFaces` like `append_polygons`), `Level` (~35), `Volume`, `LevelStructure`, `Landscape` (`Erosion/`: `McpTerrainErosion`, fill, drainage, stream power, droplets and rivers on a plain height grid, under `erode_landscape`), `Foliage`, `Spline`, `Navigation` (`Analytics/`: `analyze_level_layout`, the nav mesh sampled into a graph and read as rooms, corridors, doors and route shares), `PCG`, `Lighting`, `Render`
- VFX: `Niagara`, `NiagaraAuthoring`, `NiagaraGraph`, `Effect`
- Gameplay: `AI` (~35), `GAS`, `Character`, `Combat`, `Interaction`, `Inventory`, `BehaviorTree`, `GameFramework`, `Input`, `Networking`, `Sessions`, `MetaHuman`
- Editor/System: `ControlActor`, `ControlEditor` (`EditorUi/`: the editor's own Slate UI read as an outline with refs and used as a Slate virtual user down each widget's path, never the system cursor), `ConsoleCommand`, `Debug` (`Explain/`, `Contracts/`, and `Logpoints/`: marked Blueprint nodes' pin values read from the script tracepoints while Play runs, never pausing it), `Inspect`, `Log`, `Property`, `SystemControl` (`Plugins/`: get_plugin_info, create_plugin, remove_plugin, and list_game_features / set_game_feature_active through `IMcpGameFeaturesProvider`, which the EngineWireGameFeatures module registers), `Performance`, `Insights`, `Ui`, `Audio` (`analyze_acoustics`: `McpAcoustics::Estimate` reads a room through a trace callback, so its known answers run on analytic box rooms), `AudioAuthoring` (`measure_loudness` and `normalize_loudness`: EBU R128 integrated loudness from a wave's own samples, in `McpLoudness` with known answers from EBU Tech 3341), `Perception` (what a captured view shows; `audit_materials` reads the view's G-buffer in the Buffer Visualization view mode and puts the view mode back), `Play` (what drives a running game: `playtest` and `explore_level`, only in a Play session this server started, never one the user may be playing; `settle_physics`, a Simulate session of its own whose resting poses are kept through `RunLatentReply`), `Scripts` (`run_script`: each step is held to a direct execute's gateway validation and pre-queue gate for the caller's principal, consent aside, and dispatched in-process under a captured id; recipes in `McpRecipes`: a script kept under Saved/EngineWire/Recipes/, run through `HandleRunScript` itself, and listed by the gateway's search through `McpRecipesSearch`, which runs on the socket thread and so reads files and JSON only)

## DISPATCH CONTRACT

Every domain exposes one entry point on `UEngineWireSubsystem`:

```cpp
bool UEngineWireSubsystem::Handle<Domain>Action(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
```

The dispatcher resolves the sub-action (`subAction` first, `action` only as the
fallback — the same priority `McpHandlerUtils::NormalizeAction` and the pre-queue
gate use) and routes to concrete `Handle<Domain>Xxx` methods in sibling `.cpp`
files with an explicit compare chain. Every domain does it this way:

```cpp
if (LowerSub == TEXT("spawn") || LowerSub == TEXT("spawn_actor"))
    return HandleControlActorSpawn(RequestId, Payload, RequestingSocket);
...
if (LowerSub == TEXT("set_transform") || LowerSub == TEXT("set_actor_transform") ...)
    return HandleControlActorSetTransform(RequestId, Payload, RequestingSocket);
```

**Keep dispatchers thin.** Validation of the sub-action string and a single unknown-action error belong in the dispatcher. All real work lives in the responsibility `.cpp`. Leaf handlers must NOT emit their own "Unknown <X> subAction" — return `false` and let the dispatcher own it.

Errors go out through `SendAutomationError(Socket, RequestId, Message, Code)` with a
SCREAMING_SNAKE code (`INVALID_PAYLOAD`, `MISSING_PARAMETER`, `NOT_FOUND`).

## ADDING AN ACTION

1. Pick or create the domain folder `Domains/<Domain>/`.
2. Add a `Handle<Domain>Xxx(RequestId, Payload, Socket)` declaration to the domain support header and the body in a responsibility `.cpp`.
3. Add the route to the compare chain in `<Domain>Dispatch.cpp`.
4. Make sure the parent tool reaches the domain: `../Core/Subsystem/...HandlerRegistration.cpp` maps each of the 23 parents (plus `console_command`) to a handler, with `FSubRoute` entries for sibling domains. See `../Core/AGENTS.md`.
5. Add the capability record (TS side) and an integration case under `tests/mcp-tools/`.

## FILE-SIZE + STRUCTURE CEILINGS

Enforced by Vitest source-contract tests that read C++ text and fail CI (`tests/unit/plugin/source_structure_contracts.test.ts`, `source_structure.test.ts`):

- **250 pure lines per file** (pure = non-blank, non-`#`, non-`//`). Split oversized files into responsibility `.cpp`.
- **≤25 files per folder.**
- **No split artifacts**: never name files `Common*`, `Part\d+`, `\d+.cpp`, or `.incl`.
- **Every local `Mcp*` include must resolve on disk** — dangling includes fail CI.
- **Short file names**: `Mcp<Tag><Topic>.cpp` (`McpMatSetScalarParamValue.cpp`; tags such as `Anim`, `Mat`, `BP`, `BPGraph`, `Seq`, `Env`, `Actor`). Fab refuses a package path over 170 characters, and `source_structure.test.ts` holds every path to 150.

## CONVENTIONS

- Reuse `../Foundation/` for reflection, path, Blueprint, JSON, response, and object-resolution helpers. Do not grow domain-local copies.
- Editor work runs on the game thread via the Core queue. Never call editor APIs from a socket thread.
- Optional engine features must compile away or fail clearly when their module is missing, via the `MCP_HAS_*` defines emitted by `EngineWire.Build.cs` (e.g. `MCP_HAS_PCG`, `MCP_HAS_MOVIE_RENDER_PIPELINE`, `MCP_HAS_TAKE_RECORDER`, `MCP_HAS_MEDIA_ASSETS`). A missing module must never break the build.
- An action that promises gameplay behaviour (movement modes, ammo, inventory functions, input bindings, trigger events) writes it into the Blueprint with `McpBlueprintBehaviour::Author` and a recipe file under `Resources/Recipes/<Domain>/`. Read `BlueprintGraph/Behaviour/AGENTS.md` first; never hand-roll graph edits or report "configured" for logic that was not authored.

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
