// Every capability's generated execute cases, run through the real gateway
// (handleUnrealGatewayCall) with the bridge mocked: one minimal valid request
// per capability must dispatch exactly once, and every rule-invalid variant the
// record's own schema declares must be refused with its typed code before any
// dispatch.

import { afterEach, describe, expect, it } from 'vitest';

import { handleUnrealGatewayCall } from '../../src/server/tool-registry-gateway.js';
import { dynamicToolManager } from '../../src/tools/dynamic/dynamic-tool-manager.js';
import {
  CANONICAL_CAPABILITY_RECORDS,
  CANONICAL_CAPABILITY_RECORD_COUNT,
  CATALOG_REVISION
} from '../../src/tools/catalog/capabilities/generated/canonical-registry.generated.js';
import type { CapabilityRecord } from '../../src/tools/catalog/capabilities/model.js';
import { buildCasesForRecord, minimalValidParams, type ExecuteCase } from './gateway-discovery-suite/case-builder.js';
import { gatewayContext } from './tools/support/gateway-context-fixture.js';

const records = CANONICAL_CAPABILITY_RECORDS;
const PROTECTED = new Set(['manage_tools', 'inspect']);

// Smallest object satisfying the record's declared output schema.
function sampleOutput(schema: unknown): unknown {
  const s = (schema ?? {}) as Record<string, unknown>;
  if (Array.isArray(s.enum) && s.enum.length > 0) return s.enum[0];
  switch (Array.isArray(s.type) ? s.type[0] : s.type) {
    case 'boolean': return true;
    case 'number': case 'integer': return 1;
    case 'array': return [];
    case 'null': return null;
    case 'object': {
      const properties = (s.properties ?? {}) as Record<string, unknown>;
      const required = Array.isArray(s.required) ? (s.required as string[]) : [];
      return Object.fromEntries(required.map((name) => [name, sampleOutput(properties[name])]));
    }
    default: return 'ok';
  }
}

async function execute(
  request: Record<string, unknown>,
  reply: unknown
): Promise<{ result: Record<string, unknown>; dispatched: Array<{ tool: string; payload: Record<string, unknown> }> }> {
  const dispatched: Array<{ tool: string; payload: Record<string, unknown> }> = [];
  const result = await handleUnrealGatewayCall({ operation: 'execute', ...request }, gatewayContext({
    isConnected: () => true,
    sendAutomationRequest: async (tool, payload) => {
      dispatched.push({ tool, payload });
      return reply;
    }
  }, 'generated-execute'));
  return { result, dispatched };
}

const validReply = (record: CapabilityRecord): unknown => ({ success: true, ...(sampleOutput(record.schemas.output) as object) });

async function runCase(record: CapabilityRecord, testCase: ExecuteCase) {
  if (!testCase.toolEnabled) dynamicToolManager.disableTools([record.routing.parentTool]);
  const reply = testCase.dispatchOutput ?? validReply(record);
  return execute({ capability: record.id, params: testCase.params, ...(testCase.options ? { options: testCase.options } : {}) }, reply);
}

afterEach(() => {
  dynamicToolManager.reset();
});

describe('generated execute cases through the real gateway', () => {
  it('generates one valid case per capability plus rule-invalid variants', () => {
    const cases = records.flatMap(buildCasesForRecord);
    expect(cases.filter((entry) => entry.rule === 'valid')).toHaveLength(CANONICAL_CAPABILITY_RECORD_COUNT);
    expect(cases.length).toBeGreaterThan(CANONICAL_CAPABILITY_RECORD_COUNT * 5);
    expect(new Set(cases.map((entry) => entry.caseId)).size).toBe(cases.length);
  });

  it('accepts every minimal valid request and dispatches it exactly once to its parent tool', async () => {
    const failures: string[] = [];
    for (const record of records) {
      if (record.routing.parentTool === 'manage_tools') continue; // runs in process, never dispatched
      const { result, dispatched } = await execute({ capability: record.id, params: minimalValidParams(record) }, validReply(record));
      if (result.success !== true) failures.push(`${record.id}: ${String(result.errorCode)} ${String(result.message ?? '')}`.slice(0, 200));
      else if (dispatched.length !== 1 || dispatched[0]?.tool !== record.routing.parentTool) failures.push(`${record.id}: dispatched ${dispatched.length}x`);
    }
    expect(failures.slice(0, 10)).toEqual([]);
  });

  it('refuses every rule-invalid request with its typed code, before any dispatch', async () => {
    const failures: string[] = [];
    for (const record of records) {
      for (const testCase of buildCasesForRecord(record)) {
        if (testCase.rule === 'valid') continue;
        if (testCase.rule === 'disabled-capability' && PROTECTED.has(record.routing.parentTool)) continue;
        if (record.routing.parentTool === 'manage_tools' && testCase.rule === 'output-mismatch') continue;
        const { result, dispatched } = await runCase(record, testCase);
        dynamicToolManager.reset();
        if (result.errorCode !== testCase.expect.gatewayCode) {
          failures.push(`${testCase.caseId}: expected ${testCase.expect.gatewayCode}, got ${String(result.errorCode)}`);
        } else if (testCase.rule !== 'output-mismatch' && dispatched.length > 0) {
          failures.push(`${testCase.caseId}: reached the bridge`);
        }
      }
    }
    expect(failures.slice(0, 10)).toEqual([]);
  });

  it('dispatches the legacy {tool, action} form exactly like the canonical id', async () => {
    for (const parentTool of ['manage_asset', 'control_actor', 'manage_level']) {
      const record = records.find((entry) => entry.routing.parentTool === parentTool);
      const legacy = record?.legacyIds[0];
      expect(legacy, `${parentTool} needs a record with a legacy pair`).toBeDefined();
      if (!record || !legacy) continue;
      const params = minimalValidParams(record);
      const canonical = await execute({ capability: record.id, params }, validReply(record));
      const viaLegacy = await execute({ tool: legacy.tool, action: legacy.action, params }, validReply(record));
      expect(viaLegacy.dispatched).toEqual(canonical.dispatched);
      expect(canonical.result.catalogRevision).toBe(CATALOG_REVISION);
    }
  });

  it('refuses a request whose canonical id and legacy pair disagree', async () => {
    const [first, other] = records.filter((record) => record.routing.parentTool === 'manage_asset');
    const { result, dispatched } = await execute({
      capability: first?.id, tool: other?.legacyIds[0]?.tool, action: other?.legacyIds[0]?.action, params: {}
    }, {});
    expect(result.errorCode).toBe('FORM_CONFLICT');
    expect(dispatched).toEqual([]);
  });

  it('resolves a declared alias to its owning capability', async () => {
    const owner = records.find((record) => record.aliases.length > 0 && record.routing.parentTool !== 'manage_tools');
    expect(owner, 'no capability declares an alias').toBeDefined();
    if (!owner) return;
    const { result } = await execute({ capability: owner.aliases[0], params: minimalValidParams(owner) }, validReply(owner));
    expect(result.capability ?? result.capabilityId).toBe(owner.id);
  });

  it('keeps the structured Unreal failure as a typed execution error', async () => {
    const record = records.find((entry) => entry.routing.parentTool === 'manage_asset');
    if (!record) throw new Error('no manage_asset record');
    const { result } = await execute({ capability: record.id, params: minimalValidParams(record) },
      { success: false, message: 'Unreal handler failed', error: 'LogMcp: asset locked' });
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNREAL_EXECUTION_ERROR');
  });
});
