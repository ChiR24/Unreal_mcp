# src/tools

MCP tool implementations: 35+ consolidated tools with action-based dispatch.

## OVERVIEW
Consolidated tool architecture using action-based dispatch to native C++ handlers. Single 212KB schema file defines all tools.

## STRUCTURE
```
tools/
├── consolidated-tool-definitions.ts  # All action enums + schemas (212KB)
├── consolidated-tool-handlers.ts     # Tool routing via toolRegistry
└── handlers/                         # Domain handlers (42 files)
```

## WHERE TO LOOK
| Task | File | Notes |
|------|------|-------|
| Add new tool | `consolidated-tool-definitions.ts` | Add JSON schema with action enum |
| Add TS handler | `consolidated-tool-handlers.ts` | Register in `registerDefaultHandlers()` |
| Implement logic | `handlers/*-handlers.ts` | 42 domain handler files |
| Common utils | `handlers/common-handlers.ts` | `requireAction()`, `executeAutomationRequest()` |

## CONVENTIONS
- **Consolidated Pattern**: Tools grouped by domain; switch on `args.action`.
- **Registry Dispatch**: Use `toolRegistry.register()` in `consolidated-tool-handlers.ts`.
- **C++ Requirement**: Every TS action must have corresponding C++ handler in plugin.
- **Error Context**: Add tool/action names to all error messages.

## ANTI-PATTERNS
- **Bypassing Registry**: Never call domain handler functions directly.
- **Manual WS Calls**: Use `executeAutomationRequest()` instead of raw WebSocket.
- **Stubbed Actions**: No placeholders; 100% TS + C++ coverage required.
- **Normalization**: Ensure paths are sanitized before sending to bridge.

## NOTES
- **handlers/ subdirectory**: See `handlers/AGENTS.md` for detailed handler documentation.
- **Non-standard layout**: Handlers nested 2 levels deep (`src/tools/handlers/`).
