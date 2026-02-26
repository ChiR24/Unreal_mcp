# Copilot instructions for Unreal MCP

## Big picture
- Dual-process system: TypeScript MCP server in `src/` + Unreal Editor plugin in `plugins/McpAutomationBridge/`.

## Non-obvious constraints (don’t break MCP I/O)
- Keep stdout JSON-only: runtime logs must go through `Logger` (see `routeStdoutLogsToStderr()` in `src/index.ts`). Avoid `console.log` in runtime code.
- `.env` loading is silenced to protect stdout (see `src/config.ts`).

## Key dev commands (from `package.json`)
- Dev: `npm run dev` (ts-node-esm)
- Build: `npm run build:core`
- Plugin sync: `npm run automation:sync`

## Connection + ports
- The UE plugin listens by default on `127.0.0.1` ports `8090,8091` (see `ListenPorts` in `plugins/.../McpAutomationBridgeSettings.cpp`).
- The TS bridge connects as a WebSocket client; override with env like `MCP_AUTOMATION_CLIENT_PORT` or `MCP_AUTOMATION_WS_PORTS` (see `src/automation/bridge.ts`).
- Offline mode: set `MOCK_UNREAL_CONNECTION=true` to force `UnrealBridge.tryConnect()` to succeed.

## Tooling conventions (TS)
- Tool schemas live in `src/tools/consolidated-tool-definitions.ts` (action enums + input/output schemas). Output schemas are registered at startup in `src/index.ts` and validated by AJV in `src/utils/response-validator.ts`.
- Tool routing is via `toolRegistry.register()` in `src/tools/consolidated-tool-handlers.ts` and domain logic in `src/tools/handlers/*-handlers.ts`.
- Always call Unreal through `executeAutomationRequest()` (`src/tools/handlers/common-handlers.ts`) rather than raw WS.

## Safety + normalization
- Console commands are safety-filtered by `src/utils/command-validator.ts`; don’t bypass it.
- UE calls are throttled/retried via `src/utils/unreal-command-queue.ts`.
- Prefer `/Game/...` paths; some layers normalize `/Content` → `/Game` (e.g. `src/resources/assets.ts`, `src/tools/assets.ts`, and plugin helpers in `plugins/.../McpAutomationBridgeHelpers.h`).

## Adding a new action end-to-end
1) TS: extend the tool’s `action` enum + schemas in `src/tools/consolidated-tool-definitions.ts`.
2) TS: route the action in `src/tools/consolidated-tool-handlers.ts` (or the appropriate `src/tools/handlers/*-handlers.ts`).
3) C++: implement the action in an appropriate `plugins/.../*Handlers.cpp` and register it in `UMcpAutomationBridgeSubsystem::InitializeHandlers()`.
4) Tests: add/extend a case in `tests/integration.mjs`.
