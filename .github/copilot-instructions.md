````instructions
# Unreal MCP — AI Agent Quick Guide (for AI coding agents)

This repository runs as two cooperating processes: the Node.js MCP server
(`src/`) and a native Unreal Editor plugin
(`plugins/McpAutomationBridge/Source/`). All operations use native C++
handlers in the plugin for high performance and reliability.

Quick checklist
- Build and start the Editor plugin (or open the Editor with the built
  plugin). The plugin listens by default on ports 8090 and 8091.
- Start the MCP server or run a focused tool test, e.g.:
  - `npm run test:manage_asset`
  - `node tests/run-unreal-tool-tests.mjs`
- For offline development use mock mode: set `UNREAL_MCP_MOCK_MODE=1`.

High-value files to inspect first
- `src/unreal-bridge.ts` — Bridge coordination and command routing
- `src/automation-bridge.ts` — WebSocket transport and handshake handling
- `src/tools/*` — domain tool classes and consolidated tool dispatch
- `plugins/McpAutomationBridge/Source/*/Private/*` — native C++ handlers
  (see `McpAutomationBridge_BlueprintHandlers.cpp`,
  `McpAutomationBridge_EditorFunctionHandlers.cpp`, 
  `McpAutomationBridge_AssetHandlers.cpp`)

Native C++ workflow (required)
1. Implement automation requests using
   `automationBridge.sendAutomationRequest(actionName, params)`
2. Implement or update the server `src/tools/*` handler to call the bridge
3. Add corresponding C++ handler in the plugin if needed:
   - Add handler method in appropriate *Handlers.cpp file
   - Register action in HandleAutomationRequest dispatcher
4. Add tests under `tests/` and a Markdown test case if appropriate

Project conventions you must know
- All operations use native C++ automation bridge handlers
- Always wrap tool outputs with `responseValidator.wrapResponse(toolName, result)`
- Asset paths normalize `/Content` → `/Game` (see `src/utils/normalize.ts`)
- Vector/rotator inputs accept both `{x,y,z}` and `[x,y,z]` — use
  `toVec3Tuple()` / `toRotTuple()` helpers
- Command calls are throttled (MIN_COMMAND_DELAY ≈ 100ms); use
  `ensureConnectedOnDemand()` to obtain a connection before sending commands
- Automation bridge provides graceful error messages when operations fail

Useful commands (PowerShell)
- Check the plugin listening port: `netstat -ano | findstr :8090`.
- Clean plugin build artifacts before rebuild:
  - Delete `Plugins/McpAutomationBridge/Binaries`
  - Delete `Plugins/McpAutomationBridge/Intermediate`
- Lint: `npm run lint`
- Build: `npm run build` (TypeScript → `dist/`)
- Dev server: `npm run dev`
- Run a focused tool test: `npm run test:manage_asset`

Debugging and test notes
- The plugin exposes two health resources: `ue://health` and
  `ue://automation-bridge` for connection/diagnostics
- All tools use native C++ handlers - Editor Scripting Utilities plugin
  provides Editor Actor/Asset subsystems
- If the automation bridge cannot connect, verify Editor + plugin are running
  and listening on the configured ports

If you want a minimal PR to add a mapping + server handler + plugin stub,
specify the operation (e.g. `create_material_instance` or
`blueprint_add_component`) and I will generate a small, tested change set.

---

## Detailed Guide and preserved content

The long-form guidance and examples used by previous agents are preserved
below for deep-dive reference. Use the Quick Guide above for fast onboarding.

- Node server: `src/index.ts`, `src/unreal-bridge.ts`, `src/automation-bridge.ts`.
- Consolidated tools: `src/tools/consolidated-tool-definitions.ts`,
  `src/tools/consolidated-tool-handlers.ts`.
- Tool classes: `src/tools/*` (each implements domain-specific logic).
- Plugin native handlers: `plugins/McpAutomationBridge/Source/*/Private/*`.

Key patterns and snippets
- All operations use `automationBridge.sendAutomationRequest(actionName, params)`
- C++ handlers return JSON with `{ success, message?, error?, warnings?, data? }`

- Example automation request:
  ```typescript
  const resp = await automationBridge.sendAutomationRequest(
    'create_material',
    { name, destinationPath },
    { timeoutMs: 10000 }
  );
  ```

- Always return structured results: `{ success, message?, error?, warnings?, data? }` and
  wrap them via `responseValidator.wrapResponse()`.

- Tests: `tests/run-unreal-tool-tests.mjs` executes tool-level tests using
  Markdown-designed cases. Use `UNREAL_MCP_MOCK_MODE=1` for offline runs.
````
