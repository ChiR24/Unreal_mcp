# Copilot instructions for Unreal MCP

## Architecture
- This is a dual-process system: the TypeScript MCP server lives in `src/`, and the Unreal Editor bridge plugin lives in `plugins/McpAutomationBridge/`.
- Typical flow is the single `unreal` gateway tool -> capability lookup + strict schema validation -> `executeAutomationRequest()` -> WebSocket bridge -> C++ subsystem queue -> game-thread handler. There is no TypeScript per-action layer; only `manage_tools` runs in process.
- Keep workspace-wide guidance here and rely on the closer `AGENTS.md` files for area-specific detail under `src/tools/`, `src/server/gateway/`, `src/automation/`, `tests/`, and `plugins/McpAutomationBridge/`.

## Critical constraints
- Keep stdout JSON-only. Runtime logs must go through the project logger; do not use `console.log` in runtime code. See `routeStdoutLogsToStderr()` in `src/server/server-factory.ts`.
- `.env` loading is intentionally quiet to avoid corrupting MCP I/O. Keep it that way in `src/config.ts`.
- Send Unreal work through `executeAutomationRequest()`, not raw WebSocket calls.
- Preserve path normalization. Prefer `/Game/...` asset paths and do not add new code that depends on `/Content/...` input staying unnormalized.

## UE 5.7 safety
- Do not use `UPackage::SavePackage()` in plugin code. Use the safe wrappers under `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Safety/`.
- For Blueprint component templates, let SCS own nodes and templates through `CreateNode()` and `AddNode()` patterns.
- Do not introduce new `ANY_PACKAGE` usage; use modern lookup patterns such as `nullptr` where required by newer UE versions.

## Build and test
- `npm run dev`: run the server from TypeScript.
- `npm run build:core`: compile the TypeScript server.
- `npm run automation:sync`: sync the Unreal plugin into a target project.
- `npm run test:unit`: run Vitest unit tests without Unreal.
- `npm run test:smoke`: run the offline smoke path using mock connection mode.
- `npm test`: run the MCP integration suite. This requires Unreal Editor with the bridge plugin available.

## Connection and runtime behavior
- The Unreal plugin listens on loopback by default and commonly uses ports `8090` and `8091`.
- The TypeScript bridge connects as a WebSocket client to one host and port (`MCP_AUTOMATION_HOST`, `MCP_AUTOMATION_PORT`, or the project config found through `UE_PROJECT_PATH`).
- Mock mode is an explicit development path: set `MOCK_UNREAL_CONNECTION=true` when you need the bridge layer to succeed without a live editor.
- Connection setup includes handshake and capability negotiation in `src/automation/`; keep protocol changes aligned across TypeScript and plugin code.

## Tooling conventions
- Tool contracts are the capability records under `src/tools/catalog/capabilities/records/`; every `*.generated.*` file derives from them. Never hand-edit generated files.
- Every gateway response is validated against the `unreal` output schema, and each execute result against its record's declared output schema.
- Console commands are safety-filtered in `src/utils/commands/command-validator.ts`; do not add bypasses.

## Making changes
- New MCP action flow:
	1. Author a capability record under `src/tools/catalog/capabilities/records/<parent>/` declaring exactly the params the handler reads, then run `npm run registry:generate`.
	2. Implement the Unreal side under `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/<Domain>/` and make sure its parent tool reaches it in `Core/Subsystem/...HandlerRegistration.cpp`.
	3. Add an integration case under `tests/mcp-tools/` covering every optional param (`npm run test:params` is strict).
- Keep TypeScript strict. Avoid `as any` in runtime code.
- Do not hand-bump versions: the `bump-version.yml` workflow rewrites all four version files (`package.json`, `package-lock.json`, `server.json`, the `.uplugin`).

## Reference points
- Use `src/tools/handlers/foundation/dispatch/automation-request-dispatch.ts` for the one bridge send path (`executeAutomationRequest()`).
- Use `tests/AGENTS.md` for integration test conventions, expected result strings, and timeout tiers.
- Use `plugins/McpAutomationBridge/AGENTS.md` for plugin-side patterns and Unreal-specific caveats.
