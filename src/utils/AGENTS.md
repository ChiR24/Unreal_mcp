# src/utils

Small shared helpers for the proxy and the record generators. The security gates (paths, console commands, scopes) live in the plugin, which is the only authority.

## WHERE TO LOOK
| Utility | File | Purpose |
|---------|------|---------|
| Console command policy | `commands/console-command-policy-rules.ts` | the rules; `npm run headers:generate` writes them into the plugin's `McpConsolePolicy.generated.h`, `headers:check` keeps it current |
| Logging | `logging/logger.ts` | structured stderr logging (stdout carries only JSON-RPC) |
| Ordering | `serialization/ordering.ts` | byte-order comparison for generated artifacts (never `localeCompare`) |
| INI reads | `config/ini-reader.ts` | project config values; `projectRootFromEnv()` for the capability-token file |
| Type guards | `validation/type-guards.ts` | narrowing `unknown` |

## CONVENTIONS
- Prefer `unknown` plus type guards over casts.

## ANTI-PATTERNS
- A path or console policy in TypeScript: the plugin enforces both.
- Logging secrets, capability tokens or unbounded payloads.
- Writing to stdout.
