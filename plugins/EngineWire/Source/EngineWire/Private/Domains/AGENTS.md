# DOMAINS — Automation Implementation Layer

56 top-level domain directories, ~1,500 source files. Every editor action an MCP client can trigger is implemented here.

Cross-link, never duplicate: scope map in `../AGENTS.md`, registration in `../Core/AGENTS.md`, shared helpers in `../Foundation/AGENTS.md`, hazardous-op wrappers in `../Safety/AGENTS.md`, native MCP in `../MCP/AGENTS.md`.

## STRUCTURE

Each domain is a folder `Domains/<Domain>/` holding its thin dispatcher `Mcp<Tag>Handlers.cpp` plus sibling behavior `.cpp` files split by responsibility (e.g. `ControlActor/McpActorSpawn.cpp`). Eight domains have no dispatcher; the domains that use them call in: AssetQuery from AssetWorkflow, Foliage and Landscape from Environment, MetaHuman from Character, Perception from Inspect, Play from ControlActor and ControlEditor, Property from Inspect, ControlActor, Blueprint and SCS, Scripts from SystemControl and the gateway.

Grouped index (~N: source files, subfolders included, counted 2026-11-09; the rest are discoverable by folder name):

- Cinematics: `Sequence` (~131; `Planner/`: `frame_shot`, a camera placed by where its subjects must sit on screen, two subjects through toric space, refined by `McpNelderMead` over sight-line costs; `fly_through`, A* through free cells, cut short by sweeps, smoothed by `McpMinSnap` and keyed with a camera cut; `auto_edit`, segment x camera scores and continuity cut costs, cut by dynamic programming), `Animation` (~25), `AnimationAuthoring` (~39; `Blueprints/`: the Anim Blueprint graph actions and the node and state helpers they share; `Analysis/`: `analyze_animation`, and `auto_blend_space`, each clip's ground velocity from `McpAnimLocomotion` placed by the pure `McpAnimBlendLayout`), `Skeleton` (~32)
- Asset/Blueprint: `AssetWorkflow` (~98; `DataRegistries/`: data registries by reflection, no DataRegistry link), `MaterialAuthoring` (~45), `Blueprint` (~70), `WidgetAuthoring` (~72; `Audit/`: `audit_widget`, the preview's offscreen draw read back as laid-out rectangles and pixels; `Arrange/`: `arrange_widgets`, relations solved by `McpCassowary`, anchors from what pins each widget; `ViewModel/`: MVVM viewmodels and bindings by reflection, the MVVM editor subsystem's UFUNCTIONs called through ProcessEvent and binding paths written field by field, no ModelViewViewModel link), `BlueprintGraph` (`Lint/`: `lint_blueprint`'s rules on the exec flow and its dominators; `Lint/Interp/`: an abstract interpreter, intervals with bounds against array lengths, run to a fixpoint per event or function, for the value rules), `BlueprintCreation`, `SCS`, `Texture`, `AssetQuery` (`Retrieval/`: `thumbnail_sheet`, `caption_assets`, `find_assets_like`; an index of each asset's words, registry size and stored-thumbnail look under Saved/EngineWire, captions beside it, criteria fused by reciprocal rank)
- World: `Environment` (~39; `build_environment`'s dispatcher, which calls into `Landscape`, `Foliage` and `Lighting` for their parts, plus sky, weather, water, snapshots and procedural terrain; `Roads/`: `McpRoadPlan`, the lattice search under `plan_road`; `Modules/`: `McpModuleKit`, the face reading and wave function collapse under `assemble_modules`), `Geometry` (~58; `Roof/`: `McpRoofSkeleton`, the straight skeleton a hip roof follows, its faces through `AppendPolygonFaces` like `append_polygons`), `Level` (~40), `Volume`, `LevelStructure`, `Landscape` (also the landscape work `build_environment` calls, heightmap files, layer info assets, landscape splines and streaming proxies, declared in `McpLandscapeShared.h`; `McpLandscapeLookup` holds both lookups, `ResolveLandscapeOrReply` for the edit actions, which replies on a miss, and `McpFindLandscape` for `build_environment`'s, which takes more spellings, falls back to the first landscape the caller may touch and never replies; `Erosion/`: `McpTerrainErosion`, fill, drainage, stream power, droplets and rivers on a plain height grid, under `erode_landscape`), `Foliage` (`McpFoliageConfigure`: `build_environment`'s configure_foliage_*), `Spline`, `Navigation` (`Analytics/`: `analyze_level_layout`, the nav mesh sampled into a graph and read as rooms, corridors, doors and route shares), `PCG`, `Lighting` (`bake_lightmap` forwards to `HandleLightingAction` with Preview quality), `Render`
- VFX: `NiagaraAuthoring`, `NiagaraGraph`, `Effect`
- Gameplay: `AI` (~37), `GAS`, `Character`, `Combat`, `Interaction`, `Inventory`, `BehaviorTree`, `GameFramework`, `Input`, `Networking`, `Sessions`, `MetaHuman`
- `manage_ai`'s dispatcher (`AI/McpAIHandlers.cpp`) runs AI's own handlers, filed by topic under `AI/`, and sends the actions in `McpConsolidatedActions::BehaviorTree()` and `::Navigation()` to the `BehaviorTree` and `Navigation` domains. Two pairs overlap on purpose, both public with their own defaults and replies, so neither replaces the other. `AI/BehaviorTree` (create_behavior_tree, add_composite_node, add_task_node, add_decorator, add_service, configure_bt_node) edits the runtime tree. `BehaviorTree` (create, add_node, add_subnode, connect_nodes...) edits the graph and owns what both share: `EnsureBehaviorTreeGraph` gives runtime-tree edits their graph nodes, `FindBehaviorTreeGraphRoot`, and `MCP_HAS_BEHAVIOR_TREE_GRAPH`. In the other pair, `AI/Navigation`'s create_nav_modifier places a NavModifierVolume or gives a Blueprint a component, with an Obstacle default and short area names. `Navigation`'s create_nav_modifier_component gives a Blueprint or a placed actor a component, with failsafeExtent and save on request.
- Editor/System: `ControlActor`, `ControlEditor` (`EditorUi/`: the editor's own Slate UI read as an outline with refs; it only reads), `ConsoleCommand`, `Debug` (`Explain/`, `Contracts/`, and `Logpoints/`: marked Blueprint nodes' pin values read from the script tracepoints while Play runs, never pausing it), `Inspect` (the inspect tool: its dispatcher `McpInspectHandlers.cpp`, its handlers and their shared header `McpInspectShared.h`, namespace `McpInspectHandlers`; `McpInspectRuntimeDescribe`: the world a runtime report reads and its actors described; `View/`: describe_view, the view ids and the capture API `McpCaptureView` that Perception and the frame budget also use; `MeshCheck/`: `McpMeshCheck`, a static or skeletal mesh's source triangles read part by part, each closed part's signed volume saying whether it is wound inside out, under `check_mesh` and the import and reimport warnings), `Log`, `Property`, `SystemControl` (`Plugins/`: get_plugin_info, create_plugin, remove_plugin, and list_game_features / set_game_feature_active through `IMcpGameFeaturesProvider`, which the EngineWireGameFeatures module registers), `Performance`, `Insights`, `Ui`, `Audio` (`analyze_acoustics`: `McpAcoustics::Estimate` reads a room through a trace callback, so its known answers run on analytic box rooms), `AudioAuthoring` (`measure_loudness` and `normalize_loudness`: EBU R128 integrated loudness from a wave's own samples, in `McpLoudness` with known answers from EBU Tech 3341), `Perception` (what a captured view shows; `audit_materials` reads the view's G-buffer in the Buffer Visualization view mode and puts the view mode back), `Play` (what drives a running game: `playtest` and `explore_level`, only in a Play session this server started, never one the user may be playing; `settle_physics`, a Simulate session of its own whose resting poses are kept through `RunLatentReply`), `Scripts` (`run_script`: each step is held to a direct execute's gateway validation and pre-queue gate for the caller's principal, consent aside, and dispatched in-process under a captured id; a step that answers later is awaited within the deadline by `McpScriptAwait`, the run going on inside `RunLatentReply` from its first form, every call and expectation an earlier pass reached taken from the runner's memo, under a restore point of its own that undoes only its request's steps; recipes in `McpRecipes`: a script kept under Saved/EngineWire/Recipes/, run through `HandleRunScript` itself, and listed by the gateway's search through `McpRecipesSearch`, which runs on the socket thread and so reads files and JSON only)

## DISPATCH CONTRACT

Every domain with a dispatcher exposes one entry point on `UEngineWireSubsystem`:

```cpp
bool UEngineWireSubsystem::Handle<Domain>Action(
    const FString &RequestId, const FString &Action,
    const TSharedPtr<FJsonObject> &Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket);
```

The dispatcher resolves the sub-action (`subAction` first, `action` only as the
fallback — the same priority `McpHandlerUtils::NormalizeAction` and the pre-queue
gate use) and routes to concrete `Handle<Domain>Xxx` methods in sibling `.cpp`
files: by an explicit compare chain, or by a static table of action names and
handlers (Animation, Level, Networking, Skeleton; AI's table is a `TMap`).
AnimationAuthoring, Blueprint and WidgetAuthoring ask each group handler in
turn, and a group declines an action that is not its own. A compare chain:

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
3. Add the route to the domain's dispatcher `Mcp<Tag>Handlers.cpp` (its compare chain or route table).
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
- A guard that reads an input, resolves it and refuses on a miss is one `...OrReply` call, not a pasted preamble: Foundation's `McpReadRequiredField` and `McpReadAssetPathOrReply`, or the domain's own (`FindActorOrReply`, `FCombatActionContext::LoadBlueprintOrReply`, `McpSequence::LoadAssetOrReply`, `LoadActorDefaultsOrReply`, `McpLoadStateTreeEditorDataOrReply`, `LoadSoundOrReply`). Each keeps its sites' exact code and wording.
- A refusal that carries no result object is `SendAutomationError(Socket, RequestId, Message, Code)`, never `SendAutomationResponse(..., false, Message, nullptr, Code)` written out: the same reply, and the warning line every refusal logs.
- Domains do not include `Transport/` headers: a handler passes on the `TSharedPtr<FMcpBridgeWebSocket>` it was given, a type `EngineWireSubsystem.h` forward-declares.
- Editor work runs on the game thread via the Core queue. Never call editor APIs from a socket thread.
- Optional engine features must compile away or fail clearly when their module is missing, via the `MCP_HAS_*` defines emitted by `EngineWire.Build.cs` (e.g. `MCP_HAS_PCG`, `MCP_HAS_MOVIE_RENDER_PIPELINE`, `MCP_HAS_TAKE_RECORDER`). A missing module must never break the build.
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
| Writing to the socket (`FMcpBridgeWebSocket::Send`) from a handler | `SendAutomationResponse` / `SendAutomationError`; `BroadcastAutomationEvent` for an event |
| Leaf handler emits "Unknown <X> subAction" | return `false`; let the dispatcher own it |
