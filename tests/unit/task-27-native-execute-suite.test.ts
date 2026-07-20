/// <reference types="node" />

import { mkdirSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import {
  CANONICAL_CAPABILITY_RECORDS,
  CANONICAL_CAPABILITY_RECORD_COUNT,
  CATALOG_REVISION
} from '../../src/tools/catalog/capabilities/generated/canonical-registry.generated.js';

import { buildCasesForRecord, EXECUTE_RULES, minimalValidParams, type ExecuteCase } from './task-27-suite/case-builder.js';
import {
  buildResolverIndex,
  executeReference,
  legacyKey,
  resolveCapability,
  validateOptions,
  type CapabilityLike,
  type DispatchResult,
  type ExecuteDeps
} from './task-27-suite/execute-reference.js';
import { validateAgainstSubset, VIOLATION_GATEWAY_CODES } from './task-27-suite/schema-subset.js';

const records = CANONICAL_CAPABILITY_RECORDS as readonly CapabilityLike[];
const index = buildResolverIndex(records);
// Capability ids use semantic namespaces (Task 5), so samples are derived from
// routing.parentTool rather than assuming an id spelling.
const firstOfParent = (parentTool: string): CapabilityLike => {
  const record = records.find((entry) => entry.routing.parentTool === parentTool);
  if (!record) throw new Error(`no canonical record routes to ${parentTool}`);
  return record;
};

// A dispatch stub that echoes a schema-valid output unless the case overrides it.
// `queued` records whether the request would have reached the subsystem queue.
function makeDeps(overrides: {
  enabled?: boolean;
  output?: unknown;
  ok?: boolean;
  queued?: string[];
} = {}): ExecuteDeps {
  return {
    index,
    isEnabled: () => overrides.enabled ?? true,
    dispatch: (record, params): DispatchResult => {
      overrides.queued?.push(record.id);
      if (overrides.ok === false) {
        return { ok: false, data: 'Unreal handler failed', detail: { unrealError: 'LogMcp: asset locked', params } };
      }
      return { ok: true, data: overrides.output ?? validOutputFor(record) };
    }
  };
}

// Smallest object satisfying the record's declared output schema.
function validOutputFor(record: CapabilityLike): Record<string, unknown> {
  const schema = record.schemas.output as Record<string, unknown> | undefined;
  const properties = (schema?.properties ?? {}) as Record<string, Record<string, unknown>>;
  const required = Array.isArray(schema?.required) ? (schema?.required as string[]) : [];
  const output: Record<string, unknown> = {};
  for (const name of required) {
    const propertySchema = properties[name] ?? {};
    output[name] = sampleOutputValue(propertySchema);
  }
  return output;
}

function sampleOutputValue(propertySchema: Record<string, unknown>): unknown {
  if (Array.isArray(propertySchema.enum) && propertySchema.enum.length > 0) return propertySchema.enum[0];
  const declared = Array.isArray(propertySchema.type) ? propertySchema.type[0] : propertySchema.type;
  switch (declared) {
    case 'boolean': return true;
    case 'number': case 'integer': return 1;
    case 'array': return [];
    case 'object': {
      const nested = (propertySchema.properties ?? {}) as Record<string, Record<string, unknown>>;
      const required = Array.isArray(propertySchema.required) ? (propertySchema.required as string[]) : [];
      const value: Record<string, unknown> = {};
      for (const name of required) value[name] = sampleOutputValue(nested[name] ?? {});
      return value;
    }
    case 'null': return null;
    default: return 'ok';
  }
}

const runCase = (testCase: ExecuteCase, queued: string[] = []) =>
  executeReference(
    { capability: testCase.capabilityId, params: testCase.params, options: testCase.options },
    makeDeps({
      enabled: testCase.toolEnabled,
      output: testCase.dispatchOutput,
      queued
    })
  );

describe('Task 27: generated execute suite agrees with the canonical rules', () => {
  it('generates one minimal valid case plus rule-invalid cases for every capability', () => {
    expect(records).toHaveLength(CANONICAL_CAPABILITY_RECORD_COUNT);
    const cases = records.flatMap(buildCasesForRecord);
    const validCases = cases.filter((entry) => entry.rule === 'valid');
    expect(validCases).toHaveLength(CANONICAL_CAPABILITY_RECORD_COUNT);
    expect(cases.length).toBeGreaterThan(CANONICAL_CAPABILITY_RECORD_COUNT * 6);
    expect(new Set(cases.map((entry) => entry.caseId)).size).toBe(cases.length);
  });

  it('accepts every generated minimal valid request and reaches the queue exactly once', () => {
    const failures: string[] = [];
    for (const record of records) {
      const queued: string[] = [];
      const receipt = runCase(
        { caseId: `${record.id}#valid`, rule: 'valid', capabilityId: record.id, params: minimalValidParams(record), toolEnabled: true, expect: { status: 'success' } },
        queued
      );
      if (receipt.status !== 'success') {
        failures.push(`${record.id}: ${receipt.error.gatewayCode} ${receipt.error.message}`);
      } else if (queued.length !== 1) {
        failures.push(`${record.id}: queued ${queued.length} times`);
      }
    }
    expect(failures.slice(0, 10)).toEqual([]);
  });

  it('rejects every generated rule-invalid request with the expected typed error', () => {
    const failures: string[] = [];
    for (const record of records) {
      for (const testCase of buildCasesForRecord(record)) {
        if (testCase.rule === 'valid') continue;
        const receipt = runCase(testCase);
        if (receipt.status !== 'error') {
          failures.push(`${testCase.caseId}: expected error, got success`);
          continue;
        }
        if (receipt.error.kind !== testCase.expect.kind || receipt.error.gatewayCode !== testCase.expect.gatewayCode) {
          failures.push(
            `${testCase.caseId}: expected ${testCase.expect.kind}/${testCase.expect.gatewayCode}, got ${receipt.error.kind}/${receipt.error.gatewayCode}`
          );
        }
      }
    }
    expect(failures.slice(0, 10)).toEqual([]);
  });

  it('never reaches the subsystem queue for an invalid request', () => {
    const reached: string[] = [];
    for (const record of records.slice(0, 200)) {
      for (const testCase of buildCasesForRecord(record)) {
        if (testCase.rule === 'valid' || testCase.rule === 'output-mismatch') continue;
        const queued: string[] = [];
        runCase(testCase, queued);
        if (queued.length > 0) reached.push(testCase.caseId);
      }
    }
    expect(reached).toEqual([]);
  });
});

describe('Task 27: canonical and legacy forms normalize to one operation', () => {
  const sample = ['manage_asset', 'control_actor', 'manage_level'];

  it('produces the same capability, dispatch action and params from both forms', () => {
    for (const parentTool of sample) {
      const record = firstOfParent(parentTool);
      const params = minimalValidParams(record);
      const legacy = record.legacyIds[0];
      expect(legacy, `${record.id} must carry a generated legacy id`).toBeDefined();

      const canonicalQueue: string[] = [];
      const canonicalReceipt = executeReference({ capability: record.id, params }, makeDeps({ queued: canonicalQueue }));
      const legacyQueue: string[] = [];
      const legacyReceipt = executeReference(
        { tool: legacy?.tool, action: legacy?.action, params },
        makeDeps({ queued: legacyQueue })
      );

      expect(canonicalReceipt.status).toBe('success');
      expect(legacyReceipt.status).toBe('success');
      if (canonicalReceipt.status !== 'success' || legacyReceipt.status !== 'success') continue;
      expect(legacyReceipt.capabilityId).toBe(canonicalReceipt.capabilityId);
      expect(legacyReceipt.dispatch).toEqual(canonicalReceipt.dispatch);
      expect(canonicalQueue).toEqual([record.id]);
      expect(legacyQueue).toEqual([record.id]);
    }
  });

  it('resolves every generated legacy id back to its own canonical record', () => {
    const mismatches: string[] = [];
    for (const record of records) {
      for (const legacy of record.legacyIds) {
        const resolved = index.byLegacy.get(legacyKey(legacy.tool, legacy.action));
        if (resolved?.id !== record.id) {
          mismatches.push(`${legacy.tool}.${legacy.action} -> ${resolved?.id ?? 'none'} (expected ${record.id})`);
        }
      }
    }
    expect(mismatches.slice(0, 10)).toEqual([]);
  });

  it('rejects a request whose canonical and legacy forms disagree', () => {
    const first = firstOfParent('manage_asset');
    const other = records.find((record) => record.id !== first.id && record.legacyIds.length > 0);
    expect(other).toBeDefined();
    const outcome = resolveCapability(
      { capability: first.id, tool: other?.legacyIds[0]?.tool, action: other?.legacyIds[0]?.action },
      index
    );
    expect(outcome.ok).toBe(false);
    if (!outcome.ok) expect(outcome.error.gatewayCode).toBe('FORM_CONFLICT');
  });
});

describe('Task 27: alias resolution is visible and conflict-free', () => {
  it('never lets one alias resolve to two capabilities in the shipped catalog', () => {
    const conflicts = [...index.byAlias.entries()]
      .filter(([, owners]) => new Set(owners).size > 1)
      .map(([alias, owners]) => `${alias} -> ${[...new Set(owners)].sort().join(', ')}`);
    expect(conflicts).toEqual([]);
  });

  it('reports a typed ALIAS_CONFLICT when an alias is ambiguous', () => {
    const [a, b] = records;
    const ambiguous = buildResolverIndex([
      { ...a, aliases: ['task27_ambiguous'] },
      { ...b, aliases: ['task27_ambiguous'] }
    ]);
    const outcome = resolveCapability({ capability: 'task27_ambiguous' }, ambiguous);
    expect(outcome.ok).toBe(false);
    if (!outcome.ok) {
      expect(outcome.error.gatewayCode).toBe('ALIAS_CONFLICT');
      expect(outcome.error.kind).toBe('validation');
    }
  });

  it('resolves a declared alias to its single owning capability', () => {
    const owner = records.find((record) => record.aliases.length > 0);
    if (!owner) return;
    const outcome = resolveCapability({ capability: owner.aliases[0] }, index);
    expect(outcome.ok).toBe(true);
    if (outcome.ok) expect(outcome.record.id).toBe(owner.id);
  });
});

describe('Task 27: options are bounded and never leak into params', () => {
  it('accepts only the Task 3 execution option keys', () => {
    expect(validateOptions({ preview: true, timeoutMs: 1000 })).toBeUndefined();
    const failure = validateOptions({ durationSeconds: 5 });
    expect(failure?.gatewayCode).toBe('UNSUPPORTED_OPTION');
    expect(failure?.kind).toBe('option');
    expect(failure?.option).toBe('durationSeconds');
  });

  it('bounds timeoutMs to a positive integer no greater than 600000', () => {
    expect(validateOptions({ timeoutMs: 600_000 })).toBeUndefined();
    expect(validateOptions({ timeoutMs: 600_001 })?.gatewayCode).toBe('OUT_OF_RANGE');
    expect(validateOptions({ timeoutMs: 0 })?.gatewayCode).toBe('OUT_OF_RANGE');
    expect(validateOptions({ timeoutMs: 1.5 })?.gatewayCode).toBe('OUT_OF_RANGE');
  });

  it('rejects a gateway control smuggled through action params', () => {
    const record = firstOfParent('manage_asset');
    const receipt = executeReference(
      { capability: record.id, params: { ...minimalValidParams(record), savePolicy: 'always' } },
      makeDeps()
    );
    expect(receipt.status).toBe('error');
    if (receipt.status === 'error') expect(receipt.error.gatewayCode).toBe('UNSUPPORTED_OPTION');
  });
});

describe('Task 27: schema enforcement is exact and fail-closed', () => {
  it('rejects a schema keyword the canonical validator does not implement', () => {
    const violation = validateAgainstSubset(
      { name: 'x' },
      { type: 'object', properties: { name: { type: 'string' } }, if: { required: ['name'] } }
    );
    expect(violation?.reason).toBe('unsupported-keyword');
    expect(VIOLATION_GATEWAY_CODES[violation?.reason ?? 'undeclared']).toBe('UNSUPPORTED_SCHEMA_KEYWORD');
  });

  it('confirms no shipped record relies on an unimplemented keyword', () => {
    const offenders: string[] = [];
    for (const record of records) {
      const probe = validateAgainstSubset(minimalValidParams(record), record.schemas.input);
      if (probe?.reason === 'unsupported-keyword') offenders.push(`${record.id}: ${probe.message}`);
      const outputProbe = validateAgainstSubset(validOutputFor(record), record.schemas.output);
      if (outputProbe?.reason === 'unsupported-keyword') offenders.push(`${record.id} (output): ${outputProbe.message}`);
    }
    expect(offenders.slice(0, 10)).toEqual([]);
  });

  it('leaves reflection-boundary objects open while still requiring an object', () => {
    const boundary = { type: 'object', 'x-unreal-reflection-boundary': true };
    expect(validateAgainstSubset({ anyUnrealProperty: 1 }, boundary)).toBeUndefined();
    expect(validateAgainstSubset('not-an-object', boundary)?.reason).toBe('type');
  });

  it('enforces the exact per-action schema rather than the parent tool union', () => {
    const parent = 'manage_asset';
    const siblings = records.filter((record) => record.routing.parentTool === parent);
    const target = siblings.find((record) => Object.keys((record.schemas.input as { properties?: object }).properties ?? {}).length > 1);
    expect(target).toBeDefined();
    if (!target) return;
    const foreign = siblings.find((record) => {
      const own = Object.keys((target.schemas.input as { properties?: object }).properties ?? {});
      return Object.keys((record.schemas.input as { properties?: object }).properties ?? {}).some((name) => !own.includes(name));
    });
    expect(foreign, 'the parent tool must have a union wider than one action').toBeDefined();
    if (!foreign) return;
    const own = Object.keys((target.schemas.input as { properties?: object }).properties ?? {});
    const foreignParam = Object.keys((foreign.schemas.input as { properties?: object }).properties ?? {})
      .find((name) => !own.includes(name));
    expect(foreignParam).toBeDefined();
    const receipt = executeReference(
      { capability: target.id, params: { ...minimalValidParams(target), [foreignParam as string]: 'x' } },
      makeDeps()
    );
    expect(receipt.status).toBe('error');
    if (receipt.status === 'error') expect(receipt.error.gatewayCode).toBe('UNDECLARED_PARAMETER');
  });
});

describe('Task 27: receipts and error envelopes are semantic', () => {
  it('carries capabilityId and catalogRevision on success', () => {
    const record = firstOfParent('manage_asset');
    const receipt = executeReference({ capability: record.id, params: minimalValidParams(record) }, makeDeps());
    expect(receipt.status).toBe('success');
    expect(receipt.capabilityId).toBe(record.id);
    expect(receipt.catalogRevision).toBe(CATALOG_REVISION);
  });

  it('reports an output-schema violation as an error, never as success', () => {
    const record = firstOfParent('manage_asset');
    const receipt = executeReference(
      { capability: record.id, params: minimalValidParams(record) },
      makeDeps({ output: { task27Undeclared: true } })
    );
    expect(receipt.status).toBe('error');
    if (receipt.status === 'error') {
      expect(receipt.error.gatewayCode).toBe('OUTPUT_SCHEMA_VIOLATION');
      expect(receipt.error.kind).toBe('validation');
    }
  });

  it('preserves the structured Unreal failure as a typed execution error', () => {
    const record = firstOfParent('manage_asset');
    const receipt = executeReference(
      { capability: record.id, params: minimalValidParams(record) },
      makeDeps({ ok: false })
    );
    expect(receipt.status).toBe('error');
    if (receipt.status === 'error') {
      expect(receipt.error.kind).toBe('execution');
      expect(receipt.error.code).toBe('UNREAL_ENGINE_ERROR');
      expect(receipt.error.gatewayCode).toBe('UNREAL_EXECUTION_ERROR');
    }
  });

  it('reports a disabled capability without dispatching', () => {
    const record = firstOfParent('manage_asset');
    const queued: string[] = [];
    const receipt = executeReference(
      { capability: record.id, params: minimalValidParams(record) },
      makeDeps({ enabled: false, queued })
    );
    expect(receipt.status).toBe('error');
    if (receipt.status === 'error') expect(receipt.error.gatewayCode).toBe('TOOL_DISABLED');
    expect(queued).toEqual([]);
  });
});

describe('Task 27: the rule-outcome matrix is the cross-surface contract', () => {
  it('publishes one expected kind/code per rule and records the suite digest', () => {
    const matrix = new Map<string, string>();
    for (const record of records) {
      for (const testCase of buildCasesForRecord(record)) {
        const expected = testCase.expect.status === 'success'
          ? 'success'
          : `${testCase.expect.kind}/${testCase.expect.gatewayCode}`;
        const seen = matrix.get(testCase.rule);
        expect(seen ?? expected, `rule '${testCase.rule}' must map to one outcome`).toBe(expected);
        matrix.set(testCase.rule, expected);
      }
    }
    expect([...matrix.keys()].sort()).toEqual([...EXECUTE_RULES].sort());

    const cases = records.flatMap(buildCasesForRecord);
    const digest = createHash('sha256')
      .update(JSON.stringify(cases.map((entry) => [entry.caseId, entry.rule, entry.expect])))
      .digest('hex');

    const outDir = resolve(process.cwd(), '.omo/evidence/task-27');
    mkdirSync(outDir, { recursive: true });
    writeFileSync(
      resolve(outDir, 'execute-suite-summary.json'),
      `${JSON.stringify({
        catalogRevision: CATALOG_REVISION,
        capabilities: records.length,
        totalCases: cases.length,
        casesByRule: Object.fromEntries(
          EXECUTE_RULES.map((rule) => [rule, cases.filter((entry) => entry.rule === rule).length])
        ),
        ruleOutcomeMatrix: Object.fromEntries([...matrix.entries()].sort()),
        suiteDigestSha256: digest
      }, null, 2)}\n`
    );
  });
});
