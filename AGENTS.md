# PROJECT KNOWLEDGE BASE

**Generated:** 2026-01-05 12:00:00 UTC
**Commit:** 4f1bacc
**Branch:** adv
**Tools:** 36 consolidated tools

## OVERVIEW
MCP server for Unreal Engine 5 (5.0-5.7). Dual-process: TS (JSON-RPC) + Native C++ (Bridge Plugin). 36 action-based tools covering assets, actors, blueprints, levels, animation, VFX, audio, AI, gameplay systems, and procedural content generation.

## STRUCTURE
```
./
├── src/           # TS Server (NodeNext ESM)
│   ├── automation/ # Bridge Client & Handshake
│   ├── tools/     # Tool Definitions & Handlers
│   └── utils/     # Normalization & Security
├── plugins/       # UE Plugin (C++)
│   └── McpAutomationBridge/
│       ├── Source/ # Native Handlers & Subsystem
│       └── Config/ # Plugin Settings
├── wasm/          # Rust Source (Math/Parsing)
├── tests/         # Consolidated Integration (.mjs)
└── scripts/       # Maintenance & CI Helpers
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Add MCP Tool | `src/tools/` | Schema in `consolidated-tool-definitions.ts` |
| Add UE Action | `plugins/.../Private/` | Signature in `Subsystem.h`, impl in `*Handlers.cpp` |
| Fix UE Crashes | `McpAutomationBridgeHelpers.h` | Use `McpSafeAssetSave` for 5.7+ |
| Path Handling | `src/utils/normalize.ts` | Force `/Game/` prefix |
| CI Workflows | `.github/workflows/` | Check for future-dated versions (CRITICAL) |

## CONVENTIONS
### Dual-Process Flow
1. **TS (MCP)**: Validates JSON Schema → Executes Tool Handler.
2. **Bridge (WS)**: TS sends JSON payload → C++ Subsystem dispatches to Game Thread.
3. **Execution**: C++ handler performs native UE API calls → Returns JSON result.

### UE 5.7 Safety
- **NO `UPackage::SavePackage()`**: Causes access violations in 5.7. Use `McpSafeAssetSave`.
- **SCS Ownership**: Component templates must be created via `SCS->CreateNode()` and `AddNode()`.
- **`ANY_PACKAGE`**: Deprecated. Use `nullptr` for path lookups.

### TypeScript Zero-Any Policy
- Strictly no `as any` in runtime code. Use `unknown` or interfaces.
- Colocate unit tests (`.test.ts`) with source.

## ANTI-PATTERNS
- **Console Hacks**: Never use `scripts/remove-saveasset.py` (legacy).
- **Hardcoded Paths**: Avoid `X:\` or `C:\` absolute paths in scripts.
- **Breaking STDOUT**: Never `console.log` in runtime (JSON-RPC only).
- **Incomplete Tools**: No "Not Implemented" stubs. 100% TS + C++ coverage required.

## UNIQUE STYLES
- **Consolidated Tests**: All integration tests reside in `tests/integration.mjs`.
- **WASM Fallback**: Math-heavy logic uses Rust/WASM with automatic TS fallback.
- **Mock Mode**: Set `MOCK_UNREAL_CONNECTION=true` for offline CI.

## COMMANDS
```bash
npm run build:core   # TS only
npm run start        # Launch server
npm run test:unit    # Vitest
npm test             # UE Integration (Requires Editor)
```

## NOTES
- **Critical**: Check `.github/workflows` for hallucinated versions (e.g., checkout@v6).
- **Assets**: Root `Public/` assets should be moved to `docs/assets/`.
- Always check engine code for help at path X:\Unreal_Engine\UE_5.7\Engine, X:\Unreal_Engine\UE_5.6\Engine, X:\Unreal_Engine\UE_5.3\Engine.