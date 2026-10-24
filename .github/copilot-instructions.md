# Copilot instructions for Unreal MCP

## Architecture
- The Unreal Editor plugin in `plugins/EngineWire/` does all the work and serves MCP itself at `/mcp` (port 3000). `src/` holds the capability records every contract is generated from, and a stdio proxy (`src/proxy/native-proxy.ts`) that relays to `/mcp`.
- Typical flow is the single `unreal` gateway tool -> native capability lookup + strict schema validation -> C++ subsystem queue -> game-thread handler. There is no TypeScript gateway or per-action layer.
- Keep workspace-wide guidance here and rely on the closer `AGENTS.md` files for area-specific detail under `src/tools/`, `tests/`, and `plugins/EngineWire/`.

## Critical constraints
- Keep stdout JSON-only. Logs go through `src/utils/logging/logger.ts` to stderr; do not use `console.log` in runtime code.
- Preserve path normalization. Prefer `/Game/...` asset paths and do not add new code that depends on `/Content/...` input staying unnormalized.

## UE 5.7 safety
- Do not use `UPackage::SavePackage()` in plugin code. Use the safe wrappers under `plugins/EngineWire/Source/EngineWire/Private/Safety/`.
- For Blueprint component templates, let SCS own nodes and templates through `CreateNode()` and `AddNode()` patterns.
- Do not introduce new `ANY_PACKAGE` usage; use modern lookup patterns such as `nullptr` where required by newer UE versions.

## Build and test
- `npm run dev`: run the proxy from TypeScript.
- `npm run build:core`: compile the TypeScript.
- `npm run automation:sync`: sync the Unreal plugin into a target project.
- `npm run test:unit`: run Vitest unit tests without Unreal. No mocks or fakes.
- `npm test`: run the MCP integration suite. This requires Unreal Editor serving `/mcp`.

## Connection and runtime behavior
- The plugin listens on loopback by default: native MCP on `3000` (on by default), the WebSocket bridge on `8090` and `8091`.
- The proxy talks HTTP to `UNREAL_MCP_URL` (default `http://127.0.0.1:3000/mcp`) and reads the capability token from `UE_PROJECT_PATH` (or `MCP_AUTOMATION_CAPABILITY_TOKEN`). While the editor is down every call answers `NOT_CONNECTED`.

## Tooling conventions
- Tool contracts are the capability records under `src/tools/catalog/capabilities/records/`; every `*.generated.*` file derives from them. Never hand-edit generated files.
- Every execute result is validated against its record's declared output schema, in the plugin.
- Console commands are safety-filtered in the plugin by the policy generated from `src/utils/commands/console-command-policy-rules.ts`; do not add bypasses.

## Making changes
- New MCP action flow:
	1. Author a capability record under `src/tools/catalog/capabilities/records/<parent>/` declaring exactly the params the handler reads, then run `npm run registry:generate`.
	2. Implement the Unreal side under `plugins/EngineWire/Source/EngineWire/Private/Domains/<Domain>/` and make sure its parent tool reaches it in `Core/Subsystem/...HandlerRegistration.cpp`.
	3. Add an integration case under `tests/mcp-tools/` covering every optional param (`npm run test:params` is strict).
- Keep TypeScript strict. Avoid `as any` in runtime code.
- Do not hand-bump versions: the `bump-version.yml` workflow rewrites all four version files (`package.json`, `package-lock.json`, `server.json`, the `.uplugin`).

## Reference points
- Use `tests/AGENTS.md` for integration test conventions, expected result strings, and timeout tiers.
- Use `plugins/EngineWire/AGENTS.md` for plugin-side patterns and Unreal-specific caveats.
