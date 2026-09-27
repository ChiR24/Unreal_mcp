# src/server

MCP SDK construction, stdio lifecycle, and tool/resource registration. Tool contracts live under `src/tools/`; action behaviour lives in the plugin.

## STRUCTURE
```
server/
|-- server-factory.ts              # SDK server, bridges, notifications/cancelled, stdout guard
|-- stdio-lifecycle.ts             # transport startup and idempotent shutdown
|-- tool-registry.ts               # tools/list (the one `unreal` tool) + tools/call
|-- tool-registry-gateway.ts       # gateway ENTRY: handleUnrealGatewayCall() operation switch
|-- tool-registry-manage-tools.ts  # manage_tools, the only parent that runs in process
|-- gateway/                       # gateway ROUTING ENGINE; own AGENTS.md
`-- mcp-primitives/                # resources/prompts/completions/logging wiring; own AGENTS.md
```
`gateway/` is request routing; `src/gateway/` is the generated manifest data + loader.

## WHERE TO LOOK
| Task | File |
|------|------|
| Construct the server | `server-factory.ts` |
| Change stdio cleanup | `stdio-lifecycle.ts` |
| `tools/list` / `tools/call` | `tool-registry.ts` |
| Gateway operations | `tool-registry-gateway.ts` → `gateway/` |
| What a model reads first | `../tools/catalog/unreal-gateway-definition.ts` (`UNREAL_GATEWAY_DESCRIPTION`, `UNREAL_GATEWAY_INSTRUCTIONS`); native copies in `McpNativeGatewayDefinition.cpp` and `Resources/MCP/server-info.json`, kept equal by `tests/unit/tools/unreal-gateway-guidance-contract.test.ts` |
| `manage_tools` | `tool-registry-manage-tools.ts` (local state; protected tools stay enabled) |
| Resources | `src/handlers/resource-handlers.ts` (`new ResourceHandler(...).registerHandlers()`) |

## REQUEST FLOW
1. `tools/list` always returns exactly `[unrealGatewayToolDefinition]`; no list-changed notifications.
2. `tools/call` on any other name returns a `DIRECT_TOOL_CALL_REMOVED` receipt with an executable `nextCall`.
3. `tools/call unreal` → `handleUnrealGatewayCall(args, { tools, logger, ensureConnected })` → `cleanObject()` → `wrapGatewayResponse()` (validates against the `unreal` output schema, sets `isError`).
4. Cancellation: SDK AbortSignal and `notifications/cancelled` both converge on `AutomationBridge.cancelMcpRequest`, which rejects queued work, sends a best-effort `cancel_request` frame for in-flight work and ends that request's progress stream. Advisory only: editor work already dispatched still runs.

## CONVENTIONS
- Stdout is JSON-RPC-owned; `routeStdoutLogsToStderr()` runs unconditionally at startup.
- Preserve `isError`, health timing and image-payload redaction on every response path.
- Progress is keyed by the client's own `_meta.progressToken`; never allocate one.

## ANTI-PATTERNS
- Dispatching `manage_tools` to Unreal or bypassing `dynamicToolManager`.
- Adding a second public tool or a mode flag.
- Writing runtime text to stdout.
