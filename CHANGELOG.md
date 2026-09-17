# 📋 Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## 🏷️ [Unreleased]

> [!IMPORTANT]
> ### 🚪 Single-Tool Gateway, Capability Catalog & Full Source Reorganization
> Everything on this branch since the `0.5.30` tag: the permanent cutover to a single public `unreal` gateway tool, a hand-authored capability catalog that now generates both the TypeScript and the C++ contract surfaces, cinematics/render/replay automation, and a top-to-bottom split of the TypeScript handlers and the C++ plugin into per-domain modules.
> **This release contains breaking changes** — see the **⚠️ Migration** section below.

<details>
<summary><b>✨ Added</b></summary>

- **Folded capability families** — a record can stand for a whole family of bridge actions. `routing.dispatchBy` maps one selector parameter's value to the existing handler action, and every former name stays callable as a folded legacy pair whose pins supply the value it implied; both gateways apply the same two steps (pins before validation, action after it). The folds are data (`records/folds/<parent>.folds.ts`, applied by `records/shared/fold.ts`): 244 fold families across 22 parents (222 of the resulting records dispatch through a `routing.dispatchBy` selector — 221 of them from the fold specs, plus `manage_level_structure.create_volume`, whose `volumeClass` dispatch is authored on the record itself — and the remaining 23 families are pure-alias folds; 245 of the 379 records carry at least one folded legacy pair, across 1,166 pinned pairs) take the catalog from 1,379 authored pre-fold action entries to **379 records**, while all 1,545 `{tool, action}` pairs (every shipped name plus 164 new family primaries) still resolve, describe and execute, the normalization audit total is unchanged at 1,341, and a fold whose primary is one of its members keeps the selector optional so every pre-fold call is unchanged. A consent grant may name the capability by any name it answers to (canonical id, alias, or a folded `tool.action` pair) on both doors; the native completion pool completes the old names too; the search index counts a name once per identifier field and stops re-scoring folded member ids as aliases. Per-action contract tests pin the authored, unfolded records (`records/unfolded.ts`), and the integration suites derive one twin case per family (`tests/fold-twins.mjs`) so every advertised primary and selector is exercised.

#### Gateway surface

- **`unreal` gateway tool with four operations** — `search`, `describe`, `execute`, and `configure` are now the entire public MCP surface on both transports. The routing engine lives in `src/server/gateway/` (26 TypeScript modules) and covers capability indexing and views, search filters, browse/capability describe modes, execute resolve → validate → policy → authorization → dispatch, receipt context, schema normalization, guidance, and availability probing.
- **Execute idempotency ledger** — repeat `execute` calls carrying the same idempotency key return the original receipt instead of re-running the action. Mirrored on both sides of the bridge: TypeScript (`src/server/gateway/idempotency-ledger.ts`, cap 1024) and C++ (`Private/Foundation/McpIdempotencyLedger`, cap 4096).
- **Compensation receipts and capability principals** — `Private/Foundation/McpCompensationReceipt` and `McpCapabilityPrincipal`/`McpCapabilityAuthorization` give the plugin its own authorization identity rather than trusting the caller's claim.
- **`DIRECT_TOOL_CALL_REMOVED` migration receipt** — a direct `tools/call` on a canonical parent name returns a bounded, executable receipt whose `nextCall` re-runs the same request through the gateway.
- **Execute option validation and refusal** — `timeoutMs` must be an integer in 1..600000 ms and `idempotencyKey` 1..128 characters — a malformed key previously bypassed the ledger entirely, so a retry re-ran the mutation with nothing reporting that dedup was off. `preview` is refused outright with `UNSUPPORTED_PREVIEW` (no dispatch path implements a dry run, and `behavior.supportsPreview` is deliberately not consulted, because honouring it would perform the real mutation and call it a preview), and an option that is accepted but not implemented (`savePolicy`, `validationLevel`, `taskPreference`) answers `UNSUPPORTED_OPTION` instead of being validated, echoed on a success receipt, and dropped. An `expectedCatalogRevision` that no longer matches answers `STALE_STATE`. A result that would exceed the transport budget is refused as `RESULT_TOO_LARGE` (100,000 characters, 6,000,000 for image-payload capabilities) on the failure path as well as the success path.
- **Bounded search** — `limit` defaults to 12 and is capped at 25, a page is additionally held to `maxBytes` (default 24,576, floor 512, ceiling 262,144) by dropping whole trailing rows, and `hasMore`, `truncated`, `truncationReason`, `nextCursor`, `coercions` and `budgetExceeded` report exactly what happened: an argument that was clamped is named, a page that could not fit even one row answers `budgetExceeded` instead of returning an identical cursor forever, and a truncated page says why. A capability id or alias that collides in the index fails closed with `GATEWAY_INDEX_CONFLICT` rather than resolving to an arbitrary record.

#### Capability catalog and contract generation

- **Hand-authored capability records as the single source of truth** — `src/tools/catalog/capabilities/records/**` holds **379 records** across the **23** canonical parents, authored with `buildCoreRecord()` declaring deltas only. `aggregate.ts` hard-asserts the record count and throws on mismatch.
- **Generation pipeline** — `scripts/generate-canonical-registry.ts` (+ the `scripts/canonical-registry/` modules) emits the generated capability shards, the orchestration routing index, the docs action reference, and the C++ side: `Private/MCP/Generated/` shards and `McpGeneratedParentRegistry*`. `scripts/generate-gateway-manifest.ts` (+ `scripts/gateway-manifest/`) emits `src/gateway/gateway-manifest.generated.{ts,json}` with content hashing and path policy.
- **New drift gates** — `registry:generate`, `registry:check`, `manifest:check`, `policy:generate`, `policy:check`, `normalization:check`, `normalization:audit`, `migration:check`, `primitives:check`, `security:check`, `eval:check`, `version:check`, `workflow:check`.
- **Deterministic ordering helper** — `src/utils/serialization/ordering.ts` provides byte-order comparison so generated shards are byte-identical across machines and locales (`localeCompare` is no longer used for ordering).

#### MCP protocol primitives

- **Resources, prompts, completions and progress** — new `src/server/mcp-primitives/` implements the primitive registry, wiring, handlers, notifications, catalog revision reader, and fallback pointers, with a prompt catalog and typed prompt errors, a completion provider (ranking, slots, sources, fixtures), and a progress reporter/token/sink registry.
- **MCP Tasks (`2025-11-25`)** — a per-session **bounded** task store implementing the MCP SDK's own `TaskStore` contract, so `tasks/get|list|cancel|result` are auto-registered and reachable from the wire. It deliberately does not reuse the SDK's `InMemoryTaskStore`, which is unbounded, drives expiry off real `setTimeout` timers, and ignores `sessionId` entirely. Task checkpoints ride alongside it.
- **Resource subscriptions and revisions** — a subscription store with a notification coalescer so bursts of editor changes collapse into one client notification, plus resource revision stamps for change detection.
- **Session capability profiles and `configure` state** — a client profile store, a session capability profile and a session configure store model the gateway's `configure` operation and per-client capability negotiation. The session resolver is exercised by tests only — nothing in the stdio path installs one yet.
- **Elicitation decision policy** — a decision policy (`isSafeToElicit`) that answers whether a field is safe to elicit. Elicitation itself is wired into the execute path: `dispatchAndValidate()` awaits `maybeElicitMissingArgs()`, which prefills only missing required primitive fields through the client's elicitation function, under a timeout and a `missing-params` fallback. It never elicits secrets, tokens, or credentials, and never a destructive-confirmation value. Mirrored natively for cross-transport parity.
- **Resource providers** — new `src/resources/` supplies capability resources, editor-state resources, knowledge resources, the resource catalog, read router, typed resource errors, and asset pagination.
- **Native primitive parity** — the plugin gained matching `Private/MCP/Primitives/`, `Resources/`, `Routing/`, `Execute/`, `Gateway/`, and `DynamicTools/` modules (task store and methods, subscription store, notification coalescer, completion pools and provider, prompt catalog/render/argument validation, client profile store, elicitation policy), audited by the `primitives:check` parity harness.
- **Protocol negotiation** — support for `2025-11-25` plus the native transport's `MCP-Protocol-Version` header guard: a present-but-unsupported version is refused with HTTP 400, while an absent header derives from the session's negotiated version and otherwise falls back to the `2025-03-26` default. Native accepts the three modern versions; the TypeScript SDK additionally accepts two legacy ones, from the pinned SDK's own supported set rather than from code in this repository.
- **Bounded task store and progress** — only `search` and `describe` may be task-augmented; any mutating operation, and any tool other than `unreal`, is refused with `TASK_CHECKPOINT_REFUSED` before any work runs. The per-session store is capped at 32 tasks with a 5 to 30 minute lifetime, refuses creation with a JSON-RPC `-32600` `TASK_STORE_AT_CAPACITY` while every retained task is still running, and permits exactly one terminal transition (`TASK_ALREADY_TERMINAL`). Progress notifications carry the client's own `_meta` token (without one the reporter is inert rather than inventing an id), are strictly monotonic, and are bounded to 64 notifications per operation with a 512-character message clamp and a 256-entry reporter map.
- **Resource surface** — `resources/templates/list` is registered with four templates (`ue://capability/{capabilityId}`, `ue://knowledge/{engineVersion}/{topic}`, `ue://object/{objectPath}`, `ue://asset/{assetPath}`) plus five static resources (`ue://capability/catalog`, `ue://project`, `ue://editor`, `ue://selection`, `ue://state/revisions`), every read is held to a 64 KiB budget with typed `RESOURCE_*` codes (`RESOURCE_TRAVERSAL_REJECTED` for host paths and traversal), and `ue://health` now also carries `readiness`, the telemetry `diagnostics` snapshot, `currentSession`/`previousSession` and `metricsExposition` alongside the capability and editor-state resources.

#### Cinematics, render and media automation

- Complete native and WebSocket **cinematics, Movie Render Queue, media, Take Recorder, and replay** automation coverage, with live-editor verification harnesses.

#### Blueprint authoring

- **`add_event` supports component-bound events** — pass `componentName` plus `eventName` (the delegate name) to wire a component's multicast delegate (e.g. `NearMissZone.OnComponentBeginOverlap`). Previously such requests fell through to the custom-event branch and produced an unbound `Event_<guid>` that never fired. The new branch resolves the SCS component, locates the multicast delegate property on its class (accepting both the bare name and the `__DelegateSignature` suffix), and creates a properly initialized `UK2Node_ComponentBoundEvent` with `ComponentPropertyName`, `DelegatePropertyName`, and `DelegateOwnerClass` set. Idempotent on repeat calls; returns `INVALID_ARGUMENT` / `COMPONENT_NOT_FOUND` / `COMPONENT_CLASS_UNRESOLVED` / `DELEGATE_NOT_FOUND` when inputs are missing or unresolvable. Guarded by `MCP_HAS_K2NODE_COMPONENTBOUNDEVENT` so the file still compiles on engine layouts where the header isn't reachable.
- **`create_node` / `add_node` sets the widget class on CreateWidget nodes** — a `K2Node_CreateWidget` previously fell through to generic instantiation that never assigned `WidgetType`, producing a generic `UUserWidget` `Class` pin and an untyped `Return Value`. A dedicated branch now reads `targetClass` (Widget Blueprint asset path such as `/Game/Widgets/WBP_HUD`, or a class name), resolves it, and assigns `WidgetType` before pin allocation so `ReconstructNode` builds the correct typed `Return Value`. Returns `INVALID_ARGUMENT` / `CLASS_NOT_FOUND`.
- **`create_node` / `add_node` sets the target class on DynamicCast nodes** — a `K2Node_DynamicCast` ("Cast To …") previously produced an unusable "Bad cast node" with only a wildcard `Object` pin and no typed `As <Class>` output. A dedicated cast branch reads `targetClass`, resolves it via `ResolveClassByName`, and assigns `UK2Node_DynamicCast::TargetType`.
- Factored the class-resolution and payload-reading logic shared by DynamicCast and CreateWidget into `ResolveTargetClassFromString` and `ReadTargetClassPayload`, so every node branch with a class pin accepts the same input forms (Blueprint asset path, generated-class path, native class name) and the same legacy field fallbacks (`memberClass` / `nodeClass` / `widgetType`, plus a `CastTo<Class>` prefix peel from `nodeType`).
- **`add_variable` applies `defaultValue`** — the handler read the field but never assigned it, so every variable was created with a zero/empty default (a float requested as `0.35` stayed `0`). The parsed default is now written to `FBPVariableDescription::DefaultValue` with type-aware formatting: booleans as lowercase `true`/`false`, integer/byte categories as whole numbers, floats/doubles via `SanitizeFloat`, and strings/struct literals passed through.
- **18 widget-authoring actions are now routable on both surfaces** — `add_quest_tracker`, `add_safe_zone`, `add_spacer`, `add_widget_component`, `add_widget_switcher`, `bind_localized_text`, `create_credits_screen`, `create_shop_ui`, `create_widget_style`, `delete_animation`, `get_widget_slot_info`, `remove_widget`, `rename_widget`, `reparent_widget`, `set_font`, `set_localization_key`, `set_margin`, and `set_widget_binding` were absent from both the TypeScript `WIDGET_AUTHORING_ACTIONS` set and the native `WidgetAuthoring()` routing array, so the handlers behind them could not be reached. Added to both, with matching capability-record property fragments for the widget path, layout, content, and panel inputs.
- **Promoted skeleton routes are named on both surfaces** — fifteen previously hidden native skeleton routes (`add_socket`, `modify_socket`, `set_physics_asset`, `modify_physics_body`, `remove_physics_body`, `set_physics_constraint`, `get_physics_asset_info`, `list_morph_targets`, `set_morph_target_value`, `get_bone_transform`, `list_virtual_bones`, `remove_socket`, plus the `delete_*` spellings) now carry explicit route names on the TypeScript `SKELETON_ACTIONS` and the native `Skeleton()` routing array, with twelve promoted to canonical capability records (marked `post-migration` so the pre-gateway audit total stays truthful). The remaining three `delete_*` spellings (`delete_socket`, `delete_morph_target`, `delete_virtual_bone`) stay hidden pending a retrieval-IDF budget fix.

#### Plugin capabilities

- **`MCP_NATIVE_PORT` environment variable** — overrides the native MCP HTTP/SSE port at startup without editing committed ini, so several editors can run at once on distinct ports. Falls back to the `Native MCP Port` project setting when unset or invalid.
- **`IKRigEditor` optional module** — declared for the `create_ik_rig` path so IK Rig creation works without a hard dependency on the editor module being present.
- **Pre-queue capability gate** — every automation request is authorized before it reaches the editor queue (`Private/Core/Security/McpPrequeueGate`), resolving the demand with the same `NormalizeAction` the dispatchers use, so a payload cannot authorize one capability and execute another. The refusal order is fixed (scope, consent, project, paths, path coverage, console command, consent nonce, quota), a refused request performs no editor work at all (the in-handler checks remain as post-queue defence), an unknown or misspelled action fails closed to `Admin`, an ambiguous one takes the strictest scope, and the recursive console-command scan (depth 8, 4096 nodes) treats a truncated scan as `COMMAND_BLOCKED`.
- **Scoped capability tokens, quotas and single-use consent** — a scoped token carries its profile, scopes, allowed path prefixes, allowed projects and per-minute request/tool-call quotas (a scoped `Admin` entry is invalid and ignored), and the `bridge_ack` `authority` block reports the effective identity without ever carrying the token, its paths or its limits. A presented-but-unresolvable token is refused even where a token is not required, and a project-restricted principal is refused at the handshake rather than per request. The quota ledger is shared by both transports, keyed by principal rather than socket or session, bounded to 256 tracked identities with least-recently-seen eviction, charged only after every other refusal, and `QUOTA_EXCEEDED` is the one refusal marked retryable — so a reconnect cannot reset a budget. A consent grant's nonce is burned after all authorization refusals and before the quota charge, so a replay answers `CONSENT_REUSED` without spending budget; nonce-less grants from older clients keep capability-match-only behaviour.
- **New state and refusal codes** — `STALE_STATE` and the `UNDO_UNAVAILABLE_*` family (`_DURABLE_WRITE`, `_EXTERNAL_PROCESS`, `_ASYNC_PIPELINE`, `_NO_TRANSACTION_BUFFER`, `_NON_TRANSACTIONAL_OBJECT`, plus `UNDO_BROKEN_BY_DURABLE_WRITE` from a package-saved witness) join the shared strings, `EDITOR_BLOCKED` reports a game-thread stall over 15 s, and `EDITOR_STATE_MISMATCH` reports a preview/state mismatch.
- **Cancellation and deferred replies are attributed to their transport** — a cancel from a socket that does not own the request is refused, and `McpRequestOriginRegistry` (cap 512) records the admitting transport per request id. That fixes native `/mcp` deferred replies which used to fall back to the WebSocket path, get dropped, and surface to the caller as an untyped 300 s `TIMEOUT`.
- **Editor policy surfaces** — the plugin settings carry the Movie Render Queue caps (dimension 8192, 33,554,432 px aggregate, `MaxMovieRenderAggregateWork`, executor/burn-in/Take-Recorder class allowlists) and the native session limits (600 requests and 120 tool calls per minute per client).
- **Bounded native sessions** — at most 16 active sessions (`MaxActiveSessions`) with a 120 s idle reclaim, and a 32-connection ceiling (`MaxConcurrentConnections`) that answers 503 beyond it.

#### Services and tests

- **Telemetry and readiness services** — `src/services/` gained a telemetry registry, observation and schema modules plus a readiness probe.
- **New test tiers** — `tests/eval/` (own Vitest config, run by `eval:check`), `tests/audits/`, `tests/harness/`, and `tests/fixtures/`, alongside `scripts/qa/` adversarial, cross-transport matrix, and capability-metadata audits.
- **Readiness vs. health** — `/health` now answers 503 while the server is not ready instead of reporting `ok` unconditionally, and a new `/ready` probe reports the same state without the health payload.
- **Capability-aware timeouts** — operation timeouts derive from the record's declared cost tier: `resolveActionTimeoutMs()` reads the generated `capability-cost-index.generated.ts` for the `tool::action` pair and maps its latency and resources onto `CAPABILITY_TIMEOUT_TIER_MS` (15 s to 20 min), with a 15 s floor, a 120 s fallback for unclassified pairs, and an `MCP_REQUEST_TIMEOUT_MS` override — replacing one flat default for every action.
- **Asset-listing cache invalidation** — the listing is invalidated after every successful mutation (`invalidateAssetCacheForMutation()` runs straight after dispatch, with 26 listing-neutral actions exempted and everything else treated as a mutation, fail-safe). The `/Content/...` alias is now mapped before path sanitization, which fixes a deleted asset lingering in the cache for the full TTL because sanitization threw on the unaliased path and the catch swallowed it. A diagnostics snapshot reader with a 64 KiB cap and an allowlist projection (tokens, paths and session ids dropped) joins the automation client, though nothing on the production path reads it yet.

#### Handlers, catalog and native routing

- **User-defined action aliases** — a project can add its own action names in `handler-aliases.json` (schema `Resources/MCP/custom-handler-aliases.schema.json`): version 1, at most 128 aliases and 64 KB, `lower_snake_case` names only, resolved through three search paths. An alias pointing at another alias, or at the protected `inspect`/`manage_tools` actions, is rejected, and an alias whose target is not registered yet stays pending until the target arrives.
- **Geometry dynamic-mesh authoring** — `manage_geometry` gained procedural-mesh authoring through one `edit_dynamic_mesh` family: `create_procedural_mesh`, `append_vertex`, `append_triangle`, `delete_vertex`, `delete_triangle`, `get_vertex_position`, `set_vertex_position`, `set_vertex_color`, `set_uvs`, `split_normals`, `translate_mesh` and a boolean `difference`.
- **AI authoring routes** — `set_ai_perception`, `create_nav_modifier` and `set_ai_movement` join behaviour tree, blackboard, EQS query, MassEntity, SmartObject, StateTree, navigation and nav-actor configuration as named actions instead of only through the behaviour-tree umbrella.
- **Native search matches words, and pages** — the native gateway search matcher scores word-level matches and accepts `actionOffset` and `maxBytes` so a large action list can be paged.

#### Fab asset-store bridge

- **`McpAutomationBridgeFab` delay-loaded module** — a second editor module that bridges the Fab asset store through the Fab plugin's own browser widget and download API (`McpFabBrowserBridge`, `McpFabSearchOperation`, `McpFabDetailsOperation`, `McpFabAddToProject`, `McpFabImportWatcher`). Its Fab and Megascans engine dependencies are declared optional and listed in `PublicDelayLoadDLLs` on Win64, so the module compiles away when they are absent. The store actions themselves are dispatched by the main module's `Private/Domains/AssetWorkflow/`: `search_fab_listings`, `get_fab_listing_details`, `add_fab_asset_to_project`, `download_fab_asset`, `list_fab_downloads`, `list_fab_library`, `list_megascans_library`, and `import_megascans_asset`. Two further actions, `list_content_sources` and `migrate_assets`, are generic content ingestion (engine templates, engine and plugin content, downloaded Bridge packs) rather than Fab-specific, and a migrate request never carries a filesystem path: it names a root token plus a relative id, both resolved against a fixed root table.
- **Diagnostics snapshot store** — `Foundation/Diagnostics/` ships a bounded, crash-tolerant singleton that records request admission, pre-dispatch, refusal, terminal, handshake, disconnect, and session events to `<Project>/Saved/MCP/diagnostics/`. Atomic file writes with temp+rename, previous-session rotation, corrupt-file tolerance with one-shot bounded warnings.
- **Reflected function invocation** — `Foundation/Reflection/McpReflectedInvoke` provides a shared RAII parameter-block marshalling primitive for arbitrary UFunction invoke, gated behind `effect: destructive` + `consent: elevated` on both `control_editor.invoke_reflected_function` and `control_actor.call_actor_function`.
- **`native-gates.ps1`** — PowerShell script for local native compile and smoke gates (`npm run native:compile`, `native:smoke`, `native:smoke:core`, `native:smoke:fab`, `native:check`), so a non-compiling C++ security control can never pass CI.

</details>

<details>
<summary><b>🔧 Changed</b></summary>

- **Static `unreal` gateway tool replaces the 23-tool public surface.** The TypeScript stdio and native MCP transports now permanently expose a single `unreal` tool. The 23 canonical parents (`manage_asset`, `control_actor`, …) are internal and reachable only through `search`, `describe`, `execute`, and `configure`. This reduces client context pressure and eliminates hallucinated tool/action calls.
- **C++ plugin reorganized into per-domain modules** — `Private/` is now split into `Core/` (errors, requests, security, subsystem), `Domains/` (**66** domain directories), `Foundation/` (blueprint, bridge helpers, handler utils, capability authorization, idempotency, compensation), `MCP/` (transport, routing, execute, gateway, primitives, resources, dynamic tools, generated shards, tools), `Safety/`, and `Transport/`. The former per-tool `McpTool_*.cpp` files and the `McpNativeTransport.{h,cpp}` monolith are gone, replaced by generated registries and `Private/MCP/Transport/`.
- **`Private/Safety/` split into per-operation headers** — asset save, level save, map load, folder delete (assets/verify), animation delete, delete quiesce/compilation, world delete, package tools, material, and classification each have their own header; `McpSafeOperations.h` survives only as a short umbrella that includes them.
- **TypeScript tools reorganized** — the monolithic `src/tools/handlers/*-handlers.ts` files were replaced by **37** per-domain directories (**203** files, up from 64 flat ones at `v0.5.30`), and `src/tools/` now holds `catalog/` (capability records), `definitions/` (tool schemas), `orchestration/` (the canonical dispatcher, routing, and generated routing index), `dynamic/`, and `handlers/`. The former `src/tools/editor/` and `src/tools/level/` class modules had no live caller and are gone; the live code is `src/tools/handlers/editor/` and `src/tools/handlers/level/`. Types were split into `src/types/handlers/` and `src/types/tools/`; utilities were regrouped into nine `src/utils/` areas (`commands`, `paths`, `responses`, `validation`, `collections`, `serialization`, `logging`, `config`, `interaction`).
- **Automation bridge split into focused modules** — `src/automation/` now separates the client, config, frame codec, state, status, connection lifecycle, request dispatcher, request context/correlation, cancellation errors, capability-token provider, log redaction, the gateway consent/correlation/timeout/expected-revisions contexts, the read-only diagnostics snapshot reader, and the natural-timeout cancellation path.
- **`control_actor` spawn is transactional** — a requested `meshPath` that can't be applied no longer leaves a misconfigured actor in the level. It fails `MESH_NOT_FOUND` before spawning if the mesh can't load, or rolls back (`Destroy()` + `MESH_APPLY_FAILED`) if a resolved mesh can't be applied.
- **Console-command validation is generated** — the allow/deny model now lives in `src/utils/commands/console-command-policy*.ts` with a generated policy artifact and a `policy:check` drift gate, instead of a hand-maintained validator.
- **Minimum Node.js raised to `>=20.19.0`** (was `>=18`).
- **Structural scale of the reorganisation** — `src/` grew from 154 files to 832, the plugin's `Private/` tree from 133 `.cpp`/`.h` files in one flat list to 1,530 files under `Core/`, `Domains/`, `Foundation/`, `MCP/`, `Safety/`, `Tests/`, `Transport/` and `UI/`, and `src/tools/handlers/` from 64 flat files to 203 files in 37 domain directories.
- **`MCP_DEFAULT_CATEGORIES` is inert** — setting it now logs a warning and changes nothing: the public surface is the single gateway tool, so there is no category listing left to filter.
- **ESLint enforces the Node floor** — `eslint-plugin-n`'s node-builtins and ES-syntax rules run at `warn` (and therefore fail CI's `--max-warnings=0`), so an API above Node 20.19 cannot land unnoticed.
- **`manage_tools.list_tools` / `get_status` contract change** — the `description` field is dropped from the row shape, and `catalogRevision` plus `catalogStateRevision` are reported instead of hardcoded counts.

</details>

<details>
<summary><b>🛡️ Security</b></summary>

- **Scopes are exact-set membership with an `Admin` wildcard, not rank-based** — `Write` does **not** imply `Read`, and an unresolvable capability demands `Admin`.
- **Consent rides as an `automation_request` envelope sibling, never a handler param**, and is re-validated plugin-side. It is never inferred from loopback, a prior call, idempotency, or preview.
- **Capability-token auth is on by default** — tokens compare in constant time and are never logged; the plugin re-enforces every check the TypeScript layer performs.
- **Path handling routed through a shared canonicalizer** — paths are limited to `/Game`, `/Engine`, `/Script`, `/Temp`, `/Niagara` plus sanitized additions, with the `/Content` alias handled in one place instead of re-implemented per handler.
- **Render/media output hardening** — continuous local output-path validation against symlink replacement, and network-backed media URLs disabled because redirect destinations cannot be pinned.
- **Native transport hardening** — client-scoped rate limits retained across native MCP session rotation, strict native `manage_tools` argument validation, and sanitized streamed log payloads.
- **An `action`/`subAction` mismatch can no longer authorize one capability and execute another.** `AuthorizeAutomationRequest` normalizes any payload that declares both with different values (overwriting `action` from the authoritative `subAction`), the native execute stage stamps `subAction` from the server-resolved action, and the pre-queue gate resolves its demand with the same `NormalizeAction` the dispatchers call, so the gate and the dispatcher cannot disagree.
- **Scoped tokens are narrower than the legacy token by construction** — a scoped token may list only `Read`/`Write`/`Destructive` (never `Admin`), and a scoped token colliding with the legacy token wins because the narrower grant applies.
- **Reflected property access cannot reach the plugin's own settings** — `McpSafeReflectionTarget` refuses every `/Script` target with `OBJECT_NOT_ADDRESSABLE`, deliberately distinct from `OBJECT_NOT_FOUND`, from the array, element, insert and append property handlers, so a path- or project-restricted principal cannot reach the settings that govern the transport itself: a `write`-scoped principal could otherwise switch off `bRequireCapabilityToken` through `inspect.set_property`, and a `read`-scoped principal could read the Admin token out. `/Game`, `/Engine`, spawned actors and Blueprint CDOs are unaffected.
- **URL-looking arguments are refused outright** — handler URL validation rejects every URL form, including loopback and `file:` URLs, rather than trying to allow a safe subset.
- **Parameter gates use own-property lookups** — `hasOwn()` replaces an inherited-property check, so `__proto__`, `constructor` or `toString` cannot slip past an `additionalProperties` or dispatch gate, matching the native `TMap` lookup.
- **Token resolution and log redaction on the TypeScript side** — the bridge re-reads the token file on every `bridge_hello` and fails closed when it cannot resolve one, and `AutomationLogger` redacts tokens, paths and handshake metadata rather than trusting callers to keep them out.

</details>

<details>
<summary><b>🛠️ Fixed</b></summary>

#### Component trees and struct members

- **`attachTo` in a batched `edit_scs` actually attaches.** The parent search matched the requested name against the *exported text* of an `FSubobjectDataHandle` — an opaque id that never contains a component name — so it always fell through to the first handle it had, the root. Fourteen body parts landed on the collision cylinder while every op reported success. The parent is now resolved by name after the node exists, and an `attachTo` that cannot be resolved fails that op with a reason instead of being dropped.
- **Inherited components are addressable from the batch path.** A Blueprint's own SCS is only half its component tree: anything inherited from a native parent (ACharacter's `Mesh`, `CapsuleComponent`, `CharacterMovement`) lives on the CDO with no `USCS_Node`. `modify_component` answered "Component not found or template missing" for components the Blueprint plainly has, and `reparent_scs_component` answered `SCS_PARENT_NOT_FOUND` for a reparent the editor does with a drag. Both now resolve through the CDO, and a node parented to a native component records it the way the editor does.
- **`add_struct_member` honours the `members` array its own contract declares.** Only the single `memberName`/`memberType` pair was ever read, so the array form was refused with `MISSING_PARAMETER` and a ten-field struct cost ten calls. The array is validated as a whole — a bad entry refuses the batch rather than half-building a struct, because a partially applied member list is worse than none.

#### Editor capture, actor search and graph authoring

- **`control_editor.screenshot` can photograph a minimized editor again.** The window enumeration filtered minimized windows out entirely, so once the editor minimized itself (it does so on launch and after some PIE cycles) the main frame was absent from `windows[]` and unaddressable by index *or* title: `full_editor_window` answered `EDITOR_WINDOW_NOT_FOUND` with an empty window list and no in-tool way back. Minimized windows are now listed with `isMinimized`, and a minimized capture target is restored with `SW_SHOWNOACTIVATE` + `SWP_NOACTIVATE` before the capture so it never steals the user's focus or cursor; the response reports `windowRestored`.
- **`control_editor.take_screenshot` with `mode: "game_viewport"` no longer answers `NOT_IMPLEMENTED`.** The UI handler gates on the payload's own `subAction`, which still carried whichever alias the caller used, so the published `take_screenshot` spelling fell past the screenshot branch. The forward now names the canonical action.
- **`control_actor.find_actors_by_class` refuses a class name that does not resolve** instead of reporting "Found 0 actors". A Blueprint short name such as `BP_Thing_C` read as an empty level rather than as the typo it was; the refusal is `CLASS_NOT_FOUND` and names the generated-class path form that works.
- **`add_node` resolves the StandardMacros aliases that `create_node` already did.** `ForEachLoop`, `ForLoop`, `DoOnce`, `Gate` and friends are Blueprint macros, not `UK2Node_*` classes, so the same `nodeType` answered `UNSUPPORTED_NODE` on one action and succeeded on the other.
- **An unknown container type says how containers are spelled.** `TArray<Text>` and `Text[]` — the spellings a C++ author reaches for — produced a bare `Unknown type`, with no hint that the resolver takes `Array<T>`, `Set<T>` and `Map<Key,Value>`.
- **`set_widget_layout` applies every canvas-geometry field the call carries.** `layoutProperty` selects which one names the variant; `position`, `size` and `zOrder` sent together used to have two of the three silently dropped, so a widget landed in the right place at the default size behind everything else. The response lists what was `applied`.

#### Cinematics, render and replay

- Replay seek and killcam responses now wait for measured completion instead of returning optimistically.
- Movie Render Queue ownership is preserved through cancellation and held until executor settlement.
- Take Recorder panel/source state is restored after asynchronous start failures.
- Render limits are validated before queue mutation rather than after.
- Render output proof is token-aware, so tokenized output filenames verify correctly.

#### Engine compatibility

- **Source compatibility restored across the supported UE 5.0–5.8 range.** Several engine APIs and relocated headers were used without guards, and several existing guards named the wrong engine boundary, so the plugin failed to compile on parts of the range it advertises. Header selection now probes with `__has_include` instead of hard-coded version numbers wherever the engine moved a header, and the remaining guards were corrected against the engine source. Affected areas: the StructUtils headers, `FAssetCompilingManager::FinishCompilationForObjects`, `UWidgetBlueprint::WidgetVariableNameToGuidMap`, `CreateNewIKRigAsset`, `FString::RightChopInline`/`LeftInline`, and `PhysicsEngine/SkeletalBodySetup.h`. A redundant `UObject/StrProperty.h` include was dropped (`FStrProperty` comes from the already-included `UObject/UnrealType.h`). Three further shims were added for APIs that differ across the supported range (`MCP_SET_ENUMS`, `MCP_HAS_GET_OBJECTS_FLAGS`/`MCP_GET_OBJECTS_NO_NESTED`, `MCP_DISALLOW_SHRINKING`), and the `MCP_HAS_IKRETARGETER_SET_IKRIG_ENUM` guard was reversed because the boundary it named sat on the wrong minor.
- **JSON key-type change handled across the plugin** — 14 files move from `FJsonObject::Values` lookups with an `FString` key to `HasField()`, the ambiguous `TEXT("ReadOnly")` comparison is qualified, and the diagnostics filename no longer trips C2084.
- **Fab API conformance** — the adapter follows the engine's current Fab API surface, and `bCompileForEdit` (a member added in 5.6, absent on 5.5) is guarded in the shared Niagara stack-issue collector.
- **Render console handler** — use `FJsonObject::HasField()` instead of `Values.Contains(FString)`, following the `FJsonObject::Values` key-type change.
- **Asset soft-path fallback returned the wrong string shape** — `MCP_ASSET_DATA_GET_SOFT_PATH` used `PackageName` (`/Game/Foo`) where callers expected an object path (`/Game/Foo.Foo`). It now uses `FAssetData::ObjectPath`, the equivalent of the `GetSoftObjectPath()` used in the other branch.
- **Clean build fixed** — the memreport scan passed `256` as a seventh argument to `IFileManager::FindFilesRecursive`, but that parameter is `bClearFileNames`, not a result ceiling, so the call did not compile. The bound was dropped rather than reworked: truncating is also wrong here, since picking the newest of an arbitrary subset can miss the actual newest report. A real traversal bound would need `IterateDirectoryStatRecursively`.
- **Last source warning cleared** — `FLinearColor ColorValue;` left its channels uninitialized in the material-parameter track handler, and the only writer runs on one branch, so the compiler could not correlate the write with the guarded use and warned C4701. Seeded to opaque black, matching `ReadLinearColor`'s own defaults.

#### Handlers and routing

- **Asset listing accepted the wrong field name** — the asset-listing resource read `directory` where its schema declares `path`; the declared name now works.
- **`validate_niagara_system` reports real errors** — it previously hard-coded `isValid=true`. It now builds a full Niagara system view model and harvests stack issues (e.g. "The module has unmet dependencies.") across the system and emitter stacks. A data-processing-only view model cannot be used because `UNiagaraStackModuleItem::RefreshIssues()` emits no per-module issues in that mode.
- **IK Rigs created on the `NewObject` fallback path are registered with the asset registry** — `FAssetRegistryModule::AssetCreated()` is now called explicitly on that branch, which the static factory does for us on engines that have it. Without it the rig existed on disk but was unregistered, so it never appeared in the Content Browser until an unrelated rescan happened to pick it up: the asset looked lost even though creation had reported success.
- **Widget GUID registration logs a truthful no-op** — `RegisterWidgetGuid`, `UnregisterWidgetGuid`, and `RegisterAnimationGuid` each logged "registered"/"unregistered" on engine versions that have no `WidgetVariableNameToGuidMap`, claiming work they had not done. On those versions the engine owns the widget variable's GUID in `UBlueprint::NewVariables[].VarGuid`, and writing our own would overwrite a value existing bindings resolve through — so a no-op is correct, it just has to say so.
- **Bare `remove_variable` / `rename_variable` match on the native transport** — the Blueprint variable removal/rename handler matched only the `blueprint_`-prefixed forms, so the bare action names fell through unhandled. Both the snake_case (`remove_variable`, `rename_variable`) and alphanumeric-lowered (`removevariable`, `renamevariable`) bare forms are now accepted alongside the prefixed ones.
- **Every texture call was failing** — `action` is injected by the consolidated routing layer (`WithPayloadSubAction`) as the legacy dispatch verb, but it is not a client parameter and was absent from the handlers' `ValidParams` allowlists, so schema-valid texture calls were rejected with `TEXTURE_ERROR: Invalid parameter: action`. Added to all five affected handlers (gradient, noise, normal, pattern, resize).
- **`ListenPorts` drop warning** — when multi-listen is on and a partial `ListenPorts` override omits a default bridge port (8090/8091), a warning is logged instead of the drop being silent. The user's ports stay authoritative.
- **TS and native responses share one frame shape** — `normalizeAutomationFrame()` gives the TypeScript bridge the native `structuredContent` envelope, which fixed closed-output-schema failures across the whole 379-record catalog.
- **Blueprint macro nodes and material roots resolve correctly** — `ForLoop`, `ForLoopWithBreak`, `WhileLoop` and `ForEachLoop` were removed from the `K2Node_*` alias map (`ForEachLoop` was wrongly aliased to `K2Node_ForEachElementInEnum`), so the bare names reach the bridge and `TryCreateMacroNode` builds a real `K2Node_MacroInstance`. Material root targets (`root`, `output`, `materialoutput`, `materialgraphnoderoot`, `…_Root_<n>`) canonicalize to one sentinel with normalized output-pin casing for `connect_nodes`, and `propertyValue` is accepted as an alias of `value`.
- **Domain fixes surfaced by the sweep** — a property conversion that cannot coerce now answers `PROPERTY_CONVERSION_FAILED` with `partial: true` for the fields it did apply, unknown World Partition actions answer `UNKNOWN_ACTION` instead of falling through, `modify_scs` reaches the property-applying implementation, `advance_simulation` advances `steps` ticks once instead of `steps` squared, `simplify_mesh` no longer divides by zero on an empty mesh, and the pipeline status report states what it measured instead of a hardcoded value.

#### Fab asset store

- **Fab search no longer hides results** — the search call was pinned to `channels=unreal-engine`, which hid the whole Megascans library; the pin is gone, so the public catalog is discoverable and importability is settled at add time instead.
- **`add_fab_asset_to_project` claims the listing first** — it performs a real `POST /add-to-library` (the same change Fab's own UI makes when you press Add to Project) because Fab answers 404 for a download the account does not own; it then picks the first importable format, and success is decided by the asset registry rather than by Fab's response.
- **Typed Fab failures** — `FAB_NOT_READY`, `FAB_REJECTED`, `NO_IMPORTABLE_FORMAT` and `IMPORT_TIMED_OUT` replace generic refusals, so a caller can tell a missing Fab plugin from a rejected download, an unimportable format, or an import that ran out of time.

</details>

<details>
<summary><b>🗑️ Removed</b></summary>

- **Gateway-mode opt-outs and the legacy 23-tool listing (permanent single-tool cutover).** The TypeScript `MCP_GATEWAY_MODE` env var and the native **Enable Native Gateway** (`bEnableNativeGateway`) project setting are both gone. The private 23-parent dispatch and legacy action mappings stay inside `unreal.execute`. `MCP_AUTOMATION_CLIENT_MODE` is unaffected — it remains a separate WebSocket client/server topology control.
- **Superseded TypeScript modules** — the root-level `src/tools/consolidated-tool-handlers.ts`, `property-dictionary.ts`, `tool-definition-utils.ts`, `src/tools/editor.ts`, `src/tools/level.ts`, `src/tools/schemas/core-tools.ts`, and the monolithic `src/tools/handlers/*-handlers.ts` set, all replaced by the catalog and per-domain directories. `src/tools/orchestration/consolidated-tool-handlers.ts` survives as the bootstrap/export facade, and `src/tools/dynamic/dynamic-tool-manager.ts` is still live: gateway availability and the execute static stage both gate on `isToolEnabled()`, `configure` mutates it, and it backs the local `manage_tools` category state rather than a per-tool public listing.
- **Superseded plugin sources** — the per-tool `McpTool_*.cpp` definitions, `McpDynamicToolManager.cpp`, `McpConsolidatedActionRouting.h`, and the `McpNativeTransport.{h,cpp}` monolith.
- **Dead code sweep (2026-09-05)**: the legacy `src/tools/editor/` and `src/tools/level/` class modules (only their own unit tests imported them; the live paths are `src/tools/handlers/editor` and `src/tools/handlers/level`), the orphan `src/tools/handlers/niagara/` handler (Niagara authoring is served by `effect/effect-niagara-actions.ts`), the stale `material-authoring-types.ts` copy of `material-authoring-common.ts`, the unused `src/types/index.ts` and `src/utils/index.ts` barrels, 33 exported functions and constants with no callers, the security PoC harness under `tests/unit/_poc_security/`, the orphan `evidence-aggregator.mjs`, the `.jules/` sentinel notes for code that no longer exists, and the `lint:c` / `lint:csharp` npm scripts. The generated `McpNativeGatewayManifest.h` (298 KB, never included by any translation unit since the native gateway moved to the generated registry) is no longer emitted; `generate-gateway-manifest.ts` now writes only the TypeScript and JSON manifests.
- **Deep cleanup continuation (2026-09-06)**: `UnrealCommandQueue` lost its unused `retryPolicy` recovery path (no caller ever passed one; a failed command is never re-run), the retired `route:effect:shadowed_stubs` disposition and its `EFFECT_MODULE_ROUTING` evidence path are gone (76 non-public routes, 7 typed removals), the unused `UE_EDITOR_EXE` / `UE_SCREENSHOT_DIR` env keys and the `src/types/tools/tool-*.ts` type modules were dropped, `toFiniteNumber` moved into `type-coercion.ts`, record builders share `SCHEMA_URI`, `V5_0`, `V5_8_P1` and the `str`/`num`/`bool` schema props, and generator scripts share `writeManifestTargets`. `docs/handler-mapping.md` names the files that actually own the foliage, property and sequence-metadata handlers.
- **Second removal pass (2026-09-06)**: the never-wired semantic value grammars (`semantic/frame-time.ts`, `geometry.ts`, `pagination.ts`, their parse helpers and the schema-only test) are gone; `save-policy.ts` and `property-assignment.ts` keep only the schemas the envelope and execution-options modules actually import. Dead exports dropped: `resolveAlias`, `PrimitiveHandler`, `AutomationMessageSchema`, `VerbFamily`, the unused per-parent record aliases, three `z.infer` aliases in execution-options and the six unused `*Response` interfaces. `animation_physics.list_bones` now declares the bone objects the plugin really emits (name, index, parentIndex, parentName, location) instead of `string[]`, so the gateway no longer refuses its result.
- The `typescript@^6` `overrides` block in `package.json`, alongside the toolchain pinning described under *Dependencies*.
- **The experimental in-editor ACP assistant panel is gone.** The separate assistant plugin subtree was removed from this tree and ships in no release; it survives only on feature branches. External consumers that drove the editor through it must target the native `/mcp` surface or the TypeScript stdio `unreal` gateway instead.
- **Superseded repo files (2026-09-06 and later)** — `GEMINI.md`, the root `mcp-config-example.json` and `claude_desktop_config_example.json` (with their `.github/labeler.yml` globs), `tests/heartbeat-progress.test.mjs`, `tests/integration/get_ai_info_characterization.mjs`, `tests/unit/tools/level_security.test.ts`, the unreferenced `Public/Plugin_setup_guide.mp4`, and the local-only `docs/native-automation-progress.md` progress log.

</details>

<details>
<summary><b>⚠️ Migration</b></summary>

- **Direct canonical tool calls are breaking.** Any client calling a canonical name directly (`tools/call` with `name: "manage_asset"`, `name: "control_actor"`, …) now receives a `DIRECT_TOOL_CALL_REMOVED` receipt instead of a result. Update call sites to the `unreal` gateway: `search` to find capabilities, `describe` for the exact action/parameter contract, then `execute` with `tool`, `action`, and `params`. There is no opt-out — `MCP_GATEWAY_MODE` and **Enable Native Gateway** are removed and there is no legacy listing to restore. The receipt's `nextCall` is executable and re-runs the original request through the gateway.
- **`manage_post_process` is folded into `manage_render`.** The `Render/McpAutomationBridge_RenderPostProcess*.cpp` files dispatch through `manage_render`; any client calling `manage_post_process` directly now fails with `does not match prefix`. Switch to `manage_render` and pass the desired sub-action via `subAction`. The reflection-capture resolution setter was renamed from `configure_capture_resolution` to `configure_reflection_capture_resolution`; the scene-capture path keeps the original name. `McpAutomationBridge_RenderHandlers.cpp` is now a 74-line dispatcher, with per-concern handlers under `Render/McpAutomationBridge_Render*.cpp`.
- **`control_actor` spawn with an unresolvable `meshPath` now fails.** A request that previously still produced a spawned actor and a success response now returns `MESH_NOT_FOUND` and spawns nothing.
- **Node.js `>=20.19.0` is required.** Node 18 is no longer supported.

</details>

<details>
<summary><b>🧪 Tests & CI</b></summary>

- **CI gate order is now asserted** by `tests/unit/workflow_gate_order_contract.test.ts`. The pipeline runs: `eslint --max-warnings=0` → `type-check` → `test:unit` → `registry:check` → `normalization:check` → `manifest:check` → `policy:check` → `test:params` → `migration:check` → `primitives:check` → `security:check` → `eval:check` → `version:check` → `workflow:check`, then a blocking `npm audit --omit=dev --audit-level=high` and an informational full-tree audit. A second matrix job (Node 20.19.x + 26.x) adds `build` + `test:smoke`.
- **Source-contract tests** in `tests/unit/plugin/*contracts.test.ts` read the C++ as text and assert required and forbidden patterns: pure-line ceilings, resolvable `Mcp*` includes, absence of split artifacts, constant-time token comparison, and no non-loopback bind without `bRequireCapabilityToken`.
- **Plugin-failure detection is word-bounded and content-scoped** — the generic indicator list that still includes the word `unknown` is unchanged, but a separate hard short-circuit list (which does not) runs before it, and the broad list now scans only the message and error strings rather than the whole response body, so a legitimate dispatcher message such as `"Unknown subAction."` no longer reads as a plugin crash. Real plugin errors are already caught by `isError: true` / `structuredContent.success: false`.
- **The test runner propagates failures via `throw`, not `process.exit(1)`** — it still sets `process.exitCode = 1`, but the error is rethrown so wrappers catching via `try`/`catch` or `Promise.all` see the underlying failure. Consumers that relied on the runner terminating the process from a `catch` block should switch to the rethrow contract.
- Added a `scripts/ci/unreal-job-gate.mjs` job gate and `scripts/qa/` adversarial, cross-transport-matrix, and capability-metadata audits.
- **The integration harness drives the gateway** — `tests/test-runner.mjs` exports `toGatewayCall()`, which rewrites every legacy `{tool, action}` case into an `unreal.execute` call (gateway options lifted into `options`, `action`/`subAction`/`params`/`consent` stripped out of `params`, case-level consent attached as the execute envelope sibling, all timeouts clamped to a 600 s ceiling). Assertions and capture selection moved to `tests/test-runner-response-utils.mjs`, and a missing `${captured:...}` value now throws instead of substituting a placeholder.
- **Crash detection is bounded** — the runner's crash and connection-loss signals moved from substring lists to word-boundary and bounded regexes (`hasCrashConnectionSignal`), with the bare `1006` code dropped as a standalone indicator and explicit close-code and not-connected matches added.
- **New test tiers and gate composition** — `security:check` runs `tests/unit/security` plus `tests/unit/adversarial`, `migration:check` also runs the gateway migration doc contract, and the suite gained adversarial fuzz/shrink/soak harnesses, evidence oracles, engine certification and readiness records, live drivers, cross-transport checks including dist freshness, and an offline native-discovery harness that compiles the real `McpNativeGateway*` sources and generated shards against a minimal engine shim.
- **Doc claims are machine-checked** — `tests/unit/docs/docs-claim-contract.test.ts` audits every published doc and the unreleased section of both changelogs against stale-claim rules (retired public tool surface, the removed in-editor assistant panel, unsupported protocol versions, unbacked certification and engine-range claims, stale capability-record counts), each with a negative control that proves the rule can fail.
- **CI job topology** — an opt-in `package-plugin` job (gated on the engine-root repo variable, with `MCP_STRICT_DEPRECATIONS=1`) and an opt-in `live-matrix` job (which builds and runs the Unreal integration suite on a labelled runner) join the always-on `unreal-optional-status` job that announces which Unreal-dependent jobs were skipped and why; four workflow contract tests plus the release-archive contract guard the pipeline.
- **Packaging writes a SHA-256 manifest and hardens the archive** — `scripts/lib/package-manifest.mjs` writes `McpAutomationBridge-v<version>-UE<engine>-<platform>.manifest.json` beside the archive, the archive additionally excludes `.cache/` and `DerivedDataCache/` and prunes `*.pdb`, `*.debug`, `*.sym` and `*.dSYM`, and a post-archive check fails the build if a generated build directory slipped in.

</details>

<details>
<summary><b>📚 Documentation</b></summary>

- **21 `AGENTS.md` files (20 area guides plus the root workspace guide)** now cover the tree: catalog, tools, handlers, gateway, MCP primitives, server, automation, utils, resources, types, plugin scope, plugin core/domains/safety/native-MCP/foundation/transport, tests, and the two test-area guides (`tests/unit/plugin/`, `tests/unit/mcp-primitives/`).
- **Gateway migration and protocol docs** — the permanent single-`unreal` surface on both transports (the former `MCP_GATEWAY_MODE` and **Enable Native Gateway** toggles were removed, not merely defaulted off), the `DIRECT_TOOL_CALL_REMOVED` receipt, `2025-11-25` negotiation with the `MCP-Protocol-Version` header guard, and the manifest generate/`--check` workflow.
- Published a generated action reference, migration map, and capability support matrix from the capability records.
- **The `bump-version` workflow rewrites all seven version sources** — `package.json` and `package-lock.json`, `server.json`, `McpAutomationBridge.uplugin`, `Resources/MCP/server-info.json`, the `src/server/server-factory.ts` fallback and the `McpNativeTransport.h` `ServerVersion` literal, then verifies with `npm ci && npm run version:check`. Release and plugin archives exclude `Binaries/`, `Intermediate/`, `Saved/`, `.cache/` and `DerivedDataCache/`, prune debug symbols, and fail the build when a generated build directory survives into the archive.

</details>

<details>
<summary><b>🔄 Dependencies</b></summary>

| Package | Change |
|---------|--------|
| `@modelcontextprotocol/sdk` | `^1.25.0` → pinned `1.29.0` |
| `eslint` | `^10.0.2` → pinned `9.39.5` |
| `@eslint/js` | `^10.0.1` → pinned `9.39.5` |
| `@typescript-eslint/{eslint-plugin,parser}` | `^8.4x` → pinned `8.63.0` |
| `typescript` | `^6.0.2` → pinned `5.9.3` (and the `overrides` block removed) |
| `@types/node` | `^25.0.2` → `^26.0.1` |
| `eslint-plugin-n`, `js-yaml` | added (dev) |

</details>

<details>
<summary><b>✅ Verification</b></summary>

- **Supports Unreal Engine 5.0–5.8.** The range is a source-compatibility target: per-version build and live-editor results are not asserted here. See [docs/performance-and-evidence.md](docs/performance-and-evidence.md) for the engine matrix and what each version's record actually shows.
- **Live-editor acceptance is not claimed for the TypeScript gateway build.** Gateway behavior, `2025-11-25` negotiation, manifest generation, and parity/parameter audits are verified through source-contract tests and the build, not against a running Unreal Editor. The integration suite (`npm test`) requires a live editor plus the bridge plugin and runs only in the opt-in `live-matrix` CI job; it is skipped by default rather than excluded. Do not treat any unexecuted live-editor proof as verified.

</details>

<details>
<summary><b>📊 Change Statistics</b></summary>

| Metric | Count |
|--------|-------|
| Diff range | `v0.5.30..HEAD` |
| Commits since the tag | 953 |
| Files changed | 3,149 |
| Insertions / deletions | 772,137 / 204,019 |
| Capability records | 379 |
| Folded families | 244 across 22 parents (222 selector-dispatched) |
| Callable `{tool, action}` pairs | 1,545 (1,379 shipped names, 164 new family primaries, plus package_project and package_status) |
| Canonical parent tools (internal) | 23 |
| Public MCP tools | 1 (`unreal`) |
| C++ domain directories | 66 |
| TypeScript handler domains | 37 |
| Gateway routing modules | 26 |
| `AGENTS.md` files | 21 (20 area guides plus the root guide) |

> Insertion counts are dominated by committed generated artifacts (`capabilities/generated/`, native shards, manifests) and are not a useful measure of hand-written change.

</details>

---

## 🏷️ [0.5.30] - 2026-06-05

> [!IMPORTANT]
> ### 🚀 Native MCP & Code-Backed Tool Parity Release
> This release covers the `v0.5.21` to `0.5.30` release diff, including the TypeScript MCP server, native bridge plugin, tests, scripts, docs, workflows, and dependency manifests. The summary below is based on code and test changes, not commit subjects alone.

<details>
<summary><b>✨ Added</b></summary>

- **Native MCP Streamable HTTP endpoint** — added an opt-in in-plugin `/mcp` server with JSON-RPC 2.0 initialize/tools/list/tools/call handling, POST/GET/DELETE routing, `Mcp-Session-Id` session tracking, SSE tool-result streaming, progress notifications, persistent notification streams, `notifications/tools/list_changed` broadcasts, CORS handling, loopback-first binding, capability-token checks, and an editor status-bar indicator.
- **Self-describing native MCP tools** — added C++ `FMcpToolRegistry`, `FMcpSchemaBuilder`, `MCP_REGISTER_TOOL`, canonical native tool filtering, cached schema generation, and native dynamic tool/category enablement for the 23 canonical parent tools.
- **PCG automation** — added `manage_pcg` TypeScript/native schemas and handlers for graph/subgraph creation, PCG node aliases, pin connections, reflected node settings, component/world execution, partition grid configuration, save/overwrite behavior, and PCG plugin availability errors.
- **Environment systems automation** — added build-environment coverage for heightmap import/export, landscape layer info/material/splines/LOD/streaming proxies, foliage type configuration/paint/remove flows, sky and volumetric-cloud setup, weather/wind/time-of-day systems, water bodies, water waves/material/collision, and buoyancy components.
- **Behavior Tree authoring and introspection** — added `add_subnode`, root-sentinel decorators, decorator/service validation, subnode-aware lookup, `FBlackboardKeySelector` assignment, and `get_tree` runtime hierarchy serialization with root decorators, edge decorators, decorator ops, services, key properties, subtree references, and a success-with-null-root contract for graphless trees.
- **Blueprint, property, and inspection tools** — added `inspect_cdo`, Class Default Object component/property export, SCS and inherited SCS component classification, typed Blueprint custom-event pins, Enhanced Input graph nodes, inherited variable/member-class graph node lookup, and property access for Blueprint-added SCS component templates.
- **Editor, world, and input capabilities** — added full editor-window screenshots, game viewport screenshot routing, image content responses, simulated keyboard/mouse input aliases, active camera reporting, PIE runtime inspection, native `get_current_level`, actor material/view-target native actions, spawn scale support, and create-plane height handling.
- **Material, audio, animation, and system actions** — added Material Function creation/editing/calls/info, FunctionInput/FunctionOutput graph support, source-effect chains and source-effect presets, `force_rebuild_blend_space`, legacy/per-key input mapping edits, project setting writes, native asset validation, and `execute_python` for inline or project-local Python files.

</details>

<details>
<summary><b>🛡️ Security</b></summary>

- **GraphQL attack surface removed** — deleted the GraphQL server, schema/resolver/loaders, GraphQL docs, GraphQL unit tests, and direct GraphQL runtime dependencies.
- **Native MCP exposure controls** — default native MCP binding stays loopback-only unless explicitly allowed; non-loopback hosts warn, sessions are validated, stale requests/streams are cleaned up, and native HTTP requests use explicit request-origin routing instead of socket inference.
- **Capability-token and dynamic-tool protections** — native MCP validates `X-MCP-Capability-Token` when required, while both TypeScript and native dynamic tool managers protect `manage_tools`/`inspect` and protected categories from accidental disablement.
- **Python execution hardening** — `execute_python` enforces code/file exclusivity, a 1 MB inline code limit, project-root path normalization, symlink escape checks, `__file__` setup for file execution, temp-file cleanup, and direct `PythonScriptPlugin` execution.
- **Path, command, log, and workflow hardening** — tightened UE path normalization, console-command validation, snapshot/log path handling, level save/load flows, image/log redaction, safe `tmp/` cleanup, sync-script argument parsing, and GitHub Actions interpolation by moving untrusted values into environment variables.

</details>

<details>
<summary><b>🔧 Changed</b></summary>

- **Release metadata** — updated `package.json`, `package-lock.json`, `server.json`, the `src/index.ts` fallback, and `McpAutomationBridge.uplugin` to `0.5.30`.
- **Canonical TypeScript tool surface** — kept the 23 parent tools but moved action lists into shared constants, grouped tools into `core`, `world`, `gameplay`, and `utility`, merged nested `params` into top-level arguments for constrained clients, centralized handler routing, and removed legacy per-domain tool files.
- **Dynamic tool listing** — `tools/list` now checks known client support for `tools.listChanged`; dynamic clients can receive category-filtered tools, while clients without dynamic loading still see the full compatible tool surface.
- **Automation bridge lifecycle** — refactored host/port parsing, multi-port WebSocket connection attempts, handshake metadata, request queueing, progress timeout extension, stale-progress detection, absolute timeout caps, rate/message-size boundaries, disconnect/error tracking, and image-payload redaction.
- **Native bridge runtime** — split request dispatch out of the subsystem, added explicit `ERequestOrigin`, queued requests through the game thread, converted captured engine errors into failed responses, pumped GameThread tasks during native transport shutdown, and exposed native transport session/tool counts to UI.
- **Response and schema handling** — improved response validation, summary text generation, image response content, scalar result promotion, safe JSON cleanup, schema reuse, action-specific parameter descriptions, and stricter error context on tool failures.
- **Plugin compatibility** — updated bridge metadata for UE 5.8 Preview and added PythonScriptPlugin, StructUtils, Synthesis, and PCG plugin declarations where the new handlers need them.
- **Scripts and workflows** — made smoke tests run through SDK `InMemoryTransport`, added native parity/parameter audit npm scripts, changed `clean` to remove `tsconfig.tsbuildinfo`, added Linux/macOS/Windows plugin packaging scripts, strengthened sync/cleanup scripts, and made CI/publish/release gates stricter.

</details>

<details>
<summary><b>🛠️ Fixed</b></summary>

#### Routing & Native Tool Parity

- Fixed native/consolidated action routing for validation, audio creation, material graph pins, editor simulation, `add_widget_child`, `get_current_level`, AnimBP graph discovery, lighting, SCS edits, native actor/editor actions, exact action matching, and nested `params` payloads.

#### Blueprint, Graph & Property Handling

- Fixed inherited UPROPERTY lookup for VariableGet/VariableSet with `memberClass`, stale `K2Node_EnhancedInputAction` title cache refresh, Blueprint SCS component introspection, SCS template get/set paths, typed custom-event pin reconstruction, transaction ordering, null pin checks, case-insensitive pin connections, graph allocation fallback, and Blueprint busy-state cleanup.

#### Editor, World & Gameplay Behavior

- Fixed PIE/game viewport screenshots, full editor screenshot capture, simulated input dispatch, active camera view-state reporting, PIE runtime inspection, spawn scale application, plane height fields, landscape bounds fallback, prompt-save return codes, actor list response handling, editor/world handler stability, and native actor/editor contract alignment.

#### Asset, Level, Animation, Niagara & Audio

- Fixed unloaded level info via AssetRegistry fallback, classNames-only recursive asset search, normalized level path validation, source audio persistence, source-effect routing, audio authoring saves, material expression aliases, material pin routing to main inputs, UMaterialFunction graph details, animation notify validation, BlendSpace grid rebuilds, Niagara crash paths, hollow getters, parameter aliases, FText and FText-array property serialization, and `execute_python` file mode/output capture.

#### Plugin Stability & Compatibility

- Fixed native plugin compatibility across UE versions, macOS Clang audio literal builds, plugin package output detection, bridge socket/runtime handling, JSON key normalization, request telemetry, native MCP session validation, safe operations includes, handler review findings, and optional module/plugin availability paths.

</details>

<details>
<summary><b>🧪 Tests</b></summary>

- Added/expanded Vitest coverage for automation bridge connection, handshake, message schema, request tracking, config defaults, resources, health/metrics services, response validation, command validation, log reading/redaction, safe JSON, type coercion, normalization, queues, elicitation, and consolidated handler routing.
- Expanded MCP integration suites across core/world/gameplay/utility tools, including PCG, Behavior Tree subnodes/get-tree, networking/sessions/input, control-editor screenshots/input, actor list handling, audio/source effects, assets/material functions, Blueprints/SCS, levels, geometry, GAS, combat, inventory, interaction, sequence, environment, and system-control Python/project-setting flows.
- Added static native MCP action parity auditing and strict parameter-combination auditing that compare TypeScript schemas, native C++ tool definitions, native canonical registration, and test coverage.
- Hardened the custom test runner with richer assertions, captured variables, live/static reports, fake-success detection, progress output, expectation utilities, and deterministic parameter audit behavior.

</details>

<details>
<summary><b>🧰 Maintenance</b></summary>

- Refreshed AGENTS/project guidance, README/setup content, handler maps, testing guide, native automation progress notes, Roadmap, MCP coverage notes, UE 5.8 support notes, native audio routing notes, plugin READMEs, issue templates, labels, gitignore rules, production env defaults, Context7 config, and release metadata.
- Removed obsolete GraphQL API docs and GraphQL security tests with the GraphQL implementation.
- Trimmed unused plugin helpers/includes, streamlined bridge build settings, normalized Node built-in imports, simplified startup cleanup, optimized server utilities/build caching, and improved package/sync/cleanup script safety.

</details>

<details>
<summary><b>🔄 Dependencies</b></summary>

- Removed direct runtime dependencies for `@graphql-tools/schema`, `dataloader`, `graphql`, and `graphql-yoga`.
- Refreshed the lockfile across npm dependency groups, including security/maintenance updates for transitive runtime and dev packages.
- Updated pinned GitHub Actions used by checkout, setup-node, CodeQL, Release Drafter, github-script, action-gh-release, stale, labeler, and dependency-review workflows.

</details>

<details>
<summary><b>📚 Documentation</b></summary>

- Refreshed root and plugin README content, MCP/native transport setup, handler mapping, editor plugin extension notes, Roadmap, testing guide, native automation progress, MCP coverage notes, UE 5.8 support, and native audio routing notes.
- Added and updated repository guidance files for root, TypeScript server/tools/handlers/automation/utils/tests, native MCP internals, and McpAutomationBridge areas.
- Removed obsolete GraphQL API documentation.

</details>

<details>
<summary><b>👥 Contributors</b></summary>

Special thanks to the contributors in this release window, with obvious author aliases collapsed.

- **Native MCP Streamable HTTP endpoint:** Thanks @Fl0p for the native HTTP/SSE transport work.
- **GitHub Actions command-injection hardening:** @google-labs-jules[bot]
- **Dependency and workflow version updates:** @dependabot[bot]
- **Editor/runtime behavior fixes:** @xqdd for native `get_current_level` routing, spawn scale, create-plane height, lighting routing, Blueprint SCS verification, PIE runtime reporting, active camera state, per-key input mapping edits, and prompt-save return handling.
- **Behavior Tree and Blueprint introspection:** @kalihman for `get_tree`, decorator/service subnodes, Blackboard key selector assignment, inherited Blueprint/SCS introspection, and classNames-only asset search behavior.
- **Blueprint graph and animation reliability:** @VictorZhang01 for inherited UPROPERTY graph nodes, stale K2 node title refresh, AnimBP graph routing, and `force_rebuild_blend_space`.
- **Material and Blueprint component support:** @nekwo for Material Function authoring support and Blueprint-added component template property get/set coverage.
- **Plugin packaging and inspection:** @azwjp for Windows plugin packaging fixes, and @6r0m for `inspect_cdo` Blueprint CDO inspection.
- **Platform/build and routing fixes:** @jenniferied for macOS Clang audio-handler build fixes, @Miriam-R-coder for exact Blueprint action routing, and @spencer-zaid for screenshot, Niagara, hollow getter, and parameter-alias fixes.
- **Python and level metadata fixes:** @zmarx for `execute_python` file-mode handling and Python plugin initialization guards, and @codeman101 for unloaded `get_level_info` AssetRegistry fallback.

</details>

<details>
<summary><b>📊 Release Statistics</b></summary>

| Metric | Count |
|--------|-------|
| Release diff | `v0.5.21..0.5.30` |
| Files changed | 406 |
| Insertions | 61,393 |
| Deletions | 30,957 |
| TypeScript canonical tools | 23 |
| Native canonical tools | 23 |
| Native action parity mismatches | 0 |

</details>

---

## 🏷️ [0.5.21] - 2026-04-03

> [!IMPORTANT]
> ### 🔒 Security, New Features & Major Crash Fixes
> This release adds custom content mount points, full audio authoring, project settings management, vehicle physics configuration, blend tree/procedural animation/state machine creation, sequencer improvements, and critical crash prevention for deleting animation/IK assets and folders.

<details>
<summary><b>🛡️ Security</b></summary>

- **Command Injection in bump-version action** – Sanitized `release-type` input ([#327](https://github.com/ChiR24/Unreal_mcp/pull/327))
- **Command Injection in editor console commands** – Mixed-context sanitization for `start_recording`, `set_camera_fov`, `set_game_speed` ([#322](https://github.com/ChiR24/Unreal_mcp/pull/322))
- **Path Traversal in `export_level`** – Added path validation ([#305](https://github.com/ChiR24/Unreal_mcp/pull/305))
- **Path Traversal in screenshot filename** – Sanitized filenames, blocked traversal patterns ([#314](https://github.com/ChiR24/Unreal_mcp/pull/314))
- **Synchronous fs Hardening** – Replaced blocking `fs.existsSync` / `fs.readdirSync` with async versions ([#318](https://github.com/ChiR24/Unreal_mcp/pull/318))

</details>

<details>
<summary><b>✨ Added</b></summary>

- **Custom Content Mount Points** – `MCP_ADDITIONAL_PATH_PREFIXES` to whitelist plugin mount points (`/ProjectObject/`, etc.) ([#326](https://github.com/ChiR24/Unreal_mcp/pull/326) – thanks @6r0m)
- **Full Audio Authoring** – Create sound waves, sound cues, sound classes, sound mixes, attenuation settings; success flags in responses.
- **Project Settings Management** – New `manage_project_settings` tool (get/set project settings via config).
- **Animation Authoring** – `create_blend_tree`, `create_procedural_anim`, `create_state_machine` (C++ implementations, not console commands).
- **Vehicle Physics Configuration** – `configure_vehicle` with wheels, engine, transmission, mass, drag coefficient.
- **Sequencer** – `set_tick_resolution`, `set_view_range` actions.
- **Widget Authoring** – New template widgets: main menu, pause menu, HUD, crosshair, ammo counter, health bar, compass, interaction prompt, objective tracker, damage indicator, inventory grid, dialog box, radial menu, credits scroll, shop UI, quest tracker.
- **Runtime Module Checks** – Verify GameplayAbilities, EnhancedInput, BehaviorTreeEditor, LevelSequenceEditor, NiagaraEditor, StateTree, SmartObjects, MassEntity are loaded before use (clear error messages when plugins missing).

</details>

<details>
<summary><b>🛠️ Fixed</b></summary>

#### Crash Prevention (UE 5.7+)

- **Animation/Rig asset deletion** – Completely rewrote `McpSafeDeleteFolder` and added `DeleteAnimationRigClusterOrdered` to prevent 0xFFFFFFFFFFFFFFFF crashes when deleting AnimBlueprints, IKRigs, IKRetargeters, ControlRigBlueprints, and AnimSequences.
- **Folder deletion** – Replaced `UEditorAssetLibrary::DeleteDirectory` with `McpSafeDeleteFolder` (proper world switching, package unloading, compilation quiesce).
- **Blueprint creation** – Added pre‑creation checks in `CreateControlRigBlueprint` and widget blueprint creation to prevent engine assertion failures.
- **Widget creation** – Fixed widget crash ([#306](https://github.com/ChiR24/Unreal_mcp/pull/306)) by adding GUID registration (`RegisterWidgetGuid`) and safe tree replacement (`SafeAddWidgetToTree`).
- **AnimNotify/NotifyState** – Added abstract class validation and track existence checks.

#### Asset & Path Handling

- Improved asset loading reliability for newly created AI assets (removed stale `DoesAssetExist` checks).
- Resolved asset query parameter bugs and expanded `classNames` support ([#311](https://github.com/ChiR24/Unreal_mcp/pull/311)).
- Replaced custom asset directory checks with `UEditorAssetLibrary` to avoid stale cache issues.
- Fixed `searchText` filtering in `search_assets` action ([#308](https://github.com/ChiR24/Unreal_mcp/pull/308)).
- Added `offset` pagination to asset search.

#### Blueprint & Graph Editing

- Unified pin serialization across blueprint graph handlers ([#309](https://github.com/ChiR24/Unreal_mcp/pull/309)) – linked pins returned as objects with `nodeId` and `pinName`.
- Improved actor lookup to match subsystem behavior (checks both label and name).
- Aligned `get_ai_info` output with TypeScript schema ([#310](https://github.com/ChiR24/Unreal_mcp/pull/310)).

#### Performance & Console

- Delegated console command settings to C++ handler for better performance.
- Ensured successful execution of console commands (check `GEngine->Exec` return value).
- Added validation for required session parameters (interfaceType, controllerId, playerIndex, etc.).
- Removed redundant `AsyncTask` wrappers in `generate_thumbnail` and `generate_lods` (fixed 30‑second timeout).

#### Level Operations

- **`rename_level`** – Now uses `DuplicateAsset` + `DeleteAsset` to avoid modal “Find/Replace” dialog.
- **`duplicate_level`** – Validates source existence and deletes destination if already present.
- **`export_level`** – Added source level existence check before export.

#### Voice Chat & Sessions

- Improved `mute_player` – falls back to `BlockPlayers` when voice server not connected.
- Added validation for required parameters in all session actions.

#### Plugin Stability

- Used delay‑load for optional plugin modules to prevent missing dependency errors ([#317](https://github.com/ChiR24/Unreal_mcp/pull/317)).
- Refactored IK retargeter initialization using controller API (UE 5.7+) with backward compatibility fallback.
- Enhanced actor and component stability across subsystems.

#### Documentation

- Fixed rate limiting defaults and missing GraphQL heading ([#307](https://github.com/ChiR24/Unreal_mcp/pull/307)).

</details>

<details>
<summary><b>🔄 Dependencies</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `picomatch` | 4.0.3 → 4.0.4 | [#316](https://github.com/ChiR24/Unreal_mcp/pull/316) |
| Dependencies group | 9 updates | [#320](https://github.com/ChiR24/Unreal_mcp/pull/320) |
| `github/codeql-action` | 4.33.0 → 4.34.1 | [#319](https://github.com/ChiR24/Unreal_mcp/pull/319) |

</details>

<details>
<summary><b>👥 Contributors</b></summary>

- @google-labs-jules[bot] for all security fixes
- @kalihman for asset query, searchText, docs, and blueprint graph fixes
- @dependabot[bot] for dependency updates
- @6r0m for custom content mount points (first contribution)

</details>

---

## 🏷️ [0.5.20] - 2026-03-21

> [!IMPORTANT]
> ### 🛡️ Security Fix & UE 5.0 Compatibility
> This release includes a critical path traversal fix in export_asset, UE 5.0 compatibility improvements, and external actors support for World Partition.

### 🛡️ Security

<details>
<summary><b>🔒 Path Traversal in export_asset</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/5cf2a3c">5cf2a3c</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Path traversal in `export_asset` action |
| **Fix** | Added path validation to prevent directory traversal attacks |

**Files Modified:**
- `McpAutomationBridge_SystemControlHandlers.cpp`

</details>

### ✨ Added

<details>
<summary><b>🌍 External Actors Support</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/51143c3">51143c3</a>)</summary>

| Feature | Description |
|---------|-------------|
| **External Actors** | Support for World Partition external actors in level structure handlers |
| **Streaming Reference** | Streaming reference creation for external actor packages |

**Files Modified:**
- `McpAutomationBridge_LevelStructureHandlers.cpp` (+127 lines)

</details>

### 🛠️ Fixed

<details>
<summary><b>🎮 UE 5.0 Compatibility</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/1057023">1057023</a>)</summary>

| Bug | Fix |
|-----|-----|
| `bIsWorldInitialized` API not available in UE 5.0 | Direct access to `bIsWorldInitialized` for UE 5.0 compatibility |

**Files Modified:**
- `McpAutomationBridgeHelpers.h`
- `McpAutomationBridge_LevelStructureHandlers.cpp`

</details>

<details>
<summary><b>🐛 Tick Task Manager Crashes</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/8c311d7">8c311d7</a>)</summary>

| Bug | Fix |
|-----|-----|
| Crashes from tick task manager during world operations | Added safety checks and proper cleanup in world management |
| World cleanup issues | Enhanced cleanup with `FlushRenderingCommands` safety |

**Files Modified:**
- `McpAutomationBridgeHelpers.h` (+36 lines)
- `McpAutomationBridge_LevelStructureHandlers.cpp` (+65 lines)
- `McpSafeOperations.h` (+16 lines)

</details>

<details>
<summary><b>🐛 Sublevel Creation</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/bffb68c">bffb68c</a>)</summary>

| Bug | Fix |
|-----|-----|
| Sublevel creation path handling issues | Enhanced sublevel creation process with proper path handling |

**Files Modified:**
- `McpAutomationBridge_LevelStructureHandlers.cpp` (+201 lines)

</details>

<details>
<summary><b>🔧 UE 5.7 Build</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/295">#295</a>)</summary>

| Bug | Fix |
|-----|-----|
| Missing includes causing build failures on UE 5.7 | Added missing includes in `McpHandlerUtils.cpp` and `McpPropertyReflection.cpp` |

**Contributors:** @a2448825647

</details>

### 🔄 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | From | To | PR |
|---------|------|-----|-----|
| `release-drafter/release-drafter` | 7.0.0 | 7.1.1 | [#300](https://github.com/ChiR24/Unreal_mcp/pull/300) |
| `softprops/action-gh-release` | 2.5.3 | 2.6.1 | [#301](https://github.com/ChiR24/Unreal_mcp/pull/301) |
| `github/codeql-action` | 4.32.6 | 4.33.0 | [#299](https://github.com/ChiR24/Unreal_mcp/pull/299) |

</details>

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | From | To | PR |
|---------|------|-----|-----|
| `flatted` | 3.3.3 | 3.4.2 | [#304](https://github.com/ChiR24/Unreal_mcp/pull/304) |

</details>

### 🔌 Plugin

<details>
<summary><b>MCP Automation Bridge v0.1.3</b></summary>

Updated plugin version to 0.1.3 with all fixes and features from this release.

See [Plugin CHANGELOG](plugins/McpAutomationBridge/CHANGELOG.md) for details.

</details>

---

## 🏷️ [0.5.19] - 2026-03-18

> [!IMPORTANT]
> ### 🛡️ Security Hardening & Major Plugin Refactoring
> This release includes critical security fixes for command injection and path traversal vulnerabilities, a complete deep-level refactoring of 57 C++ handler files with centralized utilities, and removal of the WebAssembly integration.

### 🛡️ Security

<details>
<summary><b>🔒 Command Injection Prevention</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/288">#288</a>)</summary>

| Component | Change |
|-----------|--------|
| **sanitizeCommandArgument()** | Added semicolon sanitization to prevent command chaining attacks |
| **Physics Tools** | Sanitized constraint names, actor names, vehicle names, destruction names |
| **Animation Tools** | Sanitized state machine names, state names, transition conditions |
| **System Handlers** | Sanitized vehicle type, save paths, and all user-provided strings |

**Attack Vector Blocked:** Input like `"name;quit"` can no longer execute arbitrary commands.

</details>

<details>
<summary><b>🔒 Path Traversal Fixes</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/271">#271</a>, <a href="https://github.com/ChiR24/Unreal_mcp/pull/282">#282</a>)</summary>

| Component | Change |
|-----------|--------|
| **validateSnapshotPath()** | Fixed bypass where paths equal to CWD (without trailing separator) were incorrectly rejected |
| **Asset Handlers** | Added path sanitization to prevent traversal attacks |
| **Blueprint Creation** | Added savePath sanitization |

</details>

<details>
<summary><b>🔒 GraphQL CORS Hardening</b></summary>

| Component | Change |
|-----------|--------|
| **Default CORS** | Changed from permissive `'*'` to safe loopback origins |
| **Allowed Origins** | `localhost:4000`, `127.0.0.1:4000`, `localhost:3000`, `127.0.0.1:3000` |
| **Warning** | Added security warning when `'*'` is explicitly configured |

</details>

### 🔧 Changed

<details>
<summary><b>🏗️ Complete C++ Plugin Refactoring</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/280">#280</a>)</summary>

Deep line-by-line refactoring of 57 handler files and 8 infrastructure files:

| New Infrastructure File | Purpose |
|------------------------|---------|
| `McpHandlerUtils.h/cpp` | Standardized JSON response builders (1,900 lines) |
| `McpPropertyReflection.h/cpp` | Property reflection utilities (1,356 lines) |
| `McpSafeOperations.h` | Safe asset/level save for UE 5.7 (659 lines) |
| `McpVersionCompatibility.h` | UE 5.0-5.7 API compatibility macros (225 lines) |
| `McpHandlerDeclarations.h` | Forward declarations (844 lines) |
| `McpAutomationBridge_ConsoleCommandHandlers.cpp` | Batch and single command execution (302 lines) |

**Bugs Fixed During Refactoring:**
- EditorFunctionHandlers: use-after-free bug
- EffectHandlers: truncated condition + missing braces
- InventoryHandlers: duplicate TArray with undefined variables
- MaterialAuthoringHandlers: duplicate include + missing UE 5.0 fallback
- NavigationHandlers: case-sensitivity error
- SkeletonHandlers: duplicate verification + redundant code
- WidgetAuthoringHandlers: unreachable code block

</details>

<details>
<summary><b>⚡ Performance Improvements</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/283">#283</a>)</summary>

| Component | Change |
|-----------|--------|
| **Batch Console Commands** | New batch execution API for parallel command processing |
| **validate_assets** | Changed from sequential to `Promise.all` concurrent validation |
| **State Machine Creation** | States now added in parallel instead of sequentially |

</details>

<details>
<summary><b>🗑️ WebAssembly Integration Removed</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/240">#240</a>)</summary>

Removed WebAssembly (wasm-pack/Rust) integration:
- Deleted `src/wasm/` directory (874 lines)
- Deleted `wasm/` Rust crate (1,500+ lines)
- Removed WASM dependency from all handlers
- Native C++ handlers provide equivalent functionality

</details>

### 🛠️ Fixed

<details>
<summary><b>🐛 Blueprint Inspect Crash</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/270">#270</a>)</summary>

| Bug | Fix |
|-----|-----|
| Blueprint inspect crashed when variable list exceeded buffer | Fixed truncated variable list handling |
| Function library blueprints not supported | Added function library blueprint support (#258) |

</details>

<details>
<summary><b>🐛 GAS Duplicate Effect Creation</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/251">#251</a>)</summary>

| Bug | Fix |
|-----|-----|
| `create_gameplay_effect` assertion failure on duplicates | Prevented duplicate GameplayEffect creation |

</details>

<details>
<summary><b>🐛 Volume Handler Mobility</b></summary>

| Bug | Fix |
|-----|-----|
| Volume attachment failed for movable actors | Added mobility check for target actors in volume handlers |

</details>

<details>
<summary><b>🐛 UE 5.7 Compatibility</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/274">#274</a>)</summary>

| Bug | Fix |
|-----|-----|
| GeometryScript AppendCapsule compile error on UE 5.5+ | Added version guard for segment steps parameter |

</details>

<details>
<summary><b>🐛 Action Name Alignment</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/253">#253</a>)</summary>

| Bug | Fix |
|-----|-----|
| TypeScript action names mismatched C++ handlers | Aligned all action names with C++ handler expectations |

</details>

### 🗑️ Removed

<details>
<summary><b>Deprecated Tool Files</b></summary>

Removed deprecated standalone tool files (consolidated into handlers):
- `src/tools/audio.ts` → `src/tools/handlers/audio-handlers.ts`
- `src/tools/debug.ts` → consolidated into system handlers
- `src/tools/introspection.ts` → `src/tools/handlers/inspect-handlers.ts`
- `src/tools/materials.ts` → `src/tools/handlers/material-authoring-handlers.ts`
- `src/tools/performance.ts` → `src/tools/handlers/performance-handlers.ts`
- `src/tools/ui.ts` → consolidated into widget handlers
- `src/tools/input.ts` → `src/tools/handlers/input-handlers.ts`
- `src/tools/behavior-tree.ts` → consolidated
- `src/tools/engine.ts` → consolidated

</details>

### 🔄 Dependencies

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | From | To | PR |
|---------|------|-----|-----|
| hono | 4.12.0 | 4.12.7 | [#261](https://github.com/ChiR24/Unreal_mcp/pull/261), [#277](https://github.com/ChiR24/Unreal_mcp/pull/277) |
| express-rate-limit | 8.2.1 | 8.3.0 | [#269](https://github.com/ChiR24/Unreal_mcp/pull/269) |
| @types/node | Various updates | | Multiple PRs |

</details>

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | From | To | PR |
|---------|------|-----|-----|
| release-drafter/release-drafter | 6.2.0 | 7.0.0 | [#286](https://github.com/ChiR24/Unreal_mcp/pull/286) |
| softprops/action-gh-release | 2.5.0 | 2.5.3 | [#287](https://github.com/ChiR24/Unreal_mcp/pull/287) |
| actions/setup-node | 6.2.0 | 6.3.0 | [#257](https://github.com/ChiR24/Unreal_mcp/pull/257) |
| github/codeql-action | 4.32.5 | 4.32.6 | [#266](https://github.com/ChiR24/Unreal_mcp/pull/266) |

</details>

### 📊 Statistics

- **Commits:** 55 non-merge commits
- **Files Changed:** 185 files
- **Lines Added:** ~30,280
- **Lines Removed:** ~20,440
- **New C++ Infrastructure:** 5 files (~4,900 lines)
- **Bugs Fixed:** 15+
- **Security Fixes:** 4 critical

---

## 🏷️ [0.5.18] - 2026-02-21

> [!IMPORTANT]
> ### 🔧 Installation, Documentation & Dependency Updates
> This release fixes npm install failures when downloading from GitHub releases, adds first-time project setup guidance, and updates dependencies.

### 🛠️ Fixed

<details>
<summary><b>🐛 npm install failure from release archives</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/215">#215</a>)</summary>

| Issue | Root Cause | Fix |
|-------|------------|-----|
| `npm install` fails with ESLint config error | Release archives excluded `eslint.config.mjs` and other build files | Added `-source` archives with complete build files |
| `prepare` script runs build unnecessarily | Checked only `dist/` existence, not build artifacts | Now verifies `dist/cli.js` and `dist/index.js` exist |
| Deprecated `--ext .ts` flag in lint | ESLint 9.x removed support for `--ext` flag | Removed flag, extensions configured in `eslint.config.mjs` |

**Files Modified:**
- `package.json` (prepare script, lint scripts, removed prebuild)
- `.github/workflows/release.yml` (added source archives, fixed plugin path)
- `README.md` (added Rust/wasm-pack prerequisites)

</details>

### 📚 Documentation

<details>
<summary><b>📖 First-time project open instructions</b> (<a href="https://github.com/ChiR24/Unreal_mcp/commit/112df08">112df08</a>)</summary>

Added guidance for users opening Unreal projects for the first time:
- Explains UE prompt to rebuild missing modules
- Documents expected plugin load failure after first rebuild
- Recommends closing and reopening project to resolve

</details>

### ⬆️ Dependencies

| Package | From | To | PR |
|---------|------|-----|-----|
| hono | 4.11.7 | 4.12.0 | [#213](https://github.com/ChiR24/Unreal_mcp/pull/213) |
| ajv | 8.17.1 | 8.18.0 | [#210](https://github.com/ChiR24/Unreal_mcp/pull/210) |
| actions/stale | 10.1.1 | 10.2.0 | [#208](https://github.com/ChiR24/Unreal_mcp/pull/208) |
| actions/dependency-review-action | 4.8.2 | 4.8.3 | [#212](https://github.com/ChiR24/Unreal_mcp/pull/212) |

---

## 🏷️ [0.5.17] - 2026-02-16

> [!IMPORTANT]
> ### 🔧 World Tools Category Fixes & Security Hardening
> This release includes critical bug fixes, security hardening, and UE 5.7 compatibility improvements across all world-building tools (landscape, foliage, geometry, volumes, navigation).

### 🛡️ Security

<details>
<summary><b>🔒 Path Validation & Input Sanitization</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/207">#207</a>)</summary>

| Component | Change |
|-----------|--------|
| **SanitizeProjectRelativePath** | Rejects Windows absolute paths, normalizes slashes, collapses `//`, requires valid UE roots (`/Game`, `/Engine`, `/Script`) |
| **SanitizeProjectFilePath** | File operations with path traversal protection |
| **ValidateAssetCreationPath** | Combines folder + name validation for asset creation |
| **Actor/Volume Name Validation** | Blocks invalid characters, enforces length checks |
| **Snapshot Path Validation** | Prevents directory traversal attacks via snapshot paths |

**Files Modified:**
- `McpAutomationBridgeHelpers.h` (+326 lines of security helpers)
- `src/tools/environment.ts` (snapshot path security)
- `src/utils/path-security.ts` (path normalization)

</details>

### 🛠️ Fixed

<details>
<summary><b>🐛 Landscape Handler Silent Fallback Bug</b> (McpAutomationBridge_LandscapeHandlers.cpp)</summary>

| Bug | Root Cause | Fix |
|-----|------------|-----|
| False positives on non-existent landscapes | Path matching compared `GetPathName()` (internal path) with asset path | Normalized both paths with `.uasset` stripping |
| Silent fallback to single landscape | `if (!Landscape && LandscapeCount == 1)` used any available landscape | Removed fallback, now returns `LANDSCAPE_NOT_FOUND` error |
| Wrong response path | Returned requested path instead of actual path | Now returns `Landscape->GetPackage()->GetPathName()` |

**Affected Handlers:** `HandleModifyHeightmap`, `HandlePaintLandscapeLayer`, `HandleSculptLandscape`, `HandleSetLandscapeMaterial`

</details>

<details>
<summary><b>🐛 Rotation Yaw Bug</b> (McpAutomationBridge_LightingHandlers.cpp:200)</summary>

| Bug | Fix |
|-----|-----|
| `Rotation.Yaw` read from `LocPtr` instead of `RotPtr` | Changed to `GetJsonNumberField((*RotPtr), TEXT("yaw"))` |

**Impact:** Incorrect rotation when spawning lights with rotation parameters.

</details>

<details>
<summary><b>🐛 Integer Overflow in Heightmap Operations</b> (McpAutomationBridge_LandscapeHandlers.cpp:631-635)</summary>

| Bug | Fix |
|-----|-----|
| `static_cast<int16>(CurrentHeights[i])` overflows for values > 32767 | Changed to `static_cast<int32>` |

**Impact:** Heightmap raise/lower operations now produce correct results for heights above midpoint.

</details>

<details>
<summary><b>🐛 set_curve_key Success Reporting</b> (McpAutomationBridge_AnimationHandlers.cpp:2139)</summary>

| Bug | Fix |
|-----|-----|
| `bSuccess` initialized `false`, only set `true` inside `if (bSuccess)` block (unreachable) | Moved success logic before the condition check |

**Impact:** `set_curve_key` now correctly reports success.

</details>

<details>
<summary><b>🐛 CraftingSpeed Truncation</b> (McpAutomationBridge_InventoryHandlers.cpp:2716)</summary>

| Bug | Fix |
|-----|-----|
| `int32 CraftingSpeed` truncated fractional multipliers (1.5 → 1) | Changed to `double` |

</details>

<details>
<summary><b>🐛 Invalid Color Fallback Not Applied</b> (McpAutomationBridge_LightingHandlers.cpp:277)</summary>

| Bug | Fix |
|-----|-----|
| `SetLightColor()` only called when `bColorValid == true`, but `bColorValid = false` for invalid colors | Removed guard, always call `SetLightColor()` after correcting invalid colors to white |

</details>

<details>
<summary><b>🐛 Double-Validation in Snapshot Path</b> (src/tools/environment.ts:253, 322)</summary>

| Bug | Fix |
|-----|-----|
| Redundant second `validateSnapshotPath()` call on already-resolved absolute paths | Removed redundant call |

</details>

<details>
<summary><b>🐛 Intel GPU Driver Crash Prevention</b> (McpAutomationBridgeHelpers.h)</summary>

| Bug | Fix |
|-----|-----|
| `MONZA DdiThreadingContext` exceptions on Intel GPUs during level save | Added `McpSafeLevelSave` helper with `FlushRenderingCommands` and retry logic |

</details>

### ✨ Added

<details>
<summary><b>🛤️ LOD Generation Enhancements</b> (McpAutomationBridge_GeometryHandlers.cpp)</summary>

| Feature | Description |
|---------|-------------|
| **landscapePath support** | LOD generation now accepts single `landscapePath` or array `assetPaths` |
| **lodCount parameter** | Alternative to `numLODs` for specifying LOD count |
| **Path sanitization** | All LOD operations use `SanitizeProjectRelativePath` |

</details>

<details>
<summary><b>🌿 FoliageType Auto-Creation</b> (McpAutomationBridge_FoliageHandlers.cpp)</summary>

| Feature | Description |
|---------|-------------|
| **Auto-create FoliageType** | When painting/adding foliage, FoliageType is automatically created from StaticMesh if missing |
| **Path validation** | All foliage operations use path sanitization |

</details>

<details>
<summary><b>🏔️ Landscape Layer Auto-Creation</b> (McpAutomationBridge_LandscapeHandlers.cpp)</summary>

| Feature | Description |
|---------|-------------|
| **Auto-create layers** | When painting, landscape layers are auto-created if they don't exist (matches UE editor behavior) |

</details>

<details>
<summary><b>📊 Handler Verification</b> (Multiple Handler Files)</summary>

| Pattern | Description |
|---------|-------------|
| **AddActorVerification** | Returns `actorPath`, `actorName`, `actorGuid`, `existsAfter`, `actorClass` |
| **AddComponentVerification** | Returns `componentName`, `componentClass`, `ownerActorPath` |
| **AddAssetVerification** | Returns `assetPath`, `assetName`, `existsAfter`, `assetClass` |
| **VerifyAssetExists** | Verifies asset exists at path |

**Files Updated:** PropertyHandlers, LevelHandlers, EffectHandlers, GASHandlers, SequenceHandlers, SkeletonHandlers, and 30+ additional handler files

</details>

### 🔧 Changed

<details>
<summary><b>🎮 UE 5.7 Compatibility</b></summary>

| Component | Change |
|-----------|--------|
| **WebSocket Protocol** | `GetProtocolType()` (FName) replaces deprecated `GetProtocolFamily()` (enum) |
| **SCS Save** | `McpSafeAssetSave` replaces `SaveLoadedAssetThrottled` to prevent recursive `FlushRenderingCommands` crashes |
| **PostProcessVolume** | Conditionally compiled (removed in UE 5.7) |
| **Niagara Graph** | Initialize `GraphSource`/`NiagaraGraph` to prevent null graph crashes |
| **Landscape Edit** | `FLandscapeEditDataInterface` for UE 5.5+, deprecation suppression for 5.0-5.4 |
| **WorldPartition** | Support `RuntimeHashSet` in addition to `RuntimeSpatialHash` for UE 5.7+ |

</details>

<details>
<summary><b>📈 Performance Improvements</b></summary>

| Component | Change |
|-----------|--------|
| **Heightmap Modification** | Pass `false` to `FLandscapeEditDataInterface` to prevent 60+ second GPU sync delays |
| **Landscape Updates** | Use `MarkPackageDirty` instead of `PostEditChange` to avoid unnecessary rebuilds |
| **Geometry Operations** | Memory pressure checks and triangle limits to prevent OOM crashes |

</details>

### 📊 Statistics

- **Files Changed:** 70 files
- **Lines Added:** ~7,200
- **Lines Removed:** ~1,400
- **Bug Fixes:** 8 critical bugs
- **New Verification Helpers:** 4

---

## 🏷️ [0.5.16] - 2026-02-12

> [!IMPORTANT]
> ### 🚀 Major Feature Release: 200+ Action Handlers
> This release adds ~200 new C++ automation sub-actions across all domains, introduces progress heartbeat protocol for long-running operations, dynamic tool management, IPv6 support, and comprehensive security hardening.

### ✨ Added

<details>
<summary><b>🎮 200+ MCP Action Handlers</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/200">#200</a>)</summary>

| Domain | New Actions |
|--------|-------------|
| **AI** | 50+ actions for EQS, Perception, State Trees, Smart Objects |
| **Combat** | Weapons, projectiles, damage, melee combat |
| **Character** | Character creation, movement, advanced locomotion |
| **Inventory** | Items, equipment, loot tables, crafting |
| **GAS** | Gameplay Ability System: abilities, effects, attributes |
| **Audio** | MetaSounds, sound classes, dialogue |
| **Materials** | Material expressions, landscape layers |
| **Textures** | Texture creation, compression, virtual texturing |
| **Levels** | 15+ new sub-actions for level management |
| **Volumes** | 18 volume types |
| **Performance** | Profiling, optimization, scalability |
| **Input** | Enhanced Input Actions & Contexts |
| **Interaction** | Interactables, destructibles, triggers |
| **Misc** | System control, tests, logs |

**New Handler Files:**
- `McpAutomationBridge_CharacterHandlers.cpp` (337 lines)
- `McpAutomationBridge_CombatHandlers.cpp` (398 lines)
- `McpAutomationBridge_SystemControlHandlers.cpp` (324 lines)
- `McpAutomationBridge_MiscHandlers.cpp` (1010 lines)
- `McpAutomationBridge_WidgetAuthoringHandlers.cpp` (2404 lines)

</details>

<details>
<summary><b>💓 Progress Heartbeat Protocol</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/201">#201</a>)</summary>

| Feature | Description |
|---------|-------------|
| **Progress Updates** | C++ sends `progress_update` WebSocket messages during long-running operations |
| **Deadline Extensions** | TS extends request deadlines on each update with deadlock safeguards |
| **Stale Detection** | Detects same percentage for 3 consecutive updates |
| **Absolute Cap** | 5-minute maximum extension limit |
| **Max Extensions** | 10 extensions per request |

**Timeout Changes:**
- Default request timeout: 60s → 30s (extensions handle slow ops)

</details>

<details>
<summary><b>🔧 Dynamic Tool Management</b></summary>

| Feature | Description |
|---------|-------------|
| **manage_tools MCP Tool** | Enables AI to enable/disable tools at runtime |
| **Protected Tools** | `manage_tools`, `inspect`, and core category cannot be disabled |
| **list_changed Notifications** | Tool registry sends MCP notifications when tools change |
| **Category Filtering** | Filter tools by category (core, world, authoring, gameplay, utility) |

</details>

<details>
<summary><b>🌐 IPv6 Support</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/194">#194</a>)</summary>

| Feature | Description |
|---------|-------------|
| **IPv6 Addresses** | Full support for IPv6 addresses in automation bridge |
| **Hostname Resolution** | DNS resolution via `GetAddressInfo` instead of fallback to 127.0.0.1 |
| **Address Family Detection** | Auto-detect IPv6 by checking for colons in address |
| **Zone ID Handling** | Strip zone IDs from IPv6 addresses for Node.js compatibility |
| **Fallback Support** | Re-create socket as IPv4 when IPv6 not available |

</details>

### 🛡️ Security

<details>
<summary><b>🔒 Security Hardening</b></summary>

| Function | Description |
|----------|-------------|
| **SanitizeProjectRelativePath** | Rejects Windows absolute paths, normalizes slashes, collapses `//`, requires valid UE roots |
| **SanitizeAssetName** | Strips SQL injection patterns, invalid characters, enforces 64-char limit |
| **ValidateAssetCreationPath** | Combines folder + name validation |
| **IsValidAssetPath** | Rejects `:` (Windows drive letters) and consecutive slashes |

**TypeScript Security:**
- `src/utils/path-security.ts`: Collapse `//` normalization
- `src/utils/validation.ts`: SQL injection detection

</details>

<details>
<summary><b>🔒 String Escaping Fix</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/202">#202</a>)</summary>

| Issue | Fix |
|-------|-----|
| Incomplete string escaping in path handling | Added proper escaping for special characters |

</details>

### 🔧 Changed

<details>
<summary><b>🎮 UE 5.7 Compatibility Fixes</b></summary>

| Component | Change |
|-----------|--------|
| **WebSocket** | `GetProtocolType()` (FName) replaces `GetProtocolFamily()` (enum) |
| **SCS Save** | `McpSafeAssetSave` prevents recursive `FlushRenderingCommands` crashes |
| **PostProcessVolume** | Conditionally compiled (removed in 5.7) |
| **Niagara** | Initialize `GraphSource`/`NiagaraGraph` to prevent null graph crashes |

</details>

<details>
<summary><b>⚡ Performance & Infrastructure</b></summary>

| Change | Description |
|--------|-------------|
| **Memory Detection** | Windows `GlobalMemoryStatusEx` replaces heuristic detection |
| **Rate Limit** | `MaxAutomationRequestsPerMinute` raised 120 → 600 |
| **Logging** | Improved request/response logging with action name and filtered payload preview |
| **Blueprint Handler** | Variable name collision generates unique suffix, type validation before loading |

</details>

### 🛠️ Fixed

<details>
<summary><b>🐛 Various Fixes</b></summary>

| Fix | Description |
|-----|-------------|
| **~30 handlers** | Handlers that returned `nullptr` now return structured JSON |
| **Blueprint** | Unknown actions return explicit error instead of silent failure |
| **Level tools** | File existence checked before load, post-load path validation |
| **Eject handler** | Changed from stopping PIE to ejecting from possessed pawn |

</details>

### 📊 Statistics

- **Files Changed:** 83 files
- **Lines Added:** ~23,000
- **Lines Removed:** ~2,700
- **New Action Handlers:** ~200
- **New Handler Files:** 5

---

## 🏷️ [0.5.15] - 2026-02-06

> [!NOTE]
> ### 🌐 Network Configuration Release
> This release adds support for non-loopback binding in automation bridge settings, enabling LAN access configuration.

### ✨ Added

<details>
<summary><b>🌐 Non-Loopback Binding Support</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/193">#193</a>)</summary>

| Feature | Description |
|---------|-------------|
| **Non-Loopback Binding** | Automation bridge can now bind to non-loopback addresses (e.g., `0.0.0.0`) for LAN access |
| **Allow Non-Loopback Setting** | New `bAllowNonLoopback` setting in plugin configuration |
| **TypeScript Support** | Added `MCP_AUTOMATION_ALLOW_NON_LOOPBACK` environment variable |
| **Host Validation Tests** | New test suite for bridge host validation |

**Configuration:**
```env
# Enable LAN access
MCP_AUTOMATION_ALLOW_NON_LOOPBACK=true
MCP_AUTOMATION_HOST=0.0.0.0
```

**Security Note:** Only enable on trusted networks with appropriate firewall rules.

</details>

### 🔄 Dependencies

<details>
<summary><b>Dependabot Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `github/codeql-action` | 4.32.1 → 4.32.2 | [#189](https://github.com/ChiR24/Unreal_mcp/pull/189) |
| Dependencies group | 2 updates | [#190](https://github.com/ChiR24/Unreal_mcp/pull/190) |

</details>

### 📊 Statistics

- **Files Changed:** 8 files
- **Lines Added:** ~270
- **Lines Removed:** ~10

---

## 🏷️ [0.5.14] - 2026-02-05

> [!IMPORTANT]
> ### 🔐 TLS & Network Security Release
> This release introduces TLS/SSL support for secure WebSocket connections (`wss://`), per-connection rate limiting, loopback-only network binding enforcement, and authentication state tracking for the Automation Bridge.

### 🛡️ Security

<details>
<summary><b>🔒 Loopback-Only Binding & Handshake Enforcement</b> (<code>70c2745</code>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Loopback Binding** | Automation Bridge now only binds to loopback addresses (127.0.0.1 or ::1) |
| **Handshake Required** | Automation requests require completed `bridge_hello` handshake |

**C++ Plugin:**
- Rejects `0.0.0.0` and `::` bind attempts, falls back to `127.0.0.1` with warning
- Added `AuthenticatedSockets` tracking set in `McpConnectionManager`
- Unauthenticated sockets receive `HANDSHAKE_REQUIRED` error and connection close (code 4004)

**TypeScript Bridge:**
- Added `normalizeLoopbackHost()` to validate and enforce loopback addresses
- Non-loopback host values rejected with warning and fallback

</details>

### ✨ Added

<details>
<summary><b>🔐 TLS/SSL, Rate Limiting & Schema Validation</b> (<code>d2a94cf</code>)</summary>

| Feature | Description |
|---------|-------------|
| **TLS/SSL Support** | Full `wss://` WebSocket support with OpenSSL/TLS integration (TLS 1.2+) |
| **Rate Limiting** | Per-connection limits: configurable, defaults to disabled (0) for development |
| **Schema Validation** | New Zod schemas in `src/automation/message-schema.ts` for type-safe message parsing |

**New Plugin Settings:**
- `bEnableTls`, `TlsCertificatePath`, `TlsPrivateKeyPath` - TLS configuration
- `MaxMessagesPerMinute`, `MaxAutomationRequestsPerMinute` - Rate limit configuration

**C++ Implementation:**
- `InitializeTlsContext()`, `EstablishTls()`, `SendRaw()`, `RecvRaw()` - TLS-aware I/O
- Requires UE 5.7+ for native socket release; graceful fallback on older versions

**TypeScript Integration:**
- Added `rateLimitState` tracking with cleanup on connection close

</details>

### 🛠️ Fixed

<details>
<summary><b>🔧 TLS Memory Management</b> (<code>321206e</code>)</summary>

| Fix | Description |
|-----|-------------|
| **Struct Initialization** | Fixed `FParsedWebSocketUrl` member initialization order (Port using uninitialized `bUseTls`) |
| **SSL Context Ownership** | Added `bOwnsSslContext` to prevent double-free of client contexts owned by `ISslManager` |

</details>

<details>
<summary><b>🔧 Thread Safety & TLS Error Handling</b> (<code>6fd1553</code>)</summary>

| Fix | Description |
|-----|-------------|
| **Mutex Protection** | Added `SocketRateLimits` cleanup in `ForceReconnect` with proper mutex locking |
| **Declaration** | Moved `ShutdownTls()` declaration outside `WITH_SSL` guard for compilation compatibility |

</details>

<details>
<summary><b>🔧 Review Feedback Fixes</b> (<code>8987a3e</code>)</summary>

| Fix | Description |
|-----|-------------|
| **Duplicate Call** | Fixed duplicate `ActiveSockets.Empty()` call in connection manager |
| **TypeScript Cleanup** | Added `rateLimitState` cleanup in `closeAll()` method |

</details>

### 🔄 Dependencies

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `@modelcontextprotocol/sdk` | 1.25.3 → 1.26.0 | [#187](https://github.com/ChiR24/Unreal_mcp/pull/187) |
| `mcp-client-capabilities` | Latest | [#186](https://github.com/ChiR24/Unreal_mcp/pull/186) |

</details>

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `github/codeql-action` | 4.32.0 → 4.32.1 | [#185](https://github.com/ChiR24/Unreal_mcp/pull/185) |
| `actions/github-script` | 7.0.1 → 8.0.0 | [#184](https://github.com/ChiR24/Unreal_mcp/pull/184) |

</details>

---

## 🏷️ [0.5.13] - 2026-02-02

> [!IMPORTANT]
> ### 🛡️ Security & Compatibility Release
> This release includes multiple critical security fixes for command injection and path traversal vulnerabilities, along with full Unreal Engine 5.0 backward compatibility and WebSocket stability improvements.

### 🛡️ Security

<details>
<summary><b>🔒 Command Injection in UITools</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/144">#144</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Command injection via unsanitized user input in widget creation |
| **Fix** | Added `sanitizeConsoleString()` and applied `sanitizeAssetName()` to all user-provided identifiers |
| **Contributors** | @google-labs-jules[bot] |

</details>

<details>
<summary><b>🔒 Command Injection in LevelTools</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/179">#179</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Command injection via level names, event types, and game mode parameters |
| **Fix** | Added `sanitizeCommandArgument()` and applied to all console command parameters |
| **Contributors** | @google-labs-jules[bot] |

</details>

<details>
<summary><b>🔒 Path Traversal in Asset Listing</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/163">#163</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Path traversal in `listAssets` via `filter.pathStartsWith` parameter |
| **Fix** | Applied `normalizeAndSanitizePath()` to GraphQL `listAssets` and asset handler `list` action |
| **Contributors** | @google-labs-jules[bot] |

</details>

### ✨ Added

<details>
<summary><b>🎮 Unreal Engine 5.0 Compatibility</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/183">#183</a>)</summary>

| Component | Description |
|-----------|-------------|
| **API Abstractions** | Version-guarded macros for Material, Niagara, AssetRegistry, Animation, and World Partition APIs |
| **Build System** | Made plugin dependencies optional with dynamic memory-based configuration |
| **Coverage** | 41 handler files updated with UE 5.0-5.7 compatibility |

**Compatibility Macros Added:**
- `MCP_GET_MATERIAL_EXPRESSIONS()` - Abstracts material expression access
- `MCP_DATALAYER_TYPE` / `MCP_DATALAYER_ASSET_TYPE` - Data layer type abstraction
- `MCP_ASSET_FILTER_CLASS_PATHS` - Asset registry filter abstraction
- `MCP_ASSET_DATA_GET_CLASS_PATH()` - FAssetData abstraction
- `MCP_NIAGARA_EMITTER_DATA_TYPE` - Niagara emitter abstraction

</details>

### 🛠️ Fixed

<details>
<summary><b>🔌 WebSocket Stability</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/180">#180</a>, <a href="https://github.com/ChiR24/Unreal_mcp/pull/181">#181</a>)</summary>

| Fix | Description |
|-----|-------------|
| **TOCTOU Race** | Fixed Time-of-Check-Time-of-Use race condition in ListenSocket shutdown |
| **Shutdown Hang** | Fixed WebSocket server blocking cook/package builds |
| **Version Compatibility** | Fixed `PendingReceived.RemoveAt()` API differences for UE 5.4+ |

**Contributors:** @kalihman

</details>

<details>
<summary><b>🔧 Resource Handlers</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/165">#165</a>)</summary>

- Fixed broken actors and level resource handlers
- Added missing actors and level resources to MCP resource list

**Contributors:** @kalihman

</details>

<details>
<summary><b>🔧 Other Fixes</b></summary>

| Fix | Description |
|-----|-------------|
| UE 5.7 | Resolved macro handling and ControlRig dynamic loading issues |
| UE 5.5 | Fixed API compatibility issues in handlers |
| UE 5.1 | Fixed `MaterialDomain.h` inclusion path |
| JSON | Refactored JSON handling in McpAutomationBridge |

</details>

### 🧪 Testing

- Added security regression tests for UITools, LevelTools, and asset handlers

### 🔄 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `release-drafter/release-drafter` | 6.1.1 → 6.2.0 | [#160](https://github.com/ChiR24/Unreal_mcp/pull/160) |
| `actions/checkout` | 6.0.1 → 6.0.2 | [#161](https://github.com/ChiR24/Unreal_mcp/pull/161) |
| `github/codeql-action` | 4.31.10 → 4.32.0 | [#168](https://github.com/ChiR24/Unreal_mcp/pull/168), [#170](https://github.com/ChiR24/Unreal_mcp/pull/170) |
| `google-github-actions/run-gemini-cli` | Latest | [#177](https://github.com/ChiR24/Unreal_mcp/pull/177) |

</details>

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `@modelcontextprotocol/sdk` | Latest | [#154](https://github.com/ChiR24/Unreal_mcp/pull/154) |
| `hono` | 4.11.4 → 4.11.7 | [#173](https://github.com/ChiR24/Unreal_mcp/pull/173) |
| `@types/node` | Various updates | [#158](https://github.com/ChiR24/Unreal_mcp/pull/158), [#162](https://github.com/ChiR24/Unreal_mcp/pull/162), [#175](https://github.com/ChiR24/Unreal_mcp/pull/175) |

</details>

---

## 🏷️ [0.5.12] - 2026-01-15

> [!NOTE]
> ### 🔧 Handler Synchronization Release
> This release focuses on synchronizing TypeScript handler parameters with C++ handlers and dependency updates.

### 🛠️ Fixed

<details>
<summary><b>🔧 TS Handler Parameter Sync</b> (<code>5953232</code>)</summary>

- Synchronized TypeScript handler parameters with C++ handlers for consistency
- Fixed parameter mapping issues between TS and C++ layers

</details>

### 🔄 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `release-drafter/release-drafter` | 6.1.0 → 6.1.1 | [#141](https://github.com/ChiR24/Unreal_mcp/pull/141) |
| `google-github-actions/run-gemini-cli` | Latest | [#142](https://github.com/ChiR24/Unreal_mcp/pull/142) |

</details>

<details>
<summary><b>NPM Package Updates</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/143">#143</a>)</summary>

| Package | Update |
|---------|--------|
| `@types/node` | Various dev dependency updates |

</details>

---

## 🏷️ [0.5.11] - 2026-01-12

> [!IMPORTANT]
> ### 🛡️ Security Hardening & UE 5.7 Compatibility
> This release includes multiple critical security fixes for path traversal and command injection vulnerabilities, along with UE 5.7 Interchange compatibility fixes.

### 🛡️ Security

<details>
<summary><b>🔒 Path Traversal in Asset Import</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/125">#125</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Path traversal in asset import functionality |
| **Fix** | Added path sanitization and validation |

</details>

<details>
<summary><b>🔒 Command Injection Bypass</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/122">#122</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Command injection bypass via flexible whitespace |
| **Fix** | Enhanced command validation to detect and block bypass attempts |

</details>

<details>
<summary><b>🔒 Path Traversal in Screenshots</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/120">#120</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Path traversal in screenshot filenames |
| **Fix** | Implemented filename sanitization and path validation |

</details>

<details>
<summary><b>🔒 Path Traversal in GraphQL</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/135">#135</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Path traversal in GraphQL resolvers |
| **Fix** | Added input sanitization for GraphQL resolver paths |

</details>

<details>
<summary><b>🔒 GraphQL CORS Configuration</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/118">#118</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 MEDIUM |
| **Vulnerability** | Insecure GraphQL CORS configuration |
| **Fix** | Implemented secure CORS policy |

</details>

<details>
<summary><b>🔒 Enhanced Command Validation</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/113">#113</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 HIGH |
| **Vulnerability** | Command injection bypasses |
| **Fix** | Enhanced validation patterns to prevent injection bypasses |

</details>

### 🛠️ Fixed

<details>
<summary><b>🐛 UE 5.7 Asset Import Crash</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/138">#138</a>)</summary>

| Fix | Description |
|-----|-------------|
| **Interchange Compatibility** | Deferred asset import to next tick for UE 5.7 Interchange compatibility |
| **Name Sanitization** | Improved asset import robustness and name sanitization |

**Closes [#137](https://github.com/ChiR24/Unreal_mcp/issues/137)**

</details>

### 🔄 Dependencies

<details>
<summary><b>NPM Package Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `@modelcontextprotocol/sdk` | 1.25.1 → 1.25.2 | [#119](https://github.com/ChiR24/Unreal_mcp/pull/119) |
| `hono` | 4.11.1 → 4.11.4 | [#129](https://github.com/ChiR24/Unreal_mcp/pull/129) |
| `@types/node` | Various updates | [#130](https://github.com/ChiR24/Unreal_mcp/pull/130), [#133](https://github.com/ChiR24/Unreal_mcp/pull/133), [#134](https://github.com/ChiR24/Unreal_mcp/pull/134) |

</details>

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `github/codeql-action` | 4.31.9 → 4.31.10 | [#126](https://github.com/ChiR24/Unreal_mcp/pull/126) |
| `actions/setup-node` | 6.1.0 → 6.2.0 | [#133](https://github.com/ChiR24/Unreal_mcp/pull/133) |
| `dependabot/fetch-metadata` | 2.4.0 → 2.5.0 | [#114](https://github.com/ChiR24/Unreal_mcp/pull/114) |

</details>

---

## 🏷️ [0.5.10] - 2026-01-04

> [!IMPORTANT]
> ### 🚀 Context Reduction Initiative & Spline System
> This release implements the **Context Reduction Initiative** (Phases 48-53), reducing AI context overhead from ~78,000 to ~25,000 tokens, and adds a complete **Spline System** (Phase 26) with 21 new actions. ([#107](https://github.com/ChiR24/Unreal_mcp/pull/107), [#105](https://github.com/ChiR24/Unreal_mcp/pull/105))

### ✨ Added

<details>
<summary><b>🛤️ Spline System (Phase 26)</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/105">#105</a>)</summary>

New `manage_splines` tool with 21 actions for spline-based content creation:

| Category | Actions |
|----------|---------|
| **Creation** | `create_spline_actor`, `add_spline_point`, `remove_spline_point`, `set_spline_point` |
| **Properties** | `set_closed_loop`, `set_spline_type`, `set_tangent`, `get_spline_info` |
| **Mesh Components** | `create_spline_mesh`, `set_mesh_asset`, `set_spline_mesh_axis`, `set_spline_mesh_material` |
| **Scattering** | `create_mesh_along_spline`, `set_scatter_spacing`, `randomize_scatter` |
| **Quick Templates** | `create_road_spline`, `create_river_spline`, `create_fence_spline`, `create_wall_spline`, `create_cable_spline`, `create_pipe_spline` |
| **Utility** | `get_splines_info` |

**C++ Implementation:**
- `McpAutomationBridge_SplineHandlers.cpp` (1,512 lines)
- Full UE5 Spline API integration with `USplineComponent` and `USplineMeshComponent`

</details>

<details>
<summary><b>🔧 Pipeline Management Tool</b></summary>

New `manage_pipeline` tool for dynamic tool category management:

| Action | Description |
|--------|-------------|
| `set_categories` | Enable specific tool categories (core, world, authoring, gameplay, utility, all) |
| `list_categories` | Show available categories and their tools |
| `get_status` | View current state and tool counts |

**MCP Capability:**
- Server advertises `capabilities.tools.listChanged: true`
- Client capability detection via `mcp-client-capabilities` package
- Backward compatible: clients without `listChanged` support get ALL tools

</details>

### 🔧 Changed

<details>
<summary><b>📉 Context Reduction Initiative (Phases 48-53)</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/107">#107</a>)</summary>

| Phase | Description | Token Reduction |
|-------|-------------|-----------------|
| **Phase 48** | Schema Pruning - Condensed all 35+ tool descriptions to 1-2 sentences | ~23,000 |
| **Phase 49** | Common Schema Extraction - Shared schemas for paths, names, locations | ~8,000 |
| **Phase 50** | Dynamic Tool Loading - Category-based filtering | ~50,000 (when using filtering) |
| **Phase 53** | Strategic Tool Merging - Consolidated 4 tools | ~10,000 |

**Total Potential Reduction: ~91,000 tokens**

**Common Schemas Added:**
- `assetPath`, `actorName`, `location`, `rotation`, `scale`, `save`, `overwrite`
- `standardResponse` for consistent output formatting
- Helper functions: `createOutputSchema()`, `actionDescription()`

</details>

<details>
<summary><b>🔀 Tool Consolidation (Phase 53)</b></summary>

| Deprecated Tool | Merged Into | Actions Moved |
|-----------------|-------------|---------------|
| `manage_blueprint_graph` | `manage_blueprint` | 11 graph actions |
| `manage_audio_authoring` | `manage_audio` | 30 authoring actions |
| `manage_niagara_authoring` | `manage_effect` | 36 authoring actions |
| `manage_animation_authoring` | `animation_physics` | 45 authoring actions |

**Benefits:**
- Reduced tool count: 38 → 35
- Simplified tool discovery for AI assistants
- Backward compatible: deprecated tools still work with once-per-session warnings
- Action routing uses parameter sniffing to resolve conflicts

</details>

### ⚠️ Deprecated

- `manage_blueprint_graph` - Use `manage_blueprint` with graph actions instead
- `manage_audio_authoring` - Use `manage_audio` with authoring actions instead
- `manage_niagara_authoring` - Use `manage_effect` with authoring actions instead
- `manage_animation_authoring` - Use `animation_physics` with authoring actions instead

### 📊 Statistics

- **Files Changed:** 20
- **Lines Added:** 4,541
- **Lines Removed:** 3,555
- **Net Change:** +986 lines
- **New C++ Handler:** 1,512 lines (`McpAutomationBridge_SplineHandlers.cpp`)
- **New TS Handler:** 169 lines (`spline-handlers.ts`)
- **Common Schemas Added:** 50+ reusable schema definitions

### 🔗 Related Issues

Closes [#104](https://github.com/ChiR24/Unreal_mcp/issues/104), [#106](https://github.com/ChiR24/Unreal_mcp/issues/106), [#108](https://github.com/ChiR24/Unreal_mcp/issues/108), [#109](https://github.com/ChiR24/Unreal_mcp/issues/109), [#111](https://github.com/ChiR24/Unreal_mcp/issues/111)

---

## 🏷️ [0.5.9] - 2026-01-03

> [!IMPORTANT]
> ### 🎮 Major Feature Release
> This release introduces **15+ new automation tools** with comprehensive handlers for Navigation, Volumes, Level Structure, Sessions, Game Framework, and complete game development systems. ([#53](https://github.com/ChiR24/Unreal_mcp/pull/53))

### 🛡️ Security

<details>
<summary><b>🔒 Fix Arbitrary File Read in LogTools</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/103">#103</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Arbitrary file read via `logPath` parameter |
| **Impact** | Attackers could read any file on the system by manipulating the `logPath` override |
| **Fix** | Validated that `logPath` ends with `.log` and is within `Saved/Logs` directory |

**Protections Added:**
- Enforced `.log` extension requirement
- Restricted to `Saved/Logs` directory (CWD or UE_PROJECT_PATH)
- Added path traversal and sibling directory attack protection

</details>

### ✨ Added

<details>
<summary><b>🛠️ New Automation Tools</b></summary>

| Tool | Description |
|------|-------------|
| `manage_navigation` | NavMesh configuration, Nav Modifiers, Nav Links, pathfinding control |
| `manage_volumes` | 18 volume types (Trigger, Blocking, Audio, Physics, Navigation, Streaming) |
| `manage_level_structure` | World Partition, HLOD, Data Layers, Level Blueprints |
| `manage_sessions` | Split-screen, LAN play, Voice Chat configuration |
| `manage_game_framework` | GameMode, GameState, PlayerController, match flow |
| `manage_skeleton` | Bone manipulation, sockets, physics assets |
| `manage_material_authoring` | Material expressions, landscape materials |
| `manage_texture` | Texture creation, compression, virtual texturing |
| `manage_animation_authoring` | AnimBP, Control Rig, IK Rig, Retargeter |
| `manage_niagara_authoring` | Niagara systems, modules, parameters |
| `manage_gas` | Gameplay Ability System (Abilities, Effects, Attributes) |
| `manage_character` | Character creation, movement, locomotion |
| `manage_combat` | Weapons, projectiles, damage, melee combat |
| `manage_ai` | EQS, Perception, State Trees, Smart Objects |
| `manage_inventory` | Items, equipment, loot tables, crafting |
| `manage_interaction` | Interactables, destructibles, triggers |
| `manage_widget_authoring` | UMG widgets, layout, styling |
| `manage_networking` | Replication, RPCs, network prediction |
| `manage_audio_authoring` | MetaSounds, sound classes, dialogue |

</details>

### 🔧 Changed

<details>
<summary><b>Build & Infrastructure Improvements</b></summary>

| Change | Description |
|--------|-------------|
| Bounded Directory Search | Replaced unbounded recursive search with bounded depth search (3-4 levels) |
| Property Management | Enhanced property management across all automation handlers |
| Connection Manager | Added `IsReconnectPending()` method to McpConnectionManager |
| State Machine | Improved state machine creation with enhanced error handling |

</details>

### 📊 Statistics

- **New Tools:** 15+
- **New C++ Handler Files:** 20+

---

## 🏷️ [0.5.8] - 2026-01-02

> [!IMPORTANT]
> ### 🛡️ Security Release
> Critical security fix for path traversal vulnerability and material graph parameter improvements.

### 🛡️ Security

<details>
<summary><b>🔒 Fix Path Traversal in INI Reader</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/48">#48</a>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Path traversal in `getProjectSetting()` |
| **Impact** | Attackers could access arbitrary files by injecting `../` sequences into the category parameter |
| **Fix** | Added strict regex validation `^[a-zA-Z0-9_-]+$` to `cleanCategory` in `src/utils/ini-reader.ts` |

</details>

### 🛠️ Fixed

<details>
<summary><b>Material Graph Parameter Mapping</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/50">#50</a>)</summary>

| Schema Parameter | C++ Handler Expected | Status |
|------------------|---------------------|--------|
| `fromNodeId` | `sourceNodeId` | ✅ Auto-mapped |
| `toNodeId` | `targetNodeId` | ✅ Auto-mapped |
| `toPin` | `inputName` | ✅ Auto-mapped |

Closes [#49](https://github.com/ChiR24/Unreal_mcp/issues/49)

</details>

---

## 🏷️ [0.5.7] - 2026-01-01

> [!IMPORTANT]
> ### 🛡️ Security Release
> Critical security fix for Python execution bypass vulnerability.

### 🛡️ Security

<details>
<summary><b>🔒 Fix Python Execution Bypass</b> (<code>e16dab0</code>)</summary>

| Aspect | Details |
|--------|---------|
| **Severity** | 🚨 CRITICAL |
| **Vulnerability** | Python execution restriction bypass |
| **Impact** | Attackers could execute arbitrary Python code by using tabs instead of spaces after the `py` command |
| **Fix** | Updated `CommandValidator` to use regex `^py(?:\s|$)` which correctly matches `py` followed by any whitespace |

</details>

### 🔧 Changed

<details>
<summary><b>Release Process Improvements</b></summary>

- Removed automatic git tag creation from release workflow
- Updated release summary instructions for manual tag management

</details>

### 🔄 Dependencies

<details>
<summary><b>Package Updates</b></summary>

| Package | Update | Type |
|---------|--------|------|
| `zod` | 4.2.1 → 4.3.4 | Minor |
| `qs` | 6.14.0 → 6.14.1 | Patch (indirect) |
| `github/codeql-action` | 3.28.1 → 4.31.9 | Major |

</details>

---

## 🏷️ [0.5.6] - 2025-12-30

> [!IMPORTANT]
> ### 🛡️ Type Safety Milestone
> This release achieves **near-zero `any` type usage** across the entire codebase. All tool interfaces, handlers, automation bridge, GraphQL resolvers, and WASM integration now use strict TypeScript types with `unknown` and proper type guards.

### ✨ Added

<details>
<summary><b>📐 New Zod Schema Infrastructure</b></summary>

| File | Description |
|------|-------------|
| `src/schemas/primitives.ts` | 261 lines of Zod schemas for Vector3, Rotator, Transform, Color, etc. |
| `src/schemas/responses.ts` | 380 lines of response validation schemas |
| `src/schemas/parser.ts` | 167 lines of safe parsing utilities with type guards |
| `src/schemas/index.ts` | 173 lines of unified schema exports |

**Total:** 981 lines of new type-safe schema infrastructure

</details>

<details>
<summary><b>🔧 Type-Safe Argument Helpers</b> (<code>d5e6d1e</code>)</summary>

New extraction functions in `argument-helper.ts`:

| Function | Description |
|----------|-------------|
| `extractString(params, key)` | Extract required string with assertion |
| `extractOptionalString(params, key)` | Extract optional string |
| `extractNumber(params, key)` | Extract required number with assertion |
| `extractOptionalNumber(params, key)` | Extract optional number |
| `extractBoolean(params, key)` | Extract required boolean with assertion |
| `extractOptionalBoolean(params, key)` | Extract optional boolean |
| `extractArray<T>(params, key, validator?)` | Extract typed array with optional validation |
| `extractOptionalArray<T>(params, key, validator?)` | Extract optional array |
| `normalizeArgsTyped(args, configs)` | Returns `NormalizedArgs` interface with accessor methods |

**NormalizedArgs Interface:**
- `getString(key)`, `getOptionalString(key)`
- `getNumber(key)`, `getOptionalNumber(key)`
- `getBoolean(key)`, `getOptionalBoolean(key)`
- `get(key)` for raw `unknown` access
- `raw()` for full object access

</details>

<details>
<summary><b>🔌 WASM Module Interface</b> (<code>d5e6d1e</code>)</summary>

Defined structured `WASMModule` interface replacing `any`:

```typescript
interface WASMModule {
  PropertyParser?: new () => { parse_properties(json, maxDepth) };
  TransformCalculator?: new () => { composeTransform, decomposeMatrix };
  Vector?: new (x, y, z) => { x, y, z, add(other) };
  DependencyResolver?: new () => { analyzeDependencies, calculateDepth, ... };
}
```

</details>

<details>
<summary><b>📝 Automation Bridge Types</b> (<code>f97b008</code>)</summary>

| Type | Location | Description |
|------|----------|-------------|
| `QueuedRequestItem` | `automation/types.ts` | Typed interface for queued request items |
| `ASTFieldNode` | `graphql/resolvers.ts` | GraphQL AST node types for parseLiteral |
| `ASTNode` | `graphql/resolvers.ts` | Typed AST parsing |

</details>

### 🔧 Changed

<details>
<summary><b>🎯 Tool Interfaces Refactored</b> (<code>d5e6d1e</code>)</summary>

**ITools Interface - Replaced all `any` with concrete types:**

| Property | Before | After |
|----------|--------|-------|
| `materialTools` | `any` | `MaterialTools` |
| `niagaraTools` | `any` | `NiagaraTools` |
| `animationTools` | `any` | `AnimationTools` |
| `physicsTools` | `any` | `PhysicsTools` |
| `lightingTools` | `any` | `LightingTools` |
| `debugTools` | `any` | `DebugVisualizationTools` |
| `performanceTools` | `any` | `PerformanceTools` |
| `audioTools` | `any` | `AudioTools` |
| `uiTools` | `any` | `UITools` |
| `introspectionTools` | `any` | `IntrospectionTools` |
| `engineTools` | `any` | `EngineTools` |
| `behaviorTreeTools` | `any` | `BehaviorTreeTools` |
| `logTools` | `any` | `LogTools` |
| `inputTools` | `any` | `InputTools` |
| Index signature | `[key: string]: any` | `[key: string]: unknown` |

**StandardActionResponse:**
- Changed `StandardActionResponse<T = any>` → `StandardActionResponse<T = unknown>`

**IBlueprintTools:**
- `operations: any[]` → `operations: Array<Record<string, unknown>>`
- `defaultValue?: any` → `defaultValue?: unknown`
- `propertyValue: any` → `propertyValue: unknown`

**IAssetResources:**
- `list(): Promise<any>` → `list(): Promise<Record<string, unknown>>`

</details>

<details>
<summary><b>🔷 GraphQL Resolvers Type Safety</b> (<code>f97b008</code>, <code>fa4dddc</code>)</summary>

All scalar resolvers now use typed parameters:

| Scalar | Before | After |
|--------|--------|-------|
| `Vector.serialize` | `(value: any)` | `(value: unknown)` |
| `Rotator.serialize` | `(value: any)` | `(value: unknown)` |
| `Transform.parseLiteral` | `(ast: any)` | `(ast: ASTNode)` |
| `JSON.parseLiteral` | `(ast: any)` | `(ast: ASTNode): unknown` |

**Internal interfaces typed:**
- `Asset.metadata?: Record<string, any>` → `Record<string, unknown>`
- `Actor.properties?: Record<string, any>` → `Record<string, unknown>`
- `Blueprint.defaultValue?: any` → `unknown`

</details>

<details>
<summary><b>🌐 Automation Bridge Type Safety</b> (<code>f97b008</code>)</summary>

| Location | Before | After |
|----------|--------|-------|
| `onError` callback | `(err: any)` | `(err: unknown)` |
| `onHandshakeFail` callback | `(err: any)` | `(err: Record<string, unknown>)` |
| `catch` block | `catch (err: any)` | `catch (err: unknown)` with type guard |
| `onMessage` handler | `(data: any)` | `(data: Buffer \| string)` |
| `queuedRequestItems` | inline type with `any` | `QueuedRequestItem[]` |

</details>

<details>
<summary><b>🔌 WASM Integration Type Safety</b> (<code>d5e6d1e</code>)</summary>

| Method | Before | After |
|--------|--------|-------|
| `parseProperties()` | `Promise<any>` | `Promise<unknown>` |
| `analyzeDependencies()` | `Promise<any>` | `Promise<unknown>` |
| `fallbackParseProperties()` | `any` | `unknown` |
| `fallbackAnalyzeDependencies()` | `any` | `Record<string, unknown>` |
| `globalThis.fetch` patch | `(globalThis as any).fetch` | Typed with `GlobalThisWithFetch` |
| Error handling | `(error as any)?.code` | `(error as Record<string, unknown>)?.code` |

</details>

<details>
<summary><b>📊 Handler Types Expanded</b> (<code>d5e6d1e</code>)</summary>

`src/types/handler-types.ts` expanded with 147+ lines of new typed interfaces for all handler argument types.

</details>

### 🛠️ Fixed

<details>
<summary><b>✅ extractOptionalArray Behavior</b> (<code>f97b008</code>)</summary>

- Now returns `undefined` (instead of throwing) when value is not an array
- Documented behavior: graceful fallback for type mismatches
- Allows handlers to use default behavior when optional arrays are invalid

</details>

### 📊 Statistics

- **Files Changed:** 70 source files
- **Lines Added:** 3,806
- **Lines Removed:** 1,816
- **Net Change:** +1,990 lines (mostly type definitions)
- **New Schema Files:** 4 (981 lines total)
- **`any` → `unknown` Replacements:** 100+ occurrences

### 🔄 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR |
|---------|--------|-----|
| `actions/first-interaction` | 1.3.0 → 3.1.0 | [#38](https://github.com/ChiR24/Unreal_mcp/pull/38) |
| `actions/labeler` | 5.0.0 → 6.0.1 | Dependabot |
| `github/codeql-action` | SHA update | Dependabot |
| `release-drafter/release-drafter` | SHA update | Dependabot |
| Dev dependencies group | 2 updates | Dependabot |

</details>

---

## 🏷️ [0.5.5] - 2025-12-29

> [!NOTE]
> ### 📝 Quality & Validation Release
> This release focuses on **input validation**, **structured logging**, and **developer experience** improvements. WebSocket connections now enforce message size limits, Blueprint graph editing supports user-friendly node names, and all tools use structured logging.

### ✨ Added

<details>
<summary><b>🔌 WebSocket Message Size Limits</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/36">#36</a>)</summary>

| Feature | Description |
|---------|-------------|
| **Max Message Size** | 5MB limit for WebSocket frames and accumulated messages |
| **Close Code 1009** | Connections close with standard "Message Too Big" code when exceeded |
| **Fragment Accumulation** | Size checks applied during fragmented message assembly |

**C++ Changes:**
- Added `MaxWebSocketMessageBytes` (5MB) and `MaxWebSocketFramePayloadBytes` constants
- Implemented size validation at frame receive, fragment accumulation, and initial payload
- Proper teardown with `WebSocketCloseCodeMessageTooBig` (1009)

</details>

<details>
<summary><b>🔷 Blueprint Node Type Aliases</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/37">#37</a>)</summary>

User-friendly node names now map to internal K2Node classes:

| Alias | K2Node Class |
|-------|-------------|
| `Branch` | `K2Node_IfThenElse` |
| `Sequence` | `K2Node_ExecutionSequence` |
| `ForLoop` | `K2Node_ForLoop` |
| `ForLoopWithBreak` | `K2Node_ForLoopWithBreak` |
| `WhileLoop` | `K2Node_WhileLoop` |
| `Switch` | `K2Node_SwitchInteger` |
| `Select` | `K2Node_Select` |
| `DoOnce`, `DoN`, `FlipFlop`, `Gate`, `MultiGate` | Flow control nodes |
| `SpawnActorFromClass`, `GetAllActorsOfClass` | Actor manipulation |
| `Timeline`, `MakeArray`, `MakeStruct`, `BreakStruct` | Data/utility nodes |

**C++ & TypeScript Sync:**
- `BLUEPRINT_NODE_ALIASES` map in `graph-handlers.ts`
- `NodeTypeAliases` map in `McpAutomationBridge_BlueprintGraphHandlers.cpp`

</details>

<details>
<summary><b>🌳 Behavior Tree Generic Node Types</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/37">#37</a>)</summary>

| Node Type | Default Class | Category |
|-----------|---------------|----------|
| `Task` | `BTTask_Wait` | task |
| `Decorator` / `Blackboard` | `BTDecorator_Blackboard` | decorator |
| `Service` / `DefaultFocus` | `BTService_DefaultFocus` | service |
| `Composite` | `BTComposite_Sequence` | composite |

Aliases for common BT nodes: `Wait`, `MoveTo`, `PlaySound`, `Cooldown`, `Loop`, `TimeLimit`, `Selector`, etc.

</details>

<details>
<summary><b>📊 show_stats Action</b></summary>

New `show_stats` action in `system_control` tool:
- Toggle engine stats display (`stat Unit`, `stat FPS`, etc.)
- Parameters: `category` (string), `enabled` (boolean)

</details>

### 🔧 Changed

<details>
<summary><b>📋 Structured Logging</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/36">#36</a>)</summary>

Replaced `console.error`/`console.warn` with structured `Logger` across all tools:

| File | Change |
|------|--------|
| `actors.ts` | WASM debug logging |
| `debug.ts` | Viewmode stability warnings |
| `dynamic-handler-registry.ts` | Handler overwrite warnings |
| `editor.ts` | Removed commented debug logs |
| `physics.ts` | Improved error handling with fallback mesh resolution |

</details>

<details>
<summary><b>🎯 Handler Response Improvements</b></summary>

| Handler | Change |
|---------|--------|
| `actor-handlers.ts` | Returns clean responses without `ResponseFactory.success()` wrapping |
| `blueprint-handlers.ts` | Includes `blueprintPath` in responses |
| `environment.ts` | Changed default snapshot path to `./tmp/unreal-mcp/` |

</details>

### 🛠️ Fixed

<details>
<summary><b>✅ Input Validation Enhancements</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/37">#37</a>)</summary>

| Handler | Validation Added |
|---------|------------------|
| `editor-handlers.ts` | Viewport resolution requires positive numbers |
| `asset-handlers.ts` | Folder paths must start with `/` |
| `lighting-handlers.ts` | Valid light types: `point`, `directional`, `spot`, `rect`, `sky` |
| `lighting-handlers.ts` | Valid GI methods: `lumen`, `screenspace`, `none`, `raytraced`, `ssgi` |
| `performance-handlers.ts` | Valid profiling types with clear error messages |
| `performance-handlers.ts` | Scalability levels clamped to 0-4 range |
| `system-handlers.ts` | Quality level clamped to 0-4 range |

</details>

<details>
<summary><b>🔧 WASM Binding Patching</b> (<code>7cc602a</code>)</summary>

- Fixed TOCTOU (Time-of-Check-Time-of-Use) race condition in `patch-wasm.js`
- Uses atomic file operations with file descriptors (`openSync`, `ftruncateSync`, `writeSync`)
- Proper error handling for missing WASM files

</details>

### 🗑️ Removed

<details>
<summary><b>🧹 Code Cleanup</b></summary>

| Removed | Lines | Reason |
|---------|-------|--------|
| `src/types/responses.ts` content | 355 | Obsolete response type definitions |
| `scripts/validate-server.js` | 46 | Unused validation script |
| `scripts/verify-automation-bridge.js` | 177 | Unused functions and broken code |

</details>

### 📊 Statistics

- **Files Changed:** 28+ source files
- **Lines Removed:** 436 (cleanup)
- **Lines Added:** 283 (validation + features)
- **New Node Aliases:** 30+ Blueprint, 20+ Behavior Tree

---

## 🏷️ [0.5.4] - 2025-12-27

> [!IMPORTANT]
> ### 🛡️ Security Release
> This release focuses on **security hardening** and **defensive improvements** across the entire stack, including command injection prevention, network isolation, and resource management.

### 🛡️ Security & Command Hardening

<details>
<summary><b>UBT Validation & Safe Execution</b></summary>

| Feature | Description |
|---------|-------------|
| **UBT Argument Validation** | Added `validateUbtArgumentsString` and `tokenizeArgs` to block dangerous characters (`;`, `|`, backticks) |
| **Safe Process Spawning** | Updated child process spawning to use `shell: false`, preventing shell injection attacks |
| **Console Command Validation** | Implemented strict input validation for the Unreal Automation Bridge to block chained or multi-line commands |
| **Argument Quoting** | Improved logging and execution logic to correctly quote arguments containing spaces |

</details>

### 🌐 Network & Host Binding

<details>
<summary><b>Localhost Default & Remote Configuration</b></summary>

| Feature | Description |
|---------|-------------|
| **Localhost Default** | WebSocket, Metrics, and GraphQL servers now bind to `127.0.0.1` by default |
| **Remote Exposure Prevention** | Prevents accidental remote exposure of services |
| **GRAPHQL_ALLOW_REMOTE** | Added environment variable check for explicit remote binding configuration |
| **Security Warnings** | Warnings logged for unsafe/permissive network settings |

</details>

### 🚦 Resource Management

<details>
<summary><b>Rate Limiting & Queue Management</b></summary>

| Feature | Description |
|---------|-------------|
| **IP-Based Rate Limiting** | Implemented rate limiting on the metrics server |
| **Queue Limits** | Introduced `maxQueuedRequests` to automation bridge to prevent memory exhaustion |
| **Message Size Enforcement** | Enforced `MAX_WS_MESSAGE_SIZE_BYTES` for WebSocket connections to reject oversized payloads |

</details>

### 🧪 Testing & Cleanup

<details>
<summary><b>Test Updates & File Cleanup</b></summary>

| Change | Description |
|--------|-------------|
| **Path Sanitization Tests** | Modified validation tests to verify path sanitization and expect errors for traversal attempts |
| **Removed Legacy Tests** | Removed outdated test files (`run-unreal-tool-tests.mjs`, `test-asset-errors.mjs`) |
| **Response Logging** | Implemented better response logging in the test runner |

</details>

### 🔄 Dependencies

- **dependencies group**: Bumped 2 updates via @dependabot ([#33](https://github.com/ChiR24/Unreal_mcp/pull/33))

---

## 🏷️ [0.5.3] - 2025-12-21

> [!IMPORTANT]
> ### 🔄 Major Enhancements
> - **Dynamic Type Discovery** - New runtime introspection for lights, debug shapes, and sequencer tracks
> - **Metrics Rate Limiting** - Per-IP rate limiting (60 req/min) on Prometheus endpoint
> - **Centralized Class Configuration** - Unified Unreal Engine class aliases
> - **Enhanced Type Safety** - Comprehensive TypeScript interfaces replacing `any` types

### ✨ Added

<details>
<summary><b>🔍 Dynamic Discovery & Engine Handlers</b></summary>

| Feature | Description |
|---------|-------------|
| **list_light_types** | Discovers all available light class types at runtime |
| **list_debug_shapes** | Enumerates supported debug shape types |
| **list_track_types** | Lists all sequencer track types available in the engine |
| **Heuristic Resolution** | Improved C++ handlers use multiple naming conventions and inheritance validation |
| **Vehicle Type Support** | Expanded vehicle type from union to string for flexibility |

**C++ Changes:**
- `McpAutomationBridge_LightingHandlers.cpp` - Runtime `ResolveUClass` for lights
- `McpAutomationBridge_SequenceHandlers.cpp` - Runtime resolution for tracks
- Added `UObjectIterator.h` for dynamic type scanning
- Unified spawn/track-creation flows
- Removed editor/PIE branching logic

</details>

<details>
<summary><b>⚙️ Tooling & Configuration</b></summary>

| Feature | Description |
|---------|-------------|
| **class-aliases.ts** | Centralized Unreal Engine class name mappings |
| **handler-types.ts** | Comprehensive TypeScript interfaces (ActorArgs, EditorArgs, LightingArgs, etc.) |
| **timeout constants** | Command-specific operation timeouts in constants.ts |
| **listDebugShapes()** | Programmatic access in DebugVisualizationTools |

**Type System:**
- Geometry types: Vector3, Rotator, Transform
- Required-component lookups
- Centralized class-alias mappings

</details>

<details>
<summary><b>📈 Metrics Server Enhancements</b></summary>

| Feature | Description |
|---------|-------------|
| **Rate Limiting** | Per-IP limit of 60 requests/minute |
| **Server Lifecycle** | Returns instance for better management |
| **Error Handling** | Improved internal error handling |

</details>

<details>
<summary><b>📚 Documentation & DX</b></summary>

| Feature | Description |
|---------|-------------|
| **handler-mapping.md** | Updated with new discovery actions |
| **README.md** | Clarified WASM build instructions |
| **Tool Definitions** | Synchronized with new discovery actions |

</details>

### 🔧 Changed

<details>
<summary><b>Handler Type Safety & Logic</b></summary>

**src/tools/handlers/common-handlers.ts:**
- Replaced `any` typings with strict `HandlerArgs`/`LocationInput`/`RotationInput`
- Added automation-bridge connectivity validation
- Enhanced location/rotation normalization with type guards

**Specialized Handlers:**
- `actor-handlers.ts` - Applied typed handler-args
- `asset-handlers.ts` - Improved argument normalization
- `blueprint-handlers.ts` - Added new action cases
- `editor-handlers.ts` - Enhanced default handling
- `effect-handlers.ts` - Added `list_debug_shapes`
- `graph-handlers.ts` - Improved validation
- `level-handlers.ts` - Type-safe operations
- `lighting-handlers.ts` - Added `list_light_types`
- `pipeline-handlers.ts` - Enhanced error handling

</details>

<details>
<summary><b>Infrastructure & Utilities</b></summary>

**Security & Validation:**
- `command-validator.ts` - Blocks semicolons, pipes, backticks
- `error-handler.ts` - Enhanced error logging
- `response-validator.ts` - Improved Ajv typing
- `safe-json.ts` - Generic typing for cleanObject
- `validation.ts` - Expanded path-traversal protection

**Performance:**
- `unreal-command-queue.ts` - Optimized queue processing (250ms interval)
- `unreal-bridge.ts` - Centralized timeout constants

</details>

### 🛠️ Fixed

- **Command Injection Prevention** - Additional dangerous command patterns blocked
- **Path Security** - Enhanced asset-name validation
- **Type Safety** - Eliminated `any` types across handler functions
- **Error Messages** - Clearer error messages for class resolution failures

### 📊 Statistics

- **Files Changed:** 20+
- **New Interfaces:** 15+ handler type definitions
- **Discovery Actions:** 3 new runtime introspection methods
- **Security Enhancements:** 5+ new validation patterns

### 🔄 Dependencies

- **graphql-yoga**: Bumped from 5.17.1 to 5.18.0 (#31)

---

## 🏷️ [0.5.2] - 2025-12-18

> [!IMPORTANT]
> ### 🔄 Breaking Changes
> - **Standardized Tools & Type Safety** - All tool handlers now use consistent interfaces with improved type safety. Some internal API signatures have changed. (`079e3c2`)

### ✨ Added

<details>
<summary><b>🛠️ Blueprint Enhancements</b> (<code>e710751</code>)</summary>

| Feature | Description |
|---------|-------------|
| **Dynamic Node Creation** | Support for creating nodes dynamically in Blueprint graphs |
| **Struct Property Support** | Added ability to set and get struct properties on Blueprint components |

</details>

### 🔄 Changed

<details>
<summary><b>🎯 Standardized Tool Interfaces</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/28">#28</a>)</summary>

| Component | Change |
|-----------|--------|
| Tool Handlers | Optimized bridge communication and standardized response handling |
| Type Safety | Hardened type definitions across all tool interfaces |
| Bridge Optimization | Improved performance and reliability of automation bridge |

</details>

### 🔧 CI/CD

- 🔗 **MCP Publisher** - Fixed download URL format in workflow steps (`0d452e7`)
- 🧹 **Workflow Cleanup** - Removed unnecessary success conditions from MCP workflow steps (`82bd575`)

---

## 🏷️ [0.5.1] - 2025-12-17

> [!WARNING]
> ### ⚠️ Breaking Changes
> - **Standardized Return Types** - All tool methods now return `StandardActionResponse` type instead of generic objects. Consumers must update their code to handle the new response structure with `success`, `data`, `warnings`, and `error` fields. (`5e615c5`)
> - **Test Suite Structure** - New test files added and existing tests enhanced with comprehensive coverage.

### 🔄 Changed

<details>
<summary><b>🎯 Standardized Tool Interfaces</b> (<code>5e615c5</code>)</summary>

| Component | Change |
|-----------|--------|
| Tool Methods | Updated all tool methods to return `StandardActionResponse` type for consistency |
| Tool Interfaces | Modified interfaces (assets, blueprint, editor, environment, foliage, landscape, level, sequence) to use standardized response format |
| Type System | Added proper type imports and exports for `StandardActionResponse` |
| Handler Files | Updated to work with new standardized response types |
| Response Structure | All implementations return correct structure with `success`/`error` fields |

</details>

### ✨ Added

<details>
<summary><b>🧪 Comprehensive Test Suite</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/25">#25</a>)</summary>

| Feature | Description |
|---------|-------------|
| **Test Coverage** | Added comprehensive test files with success, error, and edge cases |
| **GraphQL DataLoader** | Implemented N+1 query optimization with batching and caching |
| **Type-Safe Interfaces** | Added type-safe automation response interfaces for better error handling |
| **Utility Tests** | Added tests for core utilities (normalize, safe-json, validation) |
| **Real-World Scenarios** | Enhanced coverage with real-world scenarios and cleanup procedures |
| **New Test Suites** | Audio, lighting, performance, input, and asset graph management |
| **Enhanced Logging** | Improved diagnostic logging throughout tools |
| **Documentation** | Updated supported Unreal Engine versions (5.0-5.7) in testing documentation |

</details>

### 🧹 Maintenance

- 🗑️ **Prompts Module Cleanup** - Removed prompts module and related GraphQL prompt functionality ([#26](https://github.com/ChiR24/Unreal_mcp/pull/26))
- 🔒 **Security Updates** - Removed unused dependencies (axios, json5, yargs) from package.json for security (`5e615c5`)
- 📐 **Tool Interfaces** - Enhanced asset and level tools with security validation and timeout handling (`5e615c5`)

### 📦 Dependencies

<details>
<summary><b>GitHub Actions Updates</b></summary>

| Package | Update | PR | Commit |
|---------|--------|-----|--------|
| `actions/checkout` | v4 → v6 | [#23](https://github.com/ChiR24/Unreal_mcp/pull/23) | `4c6b3b5` |
| `actions/setup-node` | v4 → v6 | [#22](https://github.com/ChiR24/Unreal_mcp/pull/22) | `71aa35c` |
| `softprops/action-gh-release` | 2.0.8 → 2.5.0 | [#21](https://github.com/ChiR24/Unreal_mcp/pull/21) | `b6c8a46` |

</details>

<details>
<summary><b>NPM Package Updates</b> (<a href="https://github.com/ChiR24/Unreal_mcp/pull/24">#24</a>, <code>5e615c5</code>)</summary>

| Package | Update |
|---------|--------|
| `@modelcontextprotocol/sdk` | 1.25.0 → 1.25.1 |
| `@types/node` | 25.0.2 → 25.0.3 |

</details>

---

## 🏷️ [0.5.0] - 2025-12-16

> [!IMPORTANT]
> ### 🔄 Major Architecture Migration
> This release marks the **complete migration** from Unreal's built-in Remote Plugin to a native C++ **McpAutomationBridge** plugin. This provides:
> - ⚡ Better performance
> - 🔗 Tighter editor integration
> - 🚫 No dependency on Unreal's Remote API
>
> **BREAKING CHANGE:** Response format has been standardized across all automation tools. Clients should expect responses to follow the new `StandardActionResponse` format with `success`, `data`, `warnings`, and `error` fields.

### 🏗️ Architecture

| Change | Description |
|--------|-------------|
| 🆕 **Native C++ Plugin** | Introduced `McpAutomationBridge` - a native UE5 editor plugin replacing the Remote API |
| 🔌 **Direct Editor Integration** | Commands execute directly in the editor context via automation bridge subsystem |
| 🌐 **WebSocket Communication** | Implemented `McpBridgeWebSocket` for real-time bidirectional communication |
| 🎯 **Bridge-First Architecture** | All operations route through the native C++ bridge (`fe65968`) |
| 📐 **Standardized Responses** | All tools now return `StandardActionResponse` format (`0a8999b`) |

### ✨ Added

<details>
<summary><b>🎮 Engine Compatibility</b></summary>

- **UE 5.7 Support** - Updated McpAutomationBridge with ControlRig dynamic loading and improved sequence handling (`ec5409b`)

</details>

<details>
<summary><b>🔧 New APIs & Integrations</b></summary>

- **GraphQL API** - Broadened automation bridge with GraphQL support, WASM integration, UI/editor integrations (`ffdd814`)
- **WebAssembly Integration** - High-performance JSON parsing with 5-8x performance gains (`23f63c7`)

</details>

<details>
<summary><b>🌉 Automation Bridge Features</b></summary>

| Feature | Commit |
|---------|--------|
| Server mode on port `8091` | `267aa42` |
| Client mode with enhanced connection handling | `bf0fa56` |
| Heartbeat tracking and output capturing | `28242e1` |
| Event handling and asset management | `d10e1e2` |

</details>

<details>
<summary><b>🎛️ New Tool Systems (0a8999b, 0ac82ac)</b></summary>

| Tool | Description |
|------|-------------|
| 🎮 **Input Management** | New `manage_input` tool with EnhancedInput support for Input Actions and Mapping Contexts |
| 💡 **Lighting Manager** | Full lighting configuration via `manage_lighting` including spawn, GI setup, shadow config, build lighting |
| 📊 **Performance Manager** | `manage_performance` with profiling (CPU/GPU/Memory), optimization, scalability, Nanite/Lumen config |
| 🌳 **Behavior Tree Editing** | Full behavior tree creation and node editing via `manage_behavior_tree` |
| 🎬 **Enhanced Sequencer** | Track operations (add/remove tracks, set muted/solo/locked), display rate, tick resolution |
| 🌍 **World Partition** | Cell management, data layer toggling via `manage_level` |
| 🖼️ **Widget Management** | UI widget creation, visibility controls, child widget adding |

</details>

<details>
<summary><b>📊 Graph Editing Capabilities (0a8999b)</b></summary>

- **Blueprint Graph** - Direct node manipulation with `manage_blueprint_graph` (create_node, delete_node, connect_pins, etc.)
- **Material Graph** - Node operations via `manage_asset` (add_material_node, connect_material_pins, etc.)
- **Niagara Graph** - Module and parameter editing (add_niagara_module, set_niagara_parameter, etc.)

</details>

<details>
<summary><b>🛠️ New Handlers & Actions</b></summary>

- Blueprint graph management and Niagara functionalities (`aff4d55`)
- Physics simulation setup in AnimationTools (`83a6f5d`)
- **New Asset Actions:**
  - `generate_lods`, `add_material_parameter`, `list_instances`
  - `reset_instance_parameters`, `get_material_stats`, `exists`
  - `nanite_rebuild_mesh`
- World partition and rendering tool handlers (`83a6f5d`)
- Screenshot with base64 image encoding (`bb4f6a8`)

</details>

<details>
<summary><b>🧪 Test Suites</b></summary>

**50+ new test cases** covering:
- Animation, Assets, Materials
- Sequences, World Partition
- Blueprints, Niagara, Behavior Trees
- Audio, Input Actions
- And more! (`31c6db9`, `85817c9`, `fc47839`, `02fd2af`)

</details>

### 🔄 Changed

#### Core Refactors
| Component | Change | Commit |
|-----------|--------|--------|
| `SequenceTools` | Migrated to Automation Bridge | `c2fb15a` |
| `UnrealBridge` | Refactored for bridge connection | `7bd48d8` |
| Automation Dispatch | Editor-native handlers modernization | `c9db1a4` |
| Test Runner | Timeout expectations & content extraction | `c9766b0` |
| UI Handlers | Improved readability and organization | `bb4f6a8` |
| Connection Manager | Streamlined connection handling | `0ac82ac` |

#### Tool Improvements
- 🚗 **PhysicsTools** - Vehicle config logic updated, deprecated checks removed (`6dba9f7`)
- 🎬 **AnimationTools** - Logging and response normalization (`7666c31`)
- ⚠️ **Error Handling** - Utilities refactored, INI file reader added (`f5444e4`)
- 📐 **Blueprint Actions** - Timeout handling enhancements (`65d2738`)
- 🎨 **Materials** - Enhanced material graph editing capabilities (`0a8999b`)
- 🔊 **Audio** - Improved sound component management (`0a8999b`)

#### Other Changes
- 📡 **Connection & Logging** - Improved error messages for clarity (`41350b3`)
- 📚 **Documentation** - README updated with UE 5.7, WASM docs, architecture overview, 17 tools (`8d72f28`, `4d77b7e`)
- 🔄 **Dependencies** - Updated to latest versions (`08eede5`)
- 📝 **Type Definitions** - Enhanced tool interfaces and type coverage (`0a8999b`)

### 🐛 Fixed

- `McpAutomationBridgeSubsystem` - Header removal, logging category, heartbeat methods (`498f644`)
- `McpBridgeWebSocket` - Reliable WebSocket communication (`861ad91`)
- **AutomationBridge** - Heartbeat handling and server metadata retrieval (`0da54f7`)
- **UI Handlers** - Missing payload and invalid widget path error handling (`bb4f6a8`)
- **Screenshot** - Clearer error messages and flow (`bb4f6a8`)

### 🗑️ Removed

| Removed | Reason |
|---------|--------|
| 🔌 Remote API Dependency | Replaced by native C++ plugin |
| 🐍 Python Fallbacks | Native C++ automation preferred (`fe65968`) |
| 📦 Unused HTTP Client | Cleanup from error-handler (`f5444e4`) |

---

## 🏷️ [0.4.7] - 2025-11-16

### ✨ Added
- Output Log reading via `system_control` tool with `read_log` action. filtering by category, level, line count.
- New `src/tools/logs.ts` implementing robust log tailing.
- 🆕 Initial `McpAutomationBridge` plugin with foundational implementation (`30e62f9`)
- 🧪 Comprehensive test suites for various Unreal Engine tools (`31c6db9`)

### 🔄 Changed
- `system_control` tool schema: Added `read_log` action.
- Updated tool handlers to route `read_log` to LogTools.
- Version bumped to 0.4.7.

### 📚 Documentation
- Updated README.md with initial bridge documentation (`a24dafd`)

---

## 🏷️ [0.4.6] - 2025-10-04

### 🐛 Fixed
- Fixed duplicate response output issue where tool responses were displayed twice in MCP content
- Response validator now emits concise summaries instead of duplicating full JSON payloads
- Structured content preserved for validation while user-facing output is streamlined

---

## 🏷️ [0.4.5] - 2025-10-03

### ✨ Added
- 🔧 Expose `UE_PROJECT_PATH` environment variable across runtime config, Smithery manifest, and client configs
- 📁 Added `projectPath` to runtime `configSchema` for Smithery's session UI

### 🔄 Changed
- ⚡ Made `createServer` synchronous factory (removed `async`)
- 🏠 Default for `ueHost` in exported `configSchema`

### 📚 Documentation
- Updated `README.md`, config examples to include `UE_PROJECT_PATH`
- Updated `smithery.yaml` and `server.json` manifests

### 🔨 Build
- Rebuilt Smithery bundle and TypeScript output

### 🐛 Fixed
- Smithery UI blank `ueHost` field by defining default in runtime schema

---

## 🏷️ [0.4.4] - 2025-09-28

### ✨ Improvements

- 🤝 **Client Elicitation Helper** - Added support for Cursor, VS Code, Claude Desktop, and other MCP clients
- 📊 **Consistent RESULT Parsing** - Handles JSON5 and legacy Python literals across all tools
- 🔒 **Safe Output Stringification** - Robust handling of circular references and complex objects
- 🔍 **Enhanced Logging** - Improved validation messages for easier debugging

---

## 🏷️ [0.4.0] - 2025-09-20

> **Major Release** - Consolidated Tools Mode

### ✨ Improvements

- 🎯 **Consolidated Tools Mode Exclusively** - Removed legacy mode, all tools now use unified handler system
- 🧹 **Simplified Tool Handlers** - Removed deprecated code paths and inline plugin validation
- 📝 **Enhanced Error Handling** - Better error messages and recovery mechanisms

### 🔧 Quality & Maintenance

- ⚡ Reduced resource usage by optimizing tool handlers
- 🧹 Cleanup of deprecated environment variables

---

## 🏷️ [0.3.1] - 2025-09-19

> **BREAKING:** Connection behavior is now on-demand

### 🏗️ Architecture

- 🔄 **On-Demand Connection** - Shifted to intelligent on-demand connection model
- 🚫 **No Background Processes** - Eliminated persistent background connections

### ⚡ Performance

- Reduced resource usage and eliminated background processes
- Optimized connection state management

### 🛡️ Reliability

- Improved error handling and connection state management
- Better recovery from connection failures

---

## 🏷️ [0.3.0] - 2025-09-17

> 🎉 **Initial Public Release**

### ✨ Features

- 🎮 **13 Consolidated Tools** - Full suite of Unreal Engine automation tools
- 📁 **Normalized Asset Listing** - Auto-map `/Content` and `/Game` paths
- 🏔️ **Landscape Creation** - Returns real UE/Python response data
- 📝 **Action-Oriented Descriptions** - Enhanced tool documentation with usage examples

### 🔧 Quality & Maintenance

- Server version 0.3.0 with clarified 13-tool mode
- Comprehensive documentation and examples
- Lint error fixes and code style cleanup

---

<div align="center">

### 🔗 Links

[![GitHub](https://img.shields.io/badge/GitHub-Repository-181717?style=for-the-badge&logo=github)](https://github.com/ChiR24/Unreal_mcp)
[![npm](https://img.shields.io/badge/npm-Package-CB3837?style=for-the-badge&logo=npm)](https://www.npmjs.com/package/unreal-engine-mcp-server)
[![UE5](https://img.shields.io/badge/Unreal-5.6%20|%205.7-0E1128?style=for-the-badge&logo=unrealengine)](https://www.unrealengine.com/)

</div>
