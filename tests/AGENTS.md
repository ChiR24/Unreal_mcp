# TESTING KNOWLEDGE BASE

**Generated:** 2026-01-30
**Scope:** Unreal MCP Server Test Architecture

## OVERVIEW
Comprehensive testing strategy for Unreal Engine MCP server, combining Vitest unit tests with live-engine integration suites across 52+ MCP tools and 44 categories.

## STRUCTURE
```
tests/
├── integration.mjs          # Main integration test suite (52+ tools)
├── test-runner.mjs          # Harness for live UE connection tests
├── category-tests/          # 44 domain-specific tests (*.test.mjs)
├── reports/                 # Timestamped JSON test results
├── utils/                   # Test helpers and mock factories
│   └── test-helpers.ts      # Centralized mock generation
├── unit/                    # Global unit tests (automation, errors)
├── benchmarks/              # WASM math and baseline performance
└── assets/                  # Test artifacts (T3D, FBX)
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Add Unit Test | `src/**/*.test.ts` | Colocated with source code |
| Add Integration Test | `tests/category-tests/` | Use `automation:scaffold-test` |
| Fix Mock Data | `tests/utils/test-helpers.ts` | Centralized factories |
| Check Test Results | `tests/reports/` | Review latest JSON reports |
| Performance Profiling | `tests/benchmarks/` | WASM vs TS math baselines |
| Global Test Logic | `tests/integration.mjs` | core tool orchestration |

## CONVENTIONS
- **Dual-Layer Testing**: Unit tests (Vitest) for logic, Integration (Node ESM) for UE side-effects.
- **Colocation**: Keep unit tests next to the implementation files in `src/`.
- **Flexible Outcomes**: Use pipe-delimited expected results (e.g., `'success|already exists'`).
- **Mock Factories**: Never hardcode complex objects; use `test-helpers.ts`.
- **Environment**: Use `MOCK_UNREAL_CONNECTION=true` for CI/CD or offline logic testing.
- **Reporting**: All integration runs must output to `tests/reports/`.

## COMMANDS
```bash
npm run test:unit            # Run Vitest (no engine required)
npm test                     # Run Integration tests (requires live UE)
npm run test:unit:coverage   # Generate coverage report
npm run automation:scaffold-test # Scaffold new integration tests
```

## ANTI-PATTERNS
- **Hardcoded Engine Paths**: Always use relative `/Game/` paths in tests.
- **Engine State Pollution**: Avoid tests that don't cleanup created actors/assets.
- **Direct WS Calls**: Use the test harness; do not manually open WebSockets.
- **Manual Reports**: Do not manually edit files in `tests/reports/`.
- **Ignoring Coverage**: Ensure new handlers have colocated unit tests.
