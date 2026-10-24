/**
 * The reply a client reads is compacted by McpJsonRpcReply*.cpp. These keep the names a call used on every receipt
 * and the compaction on what a client actually reads.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const mcp = (file: string): string => readFileSync(
  resolve(process.cwd(), 'plugins/EngineWire/Source/EngineWire/Private/MCP', file),
  'utf8'
);

describe('reply compaction', () => {
  it('carries the names a call used onto every native receipt, queued or not', () => {
    expect(mcp('Execute/McpNativeGatewayExecuteRequest.cpp')).toContain('SetObjectField(TEXT("migratedFrom"), Legacy)');
    expect(mcp('Execute/McpNativeGatewayValidation.cpp')).toContain('Context.Provenance = Request.Provenance;');
    expect(mcp('Transport/McpNativeTransportGatewayStream.cpp')).toContain('Conn->Provenance = Context.Provenance;');
    expect(mcp('Transport/McpNativeTransportPendingRequests.cpp')).toContain('Context.Provenance = Conn->Provenance;');
    expect(mcp('Transport/McpNativeTransportKeepalive.cpp')).toContain('Context.Provenance = Conn->Provenance;');
  });

  it('compacts the text and structuredContent a client reads, and takes images from the whole reply', () => {
    const rpc = mcp('Protocol/McpJsonRpc.cpp');
    expect(rpc).toContain('MakeToolTextData(MakeCompactReply(Data))');
    expect(rpc).toContain('SetObjectField(TEXT("structuredContent"), Shown)');
    expect(rpc).toContain('AddImageContentIfPresent(Data, Content)');
  });
});
