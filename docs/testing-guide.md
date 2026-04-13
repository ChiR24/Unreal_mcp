# Testing Guide

## Overview

This repo now uses one consolidated live integration entrypoint in `tests/integration.mjs`, a shared harness in `tests/test-runner.mjs`, focused Vitest files for public-contract coverage, and a mock-mode smoke test for packaged discovery checks.

The integration runner supports targeted reruns through `UNREAL_MCP_INTEGRATION_SUITE`, which keeps operator-reliability proofs small and deterministic. For the current screenshot, recovery, and dense-review contract, the most important focused suites are `ui-targeting` and `graph-review`.

## Test Commands

| Command                                                                                                                                                                                                     | Description                                                                 | Requires UE? |
| ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------- | ------------ |
| `npm test`                                                                                                                                                                                                  | Run the full consolidated live integration suite in `tests/integration.mjs` | Yes          |
| `node tests/integration.mjs`                                                                                                                                                                                | Same full live suite, without the npm wrapper                               | Yes          |
| `npm run test:unit`                                                                                                                                                                                         | Run all Vitest unit suites                                                  | No           |
| `npx vitest run tests/unit/tools/editor_contract.test.ts tests/unit/tools/ui_handlers.test.ts tests/unit/tools/control-editor-navigation.test.ts`                                                           | Run the focused screenshot/recovery contract bundle                         | No           |
| `npx vitest run src/tools/consolidated-tool-inspection-contract.test.ts tests/unit/tools/blueprint_handlers.test.ts tests/unit/tools/manage_pipeline_contract.test.ts src/utils/response-validator.test.ts` | Run the focused graph-review follow-up contract bundle                      | No           |
| `npm run type-check`                                                                                                                                                                                        | TypeScript typecheck for the server layer                                   | No           |
| `npm run build`                                                                                                                                                                                             | Rebuild `dist/` for live integration runs                                   | No           |
| `npm run test:smoke`                                                                                                                                                                                        | Mock-mode packaged-surface validation                                       | No           |

## Live Integration Suites

### Prerequisites

1. Run the `UE_AutomationMCP` Unreal Editor project with the `McpAutomationBridge` plugin enabled.
2. Confirm the automation bridge is listening on `127.0.0.1:8091`. The test runner will probe `8091` first and then `8090` as an explicit compatibility fallback.
3. Re-resolve editor targets before live screenshot or input steps when the editor layout changes.

Optional environment overrides:

```bash
MCP_AUTOMATION_HOST=127.0.0.1
MCP_AUTOMATION_PORT=8091
MCP_AUTOMATION_WS_PORTS=8091,8090
```

### Run Focused Suites

PowerShell examples:

```powershell
$env:UNREAL_MCP_INTEGRATION_SUITE='ui-targeting'
node tests/integration.mjs
Remove-Item Env:UNREAL_MCP_INTEGRATION_SUITE -ErrorAction SilentlyContinue

$env:UNREAL_MCP_INTEGRATION_SUITE='graph-review'
node tests/integration.mjs
Remove-Item Env:UNREAL_MCP_INTEGRATION_SUITE -ErrorAction SilentlyContinue
```

These focused suites currently prove:

- `ui-targeting`: `manage_ui.resolve_ui_target`, `control_editor.focus_editor_surface`, targeted editor screenshots with `includeMenus` and `includedMenuWindowCount` diagnostics, and the `AMBIGUOUS_CAPTURE_TARGET` path when editor capture is retried with only `tabId`.
- `graph-review`: readable `capture_blueprint_graph_review` capture with `scope: neighborhood` plus bounded `get_graph_review_summary` follow-up that reuses `reviewTargets[].nodeId` and returns `focusedReviewTarget` context on helper graphs.
- Other supported focused suites in `tests/integration.mjs` include `public-inspection`, `targeted-window-input`, `semantic-navigation`, `public-surface-validation`, `designer-marquee`, `designer-selection`, `designer-geometry-readback`, `designer-rectangle-selection`, `ui-target-policy`, `graph-batching`, `widget-bindings`, and `capability-honesty`.

### Latest Focused Evidence

- `tests/reports/ui-targeting-test-results-2026-04-13T16-42-54.147Z.json` — passed `8/8`
- `tests/reports/graph-review-test-results-2026-04-13T18-48-32.509Z.json` — passed `7/7`

Reports are written to `tests/reports/` with timestamped filenames. Fresh reruns will create newer files alongside these examples.

### Dense Review Request Examples

Readable neighborhood capture:

```json
{
  "action": "capture_blueprint_graph_review",
  "assetPath": "/Game/IntegrationTest/BP_SemanticNavigation",
  "graphName": "ReviewFunction",
  "nodeGuid": "<matched node guid>",
  "scope": "neighborhood",
  "filename": "graph-review-blueprint.png"
}
```

Focused bounded follow-up using the first-pass summary:

```json
{
  "action": "get_graph_review_summary",
  "blueprintPath": "/Game/IntegrationTest/BP_SemanticNavigation",
  "graphName": "ReviewFunction",
  "nodeId": "<reviewTargets[0].nodeId>"
}
```

## Unit Tests

```bash
npm run test:unit
npm run test:unit:watch
npm run test:unit:coverage
```

For the screenshot and recovery contract, the most relevant focused Vitest files are:

- `tests/unit/tools/editor_contract.test.ts`
- `tests/unit/tools/ui_handlers.test.ts`
- `tests/unit/tools/control-editor-navigation.test.ts`

These suites pin the public field contract for `resolve_ui_target`, `focus_editor_surface`, `screenshot`, and graph-review capture behavior without requiring a running editor.

## Smoke Test

```bash
MOCK_UNREAL_CONNECTION=true npm run test:smoke
```

Use this in CI to verify startup and packaged tool discovery without connecting to a live Unreal session.

## Adding New Live Tests

Add or extend a focused suite function in `tests/integration.mjs`, use `TestRunner` plus `runner.addStep(...)`, and register the suite in the `UNREAL_MCP_INTEGRATION_SUITE` dispatch block near the end of the file.

Keep focused suites small and behavior-scoped. Prefer extending an existing suite such as `ui-targeting` or `graph-review` over creating a new taxonomy when the behavior already belongs to one of the shipped contract slices.

## Troubleshooting

### Automation Bridge Unavailable

- Make sure the `UE_AutomationMCP` editor session is running.
- Confirm the plugin is enabled and listening on `8091` or the explicit `8090` fallback.
- If the editor is open but the runner still fails, restart the editor session and rerun the focused suite instead of the full suite first.

### Screenshot Ambiguity Failures

- Use `manage_ui.resolve_ui_target` before retrying editor-window capture.
- Use `control_editor.focus_editor_surface` when keyboard or text input depends on a deliberate graph or Designer focus change.
- For deterministic editor screenshots, pass a live `windowTitle`. `tabId` alone is diagnostic context for ambiguity on `control_editor.screenshot`, not a direct editor-window capture selector.
- Check `includeMenus` and `includedMenuWindowCount` on successful editor-window captures when you need to verify whether popup or menu surfaces were intentionally composed into the screenshot.
- Check `captureIntentWarning`, `suggestedPreflightAction`, `targetStatus`, `requestedTargetStillLive`, and `reResolved` in the structured error payload.

### Graph Review Failures

- Verify the named helper graph is openable through the semantic navigation step before investigating capture.
- Treat `capture_blueprint_graph_review` as a visible-editor-window workflow; re-focus the asset editor if the capture precondition fails.
- For dense review follow-up, inspect `reviewTargets` first and then reuse one returned `nodeId` for the bounded `focusedReviewTarget` path instead of reaching for raw node batches immediately.

## Exit Codes

- `0` — all requested tests passed
- `1` — one or more requested tests failed
