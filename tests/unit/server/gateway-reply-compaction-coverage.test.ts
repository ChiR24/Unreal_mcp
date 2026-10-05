// Every reply the execute pipeline builds, run through the compaction a client reads: nothing may go but the
// bookkeeping only logs read, and the echo of names the caller itself sent.

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import {
  executeErrorEnvelope,
  executeSuccessEnvelope,
  type ExecuteFailure
} from '../../../src/server/gateway/gateway-execute-envelope.js';
import type { GatewayReceiptContext } from '../../../src/server/gateway/gateway-receipt-context.js';
import {
  CorrelationIdSchema,
  IdempotencyKeySchema,
  RequestIdSchema
} from '../../../src/tools/catalog/capabilities/semantic/ids.js';
import { compactGatewayReply } from '../../../src/utils/responses/gateway-reply-compaction.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

const record = capabilityIndex().byId.get('asset.list');
if (record === undefined) throw new Error('asset.list fixture is absent');
const context: GatewayReceiptContext = {
  correlationId: CorrelationIdSchema.parse('gw-coverage'),
  requestId: RequestIdSchema.parse('num:7'),
  idempotencyId: IdempotencyKeySchema.parse('key-7'),
  startedAt: Date.now() - 5
};

// Written out, not imported: a useful field added to the compaction's drop list must fail here.
const TOP_BOOKKEEPING = new Set<string>([
  'capabilityId', 'capability', 'tool', 'action', 'status', 'options', 'catalogRevision', 'capabilityRevision',
  'schemaRevision', 'correlationId', 'replayedFrom', 'liveRevisions', 'migratedFrom'
]);
const RECEIPT_BOOKKEEPING = new Set<string>([
  'status', 'capabilityId', 'correlationId', 'requestId', 'idempotencyId', 'catalogRevision', 'capabilityRevision',
  'schemaRevision', 'timingMs', 'validation', 'dataDigest', 'liveRevisions'
]);

type Pair = { readonly path: string; readonly key: string; readonly value: unknown };

/** Leaf key/value pairs, with lists of scalars kept whole; empty lists and objects say nothing. */
function pairs(value: unknown, path: string, key: string, out: Pair[]): Pair[] {
  if (Array.isArray(value)) {
    if (value.length === 0) return out;
    if (value.every((item) => !isRecord(item) && !Array.isArray(item))) {
      out.push({ path, key, value });
      return out;
    }
    value.forEach((item, index) => pairs(item, `${path}[${index}]`, key, out));
    return out;
  }
  if (isRecord(value)) {
    for (const [childKey, child] of Object.entries(value)) {
      const skip = (path === '' && TOP_BOOKKEEPING.has(childKey)) || (path === 'receipt' && RECEIPT_BOOKKEEPING.has(childKey));
      if (!skip) pairs(child, path === '' ? childKey : `${path}.${childKey}`, childKey, out);
    }
    return out;
  }
  if (value !== undefined) out.push({ path, key, value });
  return out;
}

/** What the full reply said that the compacted one no longer says anywhere. */
function lost(full: Record<string, unknown>, compact: (reply: unknown) => unknown = compactGatewayReply): string[] {
  const shown = pairs(compact(full), '', '', []);
  const same = (left: unknown, right: unknown): boolean => JSON.stringify(left) === JSON.stringify(right);
  return pairs(full, '', '', [])
    .filter(({ key, value }) => !shown.some((kept) =>
      same(kept.value, value) && (kept.key === key || typeof value === 'string')))
    .map(({ path, value }) => `${path}=${JSON.stringify(value)}`);
}

const FAILURES: ReadonlyArray<ExecuteFailure> = [
  { errorCode: 'STALE_STATE', message: 'The level moved on.', currentRevision: '7', expectedRevision: '5' },
  { errorCode: 'UNSUPPORTED_OPTION', message: 'options.bogus is not an execution option.', option: 'bogus' },
  { errorCode: 'OUT_OF_RANGE', message: 'options.timeoutMs must be 1..600000.', field: 'timeoutMs' },
  { errorCode: 'NOT_CONNECTED', message: 'Unreal Editor is not connected.' },
  { errorCode: 'DISPATCH_ERROR', message: 'The bridge dropped the request.' },
  { errorCode: 'CAPABILITY_DISABLED', message: 'manage_asset is disabled.' },
  { errorCode: 'CAPABILITY_UNAVAILABLE', message: 'A plugin this needs is off.' },
  { errorCode: 'IDEMPOTENCY_CONFLICT', message: 'This idempotency key is already executing.' },
  { errorCode: 'FORM_CONFLICT', message: "capability 'a' conflicts with tool/action 'b'." },
  { errorCode: 'OUTPUT_SCHEMA_VIOLATION', message: 'count must be a number.', pointer: '/count' },
  { errorCode: 'RESULT_TOO_LARGE', message: 'The result is 900000 characters.', resultChars: 900000 },
  { errorCode: 'SCOPE_NOT_GRANTED', message: 'write is not granted.', requiredScope: 'write', grantedScopes: ['read'] },
  { errorCode: 'CONSENT_REQUIRED', message: 'Pass the consent grant describe returned.', requiredScope: 'destructive' },
  {
    errorCode: 'UNDECLARED_PARAMETER', message: "Undeclared parameter 'bogus'.", pointer: '/bogus',
    suggestions: ['path'], allowedParameters: ['path', 'recursive'],
    nextCall: { operation: 'describe', tool: 'manage_asset', action: 'list' }
  },
  {
    errorCode: 'ASSET_NOT_FOUND', message: 'No asset at /Game/Missing.', handlerCode: 'ASSET_NOT_FOUND',
    detail: { success: false, error: 'No asset at /Game/Missing.', missing: ['/Game/Missing'], partial: { listed: 3 } }
  },
  { errorCode: 'UNREAL_EXECUTION_ERROR', message: 'The result was withheld for size.', detailOmitted: true }
];

describe('compaction keeps every useful field of every execute reply', () => {
  it('reports a useful field that goes missing', () => {
    const full = executeErrorEnvelope({ ...FAILURES[13] as ExecuteFailure, record }, context);
    const dropSuggestions = (reply: unknown) => {
      const rest = { ...(compactGatewayReply(reply) as Record<string, unknown>) };
      delete rest.suggestions;
      return rest;
    };
    expect(lost(full, dropSuggestions)).toEqual(['suggestions=["path"]']);
  });

  it.each(FAILURES.map((failure) => [failure.errorCode, failure] as const))('%s refusal', (_code, failure) => {
    expect(lost(executeErrorEnvelope({ ...failure, record }, context))).toEqual([]);
  });

  it('a refusal before any capability resolved', () => {
    const unknown = executeErrorEnvelope({
      errorCode: 'UNKNOWN_ACTION', message: "Unknown action 'lst' for tool 'manage_asset'.",
      requestedTool: 'manage_asset', requestedAction: 'lst', suggestions: ['list'], availableActions: ['list', 'delete'],
      nextCall: { operation: 'describe', tool: 'manage_asset', action: 'list' }
    }, context);
    expect(lost(unknown)).toEqual([]);
  });

  it('a success with results, warnings, changes, handles and a running task', () => {
    const result = {
      success: true, message: 'Listed 2 assets.', assetPath: '/Game/Props/SM_Crate', actorName: 'Crate_1',
      assets: [{ name: 'SM_Crate', path: '/Game/Props/SM_Crate' }], warnings: ['Two assets share a name.'],
      task: { taskId: 't1', state: 'running', progress: 0.4 },
      details: { worldName: '/Game/Maps/Demo', engineWarningCount: 4, warnings: ['Two assets share a name.'] }
    };
    const full = executeSuccessEnvelope({
      record, result, canonicalOutput: result, options: { idempotencyKey: 'key-7' },
      warnings: ['Two assets share a name.']
    }, context);
    expect(lost(full)).toEqual([]);
  });

  it('names what ran when the call used an alias or another action name, and not when it only echoes', () => {
    const run = (provenance: Record<string, unknown>) => compactGatewayReply(executeSuccessEnvelope({
      record, result: { success: true }, canonicalOutput: { success: true }, warnings: [], ...provenance
    }, context)) as Record<string, unknown>;
    expect(run({ resolvedFromAlias: 'asset.ls' })).toMatchObject({ capability: 'asset.list', tool: 'manage_asset', resolvedFromAlias: 'asset.ls' });
    expect(run({ migratedFrom: { tool: 'manage_asset', action: 'list_assets' } }))
      .toMatchObject({ tool: 'manage_asset', migratedFrom: { tool: 'manage_asset', action: 'list_assets' } });
    const echo = run({ migratedFrom: { tool: record.routing.parentTool, action: record.legacyIds[0]?.action ?? '' } });
    expect(Object.keys(echo).filter((key) => ['capability', 'tool', 'action', 'migratedFrom'].includes(key))).toEqual([]);
  });
});
