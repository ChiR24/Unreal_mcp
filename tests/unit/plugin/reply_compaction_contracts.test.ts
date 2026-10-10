/**
 * The reply a client reads is compacted on each transport by its own code (McpJsonRpcReply*.cpp natively,
 * gateway-reply-compaction.ts over stdio); these keep the two rule lists equal.
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';
import {
  LOG_ONLY_EXECUTE_FIELDS,
  RECEIPT_OUTCOME_LISTS,
  RESOLVED_IDENTITY_FIELDS,
  STOCK_OUTPUT_PROPERTIES
} from '../../../src/utils/responses/gateway-reply-compaction.js';

const protocol = (file: string): string => readFileSync(
  resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Protocol', file),
  'utf8'
);

/** The TEXT("...") entries of one C++ array literal. */
const listed = (source: string, name: string): string[] => {
  const start = source.indexOf(`${name}[]`);
  return [...source.slice(start, source.indexOf('};', start)).matchAll(/TEXT\("([^"]+)"\)/gu)].map((match) => match[1] ?? '');
};

describe('reply compaction on both transports', () => {
  it('drops the same log-only execute fields and keeps the same receipt outcome', () => {
    const execute = protocol('McpJsonRpcReplyCompaction.cpp');
    expect(listed(execute, 'LogOnlyExecuteFields')).toEqual([...LOG_ONLY_EXECUTE_FIELDS]);
    expect(listed(execute, 'ReceiptOutcomeLists')).toEqual([...RECEIPT_OUTCOME_LISTS]);
    expect(listed(execute, 'ResolvedIdentityFields')).toEqual([...RESOLVED_IDENTITY_FIELDS]);
  });

  it('carries the names a call used onto every native receipt, queued or not', () => {
    const execute = (file: string): string => readFileSync(
      resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP', file), 'utf8');
    expect(execute('Execute/McpNativeGatewayExecuteRequest.cpp')).toContain('SetObjectField(TEXT("migratedFrom"), Legacy)');
    expect(execute('Execute/McpNativeGatewayValidation.cpp')).toContain('Context.Provenance = Request.Provenance;');
    expect(execute('Transport/McpNativeTransportGatewayStream.cpp')).toContain('Conn->Provenance = Context.Provenance;');
    expect(execute('Transport/McpNativeTransportPendingRequests.cpp')).toContain('Context.Provenance = Conn->Provenance;');
    expect(execute('Transport/McpNativeTransportKeepalive.cpp')).toContain('Context.Provenance = Conn->Provenance;');
  });

  it('treats the same output properties as stock', () => {
    expect(listed(protocol('McpJsonRpcReplyDiscovery.cpp'), 'StockOutputProperties'))
      .toEqual(Object.entries(STOCK_OUTPUT_PROPERTIES).flat());
  });

  it('compacts the text and structuredContent a client reads, and takes images from the whole reply', () => {
    const rpc = protocol('McpJsonRpc.cpp');
    expect(rpc).toContain('MakeToolTextData(MakeCompactReply(Data))');
    expect(rpc).toContain('SetObjectField(TEXT("structuredContent"), Shown)');
    expect(rpc).toContain('AddImageContentIfPresent(Data, Content)');
  });
});
