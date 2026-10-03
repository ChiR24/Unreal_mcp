# PROJECT KNOWLEDGE BASE

MCP tooling for Unreal Engine 5.0-5.8. Server package version `0.6.0-beta-c`; bridge plugin version `0.6.0-beta-c` (separate `.uplugin`). Two user-facing surfaces: a TypeScript stdio MCP server, and the bridge plugin's WebSocket transport and optional native `/mcp` HTTP/SSE transport.

Area-specific guidance lives in nested `AGENTS.md` files (see **AREA GUIDES** below). This root file is the workspace-wide view; do not duplicate their detail here.

## STRUCTURE
```
./
|-- src/                         # TypeScript MCP server, NodeNext ESM (strict)
|   |-- cli.ts index.ts config.ts constants.ts server-setup.ts unreal-bridge.ts
|   |-- automation/         (38) # WebSocket CLIENT: handshake, request tracking/correlation, frames
|   |-- gateway/             (4) # gateway manifest DATA + loader; 2 of 4 are *.generated.*
|   |-- handlers/            (2) # MCP RESOURCE handlers (registration + ue:// list resources)
|   |-- proxy/               (2) # `cli.js proxy`: restart-proof stdio front for the plugin's native /mcp
|   |-- resources/          (16) # resource providers behind handlers/
|   |-- server/                  # SDK construction, stdio lifecycle, tools/list + tools/call
|   |   |-- gateway/        (27) # gateway search/describe/execute ROUTING — NOT src/gateway
|   |   `-- mcp-primitives/  (7) # prompts, completions, progress, resource revision
|   |-- services/                # health monitor, readiness, telemetry
|   |-- tools/                   # catalog/ (contracts), dynamic/, handlers/foundation/ (bridge boundary)
|   `-- types/ utils/            # utils: collections commands config logging paths responses serialization validation
|-- plugins/McpAutomationBridge/ # the ONLY plugin; editor-only UE (bridge + native MCP + Fab adapter)
|   |-- Source/McpAutomationBridge/{Public (19), Private/}
|   |   Private/: Core(26) Domains(1016 / 55 domains) Foundation(92) MCP(137) Safety(18) Transport(21) Tests(25) UI(2)
|   `-- Source/McpAutomationBridgeFab/     # Fab asset-store adapter module (delay-loaded, optional)
|-- tests/                       # Vitest unit tests + custom MCP integration runner
|-- scripts/                     # generators, packaging, sync, smoke
|-- docs/                        # protocol, security, testing, gateway client guide, generated action reference
`-- .github/workflows/           # pinned CI, release, bump-version, smoke-test
```
There is no TypeScript action layer: `execute` forwards `{ action, ...params }` to the record's parent tool and the plugin does the work. Only `manage_tools` runs in process.

**NAMING TRAPS — get these wrong and you edit the wrong layer:**
- `src/handlers/` (2 files, MCP **resources**) vs `src/tools/handlers/` (the validated **bridge boundary**, `executeAutomationRequest()`).
- `src/gateway/` (manifest **data**, generated) vs `src/server/gateway/` (the request **routing engine**, incl. the idempotency ledger).
- `src/server/mcp-primitives/` (prompts/completions/progress **protocol primitives**) vs `src/resources/` (resource **providers**) vs `src/handlers/` (resource **registration**).
- The experimental in-editor assistant panel was removed from this tree in `c5caab21`; do not document or import it as a shipping surface.

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Start TS MCP server | `src/cli.ts`, `src/index.ts`, `src/server/server-factory.ts`, `src/server/stdio-lifecycle.ts` | CLI -> public facade -> construction/lifecycle |
| Native proxy (`cli.js proxy`) | `src/proxy/native-proxy.ts` | stdio in front of the plugin's `/mcp`: NOT_CONNECTED while the editor is down, new upstream session after a restart, cached tool list |
| Add/change a tool contract | `src/tools/catalog/capabilities/records/<parent>/` + `records/parent-metadata.ts` | **THE source of truth.** Every `*.generated.*` is an OUTPUT. See `src/tools/catalog/AGENTS.md` |
| Regenerate contract artifacts | `npm run registry:generate`, then `registry:check` / `manifest:check` | Records -> TS generated defs + gateway manifest + native registry/shards + action reference doc |
| Change gateway routing | `src/server/gateway/` | Own AGENTS.md. `src/gateway/` is only the generated manifest + loader |
| Change the bridge send | `src/tools/handlers/foundation/dispatch/automation-request-dispatch.ts` | Path security, console policy, timeout, envelope controls |
| Change capability auth (scopes/consent/paths/quota) | `.../Private/Foundation/McpCapabilityAuthorization.h` (predicates), `.../Private/Core/Security/` (composition) | The plugin is the sole authority and re-enforces every request |
| Change execute idempotency | `src/server/gateway/idempotency-ledger.ts`, `.../Private/Foundation/McpIdempotencyLedger.{h,cpp}` | Two mirrors, different caps (TS 1024 / native 4096). Change both |
| Change WebSocket automation | `src/automation/` (plus `src/unreal-bridge.ts`) | Handshake, connection policy, request tracking, token/TLS plumbing |
| Change Unreal request routing | `.../Private/Core/` | Queue, game-thread drain, the one handler table |
| Add Unreal bridge behavior | `.../Private/Domains/<Domain>/` | Reached from its parent in `Core/Subsystem/...HandlerRegistration.cpp` |
| Native MCP | `.../Private/MCP/` | Tool metadata is generated from the records; no hand-authored per-tool classes |
| Shared Unreal helpers | `.../Private/Foundation/` | Reflection, Blueprint, paths, responses |
| Fix UE save/load/delete crashes | `.../Private/Safety/` | Use the wrappers; preserve verification/cleanup |
| Change bridge sockets | `.../Private/Transport/` | Framing/TLS, connection auth, rate limits |
| Path/command security | `src/utils/paths/path-security.ts`, `src/utils/commands/command-validator.ts` | UE roots and the console policy (plugin re-checks both) |
| Integration tests | `tests/integration.mjs`, `tests/mcp-tools/` (cases), `tests/test-runner.mjs` (harness) | Unreal-dependent |
| Version bump | Run the `bump-version.yml` workflow | Rewrites all 4 version files; do not hand-bump |
| Plugin packaging | `node scripts/package-plugin.mjs <UE_ROOT> [out]` | Runs RunUAT BuildPlugin |

## CODE MAP
| Symbol | Type | Location | Role |
|--------|------|----------|------|
| `createServer()` / `startStdioServer()` | TS fn | `src/server/server-factory.ts`, `src/server/stdio-lifecycle.ts` | Server construction, stdio lifecycle, stdout safety |
| `handleUnrealGatewayCall()` | TS fn | `src/server/tool-registry-gateway.ts` | `unreal` gateway entry; switches search/describe/execute/configure |
| `runCapability()` | TS fn | `src/server/gateway/gateway-execute-dispatch.ts` | manage_tools in process; everything else to the plugin |
| `executeAutomationRequest()` | TS fn | `src/tools/handlers/foundation/dispatch/automation-request-dispatch.ts` | Validated TS-to-Unreal request boundary |
| `AutomationBridge.sendAutomationRequest()` | TS method | `src/automation/bridge.ts` | WebSocket send + request correlation |
| `routeStdoutLogsToStderr()` | TS fn | `src/server/server-factory.ts` | Keeps logs off JSON-RPC stdout |
| `UMcpAutomationBridgeSubsystem` | C++ class | `.../Public/McpAutomationBridgeSubsystem.h` (+ `Private/Core/Subsystem/`) | Request queue + `AutomationHandlers`, native MCP startup |
| `ProcessPendingAutomationRequests()` | C++ method | `.../Private/Core/Subsystem/...RequestQueue.cpp` | Game-thread queue drain (16/tick); every editor action passes here |
| `FMcpNativeTransport` | C++ class | `.../Private/MCP/Transport/McpNativeTransport.h` | Native `/mcp` HTTP/SSE JSON-RPC endpoint |

## CONVENTIONS
### Transport Surfaces
1. **TypeScript stdio MCP**: permanently exposes the single `unreal` gateway tool; no mode flag, no 23-tool listing.
2. **WebSocket bridge**: plugin listen sockets default to loopback `8090,8091`; the TS server is a client of one of them.
3. **Native MCP**: optional plugin HTTP/SSE under `Private/MCP/`; `GET /mcp` opens SSE, `POST /mcp` handles JSON-RPC, `DELETE /mcp` tears down sessions. Default port `3000`. Also exposes only the `unreal` tool.

### Security Boundaries
- Loopback-only by default. Non-loopback requires `MCP_AUTOMATION_ALLOW_NON_LOOPBACK=true` (TS) or `bAllowNonLoopback` (plugin). The two flags are independent surfaces (the TS bridge is a WebSocket *client* to the plugin socket, not a second server).
- **Fail-closed LAN coupling**: the native MCP transport refuses to bind non-loopback unless `bRequireCapabilityToken` is also enabled, so a LAN-exposed surface can never start without auth. The TS stdio bridge has no server socket of its own, so its non-loopback opt-in is the Node.js `MCP_AUTOMATION_ALLOW_NON_LOOPBACK`/`MCP_AUTOMATION_HOST=0.0.0.0` pair, which must be paired with capability-token planning on the plugin side. Loopback default-allow means any LAN client can call any tool unauthenticated once exposed.
- Capability-token auth: `X-MCP-Capability-Token` (native MCP) and `bridge_hello.capabilityToken` (WebSocket) when enabled. **On by default since 0.5.30** (`bRequireCapabilityToken` default `true`): the plugin auto-generates a per-install token at `<ProjectRoot>/Saved/MCP/capability-token` (raw UTF-8, 64 lowercase hex chars, no trailing newline; C++ is the sole writer, the TS bridge reads it at handshake time via `UE_PROJECT_PATH`, resolution order = explicit `CapabilityToken` → env `MCP_AUTOMATION_CAPABILITY_TOKEN` → token file, fail-closed, never logged). Tokens are compared in **constant time** (`McpConstantTimeTokenEquals` in `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Foundation/McpSecureTokenCompare.h`) on both transports, so comparison time never leaks how much of a token matched.
- Paths limited to `/Game`, `/Engine`, `/Script`, `/Temp`, `/Niagara`, the content mounts the connected editor reports (`bridge_ack.contentRoots`, kept current by `content_roots_changed`), plus sanitized `MCP_ADDITIONAL_PATH_PREFIXES`. File keys keep the static roots plus `/Saved` and `/tmp`. Preserve `/Game/...` normalization; do not add code depending on unnormalized `/Content/...`. The `/Content` alias is mapped in ONE shared canonicalizer — route new path handling through it rather than re-implementing the alias.

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
- Strict NodeNext TypeScript (`strict`, `noUnusedLocals/Parameters`, `noImplicitReturns`). No `as any`, `@ts-ignore`, or runtime `console.log`.
- Runtime logs go through `Logger`; `routeStdoutLogsToStderr()` protects JSON-RPC stdout. CI runs `npx eslint . --max-warnings=0`: warnings fail CI.
- `.env` loading is intentionally quiet to avoid corrupting MCP I/O (`src/config.ts`).

## TESTING
- **Unit (`npm run test:unit`)**: Vitest over `src/**/*.test.ts` + `tests/unit/**/*.test.ts`. No Unreal, no build.
- **Smoke (`npm run test:smoke`)**: `scripts/smoke-test.ts` against **built `dist/`** (run `npm run build` first); self-sets `MOCK_UNREAL_CONNECTION`. Asserts exactly one public tool (`unreal`) plus hidden-parent rejection.
- **Integration (`npm test`)**: `tests/integration.mjs` is the case list; the harness is `tests/test-runner.mjs`, which spawns `dist/cli.js` and needs a live editor + bridge. It auto-builds a stale `dist/`; a failed build aborts the run (`UNREAL_MCP_ALLOW_TS_FALLBACK=1` runs source instead).
- **Parameter audit (`npm run test:params`)**: static + strict + optional-strict. Every declared optional param needs a covering case, and no case may send an undeclared param or a removed action.
- **Expectation grammar**: split on `|` (or ` or `); first token is `success`/`error`/`timeout`. Narrow alternatives (`already exists`, `not found`) only on success-primary cases. Forbidden: `success|error`, or `timeout` after `error`.
- **CI order** (`.github/workflows/ci.yml`, `lint` job): `npx eslint . --max-warnings=0` → `type-check` → `test:unit` → `registry:check` → `manifest:check` → `headers:check` → `test:params` → `eval:check`. A separate `dependency-audit` job runs `npm audit --omit=dev --audit-level=moderate` (blocking) and a full-tree audit (informational). A matrix job (Node 20.19.x + 26.x) adds `build` + `test:smoke`.
- **NOT in CI**: `npm test` (needs a live editor) and `lint:cpp`.

## ANTI-PATTERNS (THIS PROJECT)
- Raw WebSocket calls: go through `executeAutomationRequest()`.
- Unvalidated external input: commands via `CommandValidator`, paths via the shared canonicalizer.
- LAN exposure by accident: no `0.0.0.0` / non-loopback without explicit opt-in and a capability token.
- Mixing transports: native `/mcp`, plugin WebSocket and TS stdio are separate lifecycles.
- Editing generated artifacts (`*.generated.*`, `capabilities/generated/`, plugin `MCP/Generated/`, `MCP/Tools/McpGeneratedParentRegistry.cpp`): edit the records, regenerate.
- A record param the C++ never reads, or a read param the record never declares: the strict gateway refuses undeclared params.
- AGENTS files in `dist/`, `build/`, `coverage/`, `tests/reports/`, `tmp/`, plugin `Binaries/`/`Intermediate/`.
- **Folder budget** (≤25 source files per folder under `Private/`, counted 2026-09-26): at 25 — `MCP/Gateway`, `MCP/Transport`, `Foundation`, `Domains/Sequence`, `Domains/ControlEditor`; at 24 — `MCP/Generated`, `MCP/Execute`, `Domains/ControlActor`, `Domains/AnimationAuthoring`. Split into a subdirectory before adding.
- **Source-contract gates** (Vitest reads C++ text): 250 pure-line ceiling per plugin file; ≤25 files per folder; no split artifacts; every local `Mcp*` include resolves; no `UPackage::SavePackage`; constant-time token compare; no non-loopback bind without `bRequireCapabilityToken`; no browser-origin WS upgrade; `/Script` reflection targets refused; identity redaction on replies; no raw Python in logs.
- **Lint**: `no-explicit-any` and `no-console` are `error` for `src/**`; `scripts/` and `tests/` are exempt, and `logger.ts` plus `server-factory.ts` are the sanctioned console sinks.
- **Never `localeCompare`** for ordering: use `src/utils/serialization/ordering.ts` so generated shards agree byte-for-byte.
- **No knip / ts-prune** (evaluated 2026-09-19, rejected): integration cases load at run time and source-contract tests read files as text, so its findings are noise. Dead code is found by reading; `noUnusedLocals`/`noUnusedParameters` are on.

## UNIQUE STYLES
- 23 canonical parent tools hide their actions behind action enums; 394 records (239 of them folded families) keep 1,504 `{tool, action}` pairs callable.
- Dynamic tool management exists in both TS and native MCP; `manage_tools` and `inspect` are protected, the `core` category is fixed.
- The plugin is responsibility-split: `Core` routes, `Domains` implement, `Foundation` shares primitives, `Safety` wraps hazardous editor ops, `Transport` owns sockets.
- Discovery is progressive and never dumps full schemas; invalid calls return `suggestions` plus an executable `nextCall`. A direct call to a canonical tool name returns a `DIRECT_TOOL_CALL_REMOVED` receipt whose `nextCall` re-runs it through `unreal`.
- Protocol negotiation is intentionally asymmetric: native `/mcp` supports `2025-11-25`, `2025-06-18`, `2025-03-26`; the TS SDK also accepts `2024-11-05` and `2024-10-07`.

## COMMANDS
```bash
npm run build              # clean + tsc to dist/
npm run dev                # ts-node-esm src/cli.ts (no build)
npm run lint               # CI runs `npx eslint . --max-warnings=0`
npm run type-check         # src + tests
npm run test:unit
npm run test:smoke         # needs built dist/
npm test                   # integration, Unreal-dependent
npm run test:params
npm run registry:generate  # records -> generated TS, manifest inputs, native registry/shards, action reference
npm run registry:check
npm run manifest:check
npm run headers:generate    # console policy rules
npm run headers:check
npm run eval:check         # retrieval corpus, own vitest config
npm run automation:sync
npx vitest run tests/unit/<file>.test.ts
```

## NOTES
- **Version sources** (4): `package.json` (canonical) + `package-lock.json`, `server.json`, and the `.uplugin` `VersionName` (read at runtime by the native server; the TS server reads package.json). `bump-version.yml` rewrites all of them; `tests/unit/version-consistency.test.ts` asserts they agree.
- **External GitHub Actions** are pinned to full commit SHAs.
- Not instruction targets: `tests/reports/`, root `build/`, `tmp/`, `.cache/`, and build outputs.

## AREA GUIDES (read the closest one)
**TypeScript server**
- `src/server/AGENTS.md` — SDK construction, stdio lifecycle, tools/list + tools/call.
- `src/server/gateway/AGENTS.md` — gateway search/describe/execute routing engine.
- `src/server/mcp-primitives/AGENTS.md` — prompts, completions, progress, resource revision.
- `src/resources/AGENTS.md` — resource providers.
- `src/tools/AGENTS.md` — the 23 canonical parents.
- `src/tools/catalog/AGENTS.md` — capability records (contract source of truth) + generation pipeline.
- `src/tools/handlers/AGENTS.md` — the validated bridge boundary.
- `src/automation/AGENTS.md` — WebSocket bridge client.
- `src/utils/AGENTS.md` — path/command security, logging, response validation.
- `src/types/AGENTS.md` — shared type contracts.

**Unreal plugin** (all under `plugins/McpAutomationBridge/`)
- `AGENTS.md` — plugin scope, cross-surface rules, packaging.
- `Source/McpAutomationBridge/Private/Core/AGENTS.md` — request queue, game-thread dispatch, the handler table.
- `.../Private/Domains/AGENTS.md` — 55 domain implementations + dispatch contract.
- `.../Private/Domains/BlueprintGraph/Behaviour/AGENTS.md` — `McpBlueprintBehaviour::Author`: gameplay logic written into Blueprint assets from recipe files.
- `.../Private/Foundation/AGENTS.md` — reflection, handler utils, shared primitives.
- `.../Private/Safety/AGENTS.md` — safe wrappers for hazardous editor operations.
- `.../Private/Transport/AGENTS.md` — sockets, TLS, capability-token auth, loopback gate.
- `.../Private/MCP/AGENTS.md` — native MCP registry/session/transport lifecycle.

**Other**
- `tests/AGENTS.md` — integration harness, expectation grammar, parameter audit.
- `tests/unit/plugin/AGENTS.md` — plugin source-contract gates.
- `.github/copilot-instructions.md` — workspace-wide architecture + critical constraints.
