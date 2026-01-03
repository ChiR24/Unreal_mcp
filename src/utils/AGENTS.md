# src/utils

Shared utilities for normalization, security, and logging.

## OVERVIEW
Foundational utilities ensuring path safety, command validation, and consistent logging.

## WHERE TO LOOK
| Utility | File | Purpose |
|---------|------|---------|
| Path Safety | `path-security.ts` | Prevent directory traversal |
| Normalization | `normalize.ts` | Force `/Game/` and forward slashes |
| Validation | `validation.ts` | Zod schemas and runtime checks |
| Command Queue | `unreal-command-queue.ts` | Throttled execution for UE console |

## CONVENTIONS
- **Path First**: Always call `sanitizePath()` before using any user-provided path.
- **Zero-Any**: Strictly enforced here. Use `unknown` + type guards.
- **Side Effects**: Utilities should be pure functions where possible (except `logger` and `command-queue`).

## ANTI-PATTERNS
- **Direct FS Access**: Never use `fs` directly; use normalized path helpers.
- **Hardcoded Strings**: Use `constants.ts` for shared strings.
