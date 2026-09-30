# src/server/gateway/ — GATEWAY ROUTING ENGINE

24 source modules (+ 3 colocated tests) own search/describe/execute/configure for the `unreal` gateway tool. They decide what to call and how to shape the request; the plugin does the work.

`src/gateway/` (sibling) is only the generated manifest data + loader. Never route through it at runtime.

## STRUCTURE

Entry: `src/server/tool-registry-gateway.ts` → `handleUnrealGatewayCall()` switches search/describe/execute/configure (`configure` wraps `handleManageToolsCall()`).

| File | Owns |
|------|------|
| `gateway-shared.ts` | `getString` `getBoundedInteger` `gatewayError` `isGatewayFailure` `findTool` `allToolNames` `nextGatewayCorrelationId` |
| `gateway-search.ts`, `gateway-search-filters.ts` | `searchGatewayCapabilities()`; filters, candidate selection, cursor paging |
| `gateway-describe.ts`, `-browse.ts`, `-capability.ts` | describe level router; domain/family browse; one contract / one parameter |
| `gateway-capability-view.ts`, `gateway-capability-index.ts`, `gateway-availability.ts` | contract projection; id/legacy-pair index and catalog revision; availability |
| `gateway-execute.ts` | `executeGatewayCall()` stage order |
| `gateway-execute-resolve.ts`, `gateway-execute-lookup.ts` | request forms and index; `resolveExecuteTarget()` (capability id or `{tool, action}`, aliases; both forms must agree) |
| `gateway-execute-static-check.ts` | `checkStaticRequest()`: enabled, options, defaults, schema |
| `gateway-option-validate.ts` | execution-option rules (keys, timeout bounds, idempotency key, `expectedRevisions`) |
| `gateway-schema-validate.ts` | Draft-2020-12 subset validator: fail-closed on unknown keywords, `UNDECLARED_PARAMETER`, declared defaults |
| `gateway-execute-policy.ts` | catalog-revision, scope and consent checks |
| `gateway-dispatch-by.ts` | folded families: `applyFoldedPins()` and `inferSelector()` before validation, `resolveDispatchAction()` after; `unreadVariantParams()` warns about a sent param only other variants read |
| `gateway-execute-dispatch.ts` | `runCapability()` → `handleManageToolsCall` (manage_tools) or `executeAutomationRequest(tools, parentTool, { ...params, action }, controls)`; output held to the declared schema |
| `gateway-execute-idempotency.ts`, `idempotency-ledger.ts` | principal-scoped ledger (cap 1024; native mirror cap 4096 — change both) |
| `gateway-execute-envelope.ts`, `gateway-receipt-context.ts` | success/error envelopes, receipts |
| `direct-call-migration.ts` | `DIRECT_TOOL_CALL_REMOVED` receipt for direct canonical-name calls |
| `gateway-guidance.ts` | `closestMatches()` + `buildNextCall()` |

## EXECUTE STAGE ORDER (preserve exactly)
1. `resolveExecuteTarget` (a folded old name resolves to its family record).
2. `applyFoldedPins` (caller values win), then `checkStaticRequest`.
3. `checkExpectedCatalogRevision`.
4. `context.ensureConnected()` → `NOT_CONNECTED` refusal when the editor is unreachable.
5. Scope, then consent authorization against the bridge authority. Consent rides as an envelope sibling and the plugin re-validates it.
6. `resolveDispatchAction` (old name → itself; primary → `routing.dispatchBy[selector]`), then `dispatchAndValidate`. Handler failures surface their own `errorCode`, else `UNREAL_EXECUTION_ERROR`.

## PROGRESSIVE DISCLOSURE
`describe` never dumps a full inputSchema: `{}` → domains, `{domain}` → families, `{family}` → capabilities, `{capability}` → one contract, `{capability, param}` → one parameter, `{tool}` → parent summary, `{tool, action}` → the capability behind the pair. Unknown names return `suggestions` + an executable `nextCall`; an empty search page returns a rephrase hint plus `nextCall: { operation: 'describe' }`.

## CONVENTIONS
- Native mirror: plugin `Private/MCP/Gateway/` (at the 25-file folder cap). Keep behaviour in sync by hand; there is no parity harness.
- The public surface is permanently the single `unreal` tool; there is no mode flag.

## ANTI-PATTERNS
- Sending to the bridge other than through `executeAutomationRequest()`.
- Editing `src/gateway/` manifest data from here.
- Dumping full `inputSchema` at the summary level.
