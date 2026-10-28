// options.aggregate counts a list in the plugin instead of sending it (Execute/Request/McpExecuteAggregate.cpp). The
// option rides the receipt context from the request to the completion: the completion rebuilds that context from the
// pending connection, so a field the connection does not carry is silently lost (it was, the first time).

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const MCP = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'MCP');
const read = (...segments: readonly string[]): string => readFileSync(join(MCP, ...segments), 'utf8').replace(/\r\n/gu, '\n');

describe('options.aggregate', () => {
  it('is an execute option, validated with the others and declared on the unreal tool', () => {
    const request = read('Execute', 'McpNativeGatewayExecuteRequest.cpp');
    expect(request).toContain('TEXT("aggregate"),');
    expect(request).toContain('McpExecuteAggregate::Validate(Options, OutError)');
    expect(read('Gateway', 'McpNativeGatewayDefinition.cpp')).toContain('.ClosedObject(TEXT("aggregate"),');
  });

  it('rides the receipt context from the request through the pending connection to the completion', () => {
    expect(read('Execute', 'McpNativeTransportGatewayExecute.cpp')).toContain('Context.Aggregate = (*Options)->TryGetObjectField(TEXT("aggregate"), Aggregate)');
    expect(read('Transport', 'McpNativeTransportGatewayStream.cpp')).toContain('Conn->Aggregate = Context.Aggregate;');
    expect(read('Transport', 'McpNativeTransportPendingRequests.cpp')).toContain('Context.Aggregate = Conn->Aggregate;');
  });

  it('counts before the size check, and the receipt carries the block', () => {
    const build = read('Gateway', 'McpNativeGatewayExecuteReceiptBuild.cpp');
    const apply = build.indexOf('McpExecuteAggregate::Apply(Context.Aggregate, OutputSchema, Published)');
    expect(apply).toBeGreaterThan(-1);
    expect(build.indexOf('McpSerializedResultExceeds(Published', apply)).toBeGreaterThan(apply);
    expect(build).toContain('Receipt->SetObjectField(TEXT("aggregate"), AggregateBlock);');
  });

  it('is part of the idempotency fingerprint', () => {
    expect(read('Execute', 'McpNativeTransportGatewayExecute.cpp')).toContain('McpExecuteAggregate::FingerprintPart(ExecOptions)');
  });
});
