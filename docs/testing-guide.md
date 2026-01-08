# Testing Guide

## Overview

This project uses consolidated integration test suites covering all MCP tools:
- **Core Suite** (18 original tools, 44 scenarios)
- **Advanced Suite** (18 new Phase 6-27 tools, 80+ scenarios)

Plus Vitest for unit tests and a CI smoke test for mock-mode validation.

## Test Commands

| Command | Description | Requires UE? |
|---------|-------------|--------------|
| `npm test` | Run core integration suite | Yes |
| `npm run test:advanced` | Run advanced integration suite (Phases 6-20) | Yes |
| `npm run test:all` | Run both integration suites | Yes |
| `npm run test:unit` | Run Vitest unit tests | No |
| `npm run test:smoke` | CI smoke test (mock mode) | No |

## Integration Tests

### Running

```bash
# Ensure Unreal Engine is running with MCP Automation Bridge plugin enabled
npm test              # Core suite (44 tests)
npm run test:advanced # Advanced suite (80 tests)
npm run test:all      # Both suites (124 tests)
```

### Core Suite (`tests/integration.mjs`)

Covers 44 scenarios across the original 18 tool categories:
- Infrastructure & Discovery
- Asset & Material Lifecycle
- Actor Control & Introspection
- Blueprint Authoring
- Environment & Visuals
- AI & Input
- Cinematics & Audio
- Operations & Performance

### Advanced Suite (`tests/integration-advanced.mjs`)

Covers 80+ scenarios across the 18 new Phase 6-27 tools:
- Phase 6: Geometry & Mesh Creation (`manage_geometry`)
- Phase 7: Skeletal Mesh & Rigging (`manage_skeleton`)
- Phase 8: Advanced Material Authoring (`manage_material_authoring`)
- Phase 9: Texture Generation (`manage_texture`)
- Phase 10: Animation Authoring (merged into `animation_physics`)
- Phase 11: Audio Authoring (merged into `manage_audio`)
- Phase 12: Niagara VFX Authoring (merged into `manage_effect`)
- Phase 13: Gameplay Ability System (`manage_gas`)
- Phase 14: Character & Movement (`manage_character`)
- Phase 15: Combat & Weapons (`manage_combat`)
- Phase 16: AI System - Enhanced (`manage_ai`)
- Phase 17: Inventory & Items (`manage_inventory`)
- Phase 18: Interaction System (`manage_interaction`)
- Phase 19: Widget Authoring (`manage_widget_authoring`)
- Phase 20: Networking & Multiplayer (`manage_networking`)
- Phase 21: Game Framework (`manage_game_framework`)
- Phase 22: Sessions & Local Multiplayer (`manage_sessions`)
- Phase 23: Level Structure (merged into `manage_level`)
- Phase 24: Volumes & Zones (`manage_volumes`)
- Phase 25: Navigation System (`manage_navigation`)
- Phase 26: Spline System (`manage_splines`)
- Phase 27: PCG Framework (`manage_pcg`)

### Test Structure

```
tests/
├── integration.mjs          # Core test suite (44 scenarios)
├── integration-advanced.mjs # Advanced test suite (80 scenarios)
├── test-runner.mjs          # Shared test harness
└── reports/                 # JSON test results (gitignored)
```

### Adding New Tests

Edit `tests/integration.mjs` and add a test case to the `testCases` array:

```javascript
{
  scenario: 'Your test description',
  toolName: 'manage_asset',
  arguments: { action: 'list', path: '/Game/MyFolder' },
  expected: 'success'
}
```

The `expected` field supports flexible matching:
- `'success'` — response must have `success: true`
- `'success|not found'` — either success OR "not found" in response
- `'error'` — expects failure

### Test Output

Console shows pass/fail status with timing:
```
[PASSED] Asset: create test folder (234.5 ms)
[PASSED] Actor: spawn StaticMeshActor (456.7 ms)
[FAILED] Level: get summary (123.4 ms) => {"success":false,"error":"..."}
```

JSON reports are saved to `tests/reports/` with timestamps.

## Unit Tests

```bash
npm run test:unit        # Run once
npm run test:unit:watch  # Watch mode
npm run test:unit:coverage  # With coverage
```

Unit tests use Vitest and don't require Unreal Engine. They cover:
- Utility functions (`normalize.ts`, `validation.ts`, `safe-json.ts`)
- Pure TypeScript logic

## CI Smoke Test

```bash
MOCK_UNREAL_CONNECTION=true npm run test:smoke
```

Runs in GitHub Actions on every push/PR. Uses mock mode to validate server startup and basic tool registration without an actual Unreal connection.

## Prerequisites

### Unreal Engine Setup
1. **Unreal Engine 5.0–5.7** must be running
2. **MCP Automation Bridge plugin** enabled and listening on port 8091

### Environment Variables (optional)
```bash
MCP_AUTOMATION_HOST=127.0.0.1  # Default
MCP_AUTOMATION_PORT=8091       # Default
```

## Troubleshooting

### All Tests Fail with ECONNREFUSED
- Unreal Engine is not running, or
- MCP Automation Bridge plugin is not enabled, or
- Port 8091 is blocked

### Specific Tests Fail
- Check Unreal Output Log for errors
- Verify the asset/actor/level referenced in the test exists
- Some tests create temporary assets in `/Game/IntegrationTest` (cleaned up at end)

### Test Times Out
- Default timeout is 30 seconds per test
- Complex operations (lighting builds, large imports) may need longer
- Check if Unreal is frozen or unresponsive

## Exit Codes

- `0` — All tests passed
- `1` — One or more tests failed

Use in CI/CD:
```bash
npm test && echo "All tests passed"
```
