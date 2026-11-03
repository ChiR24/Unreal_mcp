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

  it('counts and pages a list a handler sends inside its data payload', () => {
    // The standard envelope ({success, data, warnings}) keeps the list in data: counted at the root alone, a find counted
    // the envelope's empty warnings, and a long one could not page.
    const build = read('Gateway', 'McpNativeGatewayExecuteReceiptBuild.cpp');
    const lift = build.indexOf('TSharedPtr<FJsonObject> Published = McpLiftReplyPayload(Result);');
    expect(lift).toBeGreaterThan(-1);
    expect(build.indexOf('McpExecuteAggregate::Apply(', lift)).toBeGreaterThan(lift);
    expect(build.indexOf('McpFirstResultPage(Published', lift)).toBeGreaterThan(lift);
  });

  it('is part of the idempotency fingerprint', () => {
    expect(read('Execute', 'McpNativeTransportGatewayExecute.cpp')).toContain('McpExecuteAggregate::FingerprintPart(ExecOptions)');
  });

  // Counting a level by label gave one group per actor (Hall_DuctBracket_1 .. _27): copies count by their stem.
  it('counts a label, name or actorName by its stem, dropping only a trailing _<number>', () => {
    const aggregate = read('Execute', 'Request', 'McpExecuteAggregate.cpp');
    expect(aggregate).toContain('return Cut > 1 && Cut < Text.Len() && Text[Cut - 1] == TEXT(\'_\') ? Text.Left(Cut - 1) : Text;');
    expect(aggregate).toContain('return GroupBy == TEXT("label") || GroupBy == TEXT("name") || GroupBy == TEXT("actorName") ? LabelStem(Text) : Text;');
  });
});
