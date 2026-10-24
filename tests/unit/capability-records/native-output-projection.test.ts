/**
 * tests/unit/capability-records/native-output-projection.test.ts
 *
 * A handler result is projected onto the capability's DECLARED output fields
 * before it is validated and published (McpProjectCanonicalOutput). Validating
 * the raw result against a closed output schema turned correct payloads
 * (compiled, saved, scsVerification, ...) into OUTPUT_SCHEMA_VIOLATION.
 * Undeclared fields fold into `details`, but the message stays on the receipt.
 */
import { readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, expect, it } from 'vitest';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../../..');
const PLUGIN = 'plugins/EngineWire/Source/EngineWire/Private';
const read = (relativePath: string): string => readFileSync(resolve(REPO_ROOT, relativePath), 'utf8');

describe('the details fold leaves the message and the expectations to the receipt', () => {
  it('skips liveRevisions, the message, the expectations and every declared field', () => {
    expect(read(`${PLUGIN}/MCP/Gateway/McpNativeGatewayOutputProjection.cpp`).replace(/\s+/gu, ' '))
      .toContain('Entry.Key == TEXT("liveRevisions") || Entry.Key == TEXT("message") || Entry.Key == TEXT("expectations") || (*Properties)->HasField(Entry.Key)');
  });
});

describe('the native seam projects the output', () => {
  it('defines McpProjectCanonicalOutput and publishes the projected output, never the raw Result', () => {
    expect(read(`${PLUGIN}/MCP/Execute/McpNativeGatewayValidation.cpp`)).toContain('McpProjectCanonicalOutput');
    expect(read(`${PLUGIN}/MCP/Execute/McpNativeGatewayValidation.h`)).toContain('McpProjectCanonicalOutput');
    const seam = read(`${PLUGIN}/MCP/Gateway/McpNativeGatewayExecuteReceiptBuild.cpp`);
    expect(seam).toContain('McpProjectCanonicalOutput');
    expect(seam).toMatch(/McpBuildSuccessReceipt\(\s*(?:Conn\.)?CapabilityId,\s*Canonical/);
    expect(seam).not.toMatch(/McpBuildSuccessReceipt\(\s*(?:Conn\.)?CapabilityId,\s*Result\b/);
  });
});
