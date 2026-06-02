# PROJECT KNOWLEDGE BASE

**Generated:** 2026-05-28 11:48:27 IST
**Commit:** 866245c
**Branch:** AgentPanel

## OVERVIEW
MCP tooling for Unreal Engine 5.0-5.8 Preview. Server package version `0.5.21`; bridge plugin version `0.1.4`; Unreal Agent plugin version `0.1.0`. The repo has three user-facing surfaces: a TypeScript stdio MCP server that talks to Unreal over WebSocket, an optional native plugin MCP HTTP/SSE endpoint at `/mcp`, and an experimental in-editor OpenCode ACP panel.

## STRUCTURE
```
./
|-- src/                         # TypeScript MCP server, NodeNext ESM
|   |-- server/                  # MCP tool/resource registration and dynamic filtering
|   |-- tools/                   # 23 parent tool schemas, action enums, TS dispatch
|   |-- automation/              # WebSocket client, handshake, request tracking
|   |-- services/                # health and optional Prometheus metrics
|   `-- utils/                   # path safety, command validation, logging, schemas
|-- plugins/McpAutomationBridge/ # Unreal editor automation bridge + native MCP
|   `-- Source/McpAutomationBridge/
|       |-- Public/              # settings, subsystem, connection manager API
|       `-- Private/             # WS bridge, native MCP, domain handlers
|-- plugins/UnrealAgent/         # in-editor OpenCode ACP assistant panel
|   `-- Source/UnrealAgent/Private/{Acp,UI}/
|-- tests/                       # Vitest unit tests and custom MCP integration runner
|-- scripts/                     # plugin packaging, sync, smoke, cleanup helpers
|-- docs/                        # handler maps, testing, and plugin extension notes
`-- .github/workflows/           # pinned CI, release, registry, security workflows
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Start TS MCP server | `src/cli.ts`, `src/index.ts`, `src/server-setup.ts` | CLI shim -> stdio server -> resource/tool registration |
| Add or change a TS tool contract | `src/tools/consolidated-tool-definitions.ts` | Source of truth for parent tools, actions, categories, output schemas |
| Register TS tool behavior | `src/tools/consolidated-tool-handlers.ts`, `src/server/tool-registry.ts` | Register through the MCP registry path only |
| Implement TS action logic | `src/tools/handlers/*-handlers.ts` | Validate/normalize, then call `executeAutomationRequest()` |
| Change WebSocket automation | `src/automation/` | Handshake, connection policy, request tracking, token/TLS plumbing |
| Add Unreal bridge behavior | `plugins/McpAutomationBridge/.../Private/McpAutomationBridge_*Handlers.cpp` | Register in `UMcpAutomationBridgeSubsystem::InitializeHandlers()` |
| Add native MCP schema/tool metadata | `plugins/McpAutomationBridge/.../Private/MCP/` | Self-register with `MCP_REGISTER_TOOL`; keep canonical names only |
| Fix UE save/load crashes | `McpSafeOperations.h`, `McpAutomationBridgeHelpers.h` | Use project safe wrappers and path guards |
| Add Unreal Agent ACP behavior | `plugins/UnrealAgent/Source/UnrealAgent/Private/Acp/` | OpenCode process lifecycle, JSON-RPC, model/agent/permission handling |
| Change Unreal Agent panel UI | `plugins/UnrealAgent/Source/UnrealAgent/Private/UI/` | Slate layout, stable widget tags, transcript rendering |
| Path and command security | `src/utils/path-security.ts`, `src/utils/command-validator.ts` | Enforce UE roots and console-command block lists |
| Integration tests | `tests/test-runner.mjs`, `tests/mcp-tools/` | Pipe-separated expectations; Unreal-dependent unless mocked |
| Unreal Agent tests | `plugins/UnrealAgent/Source/UnrealAgent/Private/UnrealAgentAutomationTests.cpp` | Slate tag/layout and ACP protocol smoke coverage |
| Version bump | `.github/workflows/bump-version.yml` | Updates server files; plugin `.uplugin` versions are separate |
| Plugin packaging | `scripts/package-plugin.*`, `scripts/sync-mcp-plugin.js` | RunUAT packaging and Engine/Project plugin sync |

## CODE MAP
| Symbol | Type | Location | Refs | Role |
|--------|------|----------|------|------|
| `createServer()` / `startStdioServer()` | TS functions | `src/index.ts` | high | Stdio MCP lifecycle and stdout safety |
| `registerDefaultHandlers()` | TS function | `src/tools/consolidated-tool-handlers.ts` | high | Parent tool -> handler map |
| `executeAutomationRequest()` | TS function | `src/tools/handlers/common-handlers.ts` | high | TS-to-Unreal request boundary |
| `AutomationBridge` | TS class | `src/automation/bridge.ts` | high | WebSocket connect/handshake/request queue |
| `UMcpAutomationBridgeSubsystem` | C++ class | `plugins/McpAutomationBridge/.../Public/McpAutomationBridgeSubsystem.h` | high | Plugin request queue, native MCP startup, handler map |
| `FMcpNativeTransport` | C++ class | `plugins/McpAutomationBridge/.../Private/MCP/McpNativeTransport.*` | medium | Native `/mcp` HTTP/SSE JSON-RPC endpoint |
| `FOpenCodeAcpClient` | C++ class | `plugins/UnrealAgent/.../Private/Acp/McpOpenCodeAcpClient.*` | medium | OpenCode ACP process and protocol client |
| `SUnrealAgentPanel` | Slate widget | `plugins/UnrealAgent/.../Private/UI/SUnrealAgentPanel.*` | medium | In-editor chat panel surface |

## CONVENTIONS
### Transport Surfaces
1. **TypeScript stdio MCP**: `src/index.ts` creates the SDK server, keeps stdout JSON-only, and connects to Unreal through `src/automation/`.
2. **WebSocket bridge**: Plugin listen sockets default to loopback ports `8090,8091`; TS sends automation requests through the negotiated bridge.
3. **Native MCP**: Optional plugin HTTP/SSE endpoint under `Private/MCP/`; `GET /mcp` opens SSE, `POST /mcp` handles JSON-RPC, `DELETE /mcp` tears down sessions.
4. **Unreal Agent ACP**: `plugins/UnrealAgent` starts `opencode acp` from the editor and may attach the configured `unreal-engine` MCP server; runtime prompt guidance lives in the plugin README and ACP client.

### Security Boundaries
- Loopback-only is the default. Non-loopback requires `MCP_AUTOMATION_ALLOW_NON_LOOPBACK=true` in TS or `bAllowNonLoopback` in plugin settings.
- Capability-token auth uses `X-MCP-Capability-Token` for native MCP and `bridge_hello.capabilityToken` for WebSocket when enabled.
- Metrics are separate: non-loopback metrics require both `MCP_METRICS_ALLOW_NON_LOOPBACK=true` and `MCP_METRICS_TOKEN`.
- Paths are limited to `/Game`, `/Engine`, `/Script`, `/Temp`, `/Niagara`, plus sanitized `MCP_ADDITIONAL_PATH_PREFIXES`.
- Unreal Agent must resolve `opencode` from `OPENCODE_ACP_COMMAND`, `~/.opencode/bin/opencode`, or absolute PATH entries outside the current project directory.

### UE Safety
- Do not call `UPackage::SavePackage()` directly. Use `McpSafeAssetSave`, `McpSafeLevelSave`, or `McpSafeLoadMap` wrappers.
- Blueprint component templates must be owned by SCS nodes created through `SCS->CreateNode()` and `SCS->AddNode()`.
- Do not introduce `ANY_PACKAGE`; use modern lookup patterns such as `nullptr` or project helper resolution.

### TypeScript Standards
- Strict NodeNext TypeScript. Do not add `as any`, `@ts-ignore`, or runtime `console.log`.
- Runtime logs must go through `Logger`; `routeStdoutLogsToStderr()` protects JSON-RPC stdout.
- Output schemas are registered at startup and should stay schema-backed.

## ANTI-PATTERNS (THIS PROJECT)
- Bypassing registry flow: never call handlers directly instead of `toolRegistry.register()` and `handleConsolidatedToolCall()`.
- Raw WebSocket calls from tools: use `executeAutomationRequest()` and the automation bridge queue.
- Unvalidated external input: command strings go through `CommandValidator`; paths go through normalization/security helpers.
- LAN exposure by accident: do not bind to `0.0.0.0` or non-loopback without explicit opt-in and token planning.
- Unreal Agent overclaiming: quick prompts may use source/config/docs/log context, but not live editor state unless ACP is configured with MCP tools.
- Generated knowledge bases: do not place AGENTS files in `dist/`, `build/`, `coverage/`, `tests/reports/`, `tmp/`, plugin `Binaries/`, plugin `Intermediate/`, or uppercase staging mirrors such as `Plugins/`.

## UNIQUE STYLES
- 23 canonical parent tools hide hundreds of actions behind action enums to reduce client context.
- Dynamic tool management exists in both TS and native MCP; `manage_tools` and `inspect` are protected.
- The native plugin has self-describing MCP tool definitions in C++ separate from TS JSON schemas.
- Test expectations use string grammar such as `success|error|timeout`; first token is the primary intent.
- Unreal Agent uses stable Slate widget tags (`UnrealAgent.*`) as its automation-test contract.

## COMMANDS
```bash
npm run build:core      # Compile TypeScript server
npm run type-check      # Type-check without emitting
npm run test:unit       # Vitest unit tests, no Unreal required
npm run test:smoke      # Mock-mode stdio smoke test
npm test                # Unreal-dependent MCP integration entry
npm run test:params     # Parameter-combination audit
npm run automation:sync # Copy/sync bridge plugin into a target project
npm run clean:tmp       # Safe cleanup of repo tmp/ artifacts
```

## NOTES
- Engine reference path: `/data/UnrealEngine/Engine/`.
- Server version sources: `package.json`, `package-lock.json`, `server.json`, `src/index.ts` fallback. Plugin version sources: `plugins/McpAutomationBridge/McpAutomationBridge.uplugin` and `plugins/UnrealAgent/UnrealAgent.uplugin`.
- External GitHub Actions are expected to be pinned to full commit SHAs.
- `tests/reports/`, root `build/`, root `tmp/`, root `Public/`, uppercase `Plugins/`, and package/plugin build outputs are not instruction targets.
