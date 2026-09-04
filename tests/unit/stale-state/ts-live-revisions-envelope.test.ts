import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import {
  executeErrorEnvelope,
  executeSuccessEnvelope,
} from '../../../src/server/gateway/gateway-execute-envelope.js';
import type { GatewayReceiptContext } from '../../../src/server/gateway/gateway-receipt-context.js';
import { normalizeAutomationFrame } from '../../../src/tools/orchestration/automation-frame-normalization.js';
import { CorrelationIdSchema } from '../../../src/tools/catalog/capabilities/semantic/ids.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

const LIVE_REVISIONS = { selection: 2, level: 3, assetRegistry: 4, package: 5 } as const;
const record = capabilityIndex().byId.get('asset.list');
if (record === undefined) throw new Error('asset.list fixture is absent');
const context: GatewayReceiptContext = {
  correlationId: CorrelationIdSchema.parse('gw-live-revisions'),
  startedAt: Date.now(),
};

function object(value: unknown): Record<string, unknown> {
  if (!isRecord(value)) throw new Error('expected an object');
  return value;
}

describe('Task 42 TypeScript live revision envelopes', () => {
  it('copies bridge metadata beside validated success data', () => {
    const result = normalizeAutomationFrame({
      type: 'automation_response',
      requestId: 'live-success',
      success: true,
      liveRevisions: LIVE_REVISIONS,
      result: { success: true, message: 'ok' },
    });
    const envelope = executeSuccessEnvelope({
      record,
      result,
      canonicalOutput: { success: true, message: 'ok' },
      warnings: [],
    }, context);
    const receipt = object(envelope.receipt);

    expect(envelope.liveRevisions).toEqual(LIVE_REVISIONS);
    expect(receipt.liveRevisions).toEqual(LIVE_REVISIONS);
    // The receipt carries a digest of the payload, not a second copy of it (dogfood #11).
    expect(receipt.data).toBeUndefined();
    expect(typeof receipt.dataDigest).toBe('string');
  });

  it('copies bridge metadata onto execution error envelopes and receipts', () => {
    const envelope = executeErrorEnvelope({
      errorCode: 'UNREAL_EXECUTION_ERROR',
      message: 'selection changed',
      record,
      detail: { liveRevisions: LIVE_REVISIONS },
    }, context);

    expect(envelope.liveRevisions).toEqual(LIVE_REVISIONS);
    expect(object(envelope.receipt).liveRevisions).toEqual(LIVE_REVISIONS);
  });
});
