# Testing Guide

## Overview

| Suite | What it covers | Needs Unreal? |
|-------|----------------|---------------|
| **Unit** (Vitest) | `src/**/*.test.ts` and `tests/unit/**/*.test.ts`: gateway behavior, security, routing, and source contracts that read the plugin's C++ without compiling it | No |
| **Smoke** (`scripts/smoke-test.ts`) | The built server in mock mode, over in-memory transports | No |
| **Parameter audit** | Every action and parameter in the generated schemas against the integration cases | No |
| **Search eval** | The search-ranking corpus | No |
| **Integration** | `tests/integration.mjs` (cross-tool workflows) and `tests/mcp-tools/**/*.test.mjs` (one suite per area), against a live editor | Yes |

The public surface is the single `unreal` gateway tool. Integration cases still name the 23 internal parent tools and their actions; the runner sends each case through `unreal` as an `execute` call.

## Commands

| Command | Description | Needs Unreal? |
|---------|-------------|---------------|
| `npm run test:unit` | Vitest unit tests | No |
| `npm run test:smoke` | Mock-mode MCP check against `dist/` (run `npm run build` first) | No |
| `npm run test:params` | Static, strict parameter audit | No |
| `npm run eval:check` | Search-ranking corpus | No |
| `npm test` or `npm run test:all` | `tests/integration.mjs` | Yes |
| `node tests/mcp-tools/<category>/<suite>.test.mjs` | One integration suite | Yes |

## Integration tests

### Prerequisites

1. An Unreal Editor (5.0 to 5.8) running with the MCP Automation Bridge plugin loaded. The runner waits for its WebSocket listener on `127.0.0.1` port `8090` or `8091`, the default **Listen Ports**.
2. `UE_PROJECT_PATH` set to that project's folder or `.uproject`. The runner launches the stdio server with its own environment, and the server needs the path to read the capability token (on by default). Alternatively, set `MCP_AUTOMATION_CAPABILITY_TOKEN`.
3. A built server. The runner runs `npm run build` itself when `dist/cli.js` is missing or older than the source; `UNREAL_MCP_NO_AUTO_BUILD=1` turns that off.

### Running

```bash
export UE_PROJECT_PATH="/path/to/MyGame"
npm test
node tests/mcp-tools/core/manage-asset.test.mjs
for f in tests/mcp-tools/{core,gameplay,utility,world}/*.test.mjs; do node "$f"; done
```

### Suites

`tests/integration.mjs` is a compact cross-tool workflow suite: assets and materials, actors, Blueprints, environment, AI and input, cinematics and audio, operations and performance.

`tests/mcp-tools/` holds the per-area suites:

- **core:** actors, editor, assets, Blueprints, levels, `inspect`, `manage_tools`, `system_control`, DataTables, enums and structs
- **gameplay:** animation and physics, AI, characters, combat, effects, GAS, interaction, inventory
- **utility:** audio, Behavior Trees, networking, sequences, media, Movie Render Queue
- **world:** environment, geometry, level structure, PCG, rendering

Cases name the pre-fold actions. For each folded family, the runner re-runs the first case that names a member as the family's advertised primary plus its selector value (`tests/fold-twins.mjs`), so every primary and selector is covered without duplicate cases. The parameter audit captures the same twins.

### Writing a case

Add an object to the suite's case array:

```javascript
{
  scenario: 'Blueprint: create Actor blueprint',
  toolName: 'manage_blueprint',
  arguments: { action: 'create', name: 'BP_IntegrationTest', savePath: TEST_FOLDER, parentClass: 'Actor' },
  expected: 'success|already exists'
}
```

A capability that needs consent carries it on the case, not in `arguments`:

```javascript
{
  scenario: 'Cleanup: delete test actor',
  toolName: 'control_actor',
  arguments: { action: 'delete', actorName: 'IT_Cube' },
  expected: 'success|not found',
  consent: { capability: 'control_actor.delete', acknowledge: 'explicit' }
}
```

Optional fields: `assertions` (checks on response paths such as `structuredContent.result.assetPath`, with the operators `equals`, `approximately`, `includes`, `notIncludes`, `length`, `minLength`, `includesObject` and `gte`), `captureResult` (save a value for later cases as `${captured:key}`), and `timeoutMs`. Use unique names and clean up what a suite creates. Suites end with `runToolTests('<suite-name>', cases)`.

### Expectations

- `expected` splits on `|` (or ` or `). The **first** token is what the case proves: put `success`, `error` or `timeout` first.
- A success-first case may list narrow alternatives such as `already exists` or `not found`.
- Don't mask with `success|error`, and don't put `timeout` after `error`. Crashes, a lost bridge and unexpected timeouts count as infrastructure failures.
- A response with `success: false`, a nested failure or `isError: true` never passes a success-first case unless an allowed alternative matches.
- Object expectations support `condition`, `successPattern` and `errorPattern`, for exact error codes.

### Output

```text
[PASSED] Asset: create test folder (234.5 ms)
[PASSED] Actor: spawn StaticMeshActor (456.7 ms)
[FAILED] Level: get summary (123.4 ms) => {"success":false,"error":"..."}
```

Each suite writes `tests/reports/<suite>-test-results-<timestamp>.json` (gitignored). The process exits with `0` when every case passed and `1` otherwise.

### Timeouts and runner settings

| Scope | Default | Override |
|-------|---------|----------|
| Integration case | 5 s | Per-case `timeoutMs`, or `UNREAL_MCP_TEST_CASE_TIMEOUT_MS` |
| Server call | 60 s | `UNREAL_MCP_TEST_CALL_TIMEOUT_MS` |
| Client, with progress | 300 s | `UNREAL_MCP_TEST_CLIENT_TIMEOUT_MS` |
| Wait for the bridge port | 10 s | Fixed |
| Pause between cases | 100 ms | `UNREAL_MCP_TEST_THROTTLE_MS` |

Other runner variables: `MCP_AUTOMATION_WS_HOST` and `MCP_AUTOMATION_WS_PORTS` (where to wait for the editor), `UNREAL_MCP_SERVER_CMD` / `_ARGS` / `_CWD` (how to launch the server), `UNREAL_MCP_FORCE_DIST`, `UNREAL_MCP_ALLOW_TS_FALLBACK` and `UNREAL_MCP_TEST_LOG_RESPONSES`.

## Unit tests

```bash
npm run test:unit            # run once
npm run test:unit:watch      # watch mode
npm run test:unit:coverage   # with coverage
```

The tests under `tests/unit/plugin/` read the plugin's C++ source and assert required or forbidden patterns; they don't compile Unreal. Structure tests enforce file boundaries, resolvable includes and the 250-line ceiling.

## Smoke test

```bash
npm run build
npm run test:smoke
```

It sets `MOCK_UNREAL_CONNECTION` itself, so no editor is involved. It checks that the server lists exactly one tool, `unreal`, that search, describe and configure work, and that a direct call to an internal tool is rejected. CI runs it on Node 20.19 and 26 for pushes and pull requests to `main`.

## Troubleshooting

| Symptom | Cause |
|---------|-------|
| *Automation bridge did not become available before tests started* | The editor isn't running, the plugin isn't loaded, or **Listen Ports** changed: point `MCP_AUTOMATION_WS_PORTS` at the new ports |
| Every case fails with *not connected* or *handshake rejected* | The server can't find the capability token: set `UE_PROJECT_PATH` |
| One case fails | Check the editor's Output Log and the newest JSON in `tests/reports/`. Suites create assets under `/Game/IntegrationTest` and `/Game/AdvancedIntegrationTest` and actors named `IT_*`, and delete them in their cleanup cases. |
| A slow case times out | Give it a `timeoutMs`, or raise `UNREAL_MCP_TEST_CASE_TIMEOUT_MS` |
