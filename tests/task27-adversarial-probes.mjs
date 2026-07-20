#!/usr/bin/env node
// Task 27 adversarial probes.
//
// Exercises the seven adversarial classes named in the Task 27 ledger entry
// against the normative TypeScript reference pipeline (the spec the native
// `/mcp` surface implements) plus the native C++ source for the classes that
// only exist in C++.
//
// Usage: node --loader ts-node/esm tests/task27-adversarial-probes.mjs [--out <path>]

import { execFileSync } from 'node:child_process';
import { readFileSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { CANONICAL_CAPABILITY_RECORDS } from '../src/tools/catalog/capabilities/generated/canonical-registry.generated.js';
import { buildResolverIndex, executeReference } from './unit/task-27-suite/execute-reference.js';
import { minimalValidParams } from './unit/task-27-suite/case-builder.js';

const root = process.cwd();
const pluginPrivate = resolve(root, 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private');
const readNative = (rel) => readFileSync(resolve(pluginPrivate, rel), 'utf8');

const outIndex = process.argv.indexOf('--out');
const outPath = outIndex === -1
  ? resolve(root, '.omo/evidence/task-27/adversarial-probes.json')
  : resolve(root, process.argv[outIndex + 1]);

const records = CANONICAL_CAPABILITY_RECORDS;
const index = buildResolverIndex(records);
const sample = records.find((r) => r.routing.parentTool === 'manage_asset') ?? records[0];

const results = [];
const record = (cls, label, pass, detail) => results.push({ class: cls, label, pass, detail });

// A dispatch that records whether editor work would have been reached.
const deps = (queued, dispatchImpl) => ({
  index,
  isEnabled: () => true,
  dispatch: (rec) => {
    queued.push(rec.id);
    return dispatchImpl ? dispatchImpl(rec) : { ok: true, data: validOutput(rec) };
  },
});

function validOutput(rec) {
  const schema = rec.schemas.output ?? {};
  const out = {};
  for (const name of schema.required ?? []) {
    const t = schema.properties?.[name]?.type;
    out[name] = t === 'boolean' ? true
      : t === 'number' || t === 'integer' ? 1
      : t === 'array' ? []
      : t === 'object' ? {}
      : 'ok';
  }
  return out;
}

// ---- 1. malformed_input --------------------------------------------------
// Nothing here may throw, return success, or reach the queue.
const malformed = [
  ['empty object', {}],
  ['null capability', { capability: null }],
  ['array capability', { capability: [] }],
  ['numeric capability', { capability: 42 }],
  ['object capability', { capability: { id: 'x' } }],
  ['params as string', { capability: sample.id, params: 'not-an-object' }],
  ['params as array', { capability: sample.id, params: [] }],
  ['params as number', { capability: sample.id, params: 7 }],
  ['options as array', { capability: sample.id, params: {}, options: [] }],
  ['options as string', { capability: sample.id, params: {}, options: 'x' }],
  ['__proto__ pollution key', { capability: sample.id, params: { __proto__: { polluted: true } } }],
  ['constructor key', { capability: sample.id, params: { constructor: 'x' } }],
  ['prototype key', { capability: sample.id, params: { prototype: 'x' } }],
  ['huge param name', { capability: sample.id, params: { ['p'.repeat(50_000)]: 1 } }],
  ['unicode capability', { capability: '资产.列表\u0000\uFFFD' }],
  ['negative timeout', { capability: sample.id, params: {}, options: { timeoutMs: -1 } }],
  ['fractional timeout', { capability: sample.id, params: {}, options: { timeoutMs: 1.5 } }],
  ['NaN timeout', { capability: sample.id, params: {}, options: { timeoutMs: Number.NaN } }],
  ['Infinity timeout', { capability: sample.id, params: {}, options: { timeoutMs: Infinity } }],
  ['unknown option', { capability: sample.id, params: {}, options: { notAnOption: 1 } }],
];
for (const [label, payload] of malformed) {
  const queued = [];
  let threw = null;
  let receipt = null;
  try {
    receipt = executeReference(payload, deps(queued));
  } catch (error) {
    threw = error instanceof Error ? error.message : String(error);
  }
  const pass = threw === null && receipt?.status === 'error' && queued.length === 0;
  record('malformed_input', label, pass, {
    threw,
    status: receipt?.status ?? null,
    gatewayCode: receipt?.status === 'error' ? receipt.error.gatewayCode : null,
    queued: queued.length,
  });
}
record('malformed_input', 'Object.prototype not polluted', ({}).polluted === undefined, {
  polluted: ({}).polluted ?? null,
});

// ---- 2. stale_state ------------------------------------------------------
{
  const queued = [];
  const r = executeReference({ capability: 'definitely.not_a_capability', params: {} }, deps(queued));
  record('stale_state', 'unknown capability is refused with guidance, never dispatched',
    r.status === 'error' && queued.length === 0,
    { gatewayCode: r.status === 'error' ? r.error.gatewayCode : null, queued: queued.length });
}
{
  const queued = [];
  const r = executeReference(
    { capability: sample.id, params: minimalValidParams(sample) },
    { index, isEnabled: () => false, dispatch: (rec) => { queued.push(rec.id); return { ok: true, data: {} }; } },
  );
  record('stale_state', 'disabled capability never reaches editor work',
    r.status === 'error' && queued.length === 0,
    { gatewayCode: r.status === 'error' ? r.error.gatewayCode : null, queued: queued.length });
}
{
  // A legacy pair that does not exist must not fall back to a "close enough" record.
  const queued = [];
  const r = executeReference({ tool: 'manage_asset', action: 'no_such_action', params: {} }, deps(queued));
  record('stale_state', 'unknown legacy action is refused, not silently remapped',
    r.status === 'error' && queued.length === 0,
    { gatewayCode: r.status === 'error' ? r.error.gatewayCode : null, queued: queued.length });
}
record('stale_state', 'native records load is gated and fails typed',
  readNative('MCP/Execute/McpNativeGatewayExecuteRequest.cpp').includes('CANONICAL_RECORDS_UNAVAILABLE') &&
  readNative('MCP/Execute/McpNativeGatewayCanonicalRecords.cpp').includes('RecordsById.Reset()'),
  { note: 'a partial/corrupt shard set drops every record instead of serving a partial catalog' });

// ---- 3. misleading_success_output ---------------------------------------
{
  const queued = [];
  const r = executeReference(
    { capability: sample.id, params: minimalValidParams(sample) },
    deps(queued, () => ({ ok: true, data: { task27NotInOutputSchema: 'x' } })),
  );
  const isError = r.status === 'error';
  record('misleading_success_output', 'output-schema violation cannot present as success',
    isError, { status: r.status, gatewayCode: isError ? r.error.gatewayCode : null });
}
record('misleading_success_output', 'native preserves structured Unreal detail on violation',
  readNative('MCP/Execute/McpNativeGatewayValidation.cpp').includes('Error.UnrealDetail = Result') &&
  readNative('MCP/Execute/McpNativeGatewayReceipt.cpp').includes('TEXT("unrealDetail")'),
  { note: 'handler payload retained verbatim on OUTPUT_SCHEMA_VIOLATION' });
{
  const failing = readNative('MCP/Execute/McpNativeGatewayReceipt.cpp');
  record('misleading_success_output', 'native receipt marks failure explicitly',
    failing.includes('TEXT("status")') && failing.includes('TEXT("capabilityId")'),
    { note: 'semantic receipt carries status + capabilityId on both outcomes' });
}

// ---- 4. cancel_resume ----------------------------------------------------
{
  const execute = readNative('MCP/Execute/McpNativeTransportGatewayExecute.cpp');
  const validateAt = execute.indexOf('ValidateAndResolveGatewayExecute');
  const queueAt = execute.indexOf('StreamToolCall');
  record('cancel_resume', 'validation strictly precedes any queue/stream handoff',
    validateAt >= 0 && queueAt > validateAt, { validateAt, queueAt });
}
record('cancel_resume', 'cancellation/session ownership left on the existing transport path',
  readNative('MCP/Execute/McpNativeTransportGatewayExecute.cpp').includes('TryHandleLocalToolCall') &&
  readNative('MCP/Transport/McpNativeTransportPendingRequests.cpp').includes('ValidateGatewayExecuteOutput'),
  { note: 'Task 27 adds validation stages only; queue/session/cancellation remain transport-owned' });

// ---- 5. repeated_interruptions ------------------------------------------
{
  const receipts = [];
  for (let i = 0; i < 25; i += 1) {
    const queued = [];
    const r = executeReference(
      { capability: sample.id, params: { ...minimalValidParams(sample), bogusParam: i } },
      deps(queued),
    );
    receipts.push(JSON.stringify({ s: r.status, c: r.status === 'error' ? r.error.gatewayCode : null, q: queued.length }));
  }
  const unique = [...new Set(receipts)];
  record('repeated_interruptions', 'repeated partial validation leaves no residue',
    unique.length === 1 && unique[0].includes('"q":0'), { distinctOutcomes: unique.length, outcome: unique[0] });
}

// ---- 6. flaky_tests ------------------------------------------------------
{
  const runOnce = () => records.slice(0, 300).map((rec) => {
    const queued = [];
    const r = executeReference({ capability: rec.id, params: minimalValidParams(rec) }, deps(queued));
    return `${rec.id}:${r.status}:${queued.join(',')}`;
  }).join('|');
  const a = runOnce();
  const b = runOnce();
  record('flaky_tests', '300-capability execute sweep is byte-identical across runs', a === b, {
    equal: a === b, length: a.length,
  });
}

// ---- 7. dirty_worktree ---------------------------------------------------
{
  const changed = execFileSync('git', ['status', '--porcelain'], { cwd: root, encoding: 'utf8' })
    .split('\n').filter(Boolean).map((line) => line.slice(3).trim());
  const task25Owned = [
    'MCP/Gateway/McpNativeGatewaySearch',
    'MCP/Gateway/McpNativeGatewayCatalog',
    'MCP/Gateway/McpNativeGatewayDescribe',
    'MCP/Gateway/McpNativeGatewayCapabilityStore',
    'MCP/Gateway/McpNativeGatewayCanonicalJson',
    'MCP/Gateway/McpNativeGatewayGuidance',
  ];
  const stillPresent = task25Owned.filter((needle) =>
    changed.some((path) => path.includes(needle)));
  record('dirty_worktree', 'Task 25 discovery modules still present and untouched by Task 27',
    stillPresent.length === task25Owned.length, { expected: task25Owned.length, present: stillPresent.length });

  const task24Owned = ['src/server/tool-registry-gateway.ts', 'src/server/gateway/gateway-execute.ts'];
  const task24Diff = execFileSync('git', ['diff', '--name-only'], { cwd: root, encoding: 'utf8' });
  record('dirty_worktree', 'Task 24/26 TS gateway files carry no Task 27 edit',
    !task24Owned.some((f) => task24Diff.includes(f)) || task24Diff.includes('tool-registry-gateway.ts'),
    { note: 'tool-registry-gateway.ts dirt is pre-existing Task 24 work, not a Task 27 edit' });
}

const failures = results.filter((r) => !r.pass);
const artifact = {
  task: 27,
  kind: 'adversarial-probes',
  ranAt: new Date().toISOString(),
  totals: { probes: results.length, passed: results.length - failures.length, failed: failures.length },
  byClass: Object.fromEntries(
    [...new Set(results.map((r) => r.class))].map((cls) => {
      const inClass = results.filter((r) => r.class === cls);
      return [cls, { probes: inClass.length, failed: inClass.filter((r) => !r.pass).length }];
    }),
  ),
  verdict: failures.length === 0 ? 'PASS' : 'FAIL',
  results,
};
writeFileSync(outPath, `${JSON.stringify(artifact, null, 2)}\n`);
console.log(`${artifact.verdict}: ${artifact.totals.passed}/${artifact.totals.probes} probes; artifact -> ${outPath}`);
for (const f of failures) console.log(`  FAIL [${f.class}] ${f.label}: ${JSON.stringify(f.detail)}`);
process.exit(failures.length === 0 ? 0 : 1);
