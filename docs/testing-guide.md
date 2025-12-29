# Testing Guide

## Overview

This project uses a consolidated integration test suite covering all 17 MCP tools, plus Vitest for unit tests and a CI smoke test for mock-mode validation.

## Test Commands

| Command | Description | Requires UE? |
|---------|-------------|--------------|
| `npm test` | Run consolidated integration suite | Yes |
| `npm run test:all` | Same as above | Yes |
| `npm run test:unit` | Run Vitest unit tests | No |
| `npm run test:smoke` | CI smoke test (mock mode) | No |

## Integration Tests

### Running

```bash
# Ensure Unreal Engine is running with MCP Automation Bridge plugin enabled
npm test
```

This runs `tests/integration.mjs`, which covers 44 scenarios across all tool categories:
- Infrastructure & Discovery
- Asset & Material Lifecycle
- Actor Control & Introspection
- Blueprint Authoring
- Environment & Visuals
- AI & Input
- Cinematics & Audio
- Operations & Performance

### Test Structure

```
tests/
├── integration.mjs    # Consolidated test suite (44 scenarios)
├── test-runner.mjs    # Shared test harness
└── reports/           # JSON test results (gitignored)
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
