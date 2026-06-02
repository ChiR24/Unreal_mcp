# src/utils

Shared TypeScript utility layer for input normalization, security gates, logging, output validation, and safe JSON handling. Keep this directory strict and dependency-light because most tool paths depend on it.

## WHERE TO LOOK
| Utility | File | Purpose |
|---------|------|---------|
| UE path security | `path-security.ts` | `sanitizePath()` enforces allowed UE roots and blocks traversal/illegal chars |
| Value normalization | `normalize.ts` | vectors, rotations, partial transforms, finite numbers |
| Console command safety | `command-validator.ts` | blocks dangerous commands, shell chaining, Python, file/system access tokens |
| Response validation | `response-validator.ts` | AJV-backed MCP output validation |
| Logging | `logger.ts` | structured stderr logging; do not write runtime logs to stdout |
| Safe JSON | `safe-json.ts` | clean values before returning/logging |
| Type guards | `type-guards.ts` | runtime narrowing for `unknown` inputs |
| Elicitation/config helpers | `elicitation.ts`, `ini-reader.ts` | MCP prompts and Unreal config reads |
| Command queue | `unreal-command-queue.ts` | throttled UE console command execution |

## CONVENTIONS
- `sanitizePath()` defaults to `/Game`, `/Engine`, `/Script`, `/Temp`, `/Niagara`, plus sanitized `MCP_ADDITIONAL_PATH_PREFIXES`.
- Path helpers reject empty strings, traversal, double slashes after normalization, Windows-reserved characters, and unapproved roots.
- `CommandValidator.validate()` blocks multiline commands, `py`/`python`, `&&`, `||`, `;`, `|`, backticks, dangerous engine commands, and shell/Python file access tokens.
- Prefer `unknown` plus local type guards over unchecked casts.
- Utility functions should be pure where practical; logging, queueing, config reads, and schema registration are the expected side-effect exceptions.

## ANTI-PATTERNS
- Adding a second path-root policy outside `path-security.ts`.
- Bypassing `CommandValidator` for `console_command` or batch console execution.
- Logging secrets, capability tokens, or raw unbounded payloads.
- Using direct stdout writes from runtime utilities.
- Adding broad filesystem access helpers that accept arbitrary host paths.
