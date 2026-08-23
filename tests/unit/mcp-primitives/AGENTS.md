# MCP PRIMITIVES NATIVE-PARITY TESTS

Vitest tests that gate **byte-for-byte parity** between the TypeScript MCP primitives (`src/server/mcp-primitives/`) and their native mirror (`plugins/McpAutomationBridge/.../Private/MCP/Primitives/`). A drift here fails CI.

## SCOPE
26 `*-parity.test.ts` files in `tests/unit/mcp-primitives/`. No subdirectories. Each file covers one primitive family and uses a GREEN/RED guard pattern (see below).

## WHERE TO LOOK
| Parity gate | Test file | TS surface vs native oracle |
|-------------|-----------|------------------------------|
| Resources (list/read/subscribe) | `resources-parity.test.ts` | TS resource handler shapes vs native fixture |
| Prompts + completions | `prompts-completions-parity.test.ts` | Prompt list + completion argument schemas |
| Session subscriptions | `session-subscription-parity.test.ts` | Subscription lifecycle + coalesced notifications |
| Client profiles fallback | `profiles-fallback-parity.test.ts` | Adaptive profile negotiation + fallback chain |

## CONVENTIONS
- **GREEN/RED guard pattern**: each test asserts the TS output matches the native fixture oracle (GREEN), then asserts a deliberately-mutated TS output does **not** match (RED). The RED guard catches a test that passes trivially because both sides are empty or wrong.
- The native oracle is a frozen fixture (JSON or typed literal) checked into the test file — it does **not** spawn the native transport. Update the fixture only when the intended native contract changes, and update both sides together.
- Parity is on **shape** (keys, types, enum values), not on runtime-produced values like timestamps or generated IDs.
- `perActionSchemas` is always `false` on both surfaces — assert it explicitly; a parity test that omits this check can miss a regression to per-action schema dumping.

## ANTI-PATTERNS
- Do not make a parity test pass by deleting the native side; the RED guard will catch the missing oracle.
- Do not compare runtime values (ports, session IDs, timestamps) — they differ by transport and make the test flaky.
- Do not add a new primitive to one surface without adding the parity test for it in the same PR; CI will not catch the drift otherwise.
- Do not relax the GREEN/RED pattern to GREEN-only — a GREEN-only test passes when both sides are broken identically.
