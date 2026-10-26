# PROJECT KNOWLEDGE BASE

MCP tooling for Unreal Engine 5.0-5.8. Server package version `0.6.0-beta-f`; bridge plugin version `0.6.0-beta-f` (separate `.uplugin`). The plugin does all the work and serves MCP itself at `/mcp`; the npm package is a stdio proxy in front of it, plus the capability records every contract is generated from.

Area-specific guidance lives in nested `AGENTS.md` files (see **AREA GUIDES** below). This root file is the workspace-wide view; do not duplicate their detail here.

## STRUCTURE
```
./
|-- src/                         # NodeNext ESM (strict): the stdio proxy + the contract records
|   |-- cli.ts constants.ts      # bin entry -> proxy/
|   |-- proxy/               (1) # stdio in front of the plugin's native /mcp (the whole runtime)
|   |-- gateway/             (2) # generated gateway manifest DATA + its types
|   |-- services/            (1) # telemetry schema (a generator input)
|   |-- tools/                   # catalog/ (records = contracts, generated/), definitions/
|   `-- utils/                   # commands (console policy rules), config, logging, serialization, validation
|-- plugins/EngineWire/ # the ONLY plugin; editor-only UE (bridge + native MCP + Fab adapter)
|   |-- Source/EngineWire/{Public (20), Private/}
|   |   Private/ (source files, subfolders included, 2026-10-26): Core(39) Domains(1312 / 58 domains) Foundation(126) MCP(155) Safety(19) Transport(23) Tests(34) UI(2)
|   `-- Source/EngineWireFab/     # Fab asset-store adapter module (delay-loaded, optional)
|-- tests/                       # Vitest unit tests + custom MCP integration runner
|-- scripts/                     # generators, packaging, sync
|-- docs/                        # protocol, security, testing, gateway client guide, generated action reference
`-- .github/workflows/           # pinned CI, release, bump-version
```
There is no TypeScript gateway or action layer: search, describe, execute and configure run in the plugin, and the proxy only relays JSON-RPC. The TypeScript records are build-time input: they generate the plugin's `MCP/Generated/` shards and the gateway manifest.

**NAMING TRAPS — get these wrong and you edit the wrong layer:**
- `src/gateway/` is generated manifest **data**; the routing engine is native (`.../Private/MCP/Gateway/`, `.../Private/MCP/Execute/`).
- The experimental in-editor assistant panel was removed from this tree in `c5caab21`; do not document or import it as a shipping surface.

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| The stdio server | `src/cli.ts` -> `src/proxy/native-proxy.ts` | Relays to `UNREAL_MCP_URL` (default `http://127.0.0.1:3000/mcp`): NOT_CONNECTED while the editor is down, a new upstream session after a restart, cached tool list, 2026-07-28 requests sent sessionless |
| Add/change a tool contract | `src/tools/catalog/capabilities/records/<parent>/` + `records/parent-metadata.ts` | **THE source of truth.** Every `*.generated.*` is an OUTPUT. See `src/tools/catalog/AGENTS.md` |
| Regenerate contract artifacts | `npm run registry:generate`, then `registry:check` / `manifest:check` | Records -> generated defs + gateway manifest + native registry/shards + action reference doc |
| Change gateway routing | `.../Private/MCP/Gateway/`, `.../Private/MCP/Execute/` | Native search/describe/execute/configure |
| Change capability auth (scopes/consent/paths/quota) | `.../Private/Foundation/McpCapabilityAuthorization.h` (predicates), `.../Private/Core/Security/` (composition) | The plugin is the sole authority |
| Change execute idempotency | `.../Private/Foundation/McpIdempotencyLedger.{h,cpp}` | Cap 4096 |
| Change Unreal request routing | `.../Private/Core/` | Queue, game-thread drain, the one handler table |
| Add Unreal bridge behavior | `.../Private/Domains/<Domain>/` | Reached from its parent in `Core/Subsystem/...HandlerRegistration.cpp` |
| Native MCP | `.../Private/MCP/` | Tool metadata is generated from the records; no hand-authored per-tool classes |
| Shared Unreal helpers | `.../Private/Foundation/` | Reflection, Blueprint, paths, responses |
| Fix UE save/load/delete crashes | `.../Private/Safety/` | Use the wrappers; preserve verification/cleanup |
| Change bridge sockets | `.../Private/Transport/` | Framing/TLS, connection auth, rate limits |
| Console command policy | `src/utils/commands/console-command-policy-rules.ts` -> `npm run headers:generate` | Generates `McpConsolePolicy.generated.h`; the plugin enforces it |
| Integration tests | `tests/integration.mjs`, `tests/mcp-tools/` (cases), `tests/test-runner.mjs` (harness) | Unreal-dependent |
| Version bump | Run the `bump-version.yml` workflow | Rewrites all 4 version files; do not hand-bump |
| Plugin packaging | `node scripts/package-plugin.mjs <UE_ROOT> [out]` | Runs RunUAT BuildPlugin |

## CODE MAP
| Symbol | Type | Location | Role |
|--------|------|----------|------|
| `startNativeProxy()` | TS fn | `src/proxy/native-proxy.ts` | The stdio server: stdin JSON-RPC -> HTTP `/mcp`, replies and SSE notifications -> stdout |
| `readCapabilityToken()` | TS fn | `src/proxy/native-proxy.ts` | `MCP_AUTOMATION_CAPABILITY_TOKEN`, else `<UE_PROJECT_PATH>/Saved/MCP/capability-token`; never logged |
| `UEngineWireSubsystem` | C++ class | `.../Public/EngineWireSubsystem.h` (+ `Private/Core/Subsystem/`) | Request queue + `AutomationHandlers`, native MCP startup |
| `ProcessPendingAutomationRequests()` | C++ method | `.../Private/Core/Subsystem/...RequestQueue.cpp` | Game-thread queue drain (16/tick); every editor action passes here |
| `FMcpNativeTransport` | C++ class | `.../Private/MCP/Transport/McpNativeTransport.h` | Native `/mcp` HTTP/SSE JSON-RPC endpoint |

## CONVENTIONS
### Transport Surfaces
1. **Native MCP**: plugin HTTP/SSE under `Private/MCP/`; `GET /mcp` opens SSE, `POST /mcp` handles JSON-RPC, `DELETE /mcp` tears down sessions. Default port `3000`, on by default (`bEnableNativeMCP`). Permanently exposes the single `unreal` gateway tool; no mode flag, no 23-tool listing.
2. **stdio proxy** (`npx unreal-engine-mcp-server`): a client of (1); it answers `initialize` and `ping` itself and relays everything else.
3. **WebSocket bridge**: plugin listen sockets default to loopback `8090,8091`. Nothing in this repository is its client any more; removing it is a separate change, since every domain handler takes its socket type.

### Security Boundaries
- Loopback-only by default. Non-loopback requires `bAllowNonLoopback` (plugin).
- **Fail-closed LAN coupling**: the native MCP transport refuses to bind non-loopback unless `bRequireCapabilityToken` is also enabled, so a LAN-exposed surface can never start without auth. A browser `Origin` the endpoint does not allow is refused.
- Capability-token auth: `X-MCP-Capability-Token` (native MCP) and `bridge_hello.capabilityToken` (WebSocket) when enabled. **On by default since 0.5.30** (`bRequireCapabilityToken` default `true`): the plugin auto-generates a per-install token at `<ProjectRoot>/Saved/MCP/capability-token` (raw UTF-8, 64 lowercase hex chars, no trailing newline; C++ is the sole writer, the proxy reads it on every request via `UE_PROJECT_PATH`, resolution order = env `MCP_AUTOMATION_CAPABILITY_TOKEN` → token file, fail-closed, never logged). Tokens are compared in **constant time** (`McpConstantTimeTokenEquals` in `plugins/EngineWire/Source/EngineWire/Private/Foundation/McpSecureTokenCompare.h`) on both transports, so comparison time never leaks how much of a token matched.
- Paths limited to `/Game`, `/Engine`, `/Script`, `/Temp`, `/Niagara` and the content mounts the editor has. File keys keep the static roots plus `/Saved` and `/tmp`. Preserve `/Game/...` normalization; do not add code depending on unnormalized `/Content/...`. The `/Content` alias is mapped in ONE shared canonicalizer — route new path handling through it rather than re-implementing the alias.

### Capability Authorization (fail-closed, plugin is the sole authority)
Every automation request is gated **before it reaches the editor queue**.
- **Scopes** (`Public/McpCapabilityScopes.h`): `Read`/`Write`/`Destructive`/`Admin`. **Exact-set membership with an `Admin` wildcard — NOT rank-based. `Write` does NOT imply `Read`.** A capability that does not resolve in the canonical catalogue demands `Admin`, so an unknown action is refused by default.
- **Predicates vs composition**: pure, side-effect-free predicates live in `Private/Foundation/McpCapabilityAuthorization.h` (Foundation-only deps, shared by BOTH transports, reproducible in a no-editor test). Composition with the catalogue, console-command policy, and quota ledger happens one layer up in `Private/Core/Security/`. Do not make the predicate header reach into `Domains/` or `MCP/`.
- **Consent** modes `none` | `explicit` | `elevated`. Consent arrives as an `automation_request` **envelope sibling — never a handler param** — and is re-validated plugin-side. A grant is honoured only when it names *that* capability. Never infer consent from loopback, a prior call, idempotency, or preview.
- **Refusal codes**: `SCOPE_NOT_GRANTED`, `CONSENT_REQUIRED`, `PATH_NOT_PERMITTED`, `PROJECT_NOT_PERMITTED`, `QUOTA_EXCEEDED`, `COMMAND_BLOCKED`.
- **Path gating scans VALUES, not an allowlist of key names**, canonicalizing first so the gate sees the string the handler will resolve. The scan is depth- and node-bounded and reports `bTruncated` honestly — a truncated scan proves nothing, so it must fail closed. `CheckPathCoverage()` closes what value scanning structurally cannot see (folder/name joins, omitted optional path params that hit a server-side default, bare-relative values): a path-restricted principal running a mutating capability that declares a path parameter must present at least one provable in-prefix target from a scan that ran to completion.
- **Scoped tokens** (`FMcpScopedCapabilityToken`) carry profile, scopes, allowed path prefixes, allowed projects, and per-minute request/tool-call quotas. A scoped token may list only `Read`/`Write`/`Destructive`, **never `Admin`**; a scoped token colliding with the legacy token wins (narrower). The secret is never emitted in a log, receipt, authority descriptor, principal identity, or evidence file.

### Execute Idempotency
- Principal-scoped ledger: `Private/Foundation/McpIdempotencyLedger.{h,cpp}` (cap **4096**). Default TTL 24h.
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
- stdout carries only JSON-RPC: logs go through `Logger` to stderr. CI runs `npx eslint . --max-warnings=0`: warnings fail CI.
- The package has no runtime dependencies and ships only the proxy's files (`package.json` `files`); keep it that way.

## TESTING
- **Unit (`npm run test:unit`)**: Vitest over `src/**/*.test.ts` + `tests/unit/**/*.test.ts`. No Unreal, no build. Record tests look records up through `tests/unit/support/capability-index.ts` and check schemas with `tests/unit/support/schema-validate.ts`; plugin contract tests read the C++ as text.
- **No mocks or fakes**: no fake upstream, mock connection or in-memory server. Behaviour is checked against the real editor.
- **Integration (`npm test`)**: `tests/integration.mjs` is the case list; the harness is `tests/test-runner.mjs`, which spawns `dist/cli.js` (the proxy) and needs a live editor serving `/mcp`. It auto-builds a stale `dist/`; a failed build aborts the run (`UNREAL_MCP_ALLOW_TS_FALLBACK=1` runs source instead).
- **Parameter audit (`npm run test:params`)**: static + strict + optional-strict. Every declared optional param needs a covering case, and no case may send an undeclared param or a removed action.
- **Expectation grammar**: split on `|` (or ` or `); first token is `success`/`error`/`timeout`. Narrow alternatives (`already exists`, `not found`) only on success-primary cases. Forbidden: `success|error`, or `timeout` after `error`.
- **CI order** (`.github/workflows/ci.yml`, `lint` job): `npx eslint . --max-warnings=0` → `type-check` → `test:unit` → `registry:check` → `manifest:check` → `headers:check` → `test:params` → `eval:check`. A separate `dependency-audit` job runs `npm audit --omit=dev --audit-level=moderate` (blocking) and a full-tree audit (informational). A matrix job (Node 20.19.x + 26.x) adds `build`.
- **NOT in CI**: `npm test` (needs a live editor) and `lint:cpp`.

## ANTI-PATTERNS (THIS PROJECT)
- Rebuilding gateway logic in TypeScript: search, describe, execute and every check belong to the plugin. The proxy relays.
- Unvalidated external input: commands via the generated console policy, paths via the shared canonicalizer.
- LAN exposure by accident: no `0.0.0.0` / non-loopback without explicit opt-in and a capability token.
- Editing generated artifacts (`*.generated.*`, `capabilities/generated/`, plugin `MCP/Generated/`, `MCP/Tools/McpGeneratedParentRegistry.cpp`): edit the records, regenerate.
- A record param the C++ never reads, or a read param the record never declares: the strict gateway refuses undeclared params.
- AGENTS files in `dist/`, `build/`, `coverage/`, `tests/reports/`, `tmp/`, plugin `Binaries/`/`Intermediate/`.
- **Folder budget** (≤25 source files per folder under `Private/`, counted 2026-10-26): at 25 — `MCP/Gateway`, `MCP/Transport`, `MCP/Execute`, `Foundation`, `Domains/ControlEditor`, `Domains/ControlActor`, `Domains/Environment/Inspection`, `Domains/BlueprintGraph`; at 24 — `MCP/Generated`, `Tests`, `Domains/Sequence`, `Domains/LevelStructure`, `Domains/AssetWorkflow/Operations`, `Domains/AnimationAuthoring`, `Domains/ControlActor/Placement`. Split into a subdirectory before adding.
- **Source-contract gates** (Vitest reads C++ text): 250 pure-line ceiling per plugin file; ≤25 files per folder; no split artifacts; every local `Mcp*` include resolves; no `UPackage::SavePackage`; constant-time token compare; no non-loopback bind without `bRequireCapabilityToken`; no browser-origin WS upgrade; `/Script` reflection targets refused; identity redaction on replies; no raw Python in logs.
- **Lint**: `no-explicit-any` and `no-console` are `error` for `src/**`; `scripts/` and `tests/` are exempt, and `logger.ts` is the sanctioned console sink.
- **Never `localeCompare`** for ordering: use `src/utils/serialization/ordering.ts` so generated shards agree byte-for-byte.
- **No knip / ts-prune** (evaluated 2026-09-19, rejected): integration cases load at run time and source-contract tests read files as text, so its findings are noise. Dead code is found by reading; `noUnusedLocals`/`noUnusedParameters` are on.

## UNIQUE STYLES
- 23 canonical parent tools hide their actions behind action enums; 438 records (242 of them folded families) keep 1,596 `{tool, action}` pairs callable.
- Dynamic tool management is native (`MCP/DynamicTools/`); `manage_tools` and `inspect` are protected, the `core` category is fixed.
- The plugin is responsibility-split: `Core` routes, `Domains` implement, `Foundation` shares primitives, `Safety` wraps hazardous editor ops, `Transport` owns sockets.
- Discovery is progressive and never dumps full schemas; invalid calls return `suggestions` plus an executable `nextCall`. A direct call to a canonical tool name returns a `DIRECT_TOOL_CALL_REMOVED` receipt whose `nextCall` re-runs it through `unreal`.
- Protocol versions: native `/mcp` and the proxy negotiate `2025-11-25`, `2025-06-18`, `2025-03-26` (a client asking for an older one is answered with `2025-11-25`, as the spec says); `2026-07-28` requests carry their own version and need no session.

## COMMANDS
```bash
npm run build              # clean + tsc to dist/
npm run dev                # ts-node-esm src/cli.ts (no build)
npm run lint               # CI runs `npx eslint . --max-warnings=0`
npm run type-check         # src + tests
npm run test:unit
npm test                   # integration, Unreal-dependent
npm run test:params
npm run registry:generate  # records -> generated TS, manifest inputs, native registry/shards, action reference
npm run registry:check
npm run manifest:check
npm run headers:generate    # console policy rules
npm run headers:check
npm run eval:check         # retrieval corpus over the native ranker's TS copy, own vitest config
npm run automation:sync
npx vitest run tests/unit/<file>.test.ts
```

## NOTES
- **Version sources** (4): `package.json` (canonical) + `package-lock.json`, `server.json`, and the `.uplugin` `VersionName` (read at runtime by the native server; the proxy reports package.json's). `bump-version.yml` rewrites all of them; `tests/unit/version-consistency.test.ts` asserts they agree.
- **External GitHub Actions** are pinned to full commit SHAs.
- Not instruction targets: `tests/reports/`, root `build/`, `tmp/`, `.cache/`, and build outputs.

## AREA GUIDES (read the closest one)
**TypeScript (records + proxy)**
- `src/tools/AGENTS.md` — the 23 canonical parents.
- `src/tools/catalog/AGENTS.md` — capability records (contract source of truth) + generation pipeline.
- `src/utils/AGENTS.md` — console policy rules, config, logging, ordering.

**Unreal plugin** (all under `plugins/EngineWire/`)
- `AGENTS.md` — plugin scope, cross-surface rules, packaging.
- `Source/EngineWire/Private/Core/AGENTS.md` — request queue, game-thread dispatch, the handler table.
- `.../Private/Domains/AGENTS.md` — 58 domain implementations + dispatch contract.
- `.../Private/Domains/BlueprintGraph/Behaviour/AGENTS.md` — `McpBlueprintBehaviour::Author`: gameplay logic written into Blueprint assets from recipe files.
- `.../Private/Foundation/AGENTS.md` — reflection, handler utils, shared primitives.
- `.../Private/Safety/AGENTS.md` — safe wrappers for hazardous editor operations.
- `.../Private/Transport/AGENTS.md` — sockets, TLS, capability-token auth, loopback gate.
- `.../Private/MCP/AGENTS.md` — native MCP registry/session/transport lifecycle.

**Other**
- `tests/AGENTS.md` — integration harness, expectation grammar, parameter audit.
- `tests/unit/plugin/AGENTS.md` — plugin source-contract gates.
- `.github/copilot-instructions.md` — workspace-wide architecture + critical constraints.
