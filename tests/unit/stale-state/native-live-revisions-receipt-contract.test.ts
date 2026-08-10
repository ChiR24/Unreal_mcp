import { readFileSync } from 'node:fs';

import { describe, expect, it } from 'vitest';

const source = readFileSync(
  `${process.cwd()}/plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Execute/McpNativeGatewayReceipt.cpp`,
  'utf8',
);

function body(name: string, nextName: string): string {
  const start = source.indexOf(name);
  const end = source.indexOf(nextName, start + name.length);
  if (start < 0 || end < 0) {
    throw new Error(`Unable to isolate ${name}`);
  }
  return source.slice(start, end);
}

describe('Task 42 native live revision receipts', () => {
  it('attaches one snapshot to both outer and canonical error receipts', () => {
    const errorBuilder = body('TSharedPtr<FJsonObject> McpBuildErrorReceipt(', 'TSharedPtr<FJsonObject> McpBuildSuccessReceipt(');

    expect(errorBuilder.match(/TEXT\("liveRevisions"\)/gu) ?? []).toHaveLength(2);
    expect(errorBuilder).toContain('FMcpLiveStateRevisions::Get().Snapshot()');
  });

  it('attaches one snapshot to both outer and canonical success receipts', () => {
    const successBuilder = body('TSharedPtr<FJsonObject> McpBuildSuccessReceipt(', 'FString McpReceiptMessage(');

    expect(successBuilder.match(/TEXT\("liveRevisions"\)/gu) ?? []).toHaveLength(2);
    expect(successBuilder).toContain('FMcpLiveStateRevisions::Get().Snapshot()');
  });
});
