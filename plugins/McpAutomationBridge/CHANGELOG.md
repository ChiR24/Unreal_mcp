# Changelog

All notable changes to the MCP Automation Bridge plugin will be documented in this file.

---

## [Unreleased]

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

### Changed
- **Second dedup pass (2026-09-06)** — Navigation and Spline read payloads through the shared `GetJson*Field` / `ExtractVectorField` accessors (their private copies are gone), the material domain / blend mode / shading model chains in create_material and the three set_* handlers share `ParseMaterialDomain` / `ParseBlendMode` / `ParseShadingModel`, material and material-function info share `AppendMaterialFunctionIO`, `SetMainMaterialInputExpression` reuses `GetMainMaterialInput` (and now accepts WorldPositionOffset), Sequence track lookups share `FindTrackByName`, the container Property handlers share `McpPropertyReflection::AssignPrimitiveFromJson`, WidgetAuthoring animation and widget lookups share `FindWidgetAnimation` / `FindWidgetByName`, the thin `FMcpAutomationBridge_*` pin wrappers call `McpBlueprintUtils` directly, `blueprint.get` merges functions and events through one lambda, and the Niagara graph handler parses `scriptType` once.
- **Shared helpers replace copy-pasted lookups (2026-09-06)** — 56 Geometry handlers resolve their dynamic mesh through `ResolveDynamicMeshForGeometry` (null-world safe), MaterialAuthoring pin chains go through `ForEachMainMaterialInput` / `GetMainMaterialInput`, Sequence binding names through `GetBindingName`, Skeleton loads through `LoadSkeletonOrMeshSkeleton`, Spline handlers through one `FindSplineMeshComponent` / `ParseSplineMeshAxis`, SCS handlers through `FindSCSNodeByVariableName` / `FindSCSParentNode` / `IsSCSRootAlias`, Level handlers dropped their unused `HandleExecuteEditorFunction`-family macro pairs, and `manage_tools.get_status` reports the plugin descriptor version and generated registry counts instead of hardcoded values.
- **`Private/` reorganized into per-domain modules** — `Core/` (errors, requests, security, subsystem), `Domains/` (66 domain directories), `Foundation/` (blueprint, bridge helpers, handler utils, capability authorization, idempotency, compensation), `MCP/`, `Safety/`, and `Transport/`. The per-tool `McpTool_*.cpp` definitions, `McpDynamicToolManager.cpp`, `McpConsolidatedActionRouting.h`, and the `McpNativeTransport.{h,cpp}` monolith are gone, replaced by generated registries and `Private/MCP/Transport/`.
- **`Private/Safety/` split into per-operation headers** — asset save, level save, map load, folder delete (assets/verify), animation delete, delete quiesce/compilation, world delete, package tools, material, and classification each have their own header instead of one `McpSafeOperations.h`.
- **`control_actor` spawn is now transactional** — a requested `meshPath` that can't be applied no longer leaves a misconfigured actor in the level: it fails `MESH_NOT_FOUND` before spawning if the mesh can't load, or rolls back (`Destroy()` + `MESH_APPLY_FAILED`) if a resolved mesh can't be applied to the spawned actor.
  - **Potentially breaking:** a request that passed a `meshPath` which failed to resolve previously still produced a spawned actor and a success response; it now returns a `MESH_NOT_FOUND` error and spawns nothing.
- Stripped redundant section and line comments across **254 files** (237 `.cpp`, 3 `.h`, 14 `.ts`), removing roughly 1,640 lines of banner and restating comments from the C++ domain handlers, the TypeScript handlers, and the capability records. Comments carrying information the code does not — including the `docs/Roadmap.md` section cross-references — were kept in condensed form.

### Removed
- **Unreachable and dead handlers (2026-09-06)** — `Blueprint/Graph/...SetDefaultObject.cpp` and the four `Blueprint/Components/Scs{SetTransform,RemoveComponent,ReparentComponent,SetProperty}.cpp` routes were shadowed by earlier routes; `Interaction/...RuntimeActors.cpp` and `...RuntimeComponents.cpp` plus their six subsystem declarations duplicated the namespace handlers that already claim those actions; `Effect/...NiagaraModuleRouting.cpp` held five `return false` stubs; `Performance/...ActorMergeSave.cpp`, `LevelStructure/...Actions.cpp` (log category moved next to its handlers), the empty `FoliageHandlers.cpp`, `NiagaraHandlers.cpp` and `PropertyHandlers.cpp` translation units, `Foundation/Render/McpRenderStateRefresh.{h,cpp}` (inlined at its only call site), `HandleSequenceGetMetadata`, `EnsureSequenceEntry` / `GSequenceRegistry`, `CopyExternalPackageDirectory`, `FindExpressionByPayload`, `LOAD_MATERIAL_OR_RETURN`-adjacent dead macros, the per-domain `GetStringField`/`GetNumberField`/`GetBoolField` copies in Misc, Networking and Texture (now the shared `GetJson*Field` accessors), `GetIntFieldSkel`, `SanitizeForLogConnMgr`, the three file-local `PersistSnapshotAsync` helpers (now `FMcpDiagnosticsSnapshot::PersistCurrentAsync`), nine dead consolidated routing sub-lists, and roughly a hundred orphan section banners and nested `#if WITH_EDITOR` arms across the domain handlers.
- **`Private/MCP/Gateway/McpNativeGatewayManifest.h`** (generated, 298 KB) is no longer emitted or committed. No translation unit has included it since the native gateway started serving describe from the generated parent registry, so it was dead weight in every build and every diff.
- **`Domains/EditorFunction/McpAutomationBridge_EditorFunctionHandlersActorComponents.cpp`** — its only function, `HandleActorComponentFunction` (`LIST_ACTOR_COMPONENTS`), was never reached by `HandleExecuteEditorFunction`.
- **Enable Native Gateway (`bEnableNativeGateway`) project setting** — the native transport permanently exposes only the `unreal` tool; there is no opt-out and no legacy 23-tool listing. A direct `tools/call` on a canonical parent name returns a bounded, executable `DIRECT_TOOL_CALL_REMOVED` receipt whose `nextCall` re-runs the request through the gateway.

### Security
- Scopes are exact-set membership with an `Admin` wildcard, not rank-based — `Write` does not imply `Read`, and an unresolvable capability demands `Admin`.
- Consent arrives as an `automation_request` envelope sibling and is re-validated plugin-side; it is never inferred from loopback, a prior call, idempotency, or preview.
- Capability tokens compare in constant time and are never logged; the plugin re-enforces every check the TypeScript layer performs.
- Added continuous local output-path validation, disabled network-backed media URLs because redirect destinations cannot be pinned, added client-scoped native rate limits that survive session rotation, enforced strict native `manage_tools` argument validation, and sanitized streamed log payloads.

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
