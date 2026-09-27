# src/tools/handlers

The validated bridge boundary. There are no per-domain TypeScript handlers: the gateway forwards `{ action, ...params }` to the record's parent tool, and the plugin owns every action's behaviour.

## STRUCTURE
```
handlers/foundation/
|-- arguments/handler-argument-validation.ts   # validateArgsSecurity(): path traversal + absolute-root checks
`-- dispatch/
    |-- automation-request-dispatch.ts         # executeAutomationRequest()
    `-- handler-timeout.ts                     # cost-tier timeout per tool/action
```

## DISPATCH CONTRACT
```typescript
executeAutomationRequest(tools, toolName, args, controls?: GatewayControls)
```
- Caller: `src/server/gateway/gateway-execute-dispatch.ts` (`runCapability`). `tools.automationBridge` is the only bridge; never construct one.
- Order: `validateArgsSecurity(args)` → `CommandValidator` for `console_command`/`execute_command`, and for the command the plugin composes from `set_cvar` / `set_resolution` / `set_fullscreen` strings → connection check → `sendAutomationRequest()`.
- Timeout: `controls.timeoutMs` (the client's own deadline) wins, else `resolveActionTimeoutMs(toolName, action)`: an `MCP_REQUEST_TIMEOUT_MS` pin, else the record's cost class. `start_render` never drops below `MRQ_START_RENDER_TRANSPORT_MS` (render deadline + cancel wait), or its natural-timeout cancel would stop a healthy render.
- `correlationId`, `consent` and `expectedRevisions` ride as automation_request envelope siblings, never handler params; `mcpRequestId` comes from the async-local request context. The plugin re-validates all of them.

## ANTI-PATTERNS
- Raw `AutomationBridge`/WebSocket calls from anywhere else.
- Re-implementing path security, console policy or timeout policy at a call site.
- Moving editor-side behaviour into TypeScript.
