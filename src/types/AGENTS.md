# src/types

Two shared compile-time contracts. Tool schemas and action names are not here: they come from the capability records under `src/tools/catalog/capabilities/records/`.

## FILES
- `tools/tool-interfaces.ts` — `AutomationRequestBridge` (`isConnected`, `sendAutomationRequest(tool, payload, options)`), `AutomationStatusBridge`, and `ITools = { automationBridge? }`, the one dependency the gateway hands to `executeAutomationRequest()`.
- `automation/automation-responses.ts` — `AutomationResponse`, the loose bridge reply shape (`success`, `message`, `error`, `errorCode`, `result`, ...).

## CONVENTIONS
- `import type` / `export type` only; no runtime code here.
- Types do not replace validation: narrow bridge replies at runtime.

## ANTI-PATTERNS
- Duplicating action enums or JSON schemas in type declarations.
- `any` to silence a contract mismatch.
