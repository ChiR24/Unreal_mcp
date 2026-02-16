# src/tools/handlers

Domain-specific tool handler implementations (42 files).

## OVERVIEW
Handler functions for each MCP tool domain. Call `executeAutomationRequest()` to dispatch to C++ bridge.

## STRUCTURE
```
handlers/
├── actor-handlers.ts        # control_actor actions
├── asset-handlers.ts        # manage_asset actions
├── blueprint-handlers.ts    # manage_blueprint actions
├── level-handlers.ts        # manage_level actions
├── editor-handlers.ts       # control_editor actions
├── lighting-handlers.ts     # manage_lighting actions
├── animation-handlers.ts    # animation_physics actions
├── effect-handlers.ts       # manage_effect actions
├── sequence-handlers.ts     # manage_sequence actions
├── geometry-handlers.ts     # manage_geometry actions
├── spline-handlers.ts       # manage_splines actions
├── navigation-handlers.ts   # manage_navigation actions
├── audio-handlers.ts        # manage_audio actions
├── gas-handlers.ts          # manage_gas actions
├── combat-handlers.ts       # manage_combat actions
├── ai-handlers.ts           # manage_ai actions
├── character-handlers.ts    # manage_character actions
├── inventory-handlers.ts    # manage_inventory actions
├── interaction-handlers.ts  # manage_interaction actions
├── network-handlers.ts      # manage_networking actions
├── session-handlers.ts      # manage_sessions actions
├── gameplay-handlers.ts     # manage_game_framework actions
├── behavior-tree-handlers.ts # manage_behavior_tree actions
├── input-handlers.ts        # manage_input actions
├── skeleton-handlers.ts     # manage_skeleton actions
├── material-handlers.ts     # manage_material_authoring actions
├── texture-handlers.ts      # manage_texture actions
├── widget-handlers.ts       # manage_widget_authoring actions
├── volume-handlers.ts       # manage_volumes actions
├── level-structure-handlers.ts # manage_level_structure actions
├── environment-handlers.ts  # build_environment actions
├── inspect-handlers.ts      # inspect actions
├── system-handlers.ts       # system_control actions
├── performance-handlers.ts  # manage_performance actions
├── common-handlers.ts       # executeAutomationRequest(), requireAction()
└── ... (42 total files)
```

## WHERE TO LOOK
| Task | File | Notes |
|------|------|-------|
| Add action handler | `*-handlers.ts` | Match domain to file name |
| Common utilities | `common-handlers.ts` | `executeAutomationRequest()`, `requireAction()` |
| Error formatting | `common-handlers.ts` | `formatToolError()` |
| Response parsing | `common-handlers.ts` | `parseAutomationResponse()` |

## CONVENTIONS
- **Action Dispatch**: Switch on `args.action` in handler function
- **Bridge Call**: Always use `executeAutomationRequest(action, params)` — never raw WebSocket
- **Error Context**: Include tool/action names in all error messages
- **Path Normalization**: Call `normalizePath()` before sending to bridge

### Handler Pattern
```typescript
export async function handleFoo(args: unknown): Promise<ToolResponse> {
  const action = requireAction(args);
  switch (action) {
    case 'bar': {
      // Validate params, normalize paths
      return executeAutomationRequest('foo_bar', params);
    }
    // ... more actions
  }
}
```

## ANTI-PATTERNS
- **Direct WebSocket**: Never use `bridge.send()` — use `executeAutomationRequest()`
- **Bypassing Registry**: Never call handler functions directly — use `toolRegistry.register()`
- **Raw Params**: Always validate/normalize before bridge call
- **Missing Error Context**: All errors must include tool/action name
- **Stubbed Actions**: No "Not Implemented" — full TS + C++ coverage required

## NOTES
- **Non-Standard Location**: Handlers nested 2 levels deep (`src/tools/handlers/`)
- **C++ Requirement**: Every TS action must have corresponding C++ handler in plugin
- **Registry Pattern**: All handlers registered in `consolidated-tool-handlers.ts`
