# PROJECT KNOWLEDGE BASE

**Generated:** 2025-12-28 13:11:47 UTC
**Commit:** 800672e
**Branch:** features

## OVERVIEW

Model Context Protocol (MCP) server for Unreal Engine 5. Dual-process architecture: TS control layer + UE C++ plugin + optional Rust/WASM acceleration.

## STRUCTURE

```
./
├── src/           # MCP server (TS)
│   ├── tools/     # 20+ tool implementations
│   ├── automation/ # WebSocket bridge client
│   ├── graphql/   # Optional GraphQL server
│   └── utils/     # Utilities (validation, normalization)
├── plugins/       # UE plugin (C++)
│   └── McpAutomationBridge/
│       ├── Source/ # Handlers, subsystem
│       └── Config/ # UE plugin config
├── wasm/          # Rust/WASM performance layer
├── tests/         # Integration tests (.mjs)
└── docs/          # Documentation
```

## WHERE TO LOOK

| Task | Location | Notes |
|------|----------|-------|
| Add MCP tool | `src/tools/` → `src/tools/handlers/` → `consolidated-tool-definitions.ts` | Must also add C++ handler |
| Add UE handler | `plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/*Handlers.cpp` | Route via subsystem |
| Bridge connection | `src/automation/` | WebSocket client, handshake |
| Path normalization | `src/utils/normalize.ts` | /Content → /Game conversion |
| Command validation | `src/utils/command-validator.ts` | Blocks dangerous console commands |
| Type definitions | `src/types/` | TS types, Zod schemas |

## CONVENTIONS

### Dual-Process Architecture
- **MCP Server (TS)**: Handles JSON-RPC protocol, tool schemas
- **Automation Bridge (UE C++)**: WebSocket server on port 8091, executes in game thread
- **Communication**: JSON payloads over WebSocket (TS → UE)

### TypeScript (NodeNext ESM)
- Local imports **must** use `.js` extensions: `import { log } from './logger.js'`
- Order: 1) Node built-ins (`node:*`) 2) Third-party 3) Local
- Prefer `unknown` over `any`, narrow types
- Empty `catch {}` allowed only if intentional

### Error Handling
- Normalize all errors: `const err = error instanceof Error ? error : new Error(String(error));`
- Add context (tool/action/port/path)
- Use repo logger (`src/utils/logger.ts`) - **never** `console.log` in runtime
- `console.log` redirected to `stderr` in `src/index.ts` (stdout reserved for JSON-RPC)

### Unreal Plugin (C++)
- Prefixes: `U*` (UObjects), `A*` (Actors), `F*` (structs), `I*` (interfaces)
- Use `FJsonObjectConverter` for JSON ↔ UStruct
- Keep request/response payloads stable

### Testing
- Unit tests: Vitest (`npm run test:unit`) - no UE required
- Integration tests: Custom .mjs runner (`npm run test:<name>`) - UE Editor required
- Custom runner handles connection waits, flexible matching (`success|not_found`)

## ANTI-PATTERNS (THIS PROJECT)

- **Breaking stdout**: Never write to stdout in runtime (reserved for JSON-RPC)
- **Empty catches without reason**: Document why errors are swallowed
- **`as any` overuse**: 193+ instances exist - reduce in new code, prefer explicit types
- **Non-null assertions**: Avoid `!` unless proven safe
- **Skipping C++ handler**: Adding tool without UE side won't work
- **Path format mismatch**: Always normalize `/Content` to `/Game`
- **Direct console commands**: Use `UnrealCommandQueue` with throttling
- **WebSocket size checks**: Must check in **BYTES**, not characters

## UNIQUE STYLES

- **Consolidated tools**: `consolidated-tool-definitions.ts` + `consolidated-tool-handlers.ts` centralize all MCP tools
- **WASM fallback**: Rust compiles to WASM, TS fallback if missing (`WASM_ENABLED=true|false`)
- **Graceful connection**: Server starts without UE, retries with exponential backoff
- **Elicitation layer**: ToolRegistry can pause to ask LLM for missing parameters
- **Docker multi-stage**: builder (node:22-alpine) → production (chainguard/node)
- **Optional GraphQL**: Disabled by default, enable via `GRAPHQL_ENABLED=true`

## COMMANDS

```bash
# Build
npm run build:core      # TS only (fast)
npm run build            # TS + WASM (optional)
npm run build:wasm        # WASM only (requires wasm-pack)

# Run
npm run dev              # ts-node ESM
npm run start            # compiled JS

# Lint
npm run lint             # ESLint TS
npm run lint:cpp         # cpplint C++
npm run lint:csharp       # dotnet-format C#

# Test
npm run test:unit        # Vitest, no UE
npm test                # All integration (requires UE)
npm run test:blueprint  # Single tool test
```

## NOTES

- **Unreal Engine**: 5.0–5.7 supported
- **Node.js**: 18+ required
- **Plugin copy**: Copy `plugins/McpAutomationBridge/` to your UE project
- **Port**: WebSocket server on 8091 (configurable via `MCP_AUTOMATION_PORT`)
- **Mock mode**: `MOCK_UNREAL_CONNECTION=true` for offline testing (TS side)
- **Engine code**: Before editing plugin C++, read engine source at `X:\Unreal_Engine\UE_5.6\Engine` or `X:\Unreal_Engine\UE_5.7\Engine`
- **Handler mapping**: Use `docs/handler-mapping.md` to find TS → C++ routes
- **Integration tests**: Most fail with `ECONNREFUSED` if UE/plugin not running
