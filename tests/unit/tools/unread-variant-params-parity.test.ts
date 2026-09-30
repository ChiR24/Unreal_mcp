// Parity gate for the unread-variant-parameter warning. unreadVariantParams in
// src/server/gateway/gateway-dispatch-by.ts and McpUnreadVariantParams in
// plugins/.../Private/MCP/Execute/McpNativeGatewayValidation.cpp must word the warning identically and
// read the caller's own parameter names (before declared defaults), and the native warning must travel
// from validation through the SSE connection into the receipt. The native side has no unit harness, so
// its source is read as text.
import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';

const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), '../../..');
const MCP = 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP';
const read = (file: string): string => readFileSync(resolve(repoRoot, file), 'utf8').replace(/\r\n/gu, '\n');

describe('a parameter only other variants read: both doors warn the same way', () => {
  it('words the warning identically', () => {
    expect(read('src/server/gateway/gateway-dispatch-by.ts'))
      .toContain("`${name} is read only when ${dispatchBy.param} is ${owners.join(' or ')}; this ${dispatchBy.param}=${selected} call did not use it.`");
    expect(read(`${MCP}/Execute/McpNativeGatewayValidation.cpp`))
      .toContain('TEXT("%s is read only when %s is %s; this %s=%s call did not use it.")');
  });

  it('reads the names the caller sent, before declared defaults fill any in', () => {
    const validation = read(`${MCP}/Execute/McpNativeGatewayValidation.cpp`);
    const sent = validation.indexOf('SentNames.Add(FString(*Pair.Key));');
    expect(sent).toBeGreaterThan(-1);
    expect(sent).toBeLessThan(validation.indexOf('McpApplyCanonicalSchemaDefaults(Request.Params, InputSchema)'));
    expect(validation).toContain('OutPlan.Warnings = McpUnreadVariantParams(*Request.Record, SentNames, WithDefaults);');
    expect(read('src/server/gateway/gateway-execute-static-check.ts'))
      .toContain('unread: unreadVariantParams(record, Object.keys(pins.params), withDefaults)');
  });

  it('carries the native warning from validation into the receipt', () => {
    expect(read(`${MCP}/Execute/McpNativeTransportGatewayExecute.cpp`)).toContain('Context.GatewayWarnings = Plan.Warnings;');
    expect(read(`${MCP}/Transport/McpNativeTransportGatewayStream.cpp`)).toContain('Conn->GatewayWarnings = Context.GatewayWarnings;');
    expect(read(`${MCP}/Transport/McpNativeTransportPendingRequests.cpp`)).toContain('Context.GatewayWarnings = Conn->GatewayWarnings;');
    expect(read(`${MCP}/Execute/McpNativeReceiptEnrichment.cpp`)).toContain('for (const FString& Warning : Context.GatewayWarnings)');
  });
});
