// Task 26 â€” generated execute fixtures across every canonical action.
//
// The acceptance criterion is "run one minimal valid request per action and
// rule-invalid fixtures". Both fixture families are DERIVED from each record's
// own generated schema (tests/unit/tools/support/capability-fixtures.ts),
// so this suite stays exhaustive across all 1,383 actions without a hand list
// that would rot the moment the catalog is regenerated.
//
// Each case is asserted inside a loop and failures are collected with their
// exact capability ID, so one broken record names itself instead of hiding
// behind a suite-level pass.

import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';

import { Logger } from '../../../src/utils/logging/logger.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import type { CapabilityRecord } from '../../../src/tools/catalog/capabilities/model.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { capabilityIndex, catalogRevision } from '../../../src/server/gateway/gateway-capability-index.js';
import { resolveMigrationEntry } from '../../../src/tools/catalog/capabilities/migration/migration-map.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';
import {
  invalidVariants,
  minimalValidOutput,
  minimalValidParams,
  type InvalidVariant
} from './support/capability-fixtures.js';

const dispatched: Array<{ tool: string; args: Record<string, unknown> }> = [];

vi.mock('../../../src/tools/orchestration/consolidated-tool-handlers.js', () => ({
  handleConsolidatedToolCall: vi.fn(async (tool: string, args: Record<string, unknown>) => {
    dispatched.push({ tool, args });
    const action = typeof args.action === 'string' ? args.action : '';
    const record = capabilityIndex().byLegacyPair.get(`${tool}::${action}`);
    return record === undefined ? { success: true } : minimalValidOutput(record);
  })
}));

function makeContext(): GatewayContext {
  const tools: ITools = {
    systemTools: {
      executeConsoleCommand: async () => ({ success: false }),
      getProjectSettings: async () => ({})
    },
    assetResources: { list: async () => ({}) }
  };
  return {
    tools,
    logger: new Logger('task26-fixtures', 'error'),
    elicitationTimeoutMs: 0,
    ensureConnected: async () => true
  };
}

async function execute(args: Record<string, unknown>): Promise<Record<string, unknown>> {
  return await handleUnrealGatewayCall({ operation: 'execute', ...args }, makeContext());
}

// The correlation id, external request id and wall-clock timing are minted per
// request, so the two request forms differ on them by design; cross-form receipt
// equality is asserted on the stable semantic content only.
function stableReceipt(value: unknown): unknown {
  if (!isRecord(value)) return value;
  const clone = { ...value };
  delete clone.correlationId;
  delete clone.requestId;
  delete clone.timingMs;
  return clone;
}

const records = capabilityIndex().records;

// A legacy verb the Task 20 migration map retired must never dispatch, so it is
// partitioned out of the "valid request succeeds" expectation and given its own
// refusal expectation instead.
function isRetired(record: CapabilityRecord): boolean {
  const legacy = record.legacyIds[0];
  if (legacy === undefined) return false;
  return resolveMigrationEntry(legacy.tool, legacy.action)?.disposition === 'removed';
}

const runnable = records.filter((record) => !isRetired(record));
const retired = records.filter(isRetired);

beforeEach(() => {
  dispatched.length = 0;
});

afterEach(() => {
  dynamicToolManager.reset();
});

describe('generated fixtures: the catalog under test', () => {
  it('covers every generated capability record', () => {
    expect(records).toHaveLength(1401);
    expect(runnable.length + retired.length).toBe(records.length);
    expect(runnable.length).toBeGreaterThan(1300);
  });
});

describe('generated fixtures: one minimal valid request per action', () => {
  it('dispatches exactly once and returns a canonical receipt for every runnable action', async () => {
    const failures: string[] = [];

    for (const record of runnable) {
      dispatched.length = 0;
      const result = await execute({
        capability: record.id,
        params: minimalValidParams(record)
      });

      if (result.success !== true) {
        failures.push(`${record.id}: expected success, got ${String(result.errorCode)} (${String(result.message)})`);
        continue;
      }
      if (dispatched.length !== 1) {
        failures.push(`${record.id}: expected exactly 1 dispatch, saw ${dispatched.length}`);
        continue;
      }
      if (result.capability !== record.id) {
        failures.push(`${record.id}: receipt named capability ${String(result.capability)}`);
        continue;
      }
      const receipt = isRecord(result.receipt) ? result.receipt : undefined;
      if (receipt?.status !== 'success' || receipt.capabilityId !== record.id) {
        failures.push(`${record.id}: receipt was ${JSON.stringify(receipt)}`);
      }
    }

    expect(failures).toEqual([]);
  });

  it('sends the canonical dispatch action and never a gateway control to the handler', async () => {
    const failures: string[] = [];
    const gatewayControls = ['idempotencyKey', 'preview', 'timeoutMs', 'savePolicy', 'validationLevel'];

    for (const record of runnable) {
      dispatched.length = 0;
      await execute({ capability: record.id, params: minimalValidParams(record) });
      const seen = dispatched[0];
      if (seen === undefined) {
        failures.push(`${record.id}: nothing dispatched`);
        continue;
      }
      if (seen.tool !== record.routing.parentTool) {
        failures.push(`${record.id}: dispatched to ${seen.tool}`);
        continue;
      }
      const leaked = gatewayControls.filter((key) => key in seen.args);
      if (leaked.length > 0) {
        failures.push(`${record.id}: gateway control(s) leaked into params: ${leaked.join(', ')}`);
      }
    }

    expect(failures).toEqual([]);
  });

  it('refuses every retired legacy verb before dispatch', async () => {
    const failures: string[] = [];

    for (const record of retired) {
      dispatched.length = 0;
      const result = await execute({ capability: record.id, params: minimalValidParams(record) });
      if (result.errorCode !== 'CAPABILITY_REMOVED' || dispatched.length !== 0) {
        failures.push(`${record.id}: ${String(result.errorCode)} with ${dispatched.length} dispatch(es)`);
      }
    }

    expect(failures).toEqual([]);
  });
});

describe('generated fixtures: rule-invalid payloads are refused before dispatch', () => {
  it('returns the exact error code for every rule each record can express', async () => {
    const failures: string[] = [];
    const rulesSeen = new Map<InvalidVariant['rule'], number>();

    for (const record of runnable) {
      for (const variant of invalidVariants(record)) {
        rulesSeen.set(variant.rule, (rulesSeen.get(variant.rule) ?? 0) + 1);
        dispatched.length = 0;
        const result = await execute({ capability: record.id, params: variant.params });

        if (result.errorCode !== variant.expectedErrorCode) {
          failures.push(
            `${record.id} [${variant.rule}]: expected ${variant.expectedErrorCode}, got ${String(result.errorCode)}`
          );
          continue;
        }
        if (result.success !== false || dispatched.length !== 0) {
          failures.push(`${record.id} [${variant.rule}]: reached dispatch (${dispatched.length}) or reported success`);
        }
      }
    }

    expect(failures).toEqual([]);
    // Every rule the validator implements is exercised by real catalog data, so
    // a rule silently ceasing to fire cannot pass as coverage.
    for (const rule of ['undeclared', 'missing-required', 'type', 'enum', 'range'] as const) {
      expect(rulesSeen.get(rule) ?? 0).toBeGreaterThan(0);
    }
  });
});

describe('generated fixtures: canonical and legacy forms agree', () => {
  it('produces the same capability, receipt and dispatch for both request forms', async () => {
    const failures: string[] = [];

    for (const record of runnable) {
      const legacy = record.legacyIds[0];
      if (legacy === undefined) continue;
      const params = minimalValidParams(record);

      dispatched.length = 0;
      const canonical = await execute({ capability: record.id, params });
      const canonicalDispatch = dispatched[0];

      dispatched.length = 0;
      const viaLegacy = await execute({ tool: legacy.tool, action: legacy.action, params });
      const legacyDispatch = dispatched[0];

      if (canonical.capability !== viaLegacy.capability) {
        failures.push(`${record.id}: capability ${String(canonical.capability)} vs ${String(viaLegacy.capability)}`);
        continue;
      }
      if (JSON.stringify(stableReceipt(canonical.receipt)) !== JSON.stringify(stableReceipt(viaLegacy.receipt))) {
        failures.push(`${record.id}: receipts differ between forms`);
        continue;
      }
      if (JSON.stringify(canonicalDispatch) !== JSON.stringify(legacyDispatch)) {
        failures.push(`${record.id}: dispatch payload differs between forms`);
        continue;
      }
      // The legacy form is the only one that reports migration provenance.
      if (!isRecord(viaLegacy.migratedFrom) || canonical.migratedFrom !== undefined) {
        failures.push(`${record.id}: migration provenance was not reported on the legacy form only`);
      }
    }

    expect(failures).toEqual([]);
  });

  it('stamps the generated catalog revision on every response', async () => {
    const record = runnable[0];
    const result = await execute({ capability: record.id, params: minimalValidParams(record) });
    expect(result.catalogRevision).toBe(catalogRevision());
  });
});
