# Changelog

All notable changes to the MCP Automation Bridge plugin will be documented in this file.

---

## [Unreleased]

### Added
- **`control_actor.list` filters** — `tag`, `className` (the actor's class or any parent, by name or path, `_C` optional) and `folder` (that outliner folder or one under it, `"(none)"` for the root), matched by `McpActorMatchesListFilters` in `ControlActorSupport.h` before counting and paging.
- **`componentNames`** on `get_components` (world actors and Blueprints) and `inspect_cdo`, applied by `McpHandlerUtils::FilterRowsByListedNames`, which reports unmatched names as `missingComponents`.
- **`spawn_batch` `unnamedActors`** — `HandleControlActorSpawnBatch` reports an item without `actorName` by `GetName()` (its label repeats) and returns `unnamedActors` in batch order after the `report` filter.
- **Foliage counts** — `HandleGetFoliageInstances` moves to `FoliageHandlersGetInstances.cpp` and returns `byType`, honours `summary` and `limit` and reports `truncated`; the environment dispatcher forwards both options into the payload it rebuilds.
- **`control_actor.sample_motion`** — `HandleControlActorSampleMotion` (`ControlActorMotionSample.cpp`) resolves the actor in the PIE world, refuses a world that is not PIE or Game with `NOT_SIMULATING`, then samples from an `FTSTicker` against `UWorld::GetTimeSeconds` (location, `GetVelocity`, `propertyNames` via `McpPropertyReflection`) and replies when the game-time duration, the 400-sample cap or `maxRealSeconds` (at most 50) is reached, or when the actor or world goes away.
- **`sample_motion` inputs and startWhen** — `McpAutomationBridge_ControlActorMotionInputs.cpp` parses `inputs` (at most 32, keys checked with `FKey::IsValid` like simulate_input) and `startWhen`, and the run's ticker presses and releases each key through `SimulateEditorInputForMcp` at its game-time offset (never releasing in the frame it went down), waits in a trigger phase until the gate property reads `equals` (a change into it unless `waitForChange` is false, numbers compared as numbers), and releases every key still down when the run ends for any reason.
- **Asset referencers** — `HandleGetDependencies` (`AssetWorkflowDependencies.cpp`) takes `referencers` and answers from `IAssetRegistry::GetReferencers` instead of `GetDependencies`, keyed by `FPackageName::ObjectPathToPackageName` so an object path resolves, and sorts the list (`FNameLexicalLess`).
- **Runtime roles in object paths** — `ResolveObjectFromPath` (`McpHandlerUtilsObjectResolution.cpp`) resolves `GameInstance`, `GameMode`, `GameState`, `PlayerController`, `PlayerPawn`, `PlayerState` and `HUD` against `GEditor->PlayWorld`, after the actor name and label lookups; `DescribeObjectNotFound` gives get/set property one not-found message that names them.
- **Actor roles** — `FindActorByName` (`ControlActorLookup.cpp`) answers a PIE miss with `McpHandlerUtils::ResolveRuntimeRole` (now exported), so every `control_actor` action that resolves through it takes `PlayerPawn` and the other actor roles.
- **Component sampling** — `HandleControlActorSampleMotion` resolves a dotted `propertyNames` entry through `McpHandlerUtils::FindActorComponentByName` and samples it from that component (`FMcpMotionProperty` keeps the label, a weak owner and the property).
- **startWhen timeout reason** — `McpStartWhenTimeoutWarning` (`ControlActorMotionInputs.cpp`) reports the trigger's last value and, when it already matched under `waitForChange`, points at `waitForChange: false`; the sample_motion ticker sends it as the reply's warning.
- **Button sounds** — `McpApplyButtonSound` (`WidgetAuthoringStyleColor.h`) sets `FButtonStyle::HoveredSlateSound` / `PressedSlateSound` from `hoverSoundPath` / `pressSoundPath` through the set_style convenience surface, refusing a non-Button target or a path that does not load as a `USoundBase`.
- **set_style persistence** — the convenience branch of `HandleWidgetAuthoringStyleClipping` calls `McpSafeAssetSave(WidgetBP)` after `MarkBlueprintAsModified` and reports `saveSucceeded`, as the clipping and reflection branches already did.
- **Targeted save** — `HandleControlEditorSaveAll` (`ControlEditorAssets.cpp`) reads `assetPaths`, reduces each to a package name, processes only the matching dirty packages and reports `leftDirtyCount`.
- **Box-slot alignment** — `McpWidgetSlotAlignment::ResolveAlignmentValue` takes the word branch only for `EJson::String` values; `FJsonValueNumber::TryGetString` succeeds too, so numbers never reached the numeric branch.
- **Slash-list messages** — the alignment, shadow-settings, vector-parameter, Niagara module-input, material-track, cvar and property-apply errors list their choices with commas, so `RedactFilesystemPathsForResponse` no longer takes them for paths.
- **Short Blueprint class names** — `ResolveClassByName` sends only a `_C` PATH down the generated-class branch, so a short `BP_Door_C` reaches the loaded-class scan; a miss with no `/` or `.` then looks the asset name up under `/Game` in the asset registry and loads its `_C` class quietly (no `UEditorAssetLibrary`, so it also works during PIE).
- **`set_transform` `offset`** — `HandleControlActorSetTransform` adds `offset` to the actor's current location (refusing it together with `location`); the `actors` path forwards each item's `offset` through the same handler.
- **Unknown-action guidance** — `GatewayGuideUnknownAction` (`McpNativeGatewayGuidance.cpp`) builds the suggestions, `nextCall` and message hint for `UNKNOWN_ACTION` on describe and execute: the one other tool that owns the action (`FMcpCapabilityStore::GetParentsWithAction`) first, a search on the action's words when no suggestion shares its verb within one edit, else the closest action.

### Changed
- **`control_actor.list` summary** rows are `{name, count}` arrays instead of `{name: count}` objects: receipt redaction classifies JSON keys, so a folder, tag or class whose name reads as a credential lost its count.
- **One dispatch table** — the fallback chain in `McpAutomationBridge_ProcessRequestDispatch.cpp` that offered an unmatched action to every handler is gone; `McpAutomationBridgeSubsystemHandlerRegistration.cpp` routes each parent's sub-actions through explicit `FSubRoute` claims, including `IsSystemUiAction` → `HandleUiAction` for `system_control`'s widget, screenshot, project-settings, sound and display actions. The TypeScript server now forwards `{action, ...params}` to the parent tool as the native door does, so the plugin is the only action layer.
- **Declared aliases are read** — `McpGetFirstStringField` reads the alias spellings the records declare: `type` (`add_material_node`), `targetPath` (`import_level`, `duplicate_level`), `sourceNode`/`sourcePin`/`targetNode`/`targetPin` (`connect_metasound_nodes`), `emitter` (Niagara authoring context) and `actorName` (`set_niagara_parameter`, which now routes through `create_effect`).
- **Strict scalar writes** — `ApplyJsonValueToProperty` hands every scalar property to `McpPropertyReflection::ApplyJsonValueToProperty` (`McpAutomationBridgeHelpersPropertyApplyScalars.h` is gone): integers refuse non-integral and out-of-range values instead of truncating, enums refuse hidden and `_MAX` entries and accept display names.
- **`create_blend_space`** routes to the authoring `create_blend_space_2d` handler (Direction −180..180 × Speed 0..600); the Animation-domain copy, which could only build a 1D 0..1 space, and its `BlendSpaces` folder are gone.
- **`apply_baseline_settings`** sets `Scalability::SetQualityLevels` (performance → Low, balanced → High, quality → Epic) plus `r.VSync` instead of three hand-coded cvar lists, and no longer touches `r.AllowHDR`.
- **Enhanced Input classes** — `set_input_trigger`, `set_input_modifier` and `add_mapping` share `ResolveInputClass` (any concrete trigger or modifier subclass); the ladder-only spellings `DoubleTap`, `SwizzleInputAxis`, `SmoothDelta` (before 5.4) and `ScaleByDeltaTime` (before 5.1) are refused instead of silently becoming Tap, Smooth or Scalar.
- **Paths and levels** — `NormalizeAssetPath` reports an invalid path instead of probing `/Game`, `/Engine` and `/Script` for a same-named asset; `McpLandscapeMetadataTags.h` and the `MCP_Landscape*` actor tags are gone; `stream_level`/`unload` answer `STREAMING_LEVEL_NOT_FOUND` for a level the editor world does not stream instead of running a console command and reporting success.
- **Game-thread handlers run inline** — landscape and asset delete/rename/refresh handlers drop their `AsyncTask(GameThread)` hops; handlers already run on the game thread.
- **Refusal codes and reply fields** — `SKELETON_NOT_FOUND`, `ASSET_CREATION_FAILED`, `BLUEPRINT_NOT_FOUND`, `SCS_NOT_FOUND` and `PARAM_TYPE_MISMATCH` replace per-handler variants; constant or duplicated fields leave the replies (`validationResult` on `validate_niagara_system`, `analyze_trace`'s header dump, `maxTailSize` on snapshots, `navMeshPresent`/`bHasNavMesh`, three `delete_level` flags, the `bridge_ack` capability lists).

### Removed
- **137 actions that echoed their input, faked success or answered `NOT_SUPPORTED`**, with their handlers (see the root `CHANGELOG.md` for the list); 1,430 `{tool, action}` pairs stay callable across 378 records.
- **WebSocket client mode** (the server keeps rejecting unmasked client frames), the **raw-socket bare action names** neither MCP door sent, and the Project Settings `LogVerbosity`, `bApplyLogVerbosityToAll`, `bEnableSocketTelemetry` and `HeartbeatIntervalMs`, which nothing read.

### Fixed
- **UE 5.8 build** — `McpAutomationBridgeFab.Build.cs` counts a module as present only when its `<Module>.Build.cs` exists (5.8 ships `MegascansPlugin` as a content-only folder, and UBT failed on the missing module definition). `McpAutomationBridge.Build.cs` links PCG normally on 5.8+, whose PCG exports a data symbol MSVC cannot delay-load (`LNK1194`); 5.2–5.7 keep the delay-load, and the now-unused `AddOptionalDynamicModule` wrapper is gone.
- **Widget root cycle** — `SeatWidgetInTree` refuses to seat a widget inside itself or its own subtree and treats re-seating the root as done; reusing the root's `slotName` for a child had made the root its own child and overflowed the stack in UMG. `add_widget_component` seats through `SafeAddWidgetToTree`, and both add paths roll back only a widget they created.
- **`create_*` framework classes during PIE** — `CreateGameFrameworkBlueprint` checks for an existing class with `McpAssetExists` (asset registry); `UEditorAssetLibrary` refused during PIE, so a second create asserted in the engine.
- **`system_control` UI actions** — `set_quality` maps its category to `sg.*Quality` (level clamped 0–4) through `RunConsole`, `play_sound` without `soundPath` plays `/Engine/EditorSounds/Notifications/CompileSuccess_Cue`, and `add_widget_child` forwards to `add_widget_component` (registration, a `RootCanvas` root for a lone leaf, `name` and `text`); these existed only in the TypeScript layer.
- **Audit fixes** — `add_mapping` resolves its classes before mapping the key (a bad class left a trigger-less mapping); `activate_ragdoll` has a route (it answered `NOT_IMPLEMENTED`); `create_render_target` parses `ETextureRenderTargetFormat` (its own `RTF_RGBA16f` example was refused, and `RG8` made `PF_G8`); `add_state_tree_state` finds parents at any depth; `inspect_struct` resolves a bare struct name; `enable_gpu_simulation` applies `fixedBoundsEnabled`/`deterministicEnabled`; `create_animation_asset` answers `ASSET_TYPE_MISMATCH` instead of reusing an asset of another class; `set_modifier_magnitude` writes SetByCaller magnitudes; `set_loot_quality_tiers` stores its tiers; `create_landscape_grass_type` honours `path`.
- **Non-unity build** — three files that compiled only through unity batching include what they use.
- **Batch placement warnings** — `HandleControlActorSetTransform`'s `actors` path re-runs `McpPlacement::DescribePlacement` on every moved actor after the whole batch, replacing the per-item warning taken while later items had not moved yet.
- **Bare struct-member reads** — `HandleControlActorGetComponentProperty` retries a dot-free name as `<StructProperty>.<Name>` when exactly one struct property of the component carries it (`CollisionProfileName` -> `BodyInstance.CollisionProfileName`) and reports the resolved path.
- **Quiet class lookups** — `McpFindTypeQuiet` (`McpAutomationBridgeHelpersClassResolution.h`) replaces `UClass::TryFindTypeSlow`, which logs a warning with a callstack for every short name it resolves, in the graph class-pin resolver, behavior-tree subnode classes and asset-factory classes; `ResolveUClass` and the class-pin resolver load with `LOAD_NoWarn | LOAD_Quiet`, since a Blueprint asset path is expected to miss there before the asset-path fallback resolves it.
- **Connection refusals** — `ConnectPins` (`BlueprintGraphHandlersPinMutations.cpp`) reports `UEdGraphSchema::CanCreateConnection`'s message, both pins' direction and category, and the source node's output pins instead of "schema rejection".
- **Actor references in replies** — `McpActorRef` (`McpAutomationBridgeHelpersResponseVerification.h`) gives the label unless another actor in the world shares it, then `GetName()`. `AddActorVerification` uses it (it overwrote the `actorName` a handler had just set with the bare label, so `set_transform` still answered "Cube"), as does every reply that named an actor by `GetActorLabel()`, in `control_actor` and the lighting, spline, Niagara, effect, environment, navigation, sequence-camera, editor-camera, geometry-primitive and level-structure handlers; and `delete_by_tag` takes all references before its first `DestroyActor`.
- **PIE lookup misses** — `FindActorByName` returns after searching the Play-In-Editor world instead of falling through to `UEditorActorSubsystem`, which refuses every call during PIE; its logged refusal made `McpAppendPieRefusalHint` tell the caller to stop play for a miss the PIE search had already settled.
- **`inspect_graph` nodeGuid** — `GetNodeDetails` / `GetPinDetails` (`BlueprintGraphHandlersDetails.cpp`) read `nodeGuid` as well as `nodeId` through `PickFirstNonEmpty`, and report the found node's full `NodeGuid` as `nodeId` instead of echoing the lookup text.
- **Ignored motion inputs** — `McpIgnoredInputsWarning` (`ControlActorMotionSample.cpp`) adds a `warnings` entry and a message suffix when `inputs` were pressed, the watched actor is a player-controlled pawn and its sampled extent stayed under 1 unit.
- **Graph filter spacing** — `get_graph_details` (`BlueprintGraphHandlersQueries.cpp`) strips spaces from the filter and the node title before matching.
- **Package fallback** — `ResolveObjectFromPath` returns the loaded `UPackage` only when the requested path is the package path; a missing object inside it falls through to `FindObject` and then to not-found.
- **Transient writes** — `set_object_property` on an object in the transient package (a running game's GameInstance, say) reports that nothing is saved and the change lasts until PIE stops, instead of "engine content is not saved".
- **Undo/redo** (`ControlEditorTransactions.cpp`) call `GEditor->UndoTransaction()` / `RedoTransaction()` after `UTransactor::CanUndo` / `CanRedo` instead of `GEditor->Exec("Undo")`, which is not an editor command, and reply with the transaction's title.
- **`get_material_info`** on a `UMaterialInstance` reports `parent`, `baseMaterial` and `parameterOverrides` instead of `ASSET_NOT_FOUND`.
- **Blueprint `get`** resolves `Component.Property` against the SCS node's `ComponentTemplate` (via `ResolveNestedPropertyPath`) when no variable or CDO property matches.
- **Deletes** (`ControlActorLifecycle.cpp`) collect their targets first and destroy them inside one `FMcpScopedEditorTransaction`, so one editor undo restores a whole `delete` or `delete_by_tag` call; the reply carries its `undo` block.
- **Unattended saves** — `McpSafeAssetSave` and `McpSafeLevelSave` save with `GIsRunningUnattendedScript` set, so a failed save logs instead of opening a modal on the game thread, and `FMcpDeferAssetSaves` holds each `build_graph` step's own save until the batch saves once.
- **Read-only pin literals** — `SetPinDefaultValue` hands a `bDefaultValueIsIgnored` pin to `FeedReadOnlyPinLiteral` (`PinMutations/McpAutomationBridge_BlueprintGraphPinLiteralNode.cpp`), which creates the `UKismetSystemLibrary::MakeLiteral*` node for the pin's category, wires it, and reuses one that already feeds only that pin. Structs, enum bytes and containers keep `PIN_REQUIRES_CONNECTION`.
- **`build_graph` step rollback** — a `create_node` step whose `pinDefaults` fail is removed with `RemoveNodeWithLiterals`, its result drops the `pinDefaults` that went with it, and its alias is registered only once the step succeeds.
- **No compile on reads** — the graph dispatcher sets `bDeferCompile` before `HandleNodeQueryAction` / `HandleNodeDetailAction`, so `FActionContext::SendResponse` no longer compiles a Blueprint a read found dirty; the compile reset the editor's undo buffer.
- **Folded pin conflict** — `McpApplyFoldedPins` reports the conflicting selector and its pinned value, and `McpNativeGatewayValidation` answers with them plus an `execute` `nextCall` on the primary action carrying the caller's params (mirror of `gateway-execute-static-check.ts`).
- **`read_log` alternatives** — `FMcpLogHistory::Read` and `ReadFileTail` split `filter` on `|` and match any alternative.
- **Inherited component defaults in `get`** — `Component.Property` falls back from the SCS node to `CDO->GetDefaultSubobjectByName` and then to the `FObjectProperty` holding the component.
- **Root outliner folder** — `McpActorFolder` reads `NAME_None` as `""` for `McpActorMatchesListFilters` and the list summary, which had spelled it `None`; the inspect actor query's `folderPath` does the same.
- **PIE-safe Blueprint lookup** — `McpAssetExists` (`BlueprintPaths.h`) reads the asset registry by package name. `FindBlueprintNormalizedPath`, `blueprint_exists`, `ensure_exists` and `probe_handle` use it in place of `UEditorAssetLibrary::DoesAssetExist`, which fails and logs an error whenever `GEditor->PlayWorld` is set, and `LoadBlueprintAsset` drops its `DoesAssetExist` step. `probe_handle` takes `assetClass` from that entry; its object-path lookup, given a package path, never matched.
- **Graph lookup misses** — `DescribeMissingGraph` (`BlueprintGraphHandlersContextEditor.cpp`) names the event graph holding an event of that name, else lists the Blueprint's graphs.
- **Native search action bonus** — `McpSearchScoreRecord` adds `McpSearchActionCoveredBonus` (50) when every query word matched and the query names every word of the record's own action (two or more words), mirrored in the TS reference `native-discovery-search.ts`.
- **Booleans under secret-named keys** — `MaskSecretsDeepInternal` (`McpNativeReceiptRedaction.cpp`) no longer masks an `EJson::Boolean` value, matching `maskSecretsDeep` in `receipt-redaction.ts`; any other value under a key `McpIsSecretKey` flags is still masked.
- **Search synonyms and alias runs** — `McpSearchWords` folds remove/destroy/erase to delete (`FoldSynonym`), and the action-covered bonus also fires for an alias whose words form a contiguous run of the query (`SpacedRun`), mirrored in `native-discovery-search.ts`.
- **Save after the reply's compile** — `FActionContext::SendResponse` calls `SaveLoadedAssetThrottled` after `McpCompileBlueprintWithDiagnostics` and reports `saved`; the handlers' own save ran before that compile, which dirties the package again.
- **Blueprint `memberClass` names** — `ResolveGraphCallFunction` falls back to `ResolveTargetClassFromString` when `ResolveUClass` rejects a Blueprint class name (`BP_X_C`) or asset path (`/Game/.../BP_X`); `DescribeMissingFunction` reports an unresolved `memberClass` for create_node and the build_graph pre-check alike.
- **Custom node output pins** — `ApplyCustomAdditionalOutputs` (`AddCustomExpression.cpp`) parses `additionalOutputs` for add and update and rebuilds `Outputs` and `bShowOutputNameOnPin` as `RebuildOutputs` does, which is exported only from 5.7.
- **Undeclared-parameter hint** — `DescribeUndeclaredParameter` skips `action` and `subAction` (case-sensitive), matching `describeUndeclaredParameter` in `gateway-schema-validate.ts`.
- **Variable node spellings** — `ParseVariableNodeType` accepts `VariableGet`, `GetVariable` and `K2Node_VariableGet` (and the Set forms) in any case for `TryCreateVariableNode` and the `build_graph` pre-check; `create_node` answered `NODE_TYPE_NOT_FOUND` for the `GetVariable` that `add_node` took.

## [0.6.0-beta-b] - 2026-09-25

Plugin-side changes since the `v0.6.0-beta-a` tag, from the code diff. The server-side view is in the root `CHANGELOG.md`.

### Added
- **In-process batches** — `FMcpResponseCaptureRegistry` (`Core/Requests/`) lets a handler run other handlers under a synthetic request id and read their replies as data: `SendAutomationResponse` checks it first and never delivers a captured reply. `build_graph` (`Domains/BlueprintGraph/...Batch*.cpp`), `build_material_graph` (`MaterialAuthoringGraphBatch.cpp`), `build_metasound`, `spawn_batch` (`ControlActorSpawnBatch.cpp`) and the list forms of `set_transform`, `set_blueprint_variables`, `set_material`, `add_tag`, `delete_by_tag`, `remove_scs_component` and `set_variable_metadata` are built on it, so every item keeps its single-call checks. A `build_graph` batch resolves every function and variable it names before running any step (`ResolveGraphCallFunction` is shared with node creation).
- **`FMcpLogHistory`** (`Domains/Log/`) keeps the recent editor log in memory from subsystem start; `read_log` serves it, or the UnrealBuildTool log, the Live Coding console log, or an earlier run's backup log (`runsBack`), and every returned line passes the response sanitizer.
- **`launch_build`** (`SystemControlHandlersLaunchBuild.cpp`) starts the packaged Win64 game from inside the project for a timed smoke run on an `FTSTicker`, and `package_status` reports its maps, errors and log tail.
- **PIE game control** — Enhanced Input injection with holds in game seconds, `key_tap` held for at least two frames, `widget_list` / `widget_click` over the live UMG tree (`ControlEditorWidgetInput.cpp`), `set_fixed_delta_time` through `FApp` (reset on `EndPIE`), multi-frame `step_frame`, `restore_editor_window` (restores the root window through its placement without activating it and turns off the background throttle), and `restart_editor`.
- **MetaHuman Creator** handlers (`Domains/MetaHuman/`), Sequencer `list_track_keys` / `remove_keyframe`, `skin_mesh_to_skeleton` (GeometryScripting, 5.5+), IK Rig and retarget pipeline construction (`AnimationRetargetPipeline.h`, IK Rig 5.6+, batch retarget 5.8), transition-rule conditions and `delete_transition`, and Fab driven by reflection (`McpFabDirectApi.cpp`, browser widget reuse, listing claims).
- **`McpSafeOperations::McpDeleteAssetAndFile`** (`Safety/McpSafeOperationsAssetDelete.h`) finishes a delete the engine left half done: it unloads the leftover package and removes the `.uasset` only once the package is really gone, and is true only when the file is.

### Changed
- **Native requests** are timed from their last progress: a live game thread sends a "still working" progress every 20 s, `MaxLifetimeSeconds` still ends a request that never answers, and progress never goes backwards (`MCP/Transport/`).
- **Native gateway** — an omitted fold selector is inferred from the parameters sent (`McpInferFoldSelector`, mirroring `inferSelector`), native `search` honours `effect` and `tool`, batch items take `{x,y,z}` vectors like the single form, `RESULT_TOO_LARGE` names the capability's own narrowing parameters, and a world edit during PIE gets a receipt warning.
- **Replies that report what happened** — dozens of handlers read their result back before answering, including the GAS mutations (#606: compile, verify on the compiled class, then save), SCS template edits that now reach placed instances, material and MetaSound parameter writes, asset delete and duplicate, `set_property`, `set_project_setting` and `set_preferences`.
- The GAS verification handlers live in `Domains/GAS/Authoring/` (the GAS folder is at the 25-file limit), and the AI domain creates its assets through one `CreateAIAssetInPackage`.
- The integer `Version` in `McpAutomationBridge.uplugin` follows the semver (`600` for 0.6.0).

### Removed
- `add_control`, `add_rig_unit` and `connect_rig_elements` (unpublished, `NOT_SUPPORTED` on every path), the always-true `SubAction` wrappers, the response send "retry" loop that never retried, and the unused `bAllowLoopbackMediaUrls` / `AllowedLoopbackMediaUrlPrefix` settings.

### Security
- Console commands whose first token is `debug`, `exec` or one of the engine crash, stall and hitch commands are refused (`DANGEROUS_ENGINE_COMMAND`) by the generated console policy.
- Identity keys (`UserId`, `AccountId`, `LoginId`) are redacted from log lines and errors; `console_command` output, `read_log` lines and the `launch_build` log tail pass the per-line sanitizer.
- The path canonicalizer admits plugin content roots and still refuses host filesystem roots; `launch_build` only runs a game inside the project; batch steps cannot leave the batch's asset, and actor list forms stop at 500 items.

## [0.6.0-beta-a] - 2026-09-18

### Added
- **Content ingestion — `manage_asset.list_content_sources` and `manage_asset.migrate_assets`.** Reusable content already on the machine was unreachable through the gateway: `asset.import` runs source-file importers and refuses any path outside the project directory, so an installed engine template or a Quixel Bridge / Fab pack could not be brought in at all. Bridge and Fab deliver cooked `.uasset` packs rather than source art, so ingestion is a package copy plus an asset-registry scan — the same operation that pulls in a template, which is why these are generic rather than Megascans-specific. `list_content_sources` enumerates engine templates, engine/plugin content, and downloaded Bridge packs; `migrate_assets` copies a tree into `/Game` with `dryRun` preview and a `maxPackages` cap. A migrate request never carries a filesystem path — it carries a root token (`engineTemplates`, `engineFeaturePacks`, `engineContent`, `enginePlugins`, `megascansLibrary`, `projectContent`, `projectPlugins`) plus a relative id, both resolved in `McpAutomationBridge_AssetWorkflowContentSourceRoots.h`, so no directory outside those roots is reachable. `destinationPath` defaults to `/Game` because reproducing the source layout is the only arrangement that keeps the `/Game/...` references stored inside the copied packages resolvable; anything deeper reports `referenceIntegrity: "at-risk"`.
- **Plugin management — `system_control.list_plugins`, `enable_plugin`, `disable_plugin`.** Migrated content usually depends on a plugin that is installed but disabled (ChaosVehicles for the advanced vehicle template), and without this the assets copy in and then fail to load their classes, which reads as a broken migration rather than a disabled module. Writes go through `IProjectManager::SetPluginEnabled` + `SaveCurrentProjectToDisk`; the response reports `restartRequired: true` because modules and content mount only at startup.
- **Native gateway surface** — the plugin's `/mcp` transport now exposes a single `unreal` tool with `search`, `describe`, `execute`, and `configure` operations, implemented by new `Private/MCP/{Gateway,Routing,Execute,Primitives,Resources,DynamicTools}/` modules.
- **Generated contract shards** — `Private/MCP/Generated/` and `Private/MCP/Tools/McpGeneratedParentRegistry*` are emitted from the TypeScript capability records by `npm run registry:generate`. They are committed but must never be hand-edited.
- **Native MCP protocol primitives** — `Private/MCP/Primitives/` adds the MCP Tasks surface (`McpTaskMethods`, `McpTaskStore`) for `2025-11-25`, a subscription store with a notification coalescer so bursts of editor changes collapse into one client notification, resource revision stamps, completion pools and provider, prompt catalog/render/argument validation, a client profile store and session capability profile, and an elicitation decision policy. The elicitation policy is metadata/decision only — no transport wiring, no server-initiated RPC, no new MCP method — and never marks a secret, token, credential, or destructive-confirmation value as safe to elicit. Cross-transport parity with the TypeScript primitives is audited by `npm run primitives:check`.
- **Protocol negotiation** — `McpSupportedProtocolVersions` accepts `2025-11-25` (latest), `2025-06-18`, and `2025-03-26`. `initialize` echoes the highest mutually supported version, or the latest for an unknown well-formed request; `McpDefaultProtocolVersion()` (`2025-03-26`) backs post-initialize requests that omit the `MCP-Protocol-Version` header.
- **Capability authorization primitives** — `Private/Foundation/McpCapabilityPrincipal`, `McpCapabilityAuthorization`, and `McpCapabilityPathScan` give the plugin its own authorization identity instead of trusting the caller's claim.
- **Idempotency ledger and compensation receipts** — `Private/Foundation/McpIdempotencyLedger` (cap 4096, mirroring the TypeScript ledger's cap of 1024) and `McpCompensationReceipt`.
- **`MCP_NATIVE_PORT` environment variable** — overrides the native MCP HTTP/SSE port (`NativeMCPPort`) at startup without editing committed ini, so a project can run several editors at once on distinct ports. Mirrors the existing `MCP_MAX_*` env overrides; falls back to the `Native MCP Port` project setting when unset or invalid.
- **`IKRigEditor` optional module** — declared for the `create_ik_rig` path so IK Rig creation does not require a hard dependency on the editor module.
- **Component-bound events in `add_event`** — pass `componentName` plus `eventName` (the delegate name) to wire a component's multicast delegate to a `UK2Node_ComponentBoundEvent` with `ComponentPropertyName`, `DelegatePropertyName`, and `DelegateOwnerClass` set. Idempotent on repeat calls; guarded by `MCP_HAS_K2NODE_COMPONENTBOUNDEVENT`.
- **Typed class pins in `create_node` / `add_node`** — dedicated branches assign `UK2Node_DynamicCast::TargetType` and `UK2Node_CreateWidget::WidgetType` from a `targetClass` payload, so casts and CreateWidget nodes come back typed instead of as wildcard/"Bad cast" nodes. Shared `ResolveTargetClassFromString` / `ReadTargetClassPayload` helpers accept the same input forms and legacy field fallbacks across every branch with a class pin.
- **18 widget-authoring actions added to the native `WidgetAuthoring()` routing array** — `add_quest_tracker`, `add_safe_zone`, `add_spacer`, `add_widget_component`, `add_widget_switcher`, `bind_localized_text`, `create_credits_screen`, `create_shop_ui`, `create_widget_style`, `delete_animation`, `get_widget_slot_info`, `remove_widget`, `rename_widget`, `reparent_widget`, `set_font`, `set_localization_key`, `set_margin`, `set_widget_binding`. The handlers existed but were unreachable because the action names were absent from the routing array; the TypeScript `WIDGET_AUTHORING_ACTIONS` set gained the same 18.
- Native cinematics, Movie Render Queue, media, Take Recorder, and replay automation with direct `/mcp`, WebSocket, and live-editor verification coverage.
- **Folded capability families on the native surface** — the native registry serves the same 377 folded records as the TypeScript surface: `McpNativeGatewayFolding` applies the folded pins before defaults and schema validation and resolves the dispatch action after them, `FindByParentAction` falls back to any legacy pair so every former `{tool, action}` name still resolves, and the native completion pool completes the old names. `describe` advertises each family once, with its selector.
- **Pre-queue capability gate** (`Core/Security/McpPrequeueGate`) — every request is authorized before it reaches the editor queue. The gate resolves its demand with the same `McpHandlerUtils::NormalizeAction` the dispatchers call, so the gate and the dispatcher cannot disagree about which capability a request needs.
- **Scoped capability tokens** — a scoped token carries its profile, scopes, allowed path prefixes, allowed projects and per-minute request/tool-call quotas, and the `bridge_ack` authority block reports the effective identity.
- **Principal-keyed quota ledger** (`McpPrincipalQuota`) — bounded to 256 tracked identities with least-recently-seen eviction, charged only after every other refusal, answering `QUOTA_EXCEEDED` as retryable.
- **Single-use consent nonces** — a consent grant is consumed by the request that used it; a replayed grant answers `CONSENT_REUSED`.
- **Typed refusals for stalls, state and addressability** — `EDITOR_BLOCKED` (game-thread stall over 15 s), `EDITOR_STATE_MISMATCH`, `STALE_STATE`, the `UNDO_UNAVAILABLE_*` family, and `OBJECT_NOT_ADDRESSABLE` from `McpSafeReflectionTarget`, which denies a reflected write to a target the principal cannot address so a `write`-scoped principal cannot reach `bRequireCapabilityToken` through `inspect.set_property`.
- **Bounded native sessions** — at most 16 active sessions with a 120 s idle reclaim, and a 32-connection ceiling that answers 503 beyond it.
- **User-defined action aliases** — a project can add action names in `handler-aliases.json` (schema `Resources/MCP/custom-handler-aliases.schema.json`, tests `Tests/McpCustomHandlerAliasTests.cpp`): version 1, at most 128 aliases and 64 KB, `lower_snake_case` only, resolved through three search paths; an alias pointing at another alias or at the protected `inspect`/`manage_tools` actions is rejected, and an alias whose target is not registered yet stays pending until it is.
- **Promoted skeleton routes** — the fifteen previously hidden skeleton routes (twelve promoted to canonical records, the three `delete_*` spellings still hidden) now carry explicit names on the native `Skeleton()` routing array.
- **Word-level native search with paging** — the native gateway search matcher scores word-level matches and accepts `actionOffset` and `maxBytes`, so a large action list can be paged.
- **Fab adapter module** — `McpAutomationBridgeFab` bridges the Fab asset store through the Fab plugin's own browser widget and download API (`McpFabBrowserBridge`, `McpFabSearchOperation`, `McpFabDetailsOperation`, `McpFabAddToProject`, `McpFabImportWatcher`). Its Fab and Megascans engine dependencies are declared optional and listed in `PublicDelayLoadDLLs` on Win64, so the module compiles away when they are absent; the store actions themselves are dispatched by `Domains/AssetWorkflow/`.
- **Diagnostics snapshot store** — `Foundation/Diagnostics/` records request admission, refusal, terminal, handshake and session events to `<Project>/Saved/MCP/diagnostics/`, with atomic temp+rename writes, previous-session rotation, and corrupt-file tolerance.

### Changed
- **Second dedup pass (2026-09-06)** — Navigation and Spline read payloads through the shared `GetJson*Field` / `ExtractVectorField` accessors (their private copies are gone), the material domain / blend mode / shading model chains in create_material and the three set_* handlers share `ParseMaterialDomain` / `ParseBlendMode` / `ParseShadingModel`, material and material-function info share `AppendMaterialFunctionIO`, `SetMainMaterialInputExpression` reuses `GetMainMaterialInput` (and now accepts WorldPositionOffset), Sequence track lookups share `FindTrackByName`, the container Property handlers share `McpPropertyReflection::AssignPrimitiveFromJson`, WidgetAuthoring animation and widget lookups share `FindWidgetAnimation` / `FindWidgetByName`, the thin `FMcpAutomationBridge_*` pin wrappers call `McpBlueprintUtils` directly, `blueprint.get` merges functions and events through one lambda, and the Niagara graph handler parses `scriptType` once.
- **Shared helpers replace copy-pasted lookups (2026-09-06)** — 56 Geometry handlers resolve their dynamic mesh through `ResolveDynamicMeshForGeometry` (null-world safe), MaterialAuthoring pin chains go through `ForEachMainMaterialInput` / `GetMainMaterialInput`, Sequence binding names through `GetBindingName`, Skeleton loads through `LoadSkeletonOrMeshSkeleton`, Spline handlers through one `FindSplineMeshComponent` / `ParseSplineMeshAxis`, SCS handlers through `FindSCSNodeByVariableName` / `FindSCSParentNode` / `IsSCSRootAlias`, Level handlers dropped their unused `HandleExecuteEditorFunction`-family macro pairs, and `manage_tools.get_status` reports the plugin descriptor version and generated registry counts instead of hardcoded values.
- **`Private/` reorganized into per-domain modules** — `Core/` (errors, requests, security, subsystem), `Domains/` (66 domain directories), `Foundation/` (blueprint, bridge helpers, handler utils, capability authorization, idempotency, compensation), `MCP/`, `Safety/`, and `Transport/`. The per-tool `McpTool_*.cpp` definitions, `McpDynamicToolManager.cpp`, `McpConsolidatedActionRouting.h`, and the `McpNativeTransport.{h,cpp}` monolith are gone, replaced by generated registries and `Private/MCP/Transport/`.
- **`Private/Safety/` split into per-operation headers** — asset save, level save, map load, folder delete (assets/verify), animation delete, delete quiesce/compilation, world delete, package tools, material, and classification each have their own header; `McpSafeOperations.h` survives only as a short umbrella that includes them.
- **`control_actor` spawn is now transactional** — a requested `meshPath` that can't be applied no longer leaves a misconfigured actor in the level: it fails `MESH_NOT_FOUND` before spawning if the mesh can't load, or rolls back (`Destroy()` + `MESH_APPLY_FAILED`) if a resolved mesh can't be applied to the spawned actor.
  - **Potentially breaking:** a request that passed a `meshPath` which failed to resolve previously still produced a spawned actor and a success response; it now returns a `MESH_NOT_FOUND` error and spawns nothing.
- Stripped redundant section and line comments across **254 files** (237 `.cpp`, 3 `.h`, 14 `.ts`), removing roughly 1,640 lines of banner and restating comments from the C++ domain handlers, the TypeScript handlers, and the capability records. Comments carrying information the code does not — including the `docs/Roadmap.md` section cross-references — were kept in condensed form.
- **Build configuration** — `PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs` replaces the `NoPCHs` setting, which made every unity blob re-parse the engine headers from scratch, with `PrivatePCHHeaderFile = "Private/Core/Module/McpAutomationBridgePCH.h"`; adaptive unity is disabled; `HTTP` and `GraphEditor` are dependencies; engine deprecation warnings are suppressed for this plugin's sources unless `MCP_STRICT_DEPRECATIONS=1` restores them for auditing (the opt-in packaging job sets it); `GameplayAbilities` and `SmartObjects` are required plugin dependencies rather than optional ones; and the `.uplugin` adds the `McpAutomationBridgeFab` module plus optional `MovieRenderPipeline`, `MoviePipelineMaskRenderPass`, `Takes`, `ElectraPlayer`, `Fab`, `Bridge`, `RigVM` and `GeometryProcessing` plugins.
- **`server-info.json` instructions** now describe the capability-first flow (377 capabilities covering 1,500+ editor actions) instead of the previous count.

### Removed
- **Unreachable and dead handlers (2026-09-06)** — `Blueprint/Graph/...SetDefaultObject.cpp` and the four `Blueprint/Components/Scs{SetTransform,RemoveComponent,ReparentComponent,SetProperty}.cpp` routes were shadowed by earlier routes; `Interaction/...RuntimeActors.cpp` and `...RuntimeComponents.cpp` plus their six subsystem declarations duplicated the namespace handlers that already claim those actions; `Effect/...NiagaraModuleRouting.cpp` held five `return false` stubs; `Performance/...ActorMergeSave.cpp`, `LevelStructure/...Actions.cpp` (log category moved next to its handlers), the empty `FoliageHandlers.cpp`, `NiagaraHandlers.cpp` and `PropertyHandlers.cpp` translation units, `Foundation/Render/McpRenderStateRefresh.{h,cpp}` (inlined at its only call site), `HandleSequenceGetMetadata`, `EnsureSequenceEntry` / `GSequenceRegistry`, `CopyExternalPackageDirectory`, `FindExpressionByPayload`, `LOAD_MATERIAL_OR_RETURN`-adjacent dead macros, the per-domain `GetStringField`/`GetNumberField`/`GetBoolField` copies in Misc, Networking and Texture (now the shared `GetJson*Field` accessors), `GetIntFieldSkel`, `SanitizeForLogConnMgr`, the three file-local `PersistSnapshotAsync` helpers (now `FMcpDiagnosticsSnapshot::PersistCurrentAsync`), nine dead consolidated routing sub-lists, and roughly a hundred orphan section banners and nested `#if WITH_EDITOR` arms across the domain handlers.
- **`Private/MCP/Gateway/McpNativeGatewayManifest.h`** (generated, 298 KB) is no longer emitted or committed. No translation unit has included it since the native gateway started serving describe from the generated parent registry, so it was dead weight in every build and every diff.
- **`Domains/EditorFunction/McpAutomationBridge_EditorFunctionHandlersActorComponents.cpp`** — its only function, `HandleActorComponentFunction` (`LIST_ACTOR_COMPONENTS`), was never reached by `HandleExecuteEditorFunction`.
- **Enable Native Gateway (`bEnableNativeGateway`) project setting** — the native transport permanently exposes only the `unreal` tool; there is no opt-out and no legacy 23-tool listing. A direct `tools/call` on a canonical parent name returns a bounded, executable `DIRECT_TOOL_CALL_REMOVED` receipt whose `nextCall` re-runs the request through the gateway.
- **The local-only progress log** — `docs/native-automation-progress.md` was a scratch log rather than published documentation and is dropped from the tree.

### Security
- Scopes are exact-set membership with an `Admin` wildcard, not rank-based — `Write` does not imply `Read`, and an unresolvable capability demands `Admin`.
- Consent arrives as an `automation_request` envelope sibling and is re-validated plugin-side; it is never inferred from loopback, a prior call, idempotency, or preview.
- Capability tokens compare in constant time and are never logged; the plugin re-enforces every check the TypeScript layer performs.
- Added continuous local output-path validation, disabled network-backed media URLs because redirect destinations cannot be pinned, added client-scoped native rate limits that survive session rotation, enforced strict native `manage_tools` argument validation, and sanitized streamed log payloads.
- **`action` and `subAction` can no longer disagree.** `AuthorizeAutomationRequest` normalizes any `automation_request` payload that declares both with different values (overwriting `action` from the authoritative `subAction`), and the native execute stage stamps `subAction` from the server-resolved action, so a client can neither lower its own scope nor raise what runs past what was authorized.
- **Capability-token auth is on by default** (`bRequireCapabilityToken`), the token is generated per install at `<ProjectRoot>/Saved/MCP/capability-token`, and both transports compare it with `McpConstantTimeTokenEquals` (`Foundation/McpSecureTokenCompare.h`) so comparison time never leaks how much of a token matched.
- **URL-looking arguments are refused** in handler validation, including loopback and `file:` URLs, rather than trying to allow a safe subset.
- **Scoped tokens are narrower than the legacy token by construction** — a scoped token may list only `Read`/`Write`/`Destructive` (never `Admin`), and a scoped token colliding with the legacy token wins because the narrower grant applies.

### Fixed
- **save_level_as on the unsaved Open World template** now answers `UNSAVED_TEMPLATE_LEVEL` instead of attempting a save that fails on private template references and asserts inside the world partition subsystem on the next frame (editor crash). `create_spline_actor` honours the declared `name` parameter (it only read the undeclared `actorName`).
- **Behaviour fixes found during the sweep (2026-09-06)** — `modify_scs` `add_component` / `modify_component` now reach the property-applying implementation (previously dead-wired); `set_axis_settings` writes the blend-space `BlendParameters`; `advance_simulation` advances `steps` ticks once instead of `steps²`; `simplify_mesh` no longer divides by zero on an empty mesh; `add_landscape_layer` honours `save`; `save_level_as` no longer requires `ULevelEditorSubsystem`; `create_light`, unknown World Partition actions, session-settings refusals and the audio playback no-editor path all answer with a receipt instead of falling through; `create_switch_actor` etc. keep their single namespace implementation.
- **Source compatibility restored across the supported UE 5.0–5.8 range.** Several engine APIs and relocated headers were used without guards, and several existing guards named the wrong engine boundary, so the plugin failed to compile on parts of the range it advertises. Header selection now probes with `__has_include` instead of hard-coded version numbers wherever the engine moved a header, and the remaining guards were corrected against the engine source. Affected areas: the StructUtils headers (`MCP_USER_DEFINED_STRUCT_HEADER`, `MCP_INSTANCED_STRUCT_HEADER`), `FAssetCompilingManager::FinishCompilationForObjects`, `UWidgetBlueprint::WidgetVariableNameToGuidMap`, `CreateNewIKRigAsset`, `FString::RightChopInline`/`LeftInline`, and `PhysicsEngine/SkeletalBodySetup.h`. A redundant `UObject/StrProperty.h` include was dropped (`FStrProperty` comes from the already-included `UObject/UnrealType.h`).
- **Render console handler** — use `FJsonObject::HasField()` instead of `Values.Contains(FString)`, following the `FJsonObject::Values` key-type change.
- **Asset soft-path fallback returned the wrong string shape** — `MCP_ASSET_DATA_GET_SOFT_PATH` used `PackageName` (`/Game/Foo`) where callers expected an object path (`/Game/Foo.Foo`). It now uses `FAssetData::ObjectPath`, the equivalent of the `GetSoftObjectPath()` used in the other branch.
- **`validate_niagara_system` now reports real errors** — previously hard-coded `isValid=true`; it now builds a full Niagara system view model and harvests stack issues (e.g. "The module has unmet dependencies.") across the system and emitter stacks. A data-processing-only view model can't be used because `UNiagaraStackModuleItem::RefreshIssues()` emits no per-module issues in that mode.
- **IK Rigs created on the `NewObject` fallback path are registered with the asset registry** — `FAssetRegistryModule::AssetCreated()` is now called explicitly on that branch, which the static factory does for us on engines that have it. Without it the rig existed on disk but was unregistered, so it never appeared in the Content Browser until an unrelated rescan happened to pick it up: the asset looked lost even though creation had reported success.
- **Widget GUID registration logs a truthful no-op** — `RegisterWidgetGuid`, `UnregisterWidgetGuid`, and `RegisterAnimationGuid` each logged "registered"/"unregistered" on engine versions that have no `WidgetVariableNameToGuidMap`, claiming work they had not done. On those versions the engine owns the widget variable's GUID in `UBlueprint::NewVariables[].VarGuid`, and writing our own would overwrite a value existing bindings resolve through — so a no-op is correct, it just has to say so. `RegisterAnimationGuid` still adds the animation to `WidgetBP->Animations`, which is the part that matters there.
- **Bare `remove_variable` / `rename_variable` now match on the native transport** — the Blueprint variable removal/rename handler matched only the `blueprint_`-prefixed forms, so the bare action names fell through unhandled. Both the snake_case (`remove_variable`, `rename_variable`) and alphanumeric-lowered (`removevariable`, `renamevariable`) bare forms are now accepted alongside the prefixed ones.
- **Every texture call was failing** — `action` is injected by the consolidated routing layer (`WithPayloadSubAction`) as the legacy dispatch verb, but it is not a client parameter and was absent from the handlers' `ValidParams` allowlists, so schema-valid texture calls were rejected with `TEXTURE_ERROR: Invalid parameter: action`. Added to all five affected handlers (gradient, noise, normal, pattern, resize).
- **`add_variable` now applies `defaultValue`** — the handler read the payload field but never assigned it, so every variable was created with a zero/empty default. The parsed default is written to `FBPVariableDescription::DefaultValue` with type-aware formatting (booleans lowercased, integer/byte categories as whole numbers, floats via `SanitizeFloat`, strings and struct literals passed through).
- **`ListenPorts` drop warning** — when multi-listen is on and a partial `ListenPorts` override omits a default bridge port (8090/8091), a warning is logged instead of the drop being silent (the user's ports stay authoritative).
- **Clean build fixed** — the memreport scan passed `256` as a seventh argument to `IFileManager::FindFilesRecursive`, but that parameter is `bClearFileNames`, not a result ceiling, so the call did not compile. The bound was dropped rather than reworked: truncating is also wrong here, since picking the newest of an arbitrary subset can miss the actual newest report. A real traversal bound would need `IterateDirectoryStatRecursively`, which can stop early and read `ModificationTime` in the same pass.
- **Last source warning cleared** — `FLinearColor ColorValue;` left its channels uninitialized in the cinematics material-parameter track handler, and the only writer runs on one branch, so the compiler could not correlate the write with the guarded use and warned C4701. Seeded to opaque black, matching `ReadLinearColor`'s own defaults.
- Wait for actual replay seek completion, keep render ownership until executor settlement, roll back Take Recorder panel/source state after asynchronous failures, reject invalid render limits before queue mutation, and verify tokenized render filenames.
- **Compatibility sweep beyond the earlier fix** — the `MCP_HAS_IKRETARGETER_SET_IKRIG_ENUM` boundary was reversed because the minor it named sat on the wrong side, and three shims were added for APIs that differ across the supported range (`MCP_SET_ENUMS`, `MCP_HAS_GET_OBJECTS_FLAGS`/`MCP_GET_OBJECTS_NO_NESTED`, `MCP_DISALLOW_SHRINKING`). Fourteen files move off `FJsonObject::Values` lookups with an `FString` key onto `HasField()` for the UE 5.8 key-type change, the ambiguous `TEXT("ReadOnly")` comparison is qualified, the diagnostics filename no longer trips C2084, `bCompileForEdit` (a member added in 5.6, absent on 5.5) is guarded in the shared Niagara stack-issue collector, and the Fab calls follow the engine's current API surface.
- **Domain correctness fixes** — a property that cannot be coerced answers `PROPERTY_CONVERSION_FAILED` with `partial: true` for the fields that did apply, an unknown World Partition action answers `UNKNOWN_ACTION` instead of falling through, and the pipeline status report states what it measured rather than a hardcoded value.

### Verification
- **Supports Unreal Engine 5.0–5.8.** The range is a source-compatibility target: per-version build and live-editor results are not asserted here. See `docs/performance-and-evidence.md` for the engine matrix and what each version's record actually shows.

### Migration
- The internal `manage_post_process` C++ action has been folded into the expanded `manage_render` action (the `Render/McpAutomationBridge_RenderPostProcess*.cpp` files now dispatch through `manage_render`). Any client that called `manage_post_process` directly will now fail with `does not match prefix` — switch to `manage_render` and pass the desired sub-action via `subAction`. The reflection-capture resolution setter was renamed from `configure_capture_resolution` to `configure_reflection_capture_resolution`; the scene-capture path keeps the original `configure_capture_resolution` name. The `McpAutomationBridge_RenderHandlers.cpp` monolith is now a 74-line dispatcher; per-concern handlers live under `Render/McpAutomationBridge_Render*.cpp`.

## [0.5.30] - 2026-06-05

### Security
- **Capability token enforcement** on native MCP transport — validates `X-MCP-Capability-Token` header when `bRequireCapabilityToken` is enabled (mirrors WebSocket bridge logic)
- **Symlink escape prevention** in `execute_python` file path validation — resolves symlinks and re-validates against project directory
- **Code size limit** in `execute_python` — enforces 1 MB maximum for inline code payloads
- **Explicit request origin tracking** (`ERequestOrigin`) — routes HTTP vs WebSocket responses by explicit origin instead of inferring from `TargetSocket==nullptr`
- **Tool registry thread safety** — `Register()` now holds `CacheMutex` for entire body, `GetAllTools()` returns copy to prevent external mutation
- **Dynamic tool manager protection** — `EnableCategory("all")` now respects protected categories and initial state instead of blindly enabling everything

### Added — Native MCP Streamable HTTP Transport
- **Native MCP endpoint** (`POST /mcp`) directly inside the C++ plugin — AI clients connect without the TypeScript bridge
- **SSE streaming** for `tools/call` — progress notifications arrive in real-time, followed by final JSON-RPC result
- **Raw socket HTTP server** (`FRunnable` + `FSocket`) replacing `FHttpServerModule` — no external dependencies
- **JSON-RPC 2.0** protocol (MCP 2025-03-26) with `initialize`, `tools/list`, `tools/call` methods
- **Multiple concurrent sessions** — Cursor, Claude Code, and other clients can connect simultaneously
- **Session management** with `Mcp-Session-Id` header, 1-hour inactivity timeout, `DELETE /mcp` termination
- **Dynamic tool manager** — enable/disable tools and categories at runtime via `manage_tools`
- **Native tool schemas** generated from self-describing C++ tool classes with full `inputSchema` and categories (core, world, authoring, gameplay, utility); the TypeScript bridge exposes 23 canonical parent MCP tools.
- **`listChanged` notifications** — broadcast `notifications/tools/list_changed` to all active SSE connections when tool state changes
- **Load All Tools on Start** project setting — toggle between the core set and all available native tool schemas at startup
- **Status bar indicator** — `● MCP :3000 (2)` in UE editor status bar, click to open settings
- **Server identity config** — `server-info.json` for name/version/instructions, plus `NativeMCPInstructions` project setting for custom instructions
- **Client info logging** — log connecting client name and version from `initialize` request
- **`execute_python` action** in `system_control` — execute Python code with stdout/stderr capture, supports inline `code` and `file` path, execution time tracking
- **Shared `ListenHost` setting** — native MCP respects `AllowNonLoopback` for network access control
- **Plugin-packaging scripts** for Win/Mac/Linux — build and package the plugin via RunUAT BuildPlugin, with smart arg parsing
- **Expanded environment systems coverage** — heightmap import/export, landscape layer info/material/splines/LOD/streaming proxies, foliage type configuration/paint/remove flows, sky/volumetric-cloud/weather/wind/time-of-day setup, water bodies, water waves/material/collision, and buoyancy components

### Changed
- Plugin descriptor metadata updated to `0.5.30` to match the server/source release version.
- Tool categories now use four groups: `core`, `world`, `gameplay`, and `utility`. The singleton `authoring` category was removed, and `manage_blueprint` moved into `core`.
- `manage_blueprint` schema: `location`, `rotation`, `scale` changed from flat number arrays to structured objects with named sub-fields (`x`/`y`/`z` or `pitch`/`yaw`/`roll`) — matches TypeScript schema
- `system_control` schema: removed `export_asset` action (not in TypeScript schema) and `additionalArgs` parameter (C++-only, never used by TS clients)
- `control_editor` schema: added `set_editor_mode` action (was missing from C++, present in TS)
- Screenshot handler: now returns `async: true` with `expectedDelay` field and timing guidance for polling
- `ScanPathsSynchronous` removed from asset query/workflow handlers to prevent GameThread blocking — documented limitation: newly-added assets may not appear until editor rescan
- Temp file cleanup in `execute_python` uses RAII scope guard for guaranteed cleanup on all exit paths

### Fixed
- `reset` action now restores initial state from `Initialize()` instead of enabling all tools unconditionally
- UE 5.6 compatibility: `TSharedPtr` for incomplete types, `Headers.Add` instead of `SetHeader`, `TryGetField` return value
- Package script arg parsing — flags no longer eaten as output directory, extra args correctly forwarded to RunUAT
- Build-environment action routing and validation now cover the expanded landscape, foliage, sky/weather, water, and buoyancy actions across native and TypeScript surfaces

### Technical Details
- Response routing via explicit `ERequestOrigin` enum (`NativeHTTP` vs `WebSocket`) — no more `TargetSocket==nullptr` inference
- Thread-safe SSE writes: per-connection `WriteMutex`, snapshot pattern for broadcast
- Thread-safe tool registry: `CacheMutex` protects `Tools`, `ToolsByName`, `CachedToolSchemas`, `bCacheValid`
- Opt-in via `bEnableNativeMCP` project setting (default: off)
- Capability token validation mirrors WebSocket bridge (`McpConnectionManager.cpp`)

### New Files

| File | Purpose |
|------|---------|
| `Private/MCP/McpNativeTransport.h/cpp` | Raw-socket HTTP+SSE server, session management, JSON-RPC dispatch |
| `Private/MCP/McpJsonRpc.h/cpp` | JSON-RPC 2.0 helpers (parse, response, error, notification, progress) |
| `Private/MCP/McpToolRegistry.h/cpp` | Singleton registry for self-describing C++ tool definitions |
| `Private/MCP/McpSchemaBuilder.h/cpp` | Fluent builder for MCP tool inputSchema JSON |
| `Private/MCP/McpDynamicToolManager.h/cpp` | Runtime tool enable/disable, protected tools, initial state reset |
| `Private/MCP/Tools/McpTool_*.cpp` | Native self-describing tool definition classes with schema + dispatch |
| `Private/UI/SMcpStatusBarWidget.h/cpp` | Editor status bar MCP indicator |
| `Resources/MCP/server-info.json` | Server name, version, default instructions |

---

## [0.1.4] - 2026-04-03

### Security
- Command injection fixes in bump-version action and editor tools with mixed-context sanitization (#327, #322)
- Path traversal fixes in `export_level` action and screenshot filenames (#305)
- Replaced synchronous file operations with async to prevent blocking (#318)

### Added
- Custom content mount points via `MCP_ADDITIONAL_PATH_PREFIXES` environment variable (#326)
- New `manage_project_settings` tool for runtime project configuration
- Audio authoring capabilities: sound wave creation, sound cues, MetaSounds, attenuation settings
- Success flags in audio asset creation responses
- Optional plugin dependencies: IKRig, ChaosVehiclesPlugin, AnimationData

### Fixed
- UE 5.0 API incompatibilities in IK Rig and widget authoring
- Crash when deleting animation/rig assets on UE 5.7+ (9ea2db4)
- Folder deletion crashes with safe deletion implementation (f0f4e44, ed56353)
- Widget creation crash (#306)
- Asset loading reliability for newly created AI assets (bb5e3bb)
- Asset query parameter bugs and expanded classNames support (#311)
- Replaced custom asset directory checks with `UEditorAssetLibrary` to avoid stale cache
- Fixed searchText filtering in `search_assets` action (4b1cb0e)
- Unified pin serialization across blueprint graph handlers (#309, 10f8f2b)
- Actor lookup to match subsystem behavior (checks both label and name)
- Console command settings delegated to C++ handler for performance
- Delay-load for optional plugin modules to prevent missing dependency errors (#317)
- IK retargeter initialization using controller API (UE 5.7+) with backward compatibility
- Rate limiting defaults and missing GraphQL heading in docs (d023284)
- `get_ai_info` schema alignment (#310)

### Dependencies
- `github/codeql-action` 4.33.0 → 4.34.1
- `picomatch` 4.0.3 → 4.0.4

---

## [0.1.3] - 2026-03-21

### Security
- Path traversal fix in `export_asset` action to prevent directory traversal attacks

### Added
- External actors support for World Partition in level structure handlers
- Streaming reference creation for external actor packages

### Fixed
- UE 5.0 compatibility using `bIsWorldInitialized` direct access
- Tick task manager crashes during world operations with proper cleanup
- World cleanup issues with `FlushRenderingCommands` safety
- Sublevel creation process with enhanced path handling
- Missing includes for UE 5.7 build (contributed by @a2448825647)

### Changed
- Enhanced `McpAutomationBridgeHelpers.h` with additional safety helpers
- Improved `McpSafeOperations.h` for safer world operations

---

## [0.1.2] - 2026-03-18

### Security
- Command injection prevention via semicolon sanitization in all user inputs
- Path traversal fixes in validateSnapshotPath and asset handlers
- Blueprint creation savePath sanitization to prevent traversal attacks

### Added
- `McpAutomationBridge_ConsoleCommandHandlers.cpp` - Batch and single command execution (302 lines)
- `McpHandlerUtils.h/cpp` - Standardized JSON response builders (1,900 lines)
- `McpPropertyReflection.h/cpp` - Property reflection utilities (1,356 lines)
- `McpSafeOperations.h` - Safe asset/level save for UE 5.7 (659 lines)
- `McpVersionCompatibility.h` - UE 5.0-5.7 API compatibility macros (225 lines)
- `McpHandlerDeclarations.h` - Forward declarations (844 lines)
- Debug visualization shapes for better testing feedback
- `list_objects`, `set_property`, `get_property` actions to control handlers

### Fixed
- EditorFunctionHandlers: use-after-free bug
- EffectHandlers: truncated condition + missing braces
- InventoryHandlers: duplicate TArray with undefined variables
- MaterialAuthoringHandlers: duplicate include + missing UE 5.0 fallback
- NavigationHandlers: case-sensitivity error
- SkeletonHandlers: duplicate verification + redundant code + duplicate parsing
- WidgetAuthoringHandlers: unreachable code block
- Volume attachment to movable actors by checking mobility
- World memory leaks in UE 5.7 by properly cleaning up created worlds
- Texture property modification errors using PreEditChange/PostEditChange lifecycle
- Blueprint loading to properly find in-memory blueprints first
- Level save/load operations for correct package name matching
- GeometryScript AppendCapsule segment steps for UE 5.5+ compatibility

### Changed
- Complete deep-level refactoring of 57 handler files with line-by-line review
- Centralized utility infrastructure for consistent error handling
- UE 5.0-5.7 cross-version compatibility with API abstraction macros
- All handlers now use standardized response builders

### Compatibility
- Unreal Engine 5.0 - 5.7
- Platforms: Win64, Mac, Linux

---

## [0.1.1] - 2026-02-16

### Added
- 200+ automation action handlers across all domains (AI, Combat, Character, Inventory, GAS, Audio, Materials, Textures, Levels, Volumes, Performance, Input)
- Progress heartbeat protocol for long-running operations
- Dynamic tool management via `manage_tools` MCP tool
- IPv6 support with hostname resolution and zone ID handling
- TLS/SSL support for secure WebSocket connections
- Per-connection rate limiting (600 messages/min, 120 automation requests/min)
- Handler verification metadata in responses (actor/asset/component identity)

### Security
- Path validation helpers: `SanitizeProjectRelativePath`, `SanitizeProjectFilePath`, `ValidateAssetCreationPath`
- Input sanitization for asset names and paths
- Loopback-only binding by default
- Handshake required before automation requests
- Command validation blocks dangerous console commands

### Fixed
- Landscape handler silent fallback bug (now returns `LANDSCAPE_NOT_FOUND` error)
- Rotation yaw bug in lighting handlers
- Integer overflow in heightmap operations (int16 → int32)
- Intel GPU crash prevention with `McpSafeLevelSave` helper
- UE 5.7 compatibility (GetProtocolType API, SCS save, Niagara graph init)

### Compatibility
- Unreal Engine 5.0 - 5.7
- Platforms: Win64, Mac, Linux

---

## [0.1.0] - 2025-12-01

### Added
- Initial release
- WebSocket-based automation bridge
- Core automation handlers for assets, actors, levels
- Blueprint graph editing support
- Niagara authoring support
- Animation and physics handlers

---

For full MCP server changelog, see: https://github.com/ChiR24/Unreal_mcp/blob/main/CHANGELOG.md
