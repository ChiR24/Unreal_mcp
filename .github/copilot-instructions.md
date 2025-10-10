# Unreal MCP – AI Agent Guide

## Quick Start — Actionable Checklist for AI agents
- Read the two-process architecture first: the Node MCP server (src/) and the Unreal Editor plugin (plugins/McpAutomationBridge/Source/). Key files:
  - Node server: `src/index.ts`, `src/automation-bridge.ts`, `src/unreal-bridge.ts`, `src/tools/**` (handlers & definitions)
  - UE plugin: `plugins/McpAutomationBridge/Source/McpAutomationBridge/Public/McpAutomationBridgeSettings.h`,
    `Private/McpAutomationBridgeSubsystem.cpp`, `Private/McpBridgeWebSocket.{h,cpp}`
- Running locally (order matters):
  1. Build and start the Editor plugin (Unreal native build or open Editor with built plugin).
  2. Start the MCP server/tests: `npm run test:blueprint` or `node tests/run-unreal-tool-tests.mjs`.
  3. If Editor not available, run tests in mock mode: set `UNREAL_MCP_MOCK_MODE=1`.
- Important env vars and defaults agents must know: `MCP_AUTOMATION_WS_PORTS` (defaults: `8090,8091`), `MCP_AUTOMATION_WS_HOST` (default `127.0.0.1`), `UNREAL_MCP_WAIT_PORT_MS` (test port-wait timeout), `MCP_AUTOMATION_CAPABILITY_TOKEN`.
- Quick debugging commands (PowerShell):
  - Check listening ports: `netstat -ano | findstr :8090` (replace port)
  - Clean plugin build artifacts: remove `Plugins/McpAutomationBridge/Binaries` and `Plugins/McpAutomationBridge/Intermediate` before a rebuild.
- When editing C++:
  - Always update ctor signatures in both header and .cpp and keep initializer order matching declaration order.
  - If you change a delegate signature, update all `AddUObject`/`AddLambda` callsites (use lambdas capturing TWeakPtr to avoid signature mismatches and cycles).
  - UENUMs must be declared at global scope (UHT will fail otherwise).


## Architecture Overview
- **MCP Server** (`src/index.ts`): Boots with consolidated tool routing (`src/tools/consolidated-tool-definitions.ts`, `src/tools/consolidated-tool-handlers.ts`) to 14 multi-action tools wrapping specialized classes in `src/tools/*`.
- **Unreal Bridge** (`src/unreal-bridge.ts`): Manages automation bridge connection, command queuing, and safety logic with on-demand connection via `ensureConnectedOnDemand()`.
- **Automation Bridge** (`src/automation-bridge.ts`): WebSocket transport layer for the MCP Automation Bridge plugin with handshake, heartbeat, and request/response handling.
- **Tool Classes**: 14 specialized classes (ActorTools, AssetTools, EditorTools, etc.) in `src/tools/*` handle domain-specific operations.
- **Resources** (`src/resources/`): Cached listings for assets, actors, and levels.
- **Utils** (`src/utils/`): Validation, Python helpers, error handling, and response validation.

## Key Patterns

### Tool Wiring & Dispatch
- **Consolidated Tools**: 14 multi-action tools defined in `consolidated-tool-definitions.ts` with action-based dispatch in `consolidated-tool-handlers.ts`.
- **Handler Pattern**: Each tool delegates to specialized classes; use `requireAction(args)` to extract action, then switch on action type.
- **Example**: `manage_asset` tool handles 'list', 'import', 'create_material', 'duplicate', 'rename', 'move', 'delete' actions.

### Python Execution
- **Primary Transport**: Use `bridge.executePythonWithResult()` for scripts that print `RESULT:` JSON; sanitize strings with `escapePythonString()` from `src/utils/python-helpers.ts`.
- **Script Format**: Python scripts must output `RESULT:` followed by JSON; use `interpretStandardResult()` for parsing.
- **Templates**: Pre-built Python templates in `UnrealBridge.PYTHON_TEMPLATES` for common operations like actor enumeration.

### Response Format & Validation
- **Standard Response**: Return plain JS `{ success, message, error, warnings }`; validate with AJV schemas registered in `responseValidator`.
- **Wrapper Pattern**: Always wrap responses via `responseValidator.wrapResponse(toolName, result)` for consistent error handling.
- **Validation**: Reuse `ensureVector3`, `ensureRotation` from `src/utils/validation.ts`; elicit missing args with `elicitMissingPrimitiveArgs()`.

### Connection & Safety
- **On-Demand Connection**: Call `ensureConnectedOnDemand()` before operations; retries 3 times with 5-second timeouts.
- **Command Throttling**: Queue commands with `MIN_COMMAND_DELAY` (100ms) to prevent console spam; special handling for stat commands.
- **Error Handling**: Use `ErrorHandler.createErrorResponse()` for consistent error formatting; track in `metrics.recentErrors`.

## Workflows

### Build & Development
- **Build**: `npm run build` compiles TypeScript to `dist/` via `tsc -p tsconfig.json`.
- **Watch Mode**: `npm run build:watch` for continuous compilation during development.
- **Development Server**: `npm run dev` runs via `ts-node-esm src/cli.ts` for quick iteration.
- **Linting**: `npm run lint` and `npm run lint:fix` for code quality.

### Testing
- **Individual Tests**: `npm run test:manage_asset`, `npm run test:control_actor`, etc. run specific tool tests.
- **Integration Tests**: `node tests/run-unreal-tool-tests.mjs` runs comprehensive test suite with Markdown test cases.
- **Test Environment**: Set `UNREAL_MCP_SERVER_CMD`, `UNREAL_MCP_SERVER_ARGS`, `UNREAL_MCP_FBX_DIR` for test configuration.
- **Reports**: Test results saved to `tests/reports/` with timestamps.

### Debugging
- **Health Checks**: Query `ue://health` resource for connection status, performance metrics, and recent errors.
- **Automation Bridge**: Check `ue://automation-bridge` resource for handshake status, pending requests, and WebSocket diagnostics.
- **Logging**: All logs route to stderr; use `Logger` class with debug/info/warn/error levels.
- **Connection Issues**: Verify automation bridge plugin enabled, ports open, project path set.

## Integration Points

### Unreal Engine Plugins (Critical)
- **MCP Automation Bridge** (`Public/McpAutomationBridge`): Primary WebSocket transport; sync with `npm run automation:sync`.
- **Python Editor Script Plugin**: Required for all Python execution paths.
- **Editor Scripting Utilities**: Enables Editor Actor/Asset subsystems for UE 5.6.
- **Remote Control API**: Required for Remote Control preset tooling.
- **Level Sequence Editor**: Required for `manage_sequence` operations.

### Environment Configuration
- **UE_HOST**: Unreal Engine host (default: 127.0.0.1)
- **UE_PROJECT_PATH**: Absolute path to .uproject file
- **MCP_AUTOMATION_WS_PORT**: WebSocket port for automation bridge (default: 8090)
- **LOG_LEVEL**: Runtime log level (debug/info/warn/error)

### Transport Evolution
- **Current**: Automation bridge WebSocket transport with capability tokens and heartbeat.
- **Legacy**: Remote Control HTTP/WebSocket no longer used; migrated to automation bridge.
- **Safety**: Command blocking for dangerous console operations; plugin dependency validation.

## Project-Specific Conventions

### Vector/Rotation Handling
- **Input Flexibility**: Accept both object `{x,y,z}` and array `[x,y,z]` formats for vectors/rotators.
- **Normalization**: Use `toVec3Tuple()`, `toRotTuple()` from `src/utils/normalize.ts` for consistent processing.
- **Validation**: `ensureVector3()`, `ensureRotation()` from `src/utils/validation.ts` for type safety.

### Asset Path Mapping
- **Content Root**: `/Content` automatically maps to `/Game` in asset paths.
- **Path Validation**: Use `sanitizeAssetName()` for asset names; enforce `MAX_PATH_LENGTH` (260 chars).
- **Caching**: 10-second TTL for asset listings to improve performance.

### Error Response Format
- **UE_NOT_CONNECTED**: Standard error when Unreal connection fails after 3 attempts.
- **Structured Errors**: Include `scope`, `retriable`, and `_debug` fields for diagnostics.
- **Clean Objects**: Always call `cleanObject()` before returning to remove circular references.

### Plugin Dependencies
- **Validation**: Tools like `setupRagdoll` accept `pluginDependencies` array to verify required plugins.
- **Caching**: Plugin status cached with `PLUGIN_CACHE_TTL` (5 minutes) to reduce queries.

## Common Patterns

### Python Script Execution
```typescript
const script = `
import unreal, json
# ... Python logic ...
print('RESULT:' + json.dumps(result))
`.trim();
return this.bridge.executePythonWithResult(script);
```

### Tool Handler Structure
```typescript
async function handleManageAsset(name: string, args: any, tools: any) {
  const action = requireAction(args);
  switch (action) {
    case 'list': return tools.assetTools.list(args.directory, args.recursive);
    case 'import': return tools.assetTools.import(args.sourcePath, args.destinationPath);
    // ... other actions
  }
}
```

### Response Validation
```typescript
const result = { success: true, data: assetList };
return responseValidator.wrapResponse('manage_asset', result);
```

## Debugging Tips

- **Connection Errors**: Check `ue://health` resource; verify automation bridge plugin and required ports.
- **Python Failures**: Ensure Python Editor Script Plugin enabled; check for `RESULT:` prefix in output.
- **Test Failures**: Set environment variables; confirm Unreal running with correct project.
- **Performance**: Monitor `ue://health` for response times; check command queue in automation bridge diagnostics.
