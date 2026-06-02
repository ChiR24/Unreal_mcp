# tests

Test infrastructure for Unreal MCP. Unit tests use Vitest; integration tests use a custom MCP runner and usually require Unreal Editor with the plugin available.

## STRUCTURE
```
tests/
|-- test-runner.mjs                 # custom MCP runner, expectations, reports, cleanup
|-- integration.mjs                 # canonical integration entry
|-- parameter-combination-audit.mjs # parameter coverage audit script
|-- mcp-tools/
|   |-- core/                       # asset, blueprint, actor/editor/level, inspect, system, tools
|   |-- world/                      # environment, level structure, geometry
|   |-- gameplay/                   # animation, effects, AI, GAS, character, combat, inventory, interaction
|   `-- utility/                    # audio, sequence, networking
|-- unit/                           # Vitest unit tests for config/security/handlers
`-- reports/                        # generated JSON output; never add AGENTS here
```

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Run unit tests | `npm run test:unit` | Vitest, no Unreal required |
| Run smoke test | `npm run test:smoke` | Mock mode path |
| Run integration | `npm test` or `npm run test:all` | Unreal-dependent integration entry |
| Add integration case | `tests/mcp-tools/*/*.test.mjs` | Use `runToolTests(toolName, testCases)` |
| Add unit test | `src/**/*.test.ts` or `tests/unit/**/*.ts` | Follow nearby Vitest setup/cleanup style |
| Audit parameter coverage | `npm run test:params` | Writes reports under `tests/reports/` |

## TEST CASE SHAPE
```javascript
{
  scenario: 'human-readable behavior',
  toolName: 'manage_ai',
  arguments: { action: '...', ... },
  expected: 'success|error',
  assertions: [/* optional custom checks */],
  captureResult: 'optionalName'
}
```

## EXPECTATION GRAMMAR
- String expectations are pipe-separated or `or`-separated; any matching condition can pass.
- The first token is the primary intent and controls whether infra failures can pass.
- Common keywords: `success`, `error`, `handled`, `skipped`, `not found`, `timeout`.
- Suite-specific phrases such as `already exists`, `not loaded`, and `NOT_PARTITIONED` are intentional.
- Object expectations can use `{ condition, successPattern, errorPattern }`.
- Crashes, disconnects, `UE_NOT_CONNECTED`, and timeouts fail unless the primary expectation explicitly allows them.

## ENVIRONMENT AND TIMEOUTS
- Server spawn: `UNREAL_MCP_SERVER_CMD`, `UNREAL_MCP_SERVER_ARGS`, `UNREAL_MCP_SERVER_CWD`.
- Build/source mode: `UNREAL_MCP_AUTO_BUILD`, `UNREAL_MCP_FORCE_DIST`.
- Logging: `UNREAL_MCP_TEST_LOG_RESPONSES`, `UNREAL_MCP_TEST_RESPONSE_MAX_CHARS`.
- Timing defaults: throttle `100ms`, case timeout `5000ms`, call timeout `60000ms`, client timeout `300000ms`.
- `callWithRetry()` defaults to 3 retries with exponential backoff.

## CLEANUP PATTERNS
- Use `Date.now()` plus counters/tags for unique asset and actor names.
- Include setup cases before dependent actions and explicit cleanup cases at the end.
- Cleanup uses delete actions, `delete_asset`, `delete_by_tag`, or folder deletion with `force: true`.
- `manage_geometry` resets around destructive operations; keep the reset cadence before high-impact edits such as poke/subdivide/triangulate/array operations.
- Runtime temp paths such as `./tmp/unreal-mcp/build-environment` are artifacts, not source.

## ANTI-PATTERNS
- Writing AGENTS or hand-authored docs under `tests/reports/`.
- Letting infrastructure failures pass through broad strings like `error|timeout|success` when success is the real intent.
- Reusing fixed actor/asset names across integration cases.
- Removing setup/cleanup because a case passes locally.
