# PROJECT KNOWLEDGE BASE

MCP tooling for Unreal Engine 5.0-5.8 Preview. Server package version `0.5.30`; bridge plugin version `0.5.30` (separate `.uplugin`). Two user-facing surfaces: a TypeScript stdio MCP server, and the bridge plugin's WebSocket transport and optional native `/mcp` HTTP/SSE transport.

Area-specific guidance lives in nested `AGENTS.md` files (see **AREA GUIDES** below). This root file is the workspace-wide view; do not duplicate their detail here.

## STRUCTURE
```
./
|-- src/                         # TypeScript MCP server, NodeNext ESM (strict)
|   |-- cli.ts index.ts config.ts constants.ts server-setup.ts   # entry + facades
|   |-- unreal-bridge*.ts        # UnrealBridge (connection/console/properties/system) at src root
|   |-- automation/              # WebSocket client, handshake, request tracking, bridge config
|   |-- config/                  # class-aliases.ts (DIR; separate from config.ts)
|   |-- handlers/                # resource-handlers.ts — SEPARATE from tools/handlers
|   |-- resources/               # MCP resource handlers (actors, assets, levels)
|   |-- server/                  # SDK construction, stdio lifecycle, tool/resource registry
|   |-- services/                # health-monitor, metrics-server (Prometheus)
|   |-- tools/                   # 23 parent tools: catalog/, definitions/{core,gameplay,shared,utility,world}/,
|   |                            #   handlers/<38 domains>/, orchestration/, dynamic/, editor/, level/, schemas/
|   |-- types/ utils/ wasm/      # utils has: commands config interaction logging paths responses serialization validation
|-- plugins/McpAutomationBridge/ # editor-only UE plugin (bridge + native MCP)
|   `-- Source/McpAutomationBridge/{Public,Private/}
|       Private/: Core/ Domains/ Foundation/ MCP/ Safety/ Transport/ Tests/ UI/
|-- tests/                       # Vitest unit tests + custom MCP integration runner
|-- scripts/                     # packaging, sync, smoke, cleanup
|-- docs/                        # handler maps, testing, plugin extension
`-- .github/workflows/           # pinned CI, release, registry, security
```
NOTE: `src/server/` tool-registry is split (`tool-registry.ts` + `tool-registry-{client,elicitation,listing,manage-tools}.ts` + `resource-registry.ts`). The plugin `Private/Core/Subsystem/` holds the registration shards; the subsystem `.cpp` is there (not directly in `Core/`).

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Start TS MCP server | `src/cli.ts`, `src/index.ts`, `src/server/server-factory.ts`, `src/server/stdio-lifecycle.ts` | CLI shim -> public facade -> construction/lifecycle -> registration |
| Add/change a TS tool contract | `src/tools/catalog/consolidated-tool-definitions.ts`, `src/tools/definitions/` | Source of truth for parent tools, actions, categories, output schemas |
| Register TS tool behavior | `src/tools/orchestration/consolidated-handler-registration.ts`, `src/server/tool-registry.ts` | `consolidated-tool-handlers.ts` is the bootstrap/export facade |
| Implement TS action logic | `src/tools/handlers/<domain>/` (38 domains) | Validate/normalize, then use the shared dispatch helpers |
| Change WebSocket automation | `src/automation/` (plus `src/unreal-bridge*.ts` at root) | Handshake, connection policy, request tracking, token/TLS plumbing |
| Change Unreal request routing | `plugins/McpAutomationBridge/.../Private/Core/` | Queue, game-thread dispatch, handler registration, responses |
| Add Unreal bridge behavior | `plugins/McpAutomationBridge/.../Private/Domains/<Domain>/` | Register through `Private/Core/Subsystem/*Registration.cpp` shard |
| Add native MCP metadata | `plugins/McpAutomationBridge/.../Private/MCP/` | Self-register with `MCP_REGISTER_TOOL`; keep canonical names only |
| Change shared Unreal helpers | `plugins/McpAutomationBridge/.../Private/Foundation/` | Reflection, Blueprint, paths, responses, handler primitives |
| Fix UE save/load/delete crashes | `plugins/McpAutomationBridge/.../Private/Safety/` | Use the project wrappers and preserve verification/cleanup |
| Change bridge sockets | `plugins/McpAutomationBridge/.../Private/Transport/` | WebSocket framing/TLS plus connection auth, rate limits, telemetry |
| Path/command security | `src/utils/paths/path-security.ts`, `src/utils/commands/command-validator.ts` | Enforce UE roots and console-command block lists |
| Vitest unit tests | `tests/unit/`, `src/**/*.test.ts` | No Unreal required |
| Integration tests | `tests/integration.mjs`, `tests/test-runner.mjs`, `tests/mcp-tools/` | Unreal-dependent unless a live editor is present |
| Version bump | package.json, server.json, `.uplugin` (see NOTES) | `bump-version.yml` is stale; bump manually |
| Plugin packaging | `scripts/package-plugin.sh <UE_ROOT> [out]`, `scripts/package-plugin.bat` | Runs RunUAT BuildPlugin |
| Plugin sync (source) | `scripts/sync-mcp-plugin.js` | Copies plugin into Engine/Project; not a build |

## CODE MAP
| Symbol | Type | Location | Role |
|--------|------|----------|------|
| `createServer()` / `startStdioServer()` | TS fn | `src/server/server-factory.ts`, `src/server/stdio-lifecycle.ts` | Server construction, stdio lifecycle, stdout safety |
| `registerDefaultHandlers()` | TS fn | `src/tools/orchestration/consolidated-handler-registration.ts` | Parent tool -> handler map |
| `executeAutomationRequest()` | TS fn | `src/tools/handlers/foundation/dispatch/automation-request-dispatch.ts` | Validated TS-to-Unreal request boundary |
| `AutomationBridge` / `UnrealBridge` | TS class | `src/automation/bridge.ts`, `src/unreal-bridge.ts` | WebSocket connect/handshake/queue; bridge client |
| `routeStdoutLogsToStderr()` | TS fn | `src/server/server-factory.ts` | Redirects logs off JSON-RPC stdout |
| `UMcpAutomationBridgeSubsystem` | C++ class | `.../Public/McpAutomationBridgeSubsystem.h` (+ `Private/Core/Subsystem/.cpp`) | Plugin request queue, native MCP startup, handler map |
| `FMcpNativeTransport` | C++ class | `.../Private/MCP/Transport/McpNativeTransport.h` | Native `/mcp` HTTP/SSE JSON-RPC endpoint |

## CONVENTIONS
### Transport Surfaces
1. **TypeScript stdio MCP**: `src/index.ts` exposes the public API; `src/server/` owns construction/lifecycle; `src/automation/` connects to Unreal. Defaults to **gateway mode** (single `unreal` tool); set `MCP_GATEWAY_MODE=false` to restore the legacy 23-tool direct listing (validated boolean in `src/config.ts` EnvSchema, default `true`).
2. **WebSocket bridge**: plugin listen sockets default to loopback `8090,8091`; TS sends automation requests through the negotiated bridge. `MCP_AUTOMATION_CLIENT_MODE=true` flips TS to server mode.
3. **Native MCP**: optional plugin HTTP/SSE under `Private/MCP/`; `GET /mcp` opens SSE, `POST /mcp` handles JSON-RPC, `DELETE /mcp` tears down sessions. Default port `3000` (override with `MCP_NATIVE_PORT`). Defaults to gateway mode via `bEnableNativeGateway` (default `true`); disable in Project Settings to restore the 23-tool native listing.

### Security Boundaries
- Loopback-only by default. Non-loopback requires `MCP_AUTOMATION_ALLOW_NON_LOOPBACK=true` (TS) or `bAllowNonLoopback` (plugin). The two flags are independent surfaces (the TS bridge is a WebSocket *client* to the plugin socket, not a second server).
- **Fail-closed LAN coupling**: the native MCP transport refuses to bind non-loopback unless `bRequireCapabilityToken` is also enabled, so a LAN-exposed surface can never start without auth. The TS stdio bridge has no server socket of its own, so its non-loopback opt-in is the Node.js `MCP_AUTOMATION_ALLOW_NON_LOOPBACK`/`MCP_AUTOMATION_HOST=0.0.0.0` pair, which must be paired with capability-token planning on the plugin side. Loopback default-allow means any LAN client can call any tool unauthenticated once exposed.
- Capability-token auth: `X-MCP-Capability-Token` (native MCP) and `bridge_hello.capabilityToken` (WebSocket) when enabled. Tokens are compared in **constant time** (`McpConstantTimeTokenEquals` in `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Foundation/McpSecureTokenCompare.h`) on both transports, so comparison time never leaks how much of a token matched.
- Metrics are separate: non-loopback metrics requires both `MCP_METRICS_ALLOW_NON_LOOPBACK=true` and `MCP_METRICS_TOKEN`.
- Paths limited to `/Game`, `/Engine`, `/Script`, `/Temp`, `/Niagara`, plus sanitized `MCP_ADDITIONAL_PATH_PREFIXES`. Preserve `/Game/...` normalization; do not add code depending on unnormalized `/Content/...`.

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
- **Smoke (`npm run test:smoke`)**: mock in-memory MCP check against **built `dist/`**. The script self-sets `MOCK_UNREAL_CONNECTION=true` and imports `dist/index.js`, so run `npm run build` first. The `MOCK_UNREAL_CONNECTION=true npm run test:smoke` form is redundant.
- **Integration (`npm test` / `npm run test:all` — identical)**: `node tests/integration.mjs`, which spawns `node dist/cli.js` over stdio and expects a **live Unreal Editor + bridge**. No mock/live branch exists in the entrypoint; it auto-builds `dist/` if missing but still needs a running editor. `UE_PROJECT_PATH` is consumed only by the spawned server via inherited env.
- **Native parity**: `npm run test:native-parity` (TS vs native canonical tool/action equality); `npm run test:params` adds static+strict+optional-strict parameter audit.
- **Expectation grammar**: split on `|` (or ` or `); first token is the primary intent and must be `success`/`error`/`timeout`. Narrow alternatives (`already exists`, `not found`) allowed on success-primary cases. Forbidden: broad masks like `success|error`, or `timeout` after `error`. Unit `testTimeout` is 10s; integration client call 15s, server `timeoutMs` 30s.
- **CI order**: `eslint --max-warnings=0` → `npm run type-check` → `npm run test:unit`. Integration is NOT run in CI.

## ANTI-PATTERNS (THIS PROJECT)
- Bypassing registry flow: never call handlers directly instead of `toolRegistry.register()` and `handleConsolidatedToolCall()`.
- Raw WebSocket calls from tools: use `executeAutomationRequest()` and the automation bridge queue.
- Unvalidated external input: command strings via `CommandValidator`; paths via normalization/security helpers.
- LAN exposure by accident: do not bind `0.0.0.0` / non-loopback without explicit opt-in and token planning.
- Mixing transports: native `/mcp`, plugin WebSocket, TS stdio, and ACP are separate lifecycles; do not route around their registry/session boundaries.
- Treating `src/handlers/` and `src/tools/handlers/` as one: the former is MCP resource handlers, the latter is tool action logic.
- Editing generated artifacts: never place AGENTS files in `dist/`, `build/`, `coverage/`, `tests/reports/`, `tmp/`, plugin `Binaries/`, plugin `Intermediate/`, or uppercase staging mirrors (`Plugins/`).

## UNIQUE STYLES
- 23 canonical parent tools hide hundreds of actions behind action enums to reduce client context.
- Dynamic tool management exists in both TS and native MCP; `manage_tools` and `inspect` are protected (cannot be disabled; `core` category is fixed).
- The native plugin has self-describing MCP tool definitions in C++ separate from TS JSON schemas; `MCP_REGISTER_TOOL` only attempts static registration and non-canonical names are silently filtered.
- The bridge plugin is responsibility-split: `Core` routes, `Domains` implement, `Foundation` shares primitives, `Safety` wraps hazardous editor ops, `Transport` owns sockets.
- Both transports default to a single `unreal` gateway tool (`search`/`describe`/`execute`/`configure`). Discovery is **progressive and never dumps full schemas**: `describe` drills down `tool` summary -> `tool+action` parameter catalog -> `tool+action+param` single schema, and `perActionSchemas` is always `false` (the parameter catalog is the tool-union). Invalid calls return guided errors with `suggestions` plus an executable `nextCall`. Opt out of gateway mode per-surface (`MCP_GATEWAY_MODE=false` for TS; `Enable Native Gateway` off for native).
- Protocol version negotiation is **intentionally asymmetric**: the native `/mcp` transport supports exactly the three modern MCP versions (`2025-11-25`, `2025-06-18`, `2025-03-26`) and deliberately does not implement the later `2026-07-28` RC. The TypeScript SDK also accepts two older legacy versions (`2024-11-05`, `2024-10-07`) from its `SUPPORTED_PROTOCOL_VERSIONS` set, so the native surface is intentionally stricter than the TS surface.
- Source-contract tests in `tests/unit/plugin/*contracts.test.ts` read C++/C# files and assert required/forbidden patterns (incl. a 250 pure-line ceiling per file).

## COMMANDS
```bash
npm run build          # clean + tsc compile to dist/
npm run build:core     # compile only (no clean)
npm run dev            # ts-node-esm src/cli.ts (no build)
npm run lint           # eslint . (CI uses --ext .ts --max-warnings=0)
npm run type-check     # tsc --noEmit
npm run test:unit      # Vitest unit tests, no Unreal
npm run test:smoke     # mock in-memory MCP smoke test (needs built dist/)
npm test               # tests/integration.mjs — Unreal-dependent
npm run test:all       # identical to npm test
npm run test:native-parity
npm run test:params    # parity + strict parameter audit
npm run automation:sync
npm run clean:tmp
# single unit file:
npx vitest run tests/unit/<file>.test.ts
npm run test:unit:coverage
```

## NOTES
- **Version sources**: `package.json` (`version`) is the canonical source. `npm version` rewrites it together with `package-lock.json`, so both stay in lockstep. `server.json` versions the npm package distribution (top-level `version` plus the npm package `version`). The bridge version lives in its `.uplugin` `VersionName`. The native HTTP/SSE transport advertises `server-info.json` (`version`) and the `McpNativeTransport.h` `ServerVersion` `TEXT` fallback; the TS server advertises the `SERVER_VERSION` fallback in `src/server/server-factory.ts` when `package.json` cannot be read. Coordinated release bumps must resync all of: `package.json` (+`package-lock.json`), `server.json`, the McpAutomationBridge `.uplugin`, `server-info.json`, `src/server/server-factory.ts`, and `McpNativeTransport.h`. The `bump-version.yml` workflow already rewrites every one of these (via `npm version`, `jq`, and `perl`), so a coordinated bump is a single workflow run. Verify with `npm run version:check` (`tests/unit/version-consistency.test.ts`), which asserts agreement across all seven sources. For a manual audit, grep the canonical version across `package.json server.json plugins/*/*.uplugin plugins/*/Resources/MCP/server-info.json` rather than a hardcoded literal.
- **Engine reference path**: `/data/UnrealEngine/Engine/`.
- **External GitHub Actions** are pinned to full commit SHAs.
- **`GEMINI.md` is stale and not authoritative** — it references `src/unreal-bridge.ts` (now split into `src/unreal-bridge*.ts`), uppercase `Plugins/`, non-existent Rust modules, and `npm run test:control_actor`. Prefer this file and the nested AGENTS.
- Not instruction targets: `tests/reports/`, root `build/`, `tmp/`, `Public/`, uppercase `Plugins/`, `.cache/`, `.opencode/node_modules/`, and package/plugin build outputs.

## AREA GUIDES (read the closest one)
- `src/server/AGENTS.md` — MCP SDK construction, stdio lifecycle, tool/resource registry split.
- `tests/AGENTS.md` — integration harness, expectation grammar, audit contracts.
- `plugins/McpAutomationBridge/AGENTS.md` — plugin scope, cross-surface rules, packaging, validation.
- `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/AGENTS.md` — native MCP registry/session/transport lifecycle.
- `.github/copilot-instructions.md` — workspace-wide architecture + critical constraints (complements this file).
