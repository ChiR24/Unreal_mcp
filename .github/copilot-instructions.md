# Copilot instructions for Unreal MCP

## Architecture
- This is a dual-process system: the TypeScript MCP server lives in `src/`, and the Unreal Editor bridge plugin lives in `plugins/McpAutomationBridge/`.
- Typical flow is schema validation -> tool registry dispatch -> domain handler -> `executeAutomationRequest()` -> WebSocket bridge -> C++ subsystem -> game-thread handler.
- Keep workspace-wide guidance here and rely on the closer `AGENTS.md` files for area-specific detail under `src/tools/`, `src/tools/handlers/`, `src/automation/`, `tests/`, and `plugins/McpAutomationBridge/`.

## Critical constraints
- Keep stdout JSON-only. Runtime logs must go through the project logger; do not use `console.log` in runtime code. See `routeStdoutLogsToStderr()` in `src/index.ts`.
- `.env` loading is intentionally quiet to avoid corrupting MCP I/O. Keep it that way in `src/config.ts`.
- Do not bypass the tool routing stack. Register tool behavior via `toolRegistry.register()` and send Unreal work through `executeAutomationRequest()`, not raw WebSocket calls.
- Preserve path normalization. Prefer `/Game/...` asset paths and do not add new code that depends on `/Content/...` input staying unnormalized.

## UE 5.7 safety
- Do not use `UPackage::SavePackage()` in plugin code. Use the safe helper wrappers in `plugins/McpAutomationBridge/Source/McpAutomationBridge/Public/McpAutomationBridgeHelpers.h`.
- For Blueprint component templates, let SCS own nodes and templates through `CreateNode()` and `AddNode()` patterns.
- Do not introduce new `ANY_PACKAGE` usage; use modern lookup patterns such as `nullptr` where required by newer UE versions.

## Build and test
- `npm run dev`: run the server from TypeScript.
- `npm run build:core`: compile the TypeScript server.
- `npm run automation:sync`: sync the Unreal plugin into a target project.
- `npm run test:unit`: run Vitest unit tests without Unreal.
- `npm run test:smoke`: run the offline smoke path using mock connection mode.
- `npm test`: run the MCP integration suite. This requires Unreal Editor with the bridge plugin available.
- `npm run test:all`: run the integration entrypoints; treat this as Unreal-dependent unless you intentionally configured mock coverage.

## Connection and runtime behavior
- The Unreal plugin listens on loopback by default and commonly uses ports `8090` and `8091`.
- The TypeScript bridge connects as a WebSocket client and can be steered with env such as `MCP_AUTOMATION_CLIENT_PORT`, `MCP_AUTOMATION_PORT`, or `MCP_AUTOMATION_WS_PORTS`.
- Mock mode is an explicit development path: set `MOCK_UNREAL_CONNECTION=true` when you need the bridge layer to succeed without a live editor.
- Connection setup includes handshake and capability negotiation in `src/automation/`; keep protocol changes aligned across TypeScript and plugin code.

## Tooling conventions
- Tool schemas and action enums live in `src/tools/consolidated-tool-definitions.ts`. Treat that file as the source of truth for inputs and outputs.
- Output schemas are registered during startup and validated before responses leave the server. Keep new tool outputs schema-backed.
- Console commands are safety-filtered in `src/utils/command-validator.ts`; do not add bypasses.
- Unreal requests are queued and retried through the existing command queue utilities; reuse that path instead of inventing parallel transport logic.

## Making changes
- New MCP action flow:
	1. Add the action enum and schemas in `src/tools/consolidated-tool-definitions.ts`.
	2. Route the action in `src/tools/consolidated-tool-handlers.ts` or the relevant handler module under `src/tools/handlers/`.
	3. Implement the Unreal side in the appropriate handler under `plugins/McpAutomationBridge/Source/` and register it in `UMcpAutomationBridgeSubsystem::InitializeHandlers()`.
	4. Add or update tests in `tests/integration.mjs`, `tests/mcp-tools/`, or colocated unit tests as appropriate.
- Keep TypeScript strict. Avoid `as any` in runtime code.
- If you change versions, update all version-bearing files together, including `package.json`, `server.json`, `src/index.ts`, and `plugins/McpAutomationBridge/McpAutomationBridge.uplugin`.

## Reference points
- Use `src/tools/handlers/` for examples of handler structure and `executeAutomationRequest()` usage.
- Use `tests/AGENTS.md` for integration test conventions, expected result strings, and timeout tiers.
- Use `plugins/McpAutomationBridge/AGENTS.md` for plugin-side patterns and Unreal-specific caveats.
