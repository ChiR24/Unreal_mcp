# Unreal MCP – AI Agent Guide

## Architecture essentials
- `src/index.ts` boots the Model Context Protocol server, registers output schemas, and connects to Unreal only when a tool/resource call requires it via `ensureConnectedOnDemand`.
- Consolidated tool routing lives in `src/tools/consolidated-tool-definitions.ts` and `src/tools/consolidated-tool-handlers.ts`, shrinking the surface to 13 multi-action tools that wrap the specialized classes in `src/tools/*`.
- The Unreal bridge (`src/unreal-bridge.ts`) owns HTTP/WS transport, command throttling, health checks, and graceful fallbacks so callers never talk to Remote Control directly.
- Health/metrics (connection status, latency, recent errors) are tracked in `index.ts` and exposed through the `ue://health` resource for quick diagnostics.

## Key directories
- `src/tools/` – domain-specific helpers (actors, landscapes, audio, niagara, etc.) that emit Python scripts or console commands.
- `src/resources/` – read-only listings (assets, actors, levels) with caching and path normalization utilities.
- `src/utils/` – shared helpers: validation/coercion, `escapePythonString`, result interpretation, AJV-powered response validation, stdout redirection.
- `docs/unreal-tool-test-cases.md` + `tests/run-unreal-tool-tests.mjs` – declarative test matrix consumed by the automated harness; reports land in `tests/reports/` with time-stamped JSON.

## Tool workflow expectations
- New tool actions must be declared in the consolidated definitions (input/output schema) and wired in the handler switch before delegating to the relevant class in `src/tools/*`.
- Always return plain JS objects with `success`, `message`, `error`, and optional `warnings` arrays; let `responseValidator.wrapResponse` handle MCP formatting.
- Shared helpers like `interpretStandardResult` and `cleanObject` keep payloads JSON-safe—use them instead of ad-hoc parsing.
- When validating inputs, reuse `ensureVector3`, `ensureRotation`, and other utilities from `src/utils/validation.ts` to keep error messaging consistent.

## Unreal bridge & Python usage
- Prefer `bridge.executePythonWithResult` for multi-line scripts; it captures stdout, parses the last `RESULT:` block, and falls back to the console `py` command when plugins are missing.
- Python snippets should print a single `RESULT:` JSON payload and sanitize inputs with `escapePythonString` or typed coercion helpers.
- `UnrealBridge.httpCall` enforces timeouts, queues commands, and blocks dangerous console strings (`buildpaths`, `rebuildnavigation`, etc.); avoid bypassing it with raw HTTP.
- Auto-reconnect is disabled by default—tool handlers should call `ensureConnectedOnDemand()` instead of assuming a live session.

## Response & validation conventions
- Every consolidated tool has an AJV schema; mismatches surface as `_validation` warnings in responses and appear in stderr logs.
- For Python-driven tools, use the `interpretResult`/`interpretStandardResult` helpers to turn raw bridge output into the normalized `{ success, message, error, warnings }` shape.
- Keep warning text short and user-facing—the test harness searches response strings for keywords to grade scenarios.

## Build & test workflow
- `npm run build` compiles TypeScript to `dist/`; `npm run lint` enforces the ESLint rules shipped in `.eslintrc.json`.
- `npm run test:tools` launches the compiled server via stdio, iterates the Markdown-defined cases, and writes a JSON run summary under `tests/reports/`.
- Override harness behavior with env vars like `UNREAL_MCP_SERVER_CMD`, `UNREAL_MCP_SERVER_ARGS`, `UNREAL_MCP_TEST_DOC`, or `UNREAL_MCP_FBX_FILE` when Unreal lives elsewhere.

## Unreal environment & configuration
- Ensure the project enables: Remote Control API, Remote Control Web Interface, Python Editor Script Plugin, Editor Scripting Utilities, Sequencer, and Level Sequence Editor before invoking automation.
- Default connection settings come from `UE_HOST`, `UE_RC_HTTP_PORT`, and `UE_RC_WS_PORT` (see README for the `DefaultEngine.ini` snippet that unlocks remote execution).
- Tools defensively return `UE_NOT_CONNECTED` or asset-not-found errors; keep that contract when extending behavior so clients can retry intelligently.

## Debugging & monitoring
- Logs are routed to stderr via `routeStdoutLogsToStderr()` to keep MCP stdout JSON-only—check the terminal for detailed stack traces or validation warnings.
- Use the `ue://health` and `ue://version` resources to confirm bridge connectivity, last health-check timestamp, and detected engine version.
- The command queue in `UnrealBridge` spaces out console/Python traffic; heavy operations may need explicit `__callTimeoutMs` in the payload to extend HTTP timeouts.
