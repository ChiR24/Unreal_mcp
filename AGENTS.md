# PROJECT KNOWLEDGE BASE

**Generated:** 2026-06-07 12:38:08 IST
**Commit:** f6c6285e
**Branch:** dev

## OVERVIEW
MCP tooling for Unreal Engine 5.0-5.8 Preview. Server package version `0.5.30`; bridge plugin version `0.5.30`. The repo has three user-facing surfaces: a TypeScript stdio MCP server, the bridge plugin's WebSocket and optional native `/mcp` HTTP/SSE transports, and an experimental UnrealAgent editor panel that drives OpenCode over ACP.

## STRUCTURE
```
./
|-- src/                         # TypeScript MCP server, NodeNext ESM
|   |-- server/                  # MCP tool/resource registration and dynamic filtering
|   |-- tools/                   # 24 parent tool schemas, action enums, TS dispatch
|   |-- automation/              # WebSocket client, handshake, request tracking
|   |-- services/                # health and optional Prometheus metrics
|   `-- utils/                   # path safety, command validation, logging, schemas
|-- plugins/McpAutomationBridge/ # Unreal editor automation bridge + native MCP
|   `-- Source/McpAutomationBridge/
|       |-- Public/              # settings, subsystem, connection manager API
|       `-- Private/             # core routing, domains, shared helpers, safety, transports
|-- plugins/UnrealAgent/         # optional in-editor OpenCode ACP assistant panel
|-- tests/                       # Vitest unit tests and custom MCP integration runner
|-- scripts/                     # plugin packaging, sync, smoke, cleanup helpers
|-- docs/                        # handler maps, testing, and plugin extension notes
`-- .github/workflows/           # pinned CI, release, registry, security workflows
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Start TS MCP server | `src/cli.ts`, `src/index.ts`, `src/server/server-factory.ts`, `src/server/stdio-lifecycle.ts` | CLI shim -> public facade -> construction/lifecycle -> registration |
| Add or change a TS tool contract | `src/tools/catalog/consolidated-tool-definitions.ts`, `src/tools/definitions/` | Source of truth for parent tools, actions, categories, output schemas |
| Register TS tool behavior | `src/tools/orchestration/consolidated-handler-registration.ts`, `src/server/tool-registry.ts` | `consolidated-tool-handlers.ts` is the bootstrap/export facade |
| Implement TS action logic | `src/tools/handlers/<domain>/` | Validate/normalize, then use the shared dispatch helpers |
| Change WebSocket automation | `src/automation/` | Handshake, connection policy, request tracking, token/TLS plumbing |
| Change Unreal request routing | `plugins/McpAutomationBridge/.../Private/Core/` | Queue, game-thread dispatch, handler registration, responses |
| Add Unreal bridge behavior | `plugins/McpAutomationBridge/.../Private/Domains/<Domain>/` | Register through the matching `Private/Core/Subsystem/*Registration.cpp` shard |
| Add native MCP schema/tool metadata | `plugins/McpAutomationBridge/.../Private/MCP/` | Self-register with `MCP_REGISTER_TOOL`; keep canonical names only |
| Change shared Unreal helpers | `plugins/McpAutomationBridge/.../Private/Foundation/` | Reflection, Blueprint helpers, paths, responses, handler utilities |
| Fix UE save/load/delete crashes | `plugins/McpAutomationBridge/.../Private/Safety/` | Use the project wrappers and preserve verification/cleanup |
| Change bridge sockets | `plugins/McpAutomationBridge/.../Private/Transport/` | WebSocket framing/TLS plus connection auth, rate limits, telemetry |
| Change UnrealAgent ACP/UI | `plugins/UnrealAgent/.../Private/Acp/`, `plugins/UnrealAgent/.../Private/UI/` | Keep process/protocol work out of Slate layout code |
| Path and command security | `src/utils/paths/path-security.ts`, `src/utils/commands/command-validator.ts` | Enforce UE roots and console-command block lists |
| Integration tests | `tests/test-runner.mjs`, `tests/mcp-tools/` | Pipe-separated expectations; Unreal-dependent unless mocked |
| Version bump | `.github/workflows/bump-version.yml` | Updates server files; plugin `.uplugin` versions are separate |
| Plugin packaging | `scripts/package-plugin.*`, `scripts/sync-mcp-plugin.js` | RunUAT packaging and Engine/Project plugin sync |

## CODE MAP
| Symbol | Type | Location | Refs | Role |
|--------|------|----------|------|------|
| `createServer()` / `startStdioServer()` | TS functions | `src/server/server-factory.ts`, `src/server/stdio-lifecycle.ts` | high | Server construction, stdio lifecycle, and stdout safety |
| `registerDefaultHandlers()` | TS function | `src/tools/orchestration/consolidated-handler-registration.ts` | medium | Parent tool -> handler map |
| `executeAutomationRequest()` | TS function | `src/tools/handlers/foundation/dispatch/automation-request-dispatch.ts` | very high | Validated TS-to-Unreal request boundary |
| `AutomationBridge` | TS class | `src/automation/bridge.ts` | high | WebSocket connect/handshake/request queue |
| `UMcpAutomationBridgeSubsystem` | C++ class | `plugins/McpAutomationBridge/.../Public/McpAutomationBridgeSubsystem.h` | high | Plugin request queue, native MCP startup, handler map |
| `FMcpNativeTransport` | C++ class | `plugins/McpAutomationBridge/.../Private/MCP/Transport/` | medium | Native `/mcp` HTTP/SSE JSON-RPC endpoint |
| `SUnrealAgentPanel` / `FOpenCodeAcpClient` | C++ classes | `plugins/UnrealAgent/.../Private/UI/Core/`, `Private/Acp/Client/` | high | In-editor ACP chat surface and process/session client |

## CONVENTIONS
### Transport Surfaces
1. **TypeScript stdio MCP**: `src/index.ts` exposes the public API, `src/server/` owns construction/lifecycle, and `src/automation/` connects to Unreal.
2. **WebSocket bridge**: Plugin listen sockets default to loopback ports `8090,8091`; TS sends automation requests through the negotiated bridge.
3. **Native MCP**: Optional plugin HTTP/SSE endpoint under `Private/MCP/`; `GET /mcp` opens SSE, `POST /mcp` handles JSON-RPC, `DELETE /mcp` tears down sessions.
4. **UnrealAgent ACP**: Optional editor panel starts `opencode acp`; it can inject the configured native `unreal-engine` MCP endpoint but does not expose MCP tools itself.

### Security Boundaries
- Loopback-only is the default. Non-loopback requires `MCP_AUTOMATION_ALLOW_NON_LOOPBACK=true` in TS or `bAllowNonLoopback` in plugin settings.
- Capability-token auth uses `X-MCP-Capability-Token` for native MCP and `bridge_hello.capabilityToken` for WebSocket when enabled.
- Metrics are separate: non-loopback metrics require both `MCP_METRICS_ALLOW_NON_LOOPBACK=true` and `MCP_METRICS_TOKEN`.
- Paths are limited to `/Game`, `/Engine`, `/Script`, `/Temp`, `/Niagara`, plus sanitized `MCP_ADDITIONAL_PATH_PREFIXES`.

### UE Safety
- Do not call `UPackage::SavePackage()` directly. Use `McpSafeAssetSave`, `McpSafeLevelSave`, or `McpSafeLoadMap` wrappers.
- Blueprint component templates must be owned by SCS nodes created through `SCS->CreateNode()` and `SCS->AddNode()`.
- Do not introduce `ANY_PACKAGE`; use modern lookup patterns such as `nullptr` or project helper resolution.
- Editor API work enters through the subsystem queue and runs on the game thread; unsafe save/GC/async-load states are deferred.

### TypeScript Standards
- Strict NodeNext TypeScript. Do not add `as any`, `@ts-ignore`, or runtime `console.log`.
- Runtime logs must go through `Logger`; `routeStdoutLogsToStderr()` protects JSON-RPC stdout.
- Output schemas are registered at startup and should stay schema-backed.

## ANTI-PATTERNS (THIS PROJECT)
- Bypassing registry flow: never call handlers directly instead of `toolRegistry.register()` and `handleConsolidatedToolCall()`.
- Raw WebSocket calls from tools: use `executeAutomationRequest()` and the automation bridge queue.
- Unvalidated external input: command strings go through `CommandValidator`; paths go through normalization/security helpers.
- LAN exposure by accident: do not bind to `0.0.0.0` or non-loopback without explicit opt-in and token planning.
- Mixing transports: native `/mcp`, plugin WebSocket, TS stdio, and ACP are separate lifecycles; do not route around their registry/session boundaries.
- Generated knowledge bases: do not place AGENTS files in `dist/`, `build/`, `coverage/`, `tests/reports/`, `tmp/`, plugin `Binaries/`, plugin `Intermediate/`, or uppercase staging mirrors such as `Plugins/`.

## UNIQUE STYLES
- 24 canonical parent tools hide hundreds of actions behind action enums to reduce client context.
- Dynamic tool management exists in both TS and native MCP; `manage_tools` and `inspect` are protected.
- The native plugin has self-describing MCP tool definitions in C++ separate from TS JSON schemas.
- Test expectations use string grammar such as `success|error|timeout`; first token is the primary intent.
- The bridge plugin is responsibility-split: `Core` routes, `Domains` implement, `Foundation` shares primitives, `Safety` wraps hazardous editor operations, and `Transport` owns sockets.

## COMMANDS
```bash
npm run build:core      # Compile TypeScript server
npm run type-check      # Type-check without emitting
npm run test:unit       # Vitest unit tests, no Unreal required
npm run test:smoke      # Mock-mode in-memory MCP smoke test
npm test                # Unreal-dependent MCP integration entry
npm run test:native-parity # Compare TS and native MCP tool surfaces
npm run test:params     # Native parity + strict parameter-combination audit
npm run automation:sync # Copy/sync bridge plugin into a target project
npm run clean:tmp       # Safe cleanup of repo tmp/ artifacts
```

## NOTES
- Engine reference path: `/data/UnrealEngine/Engine/`.
- Server version sources: `package.json`, `package-lock.json`, `server.json`, and the `src/server/server-factory.ts` fallback. Bridge and UnrealAgent versions live in their separate `.uplugin` files.
- External GitHub Actions are expected to be pinned to full commit SHAs.
- `tests/reports/`, root `build/`, root `tmp/`, root `Public/`, uppercase `Plugins/`, `.cache/`, `.opencode/node_modules/`, and package/plugin build outputs are not instruction targets.
