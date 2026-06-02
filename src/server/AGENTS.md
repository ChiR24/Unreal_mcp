# src/server

MCP server registration layer for tools and resources. This directory owns how the TypeScript MCP server lists tools, handles `tools/call`, exposes resources, and applies dynamic tool filtering.

## STRUCTURE
```
server/
|-- tool-registry.ts      # ListTools/CallTool handlers, manage_tools intercept, output validation
`-- resource-registry.ts  # MCP resource registration
```

## WHERE TO LOOK
| Task | File | Notes |
|------|------|-------|
| Change `tools/list` output | `tool-registry.ts` | Uses `dynamicToolManager` state and category filters |
| Change `tools/call` routing | `tool-registry.ts` | Calls `handleConsolidatedToolCall()` for public tools |
| Change `manage_tools` TS behavior | `tool-registry.ts` | Handled locally before Unreal dispatch |
| Change response validation | `tool-registry.ts` | Runs through `responseValidator` and safe JSON cleanup |
| Change MCP resources | `resource-registry.ts` | Keep resource behavior separate from tool behavior |

## CONVENTIONS
- Register MCP SDK request handlers here; do not scatter `ListToolsRequestSchema` or `CallToolRequestSchema` handling elsewhere.
- `manage_tools` is a local TS control tool. Do not dispatch its enable/disable/list actions to Unreal.
- Dynamic categories are `core`, `world`, `gameplay`, `utility`, and `all`.
- Respect client support for `tools.listChanged`; notify only through supported paths.
- Keep output validation and `cleanObject()` in the response path so invalid values do not leak to clients.
- Tool calls should preserve the original tool/action context in error responses.

## ANTI-PATTERNS
- Registering tools directly against the SDK outside this registry layer.
- Returning unfiltered `consolidatedToolDefinitions` when dynamic tool filtering is active.
- Disabling protected tools (`manage_tools`, `inspect`) through local control paths.
- Adding resource logic to tool handlers or tool routing logic to resource registration.
