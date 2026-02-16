# tests

Integration and unit testing infrastructure for Unreal MCP.

## OVERVIEW
Dual test runners: Vitest (unit) + Custom MCP runner (integration with Unreal Engine).

## STRUCTURE
```
tests/
├── test-runner.mjs     # Core MCP test runner (1100+ lines)
├── integration.mjs     # Main integration entry (84 test cases)
├── mcp-tools/          # Domain-specific integration tests
│   ├── core/           # manage_asset, control_actor, control_editor, inspect
│   ├── world/          # build_environment, manage_geometry, manage_navigation
│   ├── authoring/      # manage_blueprint, manage_material, manage_texture
│   ├── gameplay/       # manage_ai, manage_behavior_tree, manage_combat
│   └── utility/        # manage_sessions, manage_performance, system_control
├── unit/               # Vitest unit tests (security, validation, utils)
└── reports/            # JSON test results output
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Run integration | `npm test` | Requires Unreal Editor running |
| Run unit tests | `npm run test:unit` | Vitest, no UE required |
| Run smoke test | `npm run test:smoke` | Mock mode, no UE required |
| Add integration test | `tests/mcp-tools/*/*.test.mjs` | Domain subdirectories |
| Add unit test | `src/**/*.test.ts` | Colocate with source |

## CONVENTIONS
### Test Case Structure
```javascript
{
  id: 'action_basic_0',              // Unique test ID
  scenario: 'human description',      // Test description
  toolName: 'manage_navigation',      // MCP tool name
  arguments: { action: '...', ... },  // Tool arguments
  expected: 'success|error|timeout'   // Pipe-separated expectations
}
```

### Expectation System
- **Pipe-separated**: Pass if ANY condition matches
- **Primary intent**: First condition determines pass/fail for infrastructure errors
- **Keywords**: `success`, `error`, `handled`, `skipped`, `not found`, `timeout`

### Test Generation
Most test files auto-generate test cases:
```javascript
const IMPLEMENTED_ACTIONS = ['action1', 'action2', ...];
for (const a of IMPLEMENTED_ACTIONS) {
  tc.push({ scenario: `tool: ${a}`, toolName: '...', ... });
}
```

### Timeout Tiers
- **DEFAULT_TIMEOUT**: 10 seconds
- **HEAVY_TIMEOUT**: 60 seconds (landscape, lighting, LODs)
- **EXTREME_TIMEOUT**: 90 seconds (heightmap modifications)

## ANTI-PATTERNS
- **Infrastructure errors passing**: Crashes/disconnections must FAIL tests unless explicitly expected
- **Missing setup**: CREATE_ACTIONS must have corresponding setup tests
- **Hardcoded actor names**: Use `Date.now()` + counter for uniqueness
- **False positives**: "error|timeout|success" passes on timeout — use primary intent detection

## UNIQUE STYLES
- **Geometry Reset**: Actors reset every 10 destructive operations to prevent OOM
- **Permissive Tools**: Some tools accept empty params (listed in PERMISSIVE_TOOLS)
- **Path Security Tests**: Auto-generated for all path-accepting actions
- **Setup Tests**: Create prerequisite actors before main test loop
