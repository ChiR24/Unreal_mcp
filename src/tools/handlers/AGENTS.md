# src/tools/handlers

Domain-specific handler implementations for 37 consolidated MCP tools.

## OVERVIEW
89 handler files organized by domain. Each implements actions for one or more consolidated tools, calling `executeAutomationRequest()` to dispatch to C++.

## STRUCTURE
```
handlers/
├── common-handlers.ts          # Shared utilities (requireAction, executeAutomationRequest)
├── asset-plugins-handlers.ts   # USD, Alembic, glTF, Datasmith, Houdini, Substance
├── audio-*-handlers.ts         # Audio authoring + middleware (Wwise, FMOD)
├── blueprint-handlers.ts       # Blueprint creation + graph manipulation
├── character-*-handlers.ts     # Character + avatar (MetaHuman, Groom, RPM)
├── combat-handlers.ts          # Weapons, projectiles, damage
├── editor-*-handlers.ts        # Editor utilities + functions
├── environment-handlers.ts     # Landscape, foliage, water, weather
├── gas-handlers.ts             # Gameplay Ability System
├── geometry-handlers.ts        # Procedural mesh (Geometry Script)
├── graph-handlers.ts           # Blueprint/Material/Niagara graph nodes
└── ...                         # 50+ more domain handlers
```

## CONVENTIONS
- **Single Responsibility**: Each file handles one domain
- **Common Import**: Always import from `common-handlers.ts`
- **Action Switch**: Use `switch (args.action)` pattern
- **Error Wrapping**: Catch and add context before re-throwing
- **Type Safety**: Use `unknown` + type guards, never `as any`

## PATTERN
```typescript
import { requireAction, executeAutomationRequest } from './common-handlers.js';

export async function handleMyDomain(args: unknown) {
  const action = requireAction(args);
  switch (action) {
    case 'my_action':
      return executeAutomationRequest('my_tool', action, args);
    default:
      throw new Error(`[my_tool:${action}] Unknown action`);
  }
}
```

## ANTI-PATTERNS
- **Direct Bridge Calls**: Never use `bridge.send()` directly
- **Missing Error Context**: Always include tool + action in errors
- **Untyped Args**: Validate before accessing properties
