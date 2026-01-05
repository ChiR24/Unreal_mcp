# src/tools

MCP tool implementations: 36 consolidated tools with action-based dispatch.

## OVERVIEW
Consolidated tool architecture using action-based dispatch to native C++ handlers.

## WHERE TO LOOK
| Task | File | Notes |
|------|------|-------|
| Add new tool | `consolidated-tool-definitions.ts` | Add JSON schema with action enum |
| Add TS handler | `consolidated-tool-handlers.ts` | Register in `registerDefaultHandlers()` |
| Implement logic | `handlers/*.ts` | Implement action and call `executeAutomationRequest()` |
| Common utils | `handlers/common-handlers.ts` | `requireAction()`, `executeAutomationRequest()` |

## CONVENTIONS
- **Consolidated Pattern**: Tools are grouped by domain (e.g., `manage_asset`); switch on `args.action`.
- **Registry Dispatch**: Always use `toolRegistry.register()` in `consolidated-tool-handlers.ts`.
- **C++ Requirement**: Every TS action **must** have a corresponding C++ handler in the plugin.
- **Error Context**: Add tool/action names to all error messages.

## ANTI-PATTERNS
- **Bypassing Registry**: Do not call domain handler functions directly.
- **Manual WS Calls**: Use `executeAutomationRequest()` instead of raw WebSocket calls.
- **Stubbed Actions**: No placeholders allowed; 100% TS + C++ coverage required.
- **Normalization**: Ensure paths are sanitized before sending to bridge.
