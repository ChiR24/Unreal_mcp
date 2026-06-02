# src/tools/handlers

Domain-specific TypeScript action handlers. There are 43 runtime TS files here, including 41 `*-handlers.ts` domain files plus shared helpers. They do argument cleanup and then dispatch to Unreal; they should not contain editor-side business logic that belongs in C++.

## STRUCTURE
```
handlers/
|-- common-handlers.ts            # executeAutomationRequest(), requireAction(), parsing/errors
|-- argument-helper.ts            # shared argument extraction helpers
|-- actor/asset/blueprint/editor/level style handlers
|-- animation/audio/effect/geometry/material/widget authoring handlers
|-- ai/gas/character/combat/inventory/interaction handlers
|-- networking/sessions/game-framework/input handlers
|-- system/performance/pipeline/inspect handlers
`-- *.test.ts                    # focused Vitest coverage for subtle handler behavior
```

## WHERE TO LOOK
| Task | File | Notes |
|------|------|-------|
| Add action branch | matching `*-handlers.ts` | Match the parent tool domain and existing switch style |
| Dispatch to Unreal | `common-handlers.ts` | Use `executeAutomationRequest(action, params)` |
| Require action | `common-handlers.ts` | Use `requireAction(args)` for action-based tools |
| Parse/format errors | `common-handlers.ts` | Preserve tool/action context in returned errors |
| Normalize common inputs | `argument-helper.ts`, `src/utils/normalize.ts` | Reuse existing coercion helpers |

## CONVENTIONS
- Switch on `args.action` after validating that arguments are records.
- Keep TS action names exactly aligned with C++ action registration and native MCP schema strings.
- Normalize paths, vectors, rotations, and transforms before bridge dispatch.
- Console commands must pass through `CommandValidator`; do not create alternate command execution paths.
- Return MCP tool responses through the shared response/parser helpers so output validation remains meaningful.

### Handler Pattern
```typescript
export async function handleFoo(args: unknown): Promise<ToolResponse> {
  const action = requireAction(args);
  switch (action) {
    case 'bar': {
      const params = { /* validated and normalized fields */ };
      return executeAutomationRequest('foo_bar', params);
    }
  }
}
```

## ANTI-PATTERNS
- Raw `AutomationBridge` or WebSocket calls from handler modules.
- Broad catch blocks that erase the action/tool name from errors.
- Accepting path-like strings without `sanitizePath()` or a domain-specific normalizer.
- Adding permissive fallbacks for unreleased draft action shapes.
- Stubbed or "not implemented" branches in public action enums.
