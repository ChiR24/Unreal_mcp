# src/tools

30 consolidated MCP tools with action-based dispatch to native C++ handlers.
Token-optimized: ~26k tokens for all 1,600+ actions visible to LLM.

## OVERVIEW
Consolidated tool architecture. Each tool groups related actions (e.g., `manage_asset` handles create, delete, duplicate). TS validates schema, dispatches to C++.

## STRUCTURE
```
src/tools/
├── consolidated-tool-definitions.ts  # All 30 tool schemas + action enums (7,385 lines)
├── consolidated-tool-handlers.ts     # Registry dispatch + routing (43KB)
├── handlers/                          # Domain-specific implementations (62 files)
├── tool-definition-utils.ts          # 200+ reusable common schema definitions
├── property-dictionary.ts            # UE property mappings
├── dynamic-handler-registry.ts       # Global toolRegistry instance
└── *.ts                               # Legacy single-tool files (being consolidated)
```

## WHERE TO LOOK
| Task | File | Notes |
|------|------|-------|
| Add tool schema | `consolidated-tool-definitions.ts` | Define inputSchema + action enum |
| Register handler | `consolidated-tool-handlers.ts` | `toolRegistry.register()` in `registerDefaultHandlers()` |
| Implement action | `handlers/*-handlers.ts` | Call `executeAutomationRequest()` |
| Common utilities | `handlers/common-handlers.ts` | `requireAction()`, `executeAutomationRequest()` |
| Reusable schemas | `tool-definition-utils.ts` | Transforms, paths, names, properties |

## CONVENTIONS
- **Consolidated Pattern**: Group by domain, switch on `args.action`
- **Registry First**: Always use `toolRegistry.register()`, never call handlers directly
- **C++ Parity**: Every TS action MUST have corresponding C++ handler
- **Error Context**: Include tool/action names in all error messages
- **Path Sanitization**: Call `sanitizePathSafe()` before bridge calls

## ANTI-PATTERNS
- **Bypassing Registry**: Never call domain handlers directly from outside
- **Manual WS Calls**: Use `executeAutomationRequest()` exclusively
- **Stubbed Actions**: No placeholders. 100% implementation required
- **Missing Validation**: Always validate required params before dispatch

## TOKEN OPTIMIZATION (Phase 54)
Schema pruning removes:
- Root `type: 'object'` (implied for tool inputs)
- Primitive types (`string`, `number`, `boolean`) - LLM infers from param names
- Array `items` details - simplified to `{ type: 'array' }`
- Vector/color objects (x,y,z / r,g,b) - simplified to `{ type: 'object' }`
- Descriptions on non-action properties

Deprecated tools route to merged targets with once-per-session warnings.
