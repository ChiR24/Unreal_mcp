# TEST KNOWLEDGE BASE

Test infrastructure for the TypeScript MCP server and Unreal bridge contracts. Vitest tests run without Unreal; MCP integration suites normally require a built server plus a connected Unreal Editor.

## STRUCTURE
```text
tests/
|-- test-runner.mjs                 # stdio client, setup/cleanup, expectations, JSON reports
|-- test-runner-response-utils.mjs  # response-path assertions and filtered capture selection
|-- integration.mjs                 # compact representative parent-tool integration suite
|-- expectation-utils.mjs           # shared expectation parsing
|-- mcp-tools/
|   |-- core/                       # actor, editor, assets, blueprints, levels, inspect, tools
|   |-- gameplay/                   # animation, AI, character, combat, effects, GAS, inventory
|   |-- utility/                    # audio, behavior trees, networking, sequences
|   `-- world/                      # environment, geometry, level structure, PCG
|-- native-mcp-parity-audit.mjs     # TS versus native canonical tool/action parity
|-- parameter-combination-audit.mjs # audit CLI entrypoint
|-- parameter-audit-*.mjs           # CLI, schema extraction, suite capture, coverage, context
|-- unit/                           # Vitest behavior, security, routing, and source contracts
`-- reports/                        # generated JSON only; never add AGENTS or hand-authored files
```

The mock smoke test is `scripts/smoke-test.ts`, not under this directory. It imports `dist/index.js` (so `npm run build` must run first), uses linked in-memory transports, and self-sets `MOCK_UNREAL_CONNECTION`. It asserts exactly **one public tool (`unreal`)**, that search/describe/configure work, that `perActionSchemas` is `false`, and that a hidden parent tool called directly is **rejected**. It does NOT assert a 23-tool listing.

## TIMEOUT LADDER
| Scope | Default | Override |
|-------|---------|----------|
| Vitest unit | 10s | `vitest.config.ts` `testTimeout` |
| Integration per-case | **5s** | `UNREAL_MCP_TEST_CASE_TIMEOUT_MS`, or per-case `timeoutMs` (cleanup cases use 30s) |
| Per-call server | 60s | `UNREAL_MCP_TEST_CALL_TIMEOUT_MS` |
| Client / progress | 300s | `UNREAL_MCP_TEST_CLIENT_TIMEOUT_MS` |
| Bridge port wait | 5s/port | `UNREAL_MCP_WAIT_PORT_MS` (client-level wait is 10s) |
| Inter-case throttle | 100ms | `UNREAL_MCP_TEST_THROTTLE_MS` |

Other runner env: `MCP_AUTOMATION_WS_HOST` (127.0.0.1), `MCP_AUTOMATION_WS_PORTS` (8090,8091), `UNREAL_MCP_SERVER_CMD/ARGS/CWD`, `UNREAL_MCP_FORCE_DIST`, `UNREAL_MCP_AUTO_BUILD` / `UNREAL_MCP_NO_AUTO_BUILD`, `UNREAL_MCP_TEST_LOG_RESPONSES`.

The runner AUTO-BUILDS: if `dist/cli.js` is missing, or source is newer than dist, it runs `npm run build` unless `UNREAL_MCP_NO_AUTO_BUILD=1`. It still needs a live editor.

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| Change live test execution | `test-runner.mjs`, `test-runner-response-utils.mjs` | Owns server spawn, response normalization, retries, assertions, captures, cleanup, reports |
| Add broad integration coverage | `integration.mjs` | Canonical quick suite across parent tools |
| Add tool/action coverage | `mcp-tools/<category>/*.test.mjs` | Import `runToolTests` directly and keep setup before dependent cases |
| Change expectation parsing | `expectation-utils.mjs`, `unit/test_runner.test.ts` | Lock evaluator changes with focused unit cases |
| Change native parity rules | `native-mcp-parity-audit.mjs` | Reads TS definitions and native canonical registry/tool C++ |
| Change parameter audit | `parameter-audit-*.mjs` | Schema AST, suite capture, live/static evidence, reporting |
| Change architectural contracts | `unit/plugin/`, `unit/source_structure.test.ts`, `unit/tools/handler_structure.test.ts` | These inspect source text and layout; they do not compile Unreal |

## COMMANDS
```bash
npm run test:unit          # Vitest: src/**/*.test.ts and tests/unit/**/*.test.ts
npm run test:smoke         # mock in-memory MCP check against built dist/
npm test                   # tests/integration.mjs; Unreal-dependent
npm run test:native-parity # canonical TS/native tool and action equality
npm run test:params        # parity, then static + strict + optional-strict parameter audit
```

## INTEGRATION CASES
- Use `{ scenario, toolName, arguments, expected }`; optional fields include `assertions`, `captureResult`, and `timeoutMs`.
- Export no custom harness: end each suite with `runToolTests('<suite-name>', cases)`.
- Keep the standard relative `runToolTests` import shape; the static audit replaces that import while evaluating suite definitions.
- Use unique actor/asset names, usually timestamped, and add explicit cleanup for created state.
- Captures use `{ key, fromField }`; array captures may add `where: { path, equals|includes }` and `selectField`. Later arguments reference `${captured:key}`.
- Assertions address response paths such as `structuredContent.result.assetPath`; use `equals` for exact values, `includes` for string fragments, or `approximately` with a nonnegative `tolerance` for floating-point values.

## EXPECTATION GRAMMAR
- Strings split on literal ` or ` or `|`; the first token is the primary intent.
- Put `success`, `error`, or `timeout` first according to what the case is proving.
- Success-primary cases may name narrow state alternatives such as `already exists`, `not found`, `not loaded`, or `NOT_PARTITIONED`.
- Do not use broad masks such as `success|error` or place `timeout` after `error`; crashes, bridge loss, and non-primary timeouts are infrastructure failures.
- Object expectations support `condition`, `successPattern`, and `errorPattern`; prefer patterns for exact controlled fallback codes.
- `structuredContent.success: false`, nested failures, or `isError: true` must not pass a success-primary case unless an explicit allowed alternative matches.

## AUDIT CONTRACTS
- Native parity compares canonical tool names and `definitions/shared/action-sets.ts` enums with native MCP registry and tool definitions.
- Parameter schema extraction uses the TypeScript compiler API; suite coverage is captured from `mcp-tools/` plus `integration.mjs`.
- Missing or extra actions always fail the parameter audit. `--strict` also fails undeclared test parameters.
- `--optional-strict` fails optional schema parameters absent from static coverage; `npm run test:params` enables all strict static gates.
- Live audit mode consumes the latest `<suite>-test-results-*.json`; only successful live responses prove optional-parameter coverage.
- Treat `reports/` as disposable evidence. Diagnose failures from the newest JSON, but never edit reports to satisfy a gate.

## SOURCE-CONTRACT TESTS
- Many `unit/plugin/*contracts.test.ts` cases read C++/C# files and assert required or forbidden source patterns.
- Structure tests enforce responsibility boundaries, resolvable local includes, naming rules, and the 250 pure-line ceiling.
- When moving or renaming implementation files, update contract paths and assertions only when the intended invariant changed.
