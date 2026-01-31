# SCRIPTS KNOWLEDGE BASE

**Generated:** 2026-01-30
**Scope:** Automation, Sync, Docs, & Safety

## OVERVIEW
Collection of TypeScript and Node.js automation scripts managing the development lifecycle of the Unreal MCP server, including C++ bridge synchronization, safety linting, and documentation generation.

## STRUCTURE
### Synchronization
- `verify-handler-sync.ts`: Validates TS tool definitions against C++ action enums using AST parsing.
- `sync-mcp-plugin.js`: Deploys the `McpAutomationBridge` plugin to Engine or Project directories.
- `generate-sync-report.mjs`: Summarizes the synchronization state between MCP and Unreal.

### Safety & Quality
- `lint-cpp-safety.ts`: Scans C++ source for unsafe UE 5.7 patterns (e.g., `UPackage::SavePackage`, `GWorld`).
- `smoke-test.ts`: JSON-RPC connectivity test for the MCP server in mock mode.

### Scaffolding
- `scaffold-tool.ts`: Generates TS handler and C++ bridge boilerplate for new MCP tools.
- `scaffold-test.ts`: Creates Vitest unit tests for tool handlers.

### Documentation & Metrics
- `generate-action-docs.ts`: Generates Markdown documentation for MCP actions.
- `generate-cpp-action-docs.mjs`: Extracts documentation from C++ bridge handlers.
- `measure-tool-tokens.mjs`: Calculates token counts for tool definitions to optimize LLM context usage.

## WHERE TO LOOK
| Task | Script | Note |
|------|--------|------|
| Add New Tool | `scaffold-tool.ts` | Updates registry + C++ |
| Fix UE Crash | `lint-cpp-safety.ts` | Checks for known 5.7 pitfalls |
| Sync Plugin | `sync-mcp-plugin.js` | Target Engine or Project |
| Verify Actions | `verify-handler-sync.ts` | Uses TS AST Parser |
| Token Budget | `measure-tool-tokens.mjs` | Updates `token-baseline.json` |

## CONVENTIONS
- **TS Execution**: Run via `node --loader ts-node/esm` for ESM compatibility.
- **AST Parsing**: `verify-handler-sync.ts` is the source of truth for action parity.
- **Safety First**: No C++ PR should be merged without passing `lint-cpp-safety.ts`.
- **Mock Mode**: Smoke tests must support `MOCK_UNREAL_CONNECTION=true`.

## COMMANDS
```bash
npm run automation:sync            # Sync plugin to UE
npm run automation:verify-handler-sync # Check TS/C++ parity
npm run automation:lint-cpp-safety # Run C++ safety scanner
npm run automation:scaffold-tool   # Generate tool boilerplate
npm run docs:all                   # Run all doc generators
npm run test:smoke                 # Run MCP smoke tests
```

## ANTI-PATTERNS
- **Manual Sync**: Never manually update C++ action enums without verifying with `verify-handler-sync.ts`.
- **Raw Console**: Avoid adding `console.log` to scripts intended for CI report generation.
- **Bypassing Lint**: Ignoring `lint-cpp-safety.ts` warnings for UE 5.7+ specific crashes.
