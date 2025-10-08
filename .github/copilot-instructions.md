# Unreal MCP – AI Agent Guide

## Architecture Overview
- MCP server (`src/index.ts`) boots with consolidated tool routing (`src/tools/consolidated-tool-definitions.ts`, `src/tools/consolidated-tool-handlers.ts`) to 14 multi-action tools wrapping specialized classes in `src/tools/*`.
- Unreal bridge (`src/unreal-bridge.ts`) manages the automation bridge connection, command queuing, and safety logic.
- On-demand connection via `ensureConnectedOnDemand()`; resources (`src/resources/`) provide cached listings; utils (`src/utils/`) handle validation and Python helpers.

## Key Patterns
- **Tool Wiring**: Declare schemas in definitions, wire handlers with dispatch switch; delegate to `src/tools/*` classes.
- **Python Execution**: Use `bridge.executePythonWithResult()` for scripts printing `RESULT:` JSON; sanitize with `escapePythonString()` from `src/utils/python-helpers.ts`.
- **Response Format**: Return plain JS `{ success, message, error, warnings }`; validate with AJV schemas; wrap via `responseValidator.wrapResponse()`.
- **Validation**: Reuse `ensureVector3`, `ensureRotation` from `src/utils/validation.ts`; elicit missing args with `elicitMissingPrimitiveArgs()`.

## Workflows
- **Build**: `npm run build` compiles TypeScript to `dist/`.
- **Test**: `npm run test` runs individual tool tests; harness via `node tests/run-unreal-tool-tests.mjs` for integration (uses Markdown cases, env overrides like `UNREAL_MCP_SERVER_CMD`).
- **Debug**: Logs to stderr; check `ue://health` resource for connectivity; command queue spaces heavy ops.

## Integration Points
- **Unreal Plugins**: Enable Automation Bridge, Remote Control API, Python Editor Script Plugin, Editor Scripting Utilities.
- **Environment**: Set `UE_HOST`, `UE_PROJECT_PATH`, and automation bridge settings like `MCP_AUTOMATION_WS_PORT`; add to `DefaultEngine.ini` for remote execution.
- **Transport**: Automation bridge handles all execution; Remote Control HTTP/WebSocket is no longer used.

## Debugging Tips
- UE_NOT_CONNECTED errors: Verify automation bridge and required plugins are enabled, ports open, project path set.
- Test failures: Check env vars, Unreal running; harness reports in `tests/reports/`.
- Python errors: Ensure plugins active; use `interpretStandardResult()` for output parsing.
