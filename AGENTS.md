# PROJECT KNOWLEDGE BASE

**Generated:** 2026-02-16 11:20:00 IST
**Commit:** 1d0b04a
**Branch:** main

## OVERVIEW
MCP server for Unreal Engine 5 (5.0-5.7). Dual-process: TypeScript MCP server + C++ Bridge Plugin + optional Rust WASM. 35+ consolidated tools with action-based dispatch. Version 0.5.16.

## STRUCTURE
```
./
├── src/                    # TS Server (NodeNext ESM)
│   ├── tools/              # Tool definitions + handlers
│   │   ├── consolidated-tool-definitions.ts  # Action enums + schemas (212KB)
│   │   ├── consolidated-tool-handlers.ts     # Tool routing
│   │   └── handlers/       # Domain handlers (40 files)
│   ├── automation/         # Bridge Client & Handshake (9 files)
│   ├── utils/              # Normalization & Security
│   └── wasm/               # WASM compiled output (from ../wasm/)
├── Plugins/                # UE Plugin (C++) — note: "Plugins" not "plugins"
│   └── McpAutomationBridge/
│       ├── Source/         # Native Handlers (56 files) & Subsystem
│       └── Config/         # Plugin Settings
├── wasm/                   # Rust Source (Math/Parsing)
├── tests/                  # Integration + Unit Tests
│   ├── test-runner.mjs     # Custom MCP test runner (1100+ lines)
│   └── mcp-tools/          # Domain-specific test files (core/world/authoring/gameplay/utility)
└── scripts/                # Maintenance & CI Helpers
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Add MCP Tool | `src/tools/consolidated-tool-definitions.ts` | Add action enum + schema |
| Route Tool | `src/tools/consolidated-tool-handlers.ts` | Register in `registerDefaultHandlers()` |
| Implement Handler | `src/tools/handlers/*-handlers.ts` | Call `executeAutomationRequest()` |
| Add UE Action | `Plugins/.../Private/*Handlers.cpp` | Register in `Subsystem::InitializeHandlers()` |
| Fix UE Crashes | `McpAutomationBridgeHelpers.h` | Use `McpSafeAssetSave` for 5.7+ |
| Path Handling | `src/utils/normalize.ts` | Force `/Game/` prefix |
| CI Workflows | `.github/workflows/` | All actions use commit SHAs (secure) |
| Version Sync | `.github/workflows/bump-version.yml` | Updates 4 files atomically |

## CONVENTIONS
### Dual-Process Flow
1. **TS (MCP)**: Validates JSON Schema → Executes Tool Handler.
2. **Bridge (WS)**: TS sends JSON payload → C++ Subsystem dispatches to Game Thread.
3. **Execution**: C++ handler performs native UE API calls → Returns JSON result.

### UE 5.7 Safety
- **NO `UPackage::SavePackage()`**: Causes access violations in 5.7. Use `McpSafeAssetSave`.
- **SCS Ownership**: Component templates must be created via `SCS->CreateNode()` and `AddNode()`.
- **`ANY_PACKAGE`**: Deprecated. Use `nullptr` for path lookups.

### TypeScript Standards
- **Zero-Any Policy**: Strictly no `as any` in runtime code. Use `unknown` or interfaces.
- **Strict Mode**: Full TypeScript strict mode enabled (all checks).
- **Colocate Tests**: Unit tests (`.test.ts`) with source, integration in `tests/`.

## ANTI-PATTERNS
- **Console Hacks**: Never use `scripts/remove-saveasset.py` (legacy).
- **Hardcoded Paths**: Avoid `X:\` or `C:\` absolute paths in scripts.
- **Breaking STDOUT**: Never `console.log` in runtime (JSON-RPC only).
- **Incomplete Tools**: No "Not Implemented" stubs. 100% TS + C++ coverage required.
- **Bypass Registry**: Always use `toolRegistry.register()`, never call handlers directly.
- **Raw WS Calls**: Use `executeAutomationRequest()` instead of WebSocket directly.

## UNIQUE STYLES
- **Consolidated Tools**: 35 tools with action-based dispatch (single schema file).
- **Dual Test Runners**: Vitest (unit) + Custom MCP runner (integration).
- **WASM Fallback**: Math-heavy logic uses Rust/WASM with automatic TS fallback.
- **Mock Mode**: Set `MOCK_UNREAL_CONNECTION=true` for offline CI.
- **Non-Standard Layout**: `src/tools/handlers/` nested 2 levels deep.

## COMMANDS
```bash
npm run build:core   # TS only
npm run build        # TS + WASM (WASM failure is non-fatal)
npm run start        # Launch server
npm run test:unit    # Vitest unit tests
npm test             # UE Integration (Requires Editor)
npm run test:smoke   # Mock mode smoke test
```

## NOTES
- **Engine Reference**: Check engine code at `X:\Unreal_Engine\UE_5.7\Engine`, `UE_5.6`, `UE_5.3`.
- **Version Files**: Version in `package.json`, `server.json`, `src/index.ts`, `wasm/Cargo.toml`.
- **Test Patterns**: Integration tests use pipe-separated expectations (`success|error|timeout`).