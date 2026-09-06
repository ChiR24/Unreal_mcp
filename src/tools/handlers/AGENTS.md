# src/tools/handlers

Domain-specific TypeScript action handlers. They validate/coerce MCP arguments, translate parent actions into bridge payloads, and return structured results. Unreal editor behavior remains in the bridge plugin.

## STRUCTURE
```
handlers/
|-- foundation/
|   |-- arguments/                    # required fields, security checks, coercion
|   |-- dispatch/                     # bridge dispatch, sub-action helper, timeouts
|   |-- normalization/                # UE paths and transforms
|   `-- responses/                    # scalar/result promotion
|-- animation/{authoring,runtime}/
|-- audio/{authoring,runtime}/
|-- level/{structure,runtime}/
`-- <domain>/                         # routed domain handlers and colocated tests
```

## WHERE TO LOOK
| Task | File | Notes |
|------|------|-------|
| Add an action branch | matching `<domain>/*-handlers.ts` | Entry handlers normally receive `(action, args, tools)` |
| Implement bridge dispatch | `foundation/dispatch/automation-request-dispatch.ts` | Owns `executeAutomationRequest()` and sub-action dispatch |
| Import shared helpers | `foundation/dispatch/common-handlers.ts` | Convenience re-export; not the implementation |
| Validate arguments | `foundation/arguments/handler-argument-validation.ts` | Required strings/params and security patterns |
| Normalize paths/transforms | `foundation/normalization/` | Normalize known fields before security validation and dispatch |
| Tune timeouts | `foundation/dispatch/handler-timeout.ts` | Prefer explicit options only for exceptional actions |
| Promote response fields | `foundation/responses/` | Preserve schema-compatible response shapes |
| Route a parent action here | `../orchestration/consolidated-handler-registration.ts` | Routing belongs outside domain implementation |

## DISPATCH CONTRACT
```typescript
executeAutomationRequest(
  tools,
  toolName,
  args,
  errorMessage?,
  { timeoutMs?, forwardTimeoutMsToUnreal?, mcpRequestId? }
)
```
- `tools` supplies the current `automationBridge`; never import or construct a bridge in a handler.
- `toolName` is the Unreal bridge action/domain, which may differ from the public parent tool.
- Timeout precedence: `options.timeoutMs` → gateway async-local `getGatewayTimeoutMs()` → `args.timeoutMs` (handler-internal payloads only) → `resolveActionTimeoutMs(toolName, action)` capability cost tier.
- `forwardTimeoutMsToUnreal` is valid ONLY for `manage_sequence/start_render` (MRQ deadline escape hatch); any other tool/action throws, and otherwise `timeoutMs` is stripped before forwarding.
- Dispatch validates argument security and console-command payloads, checks connection state, then calls `sendAutomationRequest()`.
- `mcpRequestId` defaults to the async-local MCP request context; gateway correlation id, consent, and expected revisions ride as automation_request envelope siblings — never handler params.
- For repeated sub-actions, prefer `createSubActionDispatcher(tools, args, options)`.

## CONVENTIONS
- Use the existing handler signature and domain switch style; reject unknown actions with domain/action context.
- Validate required fields and normalize UE paths, vectors, rotations, and transforms before dispatch.
- Preserve `action`/`subAction` translations expected by the registered C++ handler.
- Keep TypeScript action names aligned with tool definitions, native MCP schemas, C++ registrations, and parameter-audit tests.
- Clean or promote bridge results through existing helpers; do not invent a second response envelope.
- Add focused colocated tests for validation, routing, payload shape, and exceptional timeout behavior.

## ANTI-PATTERNS
- Raw `AutomationBridge`, WebSocket, HTTP, or native MCP calls from domain modules.
- The stale two-argument form `executeAutomationRequest(action, params)`; `tools` is always first.
- Reimplementing security checks, command execution, path normalization, or timeout policy locally.
- Broad catches that erase action context, permissive draft-action fallbacks, or public stub branches.
- Moving editor-side asset/level mutation logic into TypeScript.
