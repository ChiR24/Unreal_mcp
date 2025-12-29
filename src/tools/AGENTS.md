# src/tools

MCP tool implementations: 16 consolidated tools with action-based dispatch.

## STRUCTURE

```
tools/
├── consolidated-tool-definitions.ts  # Tool schemas (JSON Schema)
├── consolidated-tool-handlers.ts     # Main dispatcher + handler registration
├── dynamic-handler-registry.ts       # Global handler registry singleton
├── handlers/                         # Action implementations
│   ├── common-handlers.ts            # Shared utilities (requireAction, executeAutomationRequest)
│   ├── *-handlers.ts                 # Domain-specific handlers (19 files)
│   └── argument-helper.ts            # Argument normalization
└── *.ts                              # Legacy domain files (mostly unused)
```

## WHERE TO LOOK

| Task | File | Notes |
|------|------|-------|
| Add new tool | `consolidated-tool-definitions.ts` | Add schema with actions enum |
| Add handler registration | `consolidated-tool-handlers.ts` | `toolRegistry.register()` in `registerDefaultHandlers()` |
| Add action logic | `handlers/*-handlers.ts` | Export function, import in consolidated-tool-handlers |
| Graph editing (BP/Material/Niagara/BT) | `handlers/graph-handlers.ts` | Unified graph operations |
| Common utilities | `handlers/common-handlers.ts` | `executeAutomationRequest()`, `requireAction()` |

## CONVENTIONS

### Consolidated Pattern
- **Definitions**: One object per tool with `name`, `description`, `inputSchema`, `outputSchema`
- **Handlers**: `toolRegistry.register(toolName, async (args, tools) => {...})`
- **Actions**: Each tool has `action` enum; handler switches on action
- **Dispatch**: `handleConsolidatedToolCall(name, args, tools)` → registry lookup → handler

### Action Routing
```typescript
// In consolidated-tool-handlers.ts:
toolRegistry.register('manage_asset', async (args, tools) => {
  const action = args.action || requireAction(args);
  if (isMaterialGraphAction(action)) return handleGraphTools(...);
  return handleAssetTools(action, args, tools);
});
```

### Cross-Domain Routing
Some actions reroute to different handlers:
- `manage_asset` + material graph actions → `handleGraphTools('manage_material_graph', ...)`
- `manage_blueprint` + graph actions → `handleGraphTools('manage_blueprint_graph', ...)`
- `manage_effect` + niagara graph actions → `handleGraphTools('manage_niagara_graph', ...)`
- `system_control` + `console_command` → `handleConsoleCommand()`

## ANTI-PATTERNS

- **Skipping C++ handler**: Every action needs corresponding C++ handler in plugin
- **Bypassing registry**: Never call handler functions directly; use `toolRegistry.getHandler()`
- **Missing action validation**: Always use `requireAction(args)` or check `args.action`
- **Forgetting normalization**: Tool names get normalized (`create_effect` → `manage_effect`)
- **Inline bridge calls**: Use `executeAutomationRequest()` from common-handlers

## ADDING A NEW TOOL

1. Add schema to `consolidated-tool-definitions.ts` (name, description, inputSchema with action enum)
2. Create handler file in `handlers/` (or add to existing domain handler)
3. Register in `consolidated-tool-handlers.ts` via `toolRegistry.register()`
4. Add C++ handler in `plugins/McpAutomationBridge/Source/.../Private/*Handlers.cpp`
5. Add integration test in `tests/test-<toolname>.mjs`
