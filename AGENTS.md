# PROJECT KNOWLEDGE BASE

MCP tooling for Unreal Engine 5.0-5.8 Preview. Server package version `0.5.30`; bridge plugin version `0.5.30` (separate `.uplugin`). Two user-facing surfaces: a TypeScript stdio MCP server, and the bridge plugin's WebSocket transport and optional native `/mcp` HTTP/SSE transport.

Area-specific guidance lives in nested `AGENTS.md` files (see **AREA GUIDES** below). This root file is the workspace-wide view; do not duplicate their detail here.

## STRUCTURE
```
./
|-- src/                         # TypeScript MCP server, NodeNext ESM (strict)
|   |-- cli.ts index.ts config.ts constants.ts server-setup.ts   # entry + facades
|   |-- unreal-bridge*.ts        # UnrealBridge (connection/console/properties/system) at src root
|   |-- automation/         (34) # WebSocket CLIENT: handshake, request tracking/correlation, frames
|   |-- config/              (1) # class-aliases.ts ONLY (DIR; env schema is src/config.ts — name collision)
|   |-- gateway/             (4) # gateway manifest DATA + loader; 2 of 4 are *.generated.*
|   |-- handlers/            (2) # MCP RESOURCE handlers — NOT tool logic (see tools/handlers)
|   |-- resources/          (17) # resource providers behind handlers/ (actors, assets, levels, editor state)
|   |-- server/             (13) # SDK construction, stdio lifecycle, tool/resource registry shards
|   |   |-- gateway/        (25) # gateway search/describe/execute ROUTING — NOT src/gateway
|   |   `-- mcp-primitives/ (43) # resources/prompts/completions/subscriptions + client profiles, configure store
|   |-- services/            (4) # health-monitor, metrics-server (Prometheus)
|   |-- tools/                   # catalog/ (contracts), handlers/<38 domains>/ (action logic),
|   |                            #   orchestration/, dynamic/, editor/, level/, schemas/
|   |   `-- definitions/shared/  # ONLY 2 files (tool-definition.ts, action-sets.ts) — NOT a contract source
|   |-- types/ utils/            # utils: commands config interaction logging paths responses serialization validation
|-- plugins/McpAutomationBridge/ # the ONLY plugin; editor-only UE (bridge + native MCP)
|   `-- Source/McpAutomationBridge/{Public (17), Private/}
|       Private/: Core(34) Domains(1098 / 66 domains) Foundation(70) MCP(152) Safety(19) Transport(23) Tests(13) UI(2)
|       Core/: Compatibility Errors Module Requests Security Settings Subsystem
|       MCP/:  DynamicTools Execute Gateway Generated Primitives Protocol Registry Resources Routing Tools Transport
|-- tests/                       # Vitest unit tests + custom MCP integration runner
|-- scripts/                     # generators, packaging, sync, smoke, cleanup
|-- docs/                        # handler maps, testing, protocol, plugin extension
`-- .github/workflows/           # pinned CI, release, registry, security
```
NOTE: `src/server/` tool-registry is split (`tool-registry.ts` + `tool-registry-{client,elicitation,gateway,listing,manage-tools}.ts` + `resource-registry.ts` — there is **no** `tool-registry-legacy.ts`). `src/unreal-bridge*.ts` is `unreal-bridge.ts` + `-{connection,console,properties,response,system,types}.ts`. The plugin `Private/Core/Subsystem/` holds the registration shards; the subsystem `.cpp` is there (not directly in `Core/`).

**NAMING TRAPS — get these wrong and you edit the wrong layer:**
- `src/handlers/` (2 files, MCP **resources**) vs `src/tools/handlers/` (38 domains, **tool action logic**) vs `src/types/handlers/` (types).
- `src/gateway/` (manifest **data**, generated) vs `src/server/gateway/` (25-file request **routing engine**, incl. the idempotency ledger).
- `src/server/mcp-primitives/` (MCP resources/prompts/completions/subscriptions **protocol primitives**) vs `src/resources/` (the resource **providers** those primitives read) vs `src/handlers/` (the 2-file resource request **handlers**).
- `src/config.ts` (env Zod schema) vs `src/config/` (UE class aliases only).
- `src/wasm/` no longer exists (the empty stub directory is gone); `src/tools/definitions/` holds only 2 shared files and is not a source of truth.
- The experimental in-editor assistant panel (formerly shipped as a separate plugin subtree) was **removed from this tree** in `c5caab21` and is excluded from 0.5.30. It survives only on feature branches (`agent-guide`, `AgentPanel`). Do not document or import it as a shipping surface; external consumers that previously drove the editor through it must target the native `/mcp` or TypeScript stdio `unreal` gateway instead.

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Start TS MCP server | `src/cli.ts`, `src/index.ts`, `src/server/server-factory.ts`, `src/server/stdio-lifecycle.ts` | CLI shim -> public facade -> construction/lifecycle -> registration |
| Add/change a TS tool contract | `src/tools/catalog/capabilities/records/<tool>/` + `records/parent-metadata.ts` | **THE source of truth.** `consolidated-tool-definitions.ts` and every `*.generated.*` are OUTPUTS — editing them is overwritten on next generate. See `src/tools/catalog/AGENTS.md` |
| Regenerate contract artifacts | `npm run registry:generate`, then `registry:check` / `manifest:check` | Records -> TS facades + routing index + gateway manifest + native C++ registry/shards |
| Change gateway routing (search/describe/execute) | `src/server/gateway/` (25 files) | Its own AGENTS.md. `src/gateway/` is only the generated manifest + loader |
| Change MCP protocol primitives (resources/prompts/completions/subscriptions) | `src/server/mcp-primitives/` (43 files) | Its own AGENTS.md. Native mirror in `Private/MCP/Primitives/`; parity gated by `tests/unit/mcp-primitives/*-parity.test.ts` |
| Change capability auth (scopes/consent/paths/quota) | `.../Private/Foundation/McpCapabilityAuthorization.h` (predicates), `.../Private/Core/Security/` (composition) | Predicates are pure + transport-shared; the plugin is the sole authority and re-enforces every request |
| Change execute idempotency | `src/server/gateway/idempotency-ledger.ts`, `.../Private/Foundation/McpIdempotencyLedger.{h,cpp}` | Two mirrors, different caps (TS 1024 / native 4096). Change both |
| Register TS tool behavior | `src/tools/orchestration/consolidated-handler-registration.ts`, `src/server/tool-registry.ts` | `consolidated-tool-handlers.ts` is the bootstrap/export facade |
| Implement TS action logic | `src/tools/handlers/<domain>/` (38 domains) | Validate/normalize, then use the shared dispatch helpers |
| Change WebSocket automation | `src/automation/` (plus `src/unreal-bridge*.ts` at root) | Handshake, connection policy, request tracking, token/TLS plumbing |
| Change Unreal request routing | `plugins/McpAutomationBridge/.../Private/Core/` | Queue, game-thread dispatch, handler registration, responses |
| Add Unreal bridge behavior | `plugins/McpAutomationBridge/.../Private/Domains/<Domain>/` | Register through `Private/Core/Subsystem/*Registration.cpp` shard |
| Add native MCP metadata | `plugins/McpAutomationBridge/.../Private/MCP/` | Canonical tool/action records are the source of truth; native registration is generated into the native registry from those records — do not hand-author per-tool `MCP_REGISTER_TOOL` classes |
| Change shared Unreal helpers | `plugins/McpAutomationBridge/.../Private/Foundation/` | Reflection, Blueprint, paths, responses, handler primitives |
| Fix UE save/load/delete crashes | `plugins/McpAutomationBridge/.../Private/Safety/` | Use the project wrappers and preserve verification/cleanup |
| Change bridge sockets | `plugins/McpAutomationBridge/.../Private/Transport/` | WebSocket framing/TLS plus connection auth, rate limits, telemetry |
| Path/command security | `src/utils/paths/path-security.ts`, `src/utils/commands/command-validator.ts` | Enforce UE roots and console-command block lists |
| Vitest unit tests | `tests/unit/`, `src/**/*.test.ts` | No Unreal required |
| Integration tests | `tests/integration.mjs` (test CASES), `tests/test-runner.mjs` (the HARNESS that spawns the server), `tests/mcp-tools/` | Unreal-dependent. Add cases to `integration.mjs`; change spawn/timeout/expectation behavior in `test-runner.mjs` |
| Version bump | Run the `bump-version.yml` workflow (see NOTES) | Workflow is CURRENT and rewrites all 7 version files; do not hand-bump |
| Plugin packaging | `scripts/package-plugin.sh <UE_ROOT> [out]`, `scripts/package-plugin.bat` | Runs RunUAT BuildPlugin |
| Plugin sync (source) | `scripts/sync-mcp-plugin.js` | Copies plugin into Engine/Project; not a build |

## CODE MAP
| Symbol | Type | Location | Role |
|--------|------|----------|------|
| `createServer()` / `startStdioServer()` | TS fn | `src/server/server-factory.ts`, `src/server/stdio-lifecycle.ts` | Server construction, stdio lifecycle, stdout safety |
| `registerDefaultHandlers()` | TS fn | `src/tools/orchestration/consolidated-handler-registration.ts` | Parent tool -> handler map |
| `executeAutomationRequest()` | TS fn | `src/tools/handlers/foundation/dispatch/automation-request-dispatch.ts` | Validated TS-to-Unreal request boundary |
| `AutomationBridge.sendAutomationRequest()` | TS method | `src/automation/bridge.ts` | WebSocket send + request correlation (the real hot path; `AutomationBridge` alone is mostly log strings) |
| `handleUnrealGatewayCall()` | TS fn | `src/server/tool-registry-gateway.ts` | `unreal` gateway entry; switches search/describe/execute/configure |
| `handleConsolidatedToolCall()` | TS fn | `src/tools/orchestration/consolidated-handler-dispatcher.ts` | Resolves registry handler; the canonical 23-tool boundary all paths converge on |
| `routeStdoutLogsToStderr()` | TS fn | `src/server/server-factory.ts` | Redirects logs off JSON-RPC stdout |
| `UMcpAutomationBridgeSubsystem` | C++ class | `.../Public/McpAutomationBridgeSubsystem.h` (+ `Private/Core/Subsystem/.cpp`) | Request queue + `TMap<FString,FAutomationHandler> AutomationHandlers`, native MCP startup |
| `ProcessPendingAutomationRequests()` | C++ method | `.../Private/Core/Subsystem/...RequestQueue.cpp` | Game-thread queue drain (16/tick); every editor action passes here |
| `FMcpNativeTransport` | C++ class | `.../Private/MCP/Transport/McpNativeTransport.h` | Native `/mcp` HTTP/SSE JSON-RPC endpoint |

## CONVENTIONS
### Transport Surfaces
1. **TypeScript stdio MCP**: `src/index.ts` exposes the public API; `src/server/` owns construction/lifecycle; `src/automation/` connects to Unreal. Permanently exposes the single `unreal` gateway tool; there is no gateway-mode opt-out and no legacy 23-tool direct listing (the `MCP_GATEWAY_MODE` env var was removed from `src/config.ts`).
2. **WebSocket bridge**: plugin listen sockets default to loopback `8090,8091`; TS sends automation requests through the negotiated bridge. `MCP_AUTOMATION_CLIENT_MODE=true` flips TS to server mode.
3. **Native MCP**: optional plugin HTTP/SSE under `Private/MCP/`; `GET /mcp` opens SSE, `POST /mcp` handles JSON-RPC, `DELETE /mcp` tears down sessions. Default port `3000` (override with `MCP_NATIVE_PORT`). Permanently exposes the single `unreal` gateway tool; the `bEnableNativeGateway` project setting was removed and there is no 23-tool native listing to restore.

### Security Boundaries
- Loopback-only by default. Non-loopback requires `MCP_AUTOMATION_ALLOW_NON_LOOPBACK=true` (TS) or `bAllowNonLoopback` (plugin). The two flags are independent surfaces (the TS bridge is a WebSocket *client* to the plugin socket, not a second server).
- **Fail-closed LAN coupling**: the native MCP transport refuses to bind non-loopback unless `bRequireCapabilityToken` is also enabled, so a LAN-exposed surface can never start without auth. The TS stdio bridge has no server socket of its own, so its non-loopback opt-in is the Node.js `MCP_AUTOMATION_ALLOW_NON_LOOPBACK`/`MCP_AUTOMATION_HOST=0.0.0.0` pair, which must be paired with capability-token planning on the plugin side. Loopback default-allow means any LAN client can call any tool unauthenticated once exposed.
- Capability-token auth: `X-MCP-Capability-Token` (native MCP) and `bridge_hello.capabilityToken` (WebSocket) when enabled. Tokens are compared in **constant time** (`McpConstantTimeTokenEquals` in `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Foundation/McpSecureTokenCompare.h`) on both transports, so comparison time never leaks how much of a token matched.
- Metrics are separate: non-loopback metrics requires both `MCP_METRICS_ALLOW_NON_LOOPBACK=true` and `MCP_METRICS_TOKEN`.
- Paths limited to `/Game`, `/Engine`, `/Script`, `/Temp`, `/Niagara`, plus sanitized `MCP_ADDITIONAL_PATH_PREFIXES`. Preserve `/Game/...` normalization; do not add code depending on unnormalized `/Content/...`. The `/Content` alias is mapped in ONE shared canonicalizer — route new path handling through it rather than re-implementing the alias.

### Capability Authorization (fail-closed, plugin is the sole authority)
Every automation request is gated **before it reaches the editor queue**. The TypeScript layer fails fast; the plugin re-enforces independently, so never treat a TS-side check as sufficient.
- **Scopes** (`Public/McpCapabilityScopes.h`): `Read`/`Write`/`Destructive`/`Admin`. **Exact-set membership with an `Admin` wildcard — NOT rank-based. `Write` does NOT imply `Read`.** A capability that does not resolve in the canonical catalogue demands `Admin`, so an unknown action is refused by default.
- **Predicates vs composition**: pure, side-effect-free predicates live in `Private/Foundation/McpCapabilityAuthorization.h` (Foundation-only deps, shared by BOTH transports, reproducible in a no-editor test). Composition with the catalogue, console-command policy, and quota ledger happens one layer up in `Private/Core/Security/`. Do not make the predicate header reach into `Domains/` or `MCP/`.
- **Consent** modes `none` | `explicit` | `elevated` (mirrors `CONSENT_MODES` in TS). Consent arrives as an `automation_request` **envelope sibling — never a handler param** — and is re-validated plugin-side. A grant is honoured only when it names *that* capability. Never infer consent from loopback, a prior call, idempotency, or preview.
- **Refusal codes are shared strings** on both sides (typed parity by construction): `SCOPE_NOT_GRANTED`, `CONSENT_REQUIRED`, `PATH_NOT_PERMITTED`, `PROJECT_NOT_PERMITTED`, `QUOTA_EXCEEDED`, `COMMAND_BLOCKED`. Add a code to both surfaces or neither.
- **Path gating scans VALUES, not an allowlist of key names**, canonicalizing first so the gate sees the string the handler will resolve. The scan is depth- and node-bounded and reports `bTruncated` honestly — a truncated scan proves nothing, so it must fail closed. `CheckPathCoverage()` closes what value scanning structurally cannot see (folder/name joins, omitted optional path params that hit a server-side default, bare-relative values): a path-restricted principal running a mutating capability that declares a path parameter must present at least one provable in-prefix target from a scan that ran to completion.
- **Scoped tokens** (`FMcpScopedCapabilityToken`) carry profile, scopes, allowed path prefixes, allowed projects, and per-minute request/tool-call quotas. A scoped token may list only `Read`/`Write`/`Destructive`, **never `Admin`**; a scoped token colliding with the legacy token wins (narrower). The secret is never emitted in a log, receipt, authority descriptor, principal identity, or evidence file.

### Execute Idempotency
- Principal-scoped ledger, mirrored: `src/server/gateway/idempotency-ledger.ts` (cap **1024**) and `Private/Foundation/McpIdempotencyLedger.{h,cpp}` (cap **4096**). Default TTL 24h. Change both or they diverge.
- The slot is `SHA-256(principal ∥ capabilityId ∥ key)` — the **raw idempotency key never enters the map**, so it cannot reach a log line, receipt, or evidence file. Keep it that way.
- **A failure is never cached**: `abandon` deletes the entry so the key stays retryable, and a refusal (which never reaches `begin`) can never be replayed as a success.
- **Eviction only removes COMPLETED entries.** Evicting an in-flight entry would admit a concurrent duplicate as a second real mutation — the exact thing the ledger exists to prevent.
- A key replayed with a different request fingerprint is a `conflict` that discloses no prior receipt.
- Native execute stage order is normative: resolve form/alias -> registry -> dynamic enabled state -> options -> defaults -> exact per-action input schema. Nothing reaches the subsystem queue until every stage passes.

### UE Safety
- Do not call `UPackage::SavePackage()` directly. Use `McpSafeAssetSave`, `McpSafeLevelSave`, or `McpSafeLoadMap` wrappers.
- Blueprint component templates must be owned by SCS nodes via `SCS->CreateNode()` / `SCS->AddNode()`.
- Do not introduce `ANY_PACKAGE`; use modern lookup (`nullptr` / project helper).
- Editor API work enters via the subsystem queue and runs on the game thread; unsafe save/GC/async-load states are deferred.

### TypeScript Standards
- Strict NodeNext TypeScript (see `tsconfig.json`: `strict`, `noUnusedLocals/Parameters`, `noImplicitReturns`). Do not add `as any`, `@ts-ignore`, or runtime `console.log`.
- Runtime logs go through `Logger`; `routeStdoutLogsToStderr()` protects JSON-RPC stdout. CI runs ESLint with `--ext .ts --max-warnings=0` — **warnings fail CI**, so lint must be clean.
- `.env` loading is intentionally quiet to avoid corrupting MCP I/O (`src/config.ts`).
- Output schemas are registered at startup and stay schema-backed.

## TESTING
- **Unit (`npm run test:unit`)**: Vitest over `src/**/*.test.ts` + `tests/unit/**/*.test.ts`. No Unreal, no build. Single file: `npx vitest run tests/unit/<file>.test.ts`. Coverage: `npm run test:unit:coverage` (`--coverage`, v8).
- **Smoke (`npm run test:smoke`)**: `scripts/smoke-test.ts` (NOT under `tests/`). Mock in-memory MCP check against **built `dist/`** — it imports `dist/index.js`, so run `npm run build` first. It self-sets `MOCK_UNREAL_CONNECTION=true`, so prefixing that env var is redundant. It asserts exactly **one public tool (`unreal`)** plus hidden-parent rejection — it does NOT assert a 23-tool listing.
- **Integration (`npm test` / `npm run test:all` — identical)**: `node tests/integration.mjs` is only the **case list** — it declares `testCases` and calls `runToolTests('integration', testCases)`. The **harness is `tests/test-runner.mjs`**, which spawns the server over stdio via `StdioClientTransport` at `dist/cli.js` (override with `UNREAL_MCP_SERVER_ARGS`) and expects a **live Unreal Editor + bridge**. It auto-builds when `dist/` is missing or older than `src/` (`UNREAL_MCP_NO_AUTO_BUILD=1` disables, `UNREAL_MCP_FORCE_DIST=1` forces dist) and **falls back to TypeScript source if the build fails** — so a green run does not prove you tested `dist/`. `UE_PROJECT_PATH` is consumed only by the spawned server via inherited env.
- **Native parity**: `npm run test:native-parity` (TS vs native canonical tool/action equality); `npm run test:params` adds static+strict+optional-strict parameter audit.
- **Expectation grammar**: split on `|` (or ` or `); first token is the primary intent and must be `success`/`error`/`timeout`. Narrow alternatives (`already exists`, `not found`) allowed on success-primary cases. Forbidden: broad masks like `success|error`, or `timeout` after `error` (a timeout passes ONLY as the primary condition).
- **Timeouts**: unit `testTimeout` 10s. Integration per-case default **5s** (`UNREAL_MCP_TEST_CASE_TIMEOUT_MS`), per-call server **60s** (`..._CALL_TIMEOUT_MS`), client/progress **300s** (`..._CLIENT_TIMEOUT_MS`); cleanup cases override to 30s.
- **CI order** (`.github/workflows/ci.yml`, `lint` job): `npx eslint . --max-warnings=0` → `type-check` → `test:unit` → `registry:check` → `normalization:check` → `manifest:check` → `policy:check` → `test:params` → `migration:check` → `primitives:check` → `security:check` → `eval:check` → `version:check` → `workflow:check` → `npm audit --omit=dev --audit-level=high` (blocking) → `npm audit --audit-level=moderate` (`continue-on-error`, informational). A second matrix job (Node 20.19.x + 26.x) adds `build` + `test:smoke`. The order itself is gated by `tests/unit/workflow_gate_order_contract.test.ts`.
- **NOT in CI**: `npm test` (integration, needs live editor) and `lint:cpp`/`lint:csharp`. Plugin packaging runs only when the `UNREAL_ENGINE_ROOT` repo var is set.
- **Audit bar**: the blocking audit is runtime-only at `high`. `--omit=dev --audit-level=moderate` exits 1 against this lockfile today (GHSA-frvp-7c67-39w9 on the production path under the pinned `@modelcontextprotocol/sdk` 1.29.0), so a runtime moderate is **tolerated, not absent** — see `docs/security-and-receipts.md`.

## ANTI-PATTERNS (THIS PROJECT)
- Bypassing registry flow: never call handlers directly instead of `toolRegistry.register()` and `handleConsolidatedToolCall()`.
- Raw WebSocket calls from tools: use `executeAutomationRequest()` and the automation bridge queue.
- Unvalidated external input: command strings via `CommandValidator`; paths via normalization/security helpers.
- LAN exposure by accident: do not bind `0.0.0.0` / non-loopback without explicit opt-in and token planning.
- Mixing transports: native `/mcp`, plugin WebSocket, TS stdio, and ACP are separate lifecycles; do not route around their registry/session boundaries.
- Treating `src/handlers/` and `src/tools/handlers/` as one: the former is MCP resource handlers, the latter is tool action logic.
- Editing generated artifacts: hand-edits to any `*.generated.*`, `capabilities/generated/`, or plugin `MCP/Generated/` file are silently overwritten by the next generate and fail drift checks. Edit the records, regenerate.
- Never place AGENTS files in `dist/`, `build/`, `coverage/`, `tests/reports/`, `tmp/`, plugin `Binaries/`, plugin `Intermediate/`, or uppercase staging mirrors (`Plugins/`).
- **Folder-budget headroom is GONE in two places**: the ≤25 files-per-folder gate is already satisfied at exactly 25 by `Private/MCP/Transport/` and `Private/Domains/Sequence/` (`Private/MCP/Generated/`, `Private/Domains/GAS/`, `Private/Domains/AnimationAuthoring/` sit at 24). Adding ONE file to any of those breaks CI — split into a subdirectory instead.
- **Automated source-contract gates** (Vitest reads C++/C# text — these fail CI): 250 pure-line ceiling per plugin file (measured on *pure* lines, so a 380-line file with comments can still pass); ≤25 files per folder; no split artifacts (`Common`/`Part\d+`/`.incl`); every local `Mcp*` include must resolve; no `UPackage::SavePackage`; constant-time token compare only; no non-loopback bind without `bRequireCapabilityToken`; no browser-origin WS upgrade; no raw Python source in logs.
- **Convention-only, NOT lint-enforced**: `no-explicit-any` and `no-console` are both `off` in `eslint.config.mjs`. `as any` / `@ts-ignore` / runtime `console.log` are still forbidden by project rule — nothing will catch them for you.

## UNIQUE STYLES
- 23 canonical parent tools hide hundreds of actions behind action enums to reduce client context.
- Dynamic tool management exists in both TS and native MCP; `manage_tools` and `inspect` are protected (cannot be disabled; `core` category is fixed).
- The native plugin's MCP tool definitions are generated into a native registry from the canonical TS records (the TypeScript `consolidated-tool-definitions.ts` is itself a generated facade over the canonical parent metadata). Handwritten per-tool `MCP_REGISTER_TOOL` classes have been removed; the generated native registration replaces them, and only canonical names survive.
- The bridge plugin is responsibility-split: `Core` routes, `Domains` implement, `Foundation` shares primitives, `Safety` wraps hazardous editor ops, `Transport` owns sockets.
- Both transports **permanently expose** a single `unreal` gateway tool (`search`/`describe`/`execute`/`configure`); there is no opt-out. Discovery is **progressive and never dumps full schemas**: `describe` drills down `tool` summary -> `tool+action` parameter catalog -> `tool+action+param` single schema, and `perActionSchemas` is always `false` (the parameter catalog is the tool-union). Invalid calls return guided errors with `suggestions` plus an executable `nextCall`. A direct call to a canonical tool name is not routed; it returns a bounded, executable `DIRECT_TOOL_CALL_REMOVED` receipt whose `nextCall` re-runs it through `unreal`.
- Protocol version negotiation is **intentionally asymmetric**: the native `/mcp` transport supports exactly the three modern MCP versions (`2025-11-25`, `2025-06-18`, `2025-03-26`) and deliberately does not implement the later `2026-07-28` RC. The TypeScript SDK also accepts two older legacy versions (`2024-11-05`, `2024-10-07`) from its `SUPPORTED_PROTOCOL_VERSIONS` set, so the native surface is intentionally stricter than the TS surface.
- Source-contract tests in `tests/unit/plugin/*contracts.test.ts` read C++/C# files and assert required/forbidden patterns (incl. a 250 pure-line ceiling per file).

## COMMANDS
```bash
npm run build          # clean + tsc compile to dist/
npm run build:core     # compile only (no clean)
npm run dev            # ts-node-esm src/cli.ts (no build)
npm run lint           # eslint . — NOTE: script has NO --max-warnings; CI runs `npx eslint . --max-warnings=0`
npm run type-check     # tsc --noEmit
npm run test:unit      # Vitest unit tests, no Unreal
npm run test:smoke     # mock in-memory MCP smoke test (needs built dist/)
npm test               # tests/integration.mjs — Unreal-dependent
npm run test:all       # identical to npm test
npm run test:native-parity
npm run test:params    # parity + strict parameter audit
# contract generation + drift gates (only manifest:check runs in CI)
npm run registry:generate  # capability records -> TS facades, routing index, native registry/shards
npm run registry:check     # drift gate for the above (RUN LOCALLY — not in CI)
npm run manifest:check     # gateway manifest drift gate (in CI)
npm run version:check      # assert all 7 version sources agree
npm run normalization:check
npm run policy:check
npm run automation:sync
npm run clean:tmp
# single unit file:
npx vitest run tests/unit/<file>.test.ts
npm run test:unit:coverage
# also present, undocumented above: lint:fix build:watch start test:unit:watch
#   normalization:audit policy:generate clean prepare lint:c lint:cpp lint:csharp
```

## NOTES
- **Version sources**: `package.json` (`version`) is the canonical source. `npm version` rewrites it together with `package-lock.json`, so both stay in lockstep. `server.json` versions the npm package distribution (top-level `version` plus the npm package `version`). The bridge version lives in its `.uplugin` `VersionName`. The native HTTP/SSE transport advertises `server-info.json` (`version`) and the `McpNativeTransport.h` `ServerVersion` `TEXT` fallback; the TS server advertises the `SERVER_VERSION` fallback in `src/server/server-factory.ts` when `package.json` cannot be read. Coordinated release bumps must resync all of: `package.json` (+`package-lock.json`), `server.json`, the McpAutomationBridge `.uplugin`, `server-info.json`, `src/server/server-factory.ts`, and `McpNativeTransport.h`. The `bump-version.yml` workflow already rewrites every one of these (via `npm version`, `jq`, and `perl`), so a coordinated bump is a single workflow run. Verify with `npm run version:check` (`tests/unit/version-consistency.test.ts`), which asserts agreement across all seven sources. For a manual audit, grep the canonical version across `package.json server.json plugins/*/*.uplugin plugins/*/Resources/MCP/server-info.json` rather than a hardcoded literal.
- **Engine reference path**: `/data/UnrealEngine/Engine/`.
- **External GitHub Actions** are pinned to full commit SHAs.
- **`GEMINI.md` is stale and not authoritative** — it references `src/unreal-bridge.ts` (now split into `src/unreal-bridge*.ts`), uppercase `Plugins/`, non-existent Rust modules, and `npm run test:control_actor`. Prefer this file and the nested AGENTS.
- Not instruction targets: `tests/reports/`, root `build/`, `tmp/`, `Public/`, uppercase `Plugins/`, `.cache/`, `.opencode/node_modules/`, and package/plugin build outputs.

## AREA GUIDES (read the closest one)
**TypeScript server**
- `src/server/AGENTS.md` — MCP SDK construction, stdio lifecycle, tool/resource registry split.
- `src/server/gateway/AGENTS.md` — gateway search/describe/execute routing engine.
- `src/server/mcp-primitives/AGENTS.md` — resources/prompts/completions/subscriptions primitives, client profiles, native parity.
- `src/resources/AGENTS.md` — resource providers (actors, assets, levels, editor state) behind those primitives.
- `src/tools/AGENTS.md` — the 23 canonical parents, orchestration, routing.
- `src/tools/catalog/AGENTS.md` — capability records (contract source of truth) + generation pipeline.
- `src/tools/handlers/AGENTS.md` — domain action handlers, dispatch contract.
- `src/automation/AGENTS.md` — WebSocket bridge client, handshake, request correlation.
- `src/utils/AGENTS.md` — path/command security, logging, response validation.
- `src/types/AGENTS.md` — shared type contracts.

**Unreal plugin** (all under `plugins/McpAutomationBridge/`)
- `AGENTS.md` — plugin scope, cross-surface rules, packaging, validation.
- `Source/McpAutomationBridge/Private/Core/AGENTS.md` — request queue, game-thread dispatch, registration shards.
- `.../Private/Domains/AGENTS.md` — 66 domain implementations + dispatch-macro contract.
- `.../Private/Foundation/AGENTS.md` — reflection, handler utils, shared primitives.
- `.../Private/Safety/AGENTS.md` — safe wrappers for hazardous editor operations.
- `.../Private/Transport/AGENTS.md` — sockets, TLS, capability-token auth, loopback gate.
- `.../Private/MCP/AGENTS.md` — native MCP registry/session/transport lifecycle.

**Other**
- `tests/AGENTS.md` — integration harness, expectation grammar, audit contracts.
- `.github/copilot-instructions.md` — workspace-wide architecture + critical constraints (complements this file).
