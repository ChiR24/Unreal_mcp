/**
 * tests/unit/task-29/task-29-transport-envelope-normalization.test.ts
 *
 * TASK 29 BLOCKER T29-B6 — the two transports must publish the SAME payload.
 *
 * Native `/mcp` publishes the Unreal handler's `Result` object verbatim as
 * `structuredContent`. The WebSocket bridge wraps that same object in an
 * automation frame — `{type, requestId, success, message, error, result}` — and
 * TypeScript used to hand the WHOLE frame to the client. No canonical record
 * models `type`, `requestId` or a nested `result`, and every record's output
 * schema is closed, so a client validating the TS surface against the canonical
 * contract rejected correct Unreal output for all 1,335 records.
 *
 * These cases drive the gateway execute PUBLIC surface through the REAL
 * consolidated handler with only the bridge faked, and exercise the shared
 * `normalizeAutomationFrame` seam directly. `handleConsolidatedToolCall` is the
 * single boundary the public surface converges on, so normalizing there is what
 * makes the transports agree by construction. (The former public legacy
 * direct-call path was removed with Task 30's single-`unreal` cutover; its
 * internal legacy-form equivalence now lives in task-29-legacy-equivalence.)
 *
 * Two fixtures are used deliberately:
 *
 *  - GATEWAY cases use `animation_physics.create_animation_blueprint`, whose
 *    required output `assetPath` exists ONLY inside the nested `result`.
 *    Pre-repair, gateway output validation projected declared keys off the
 *    frame ROOT, so `assetPath` was invisible and execute refused. That makes
 *    "validation runs on what the client receives" a real RED->GREEN
 *    transition, not an assertion of absence.
 *
 *  - SEAM cases feed frames straight to `normalizeAutomationFrame` (including a
 *    `manage_networking` control frame), pinning the flatten/strip rules
 *    independently of any transport: nested failures, outer transport-key
 *    stripping, and domain-owned `type`/`error`.
 */
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { Logger } from '../../../src/utils/logging/logger.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import type { GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { handleUnrealGatewayCall } from '../../../src/server/tool-registry-gateway.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';
import { normalizeAutomationFrame } from '../../../src/tools/orchestration/automation-frame-normalization.js';

const GATEWAY_TOOL = 'animation_physics';
const GATEWAY_ACTION = 'create_animation_blueprint';
const ASSET_PATH = '/Game/Anim/ABP_Task29';

const NETWORKING_INFO = Object.freeze({
  role: 'ROLE_Authority',
  remoteRole: 'ROLE_SimulatedProxy',
  hasAuthority: true,
});

/** Keys the automation frame carries that no canonical record models. */
const TRANSPORT_ONLY_KEYS = ['type', 'requestId', 'result'] as const;

/** A real WebSocket automation frame, exactly as `message-schema.ts` types it. */
function frame(
  message: string,
  domain: Record<string, unknown>,
  overrides: Record<string, unknown> = {},
): Record<string, unknown> {
  return {
    type: 'automation_response',
    requestId: 'task29-req-1',
    success: true,
    message,
    result: { success: true, ...domain },
    ...overrides,
  };
}

const gatewayFrame = (overrides: Record<string, unknown> = {}): Record<string, unknown> =>
  frame('Animation Blueprint created.', { assetPath: ASSET_PATH }, overrides);

const legacyFrame = (overrides: Record<string, unknown> = {}): Record<string, unknown> =>
  frame('Networking info retrieved', { networkingInfo: { ...NETWORKING_INFO } }, overrides);

const FAILURE_OVERRIDES = Object.freeze({
  success: false,
  message: 'Actor not found',
  error: 'ACTOR_NOT_FOUND',
  result: { success: false, error: 'ACTOR_NOT_FOUND' },
});

let bridgeReply: Record<string, unknown> = gatewayFrame();

function makeTools(): ITools {
  return {
    systemTools: {
      executeConsoleCommand: async () => ({ success: false }),
      getProjectSettings: async () => ({}),
    },
    assetResources: { list: async () => ({}) },
    automationBridge: {
      isConnected: () => true,
      sendAutomationRequest: async () => bridgeReply,
    },
  };
}

async function executeViaGateway(reply: Record<string, unknown>): Promise<Record<string, unknown>> {
  bridgeReply = reply;
  const context: GatewayContext = {
    tools: makeTools(),
    logger: new Logger('task29-envelope', 'error'),
    elicitationTimeoutMs: 0,
    ensureConnected: async () => true,
  };
  return await handleUnrealGatewayCall(
    { operation: 'execute', tool: GATEWAY_TOOL, action: GATEWAY_ACTION, params: { name: 'ABP_Task29' } },
    context,
  );
}

function gatewayPayloadOf(envelope: Record<string, unknown>): Record<string, unknown> {
  const result = envelope.result;
  expect(isRecord(result), 'gateway envelope carries no result payload').toBe(true);
  return isRecord(result) ? result : {};
}

beforeEach(() => {
  bridgeReply = gatewayFrame();
});

afterEach(() => {
  dynamicToolManager.reset();
  vi.restoreAllMocks();
});

describe('T29-B6 gateway execute publishes the canonical payload, not the automation frame', () => {
  it('flattens a nested domain result into canonical envelope + flat domain fields', async () => {
    const envelope = await executeViaGateway(gatewayFrame());

    expect(envelope.errorCode, `execute refused: ${String(envelope.message)}`).toBeUndefined();
    expect(envelope.success).toBe(true);
    expect(gatewayPayloadOf(envelope)).toEqual({
      success: true,
      message: 'Animation Blueprint created.',
      assetPath: ASSET_PATH,
    });
  });

  it('strips every transport-only key from the published payload', async () => {
    const payload = gatewayPayloadOf(await executeViaGateway(gatewayFrame()));
    const leaked = TRANSPORT_ONLY_KEYS.filter((key) => Object.hasOwn(payload, key));
    expect(leaked, `transport-only keys leaked to the client: ${leaked.join(', ')}`).toEqual([]);
  });

  it('holds the PUBLISHED payload, not the raw frame, to the declared output schema', async () => {
    // `assetPath` is required and lives ONLY inside the nested `result`, so
    // projecting the frame root (the pre-repair behaviour) cannot see it.
    const envelope = await executeViaGateway(gatewayFrame());

    expect(envelope.errorCode).not.toBe('OUTPUT_SCHEMA_VIOLATION');
    expect(gatewayPayloadOf(envelope).assetPath).toBe(ASSET_PATH);
  });

  it('still refuses a payload that genuinely violates the declared output schema', async () => {
    const envelope = await executeViaGateway(frame('Created.', { assetPath: 42 }));

    expect(envelope.success).toBe(false);
    expect(envelope.errorCode).toBe('OUTPUT_SCHEMA_VIOLATION');
  });

  it('does not flatten a typed failure frame into a false success', async () => {
    const envelope = await executeViaGateway(gatewayFrame({ ...FAILURE_OVERRIDES }));

    expect(envelope.success).toBe(false);
    expect(envelope.errorCode).toBe('UNREAL_EXECUTION_ERROR');
    // The exact wording is the domain handler's, not the normalizer's; what the
    // envelope must never do is present this frame as a success.
    expect(String(envelope.message ?? '').length).toBeGreaterThan(0);
  });

  it('preserves the failure detail needed for diagnostics', async () => {
    const envelope = await executeViaGateway(gatewayFrame({ ...FAILURE_OVERRIDES }));
    expect(JSON.stringify(envelope.result ?? {})).toContain('ACTOR_NOT_FOUND');
  });
});

describe('T29-B6 the normalization seam itself is deterministic', () => {
  it('returns an already-flat canonical payload unchanged', () => {
    const flat = { success: true, message: 'Created.', assetPath: ASSET_PATH };
    expect(normalizeAutomationFrame({ ...flat })).toEqual(flat);
  });

  it('leaves a payload that merely owns a `result` field alone when no frame is present', () => {
    const domain = { success: true, result: { rows: 2 } };
    expect(normalizeAutomationFrame({ ...domain })).toEqual(domain);
  });

  it('preserves a domain field named `type` that is not a transport marker', () => {
    const audioInfo = { success: true, type: 'SoundCue', duration: 1.5 };
    expect(normalizeAutomationFrame({ ...audioInfo })).toEqual(audioInfo);
  });

  it('flattens a frame nested under a handler `data` wrapper', () => {
    const wrapped = {
      success: true,
      message: 'Animation Blueprint created.',
      data: frame('Animation Blueprint created.', { assetPath: ASSET_PATH }),
    };
    expect(normalizeAutomationFrame(wrapped)).toEqual({
      success: true,
      message: 'Animation Blueprint created.',
      assetPath: ASSET_PATH,
    });
  });

  it('returns a non-object result untouched', () => {
    expect(normalizeAutomationFrame('plain text')).toBe('plain text');
    expect(normalizeAutomationFrame(null)).toBeNull();
  });

  it('never rewrites a failure frame', () => {
    const failed = legacyFrame({ ...FAILURE_OVERRIDES });
    expect(normalizeAutomationFrame(failed)).toEqual(failed);
  });

  it('never flattens a FAILED frame a handler wrapped in a success-looking envelope', () => {
    // The outer object says `success: true`; only the nested frame knows the
    // operation failed. Checking just the root would publish a false success.
    for (const wrapper of ['data', 'result'] as const) {
      const wrapped = {
        success: true,
        message: 'Animation Blueprint created.',
        [wrapper]: gatewayFrame({ ...FAILURE_OVERRIDES }),
      };
      expect(normalizeAutomationFrame({ ...wrapped })).toEqual(wrapped);
    }
  });

  it('drops the frame-level `error` field from a successful payload', () => {
    const withOuterError = gatewayFrame({ error: '' });
    const normalized = normalizeAutomationFrame(withOuterError);

    expect(isRecord(normalized) && Object.hasOwn(normalized, 'error')).toBe(false);
    expect(normalized).toEqual({
      success: true,
      message: 'Animation Blueprint created.',
      assetPath: ASSET_PATH,
    });
  });

  it('preserves a domain `error` field the handler result itself owns', () => {
    const partial = frame(
      'Created with warnings.',
      { assetPath: ASSET_PATH, error: 'THUMBNAIL_FAILED' },
      { error: '' },
    );

    expect(normalizeAutomationFrame(partial)).toEqual({
      success: true,
      message: 'Created with warnings.',
      assetPath: ASSET_PATH,
      error: 'THUMBNAIL_FAILED',
    });
  });

  it('does not mutate the result it was given', () => {
    const original = gatewayFrame({ error: '' });
    const snapshot = structuredClone(original);

    normalizeAutomationFrame(original);
    expect(original).toEqual(snapshot);
  });
});
