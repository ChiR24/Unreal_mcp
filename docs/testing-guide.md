# Testing Guide

## Overview

This project uses consolidated integration test suites covering the 23 canonical MCP tools:
- **Canonical Integration Suite** (`tests/integration.mjs`) for cross-tool workflows
- **Per-Tool MCP Suites** (`tests/mcp-tools/**/*.test.mjs`) for every exposed parent tool

Plus Vitest for unit tests and a CI smoke test for mock-mode validation.

## Test Commands

| Command | Description | Requires UE? |
|---------|-------------|--------------|
| `npm test` | Run canonical integration suite | Yes |
| `node tests/mcp-tools/<category>/<tool>.test.mjs` | Run one per-tool MCP suite | Yes |
| `npm run test:all` | Run canonical integration entrypoint | Yes |
| `npm run test:unit` | Run Vitest unit tests | No |
| `npm run test:smoke` | CI smoke test (mock mode) | No |

## Integration Tests

### Running

```bash
# Ensure Unreal Engine is running with MCP Automation Bridge plugin enabled
npm test
node tests/mcp-tools/core/manage-asset.test.mjs
for f in tests/mcp-tools/{core,gameplay,utility,world}/*.test.mjs; do node "$f"; done
```

### Canonical Integration Suite (`tests/integration.mjs`)

Covers cross-tool workflows across the 23 canonical MCP tools:
- Infrastructure & Discovery
- Asset & Material Lifecycle
- Actor Control & Introspection
- Blueprint Authoring
- Environment & Visuals
- AI & Input
- Cinematics & Audio
- Operations & Performance

### Per-Tool MCP Suites (`tests/mcp-tools/**/*.test.mjs`)

Covers every exposed parent tool with domain-specific setup and teardown:
- Core: `manage_tools`, `manage_asset`, `manage_blueprint`, `control_actor`, `control_editor`, `manage_level`, `inspect`, `system_control`
- World: `build_environment`, `manage_level_structure`, `manage_geometry`, `manage_pcg`
- Gameplay: `animation_physics`, `manage_effect`, `manage_ai`, `manage_gas`, `manage_character`, `manage_combat`, `manage_inventory`, `manage_interaction`
- Utility: `manage_audio`, `manage_sequence`, `manage_networking`

### Test Structure

```
tests/
├── integration.mjs          # Canonical integration suite
├── mcp-tools/               # Per-tool canonical MCP suites
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
1. **Unreal Engine 5.0–5.8 Preview** must be running
2. **MCP Automation Bridge plugin** enabled and listening on port 8091

### Environment Variables (optional)
```bash
MCP_AUTOMATION_HOST=127.0.0.1  # Default
MCP_AUTOMATION_PORT=8091       # Default
MCP_CONNECTION_TIMEOUT_MS=5000 # Connection and handshake timeout
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
