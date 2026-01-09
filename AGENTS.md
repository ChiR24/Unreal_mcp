# PROJECT KNOWLEDGE BASE

**Generated:** 2026-01-09
**Commit:** 2de543a
**Branch:** pcg
**Tools:** 30 consolidated tools (~26k tokens)

## OVERVIEW
MCP server for Unreal Engine 5 (5.0-5.7). Dual-process: TS (JSON-RPC) + Native C++ (Bridge Plugin). 30 consolidated tools with 1,600+ actions covering assets, actors, blueprints, levels, animation, VFX, audio, AI, gameplay systems, PCG, environment (water + weather), post-processing, cinematics (MRQ), virtual production (nDisplay, ICVFX), XR platforms, character/avatar plugins, asset plugins (USD, Alembic, glTF, Datasmith, Houdini, Substance), audio middleware (Wwise, FMOD), Live Link, AI NPC plugins, physics/destruction, accessibility, and modding/UGC.

## STRUCTURE
```
./
├── src/               # TS Server (NodeNext ESM)
│   ├── automation/    # Bridge Client & Handshake
│   ├── tools/         # Tool Definitions & Handlers (30 tools)
│   │   └── handlers/  # Domain-specific handler implementations
│   ├── utils/         # Normalization, Security, Validation
│   ├── graphql/       # Optional GraphQL API
│   └── wasm/          # WASM bindings (pkg/)
├── plugins/           # UE Plugin (C++)
│   └── McpAutomationBridge/
│       ├── Source/McpAutomationBridge/
│       │   ├── Private/   # 90+ handler implementations
│       │   └── Public/    # Subsystem & Settings headers
│       └── Config/
├── wasm/              # Rust Source (Math/Parsing)
├── tests/             # Consolidated Integration (.mjs)
└── scripts/           # Maintenance & CI Helpers
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Add MCP Tool | `src/tools/consolidated-tool-definitions.ts` | Schema + action enum |
| Add TS Handler | `src/tools/consolidated-tool-handlers.ts` | Registry dispatch |
| Implement Logic | `src/tools/handlers/*-handlers.ts` | Domain handlers |
| Add UE Action | `plugins/.../Private/*Handlers.cpp` | Register in `Subsystem.cpp` |
| Fix UE Crashes | `McpAutomationBridgeHelpers.h` | Use `McpSafeAssetSave` |
| Path Handling | `src/utils/normalize.ts` | Force `/Game/` prefix |
| CI Workflows | `.github/workflows/` | Check for future-dated versions |

## CONVENTIONS
### Dual-Process Flow
1. **TS (MCP)**: Validates JSON Schema → Executes Tool Handler
2. **Bridge (WS)**: TS sends JSON payload → C++ Subsystem dispatches to Game Thread
3. **Execution**: C++ handler performs native UE API calls → Returns JSON result

### TypeScript
- **Zero-Any Policy**: No `as any` in runtime. Use `unknown` or interfaces
- **ESM Only**: `"type": "module"`, NodeNext resolution
- **Strict Mode**: All strict flags enabled (`noUncheckedIndexedAccess`, `useUnknownInCatchVariables`, etc.)
- **Colocated Tests**: Unit tests as `*.test.ts` next to source

### UE 5.7 Safety
- **NO `UPackage::SavePackage()`**: Use `McpSafeAssetSave` (access violations in 5.7)
- **SCS Ownership**: Component templates via `SCS->CreateNode()` + `AddNode()`
- **`ANY_PACKAGE`**: Deprecated. Use `nullptr` for path lookups
- **GetActiveWorld()**: Use helper instead of `GEditor->GetEditorWorldContext().World()`

### Path Normalization
- All paths must use `/Game/` prefix (not `/Content/`)
- Forward slashes only, no backslashes
- Call `sanitizePathSafe()` before using user-provided paths

## ANTI-PATTERNS
- **Console Hacks**: Never use `scripts/remove-saveasset.py` (legacy)
- **Hardcoded Paths**: No `X:\` or `C:\` absolute paths in code
- **Breaking STDOUT**: Never `console.log` in runtime (JSON-RPC only)
- **Incomplete Tools**: No stubs. 100% TS + C++ coverage required
- **Raw WS Calls**: Use `executeAutomationRequest()`, not direct WebSocket
- **Bypassing Registry**: Always route through `toolRegistry.register()`

## COMMANDS
```bash
npm run build:core   # TS only
npm run build        # TS + WASM (best-effort)
npm run start        # Launch server
npm run dev          # Dev mode (ts-node-esm)
npm run test:unit    # Vitest (no UE required)
npm test             # Integration (requires UE + plugin)
npm run lint         # ESLint
npm run type-check   # TSC --noEmit
```

## NOTES
- **Critical**: Check `.github/workflows` for hallucinated versions (e.g., checkout@v6)
- **Engine Reference**: `X:\Unreal_Engine\UE_5.7\Engine`, `UE_5.6`, `UE_5.3`
- **Mock Mode**: `MOCK_UNREAL_CONNECTION=true` for offline CI
- **WASM Fallback**: Math-heavy ops use Rust/WASM with automatic TS fallback
- **GraphQL**: Optional, disabled by default (`GRAPHQL_ENABLED=true`)
