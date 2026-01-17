# src/utils

Shared utilities for normalization, security, validation, and logging.

## OVERVIEW
Foundational utilities ensuring path safety, command validation, and consistent error handling across the MCP server.

## STRUCTURE
```
utils/
├── path-security.ts       # Directory traversal prevention
├── normalize.ts           # Path normalization (/Game/, forward slashes)
├── validation.ts          # Zod schemas, sanitizePathSafe()
├── command-validator.ts   # Console command safety filtering
├── unreal-command-queue.ts # Throttled UE command execution
├── safe-json.ts           # Depth-limited JSON parsing (circular ref detection)
├── response-factory.ts    # Standardized response builders
├── result-helpers.ts      # Success/error result utilities
├── error-handler.ts       # Centralized error handling
├── logger.ts              # Logging (stderr only, not stdout)
├── security-logger.ts     # Security event logging
└── elicitation.ts         # User prompt utilities
```

## WHERE TO LOOK
| Utility | File | Purpose |
|---------|------|---------|
| Path Safety | `path-security.ts` | Prevent `../` traversal attacks |
| Normalization | `normalize.ts` | Force `/Game/` prefix, forward slashes |
| Validation | `validation.ts` | `sanitizePathSafe()`, Zod schemas |
| Command Safety | `command-validator.ts` | Block dangerous console commands |
| Throttling | `unreal-command-queue.ts` | Rate-limited UE execution |

## CONVENTIONS
- **Path First**: Always call `sanitizePathSafe()` on user-provided paths
- **Zero-Any**: Use `unknown` + type guards, never `as any`
- **Pure Functions**: Utilities should be side-effect free (except logger, queue)
- **Explicit Errors**: Use `sanitizePathSafe()` over deprecated `sanitizePath()`

## ANTI-PATTERNS
- **Direct FS Access**: Never use `fs` without path validation
- **Hardcoded Strings**: Use `constants.ts` for shared values
- **Silent Failures**: Always return explicit error results
- **Console.log**: Use `logger` which writes to stderr
