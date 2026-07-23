// Task 38 lane D — CHARACTERIZATION (GREEN baseline) for safe elicitation,
// secret/destructive exclusion, boolean high-impact consent, and the bounded
// direct-call-removal migration receipt. Every case must PASS; it locks the
// exact TS decision semantics the parity suite then holds the native surface to.

import { describe, expect, it, vi } from 'vitest';
import {
  collectSafeElicitableProps,
  elicitHighImpactConsent,
  isSafeToElicit,
} from '../../../src/server/tool-registry-elicitation.js';
import { buildDirectCallMigration } from '../../../src/server/gateway/direct-call-migration.js';

const logger = { debug: (): void => {} };

describe('Task 38 lane D — safe-to-elicit field policy (current TS behavior)', () => {
  it('refuses every secret/token/credential field name', () => {
    for (const field of ['token', 'apiKey', 'api_key', 'password', 'passwd', 'secret', 'secretValue',
      'credential', 'privateKey', 'bearerToken', 'authorization', 'accessKey']) {
      expect(isSafeToElicit(field)).toBe(false);
    }
  });

  it('refuses every destructive-confirmation field name', () => {
    for (const field of ['confirm', 'confirmDelete', 'force', 'forceDelete', 'delete', 'destroy',
      'purge', 'wipe', 'overwrite', 'drop']) {
      expect(isSafeToElicit(field)).toBe(false);
    }
  });

  it('allows ordinary safe primitive field names', () => {
    for (const field of ['name', 'assetPath', 'count', 'width', 'mode', 'destinationPath']) {
      expect(isSafeToElicit(field)).toBe(true);
    }
  });

  it('collects only missing, safe, primitive-typed required fields', () => {
    const schema = {
      properties: {
        name: { type: 'string' },
        token: { type: 'string' },
        confirmDelete: { type: 'boolean' },
        count: { type: 'integer' },
        mode: { enum: ['a', 'b'] },
        opts: { type: 'object', properties: { x: { type: 'string' } } },
      },
      required: ['name', 'token', 'confirmDelete', 'count', 'mode', 'opts'],
    };
    const collected = collectSafeElicitableProps(schema, {});
    expect(Object.keys(collected).sort()).toEqual(['count', 'mode', 'name']);
    expect(collected.token).toBeUndefined();
    expect(collected.confirmDelete).toBeUndefined();
    expect(collected.opts).toBeUndefined();
  });

  it('never re-elicits a field the caller already supplied', () => {
    const schema = { properties: { name: { type: 'string' }, count: { type: 'integer' } }, required: ['name', 'count'] };
    const collected = collectSafeElicitableProps(schema, { name: 'given' });
    expect(Object.keys(collected)).toEqual(['count']);
  });
});

describe('Task 38 lane D — high-impact boolean consent (current TS behavior)', () => {
  it('returns unsupported without eliciting when the client cannot elicit', async () => {
    const elicitFn = vi.fn();
    const decision = await elicitHighImpactConsent('delete asset', { hasElicitation: false }, elicitFn, 1000, logger);
    expect(decision).toEqual({ granted: false, reason: 'unsupported' });
    expect(elicitFn).not.toHaveBeenCalled();
  });

  it('grants only on an explicit consent:true and asks for a single boolean (never a secret)', async () => {
    const elicitFn = vi.fn(async (_message: string, schema: { properties: Record<string, unknown>; required: string[] }) => {
      expect(Object.keys(schema.properties)).toEqual(['consent']);
      expect(schema.required).toEqual(['consent']);
      expect((schema.properties.consent as { type: string }).type).toBe('boolean');
      return { ok: true, value: { consent: true } };
    });
    const decision = await elicitHighImpactConsent('delete asset', { hasElicitation: true }, elicitFn, 1000, logger);
    expect(decision).toEqual({ granted: true, reason: 'granted' });
    const message = elicitFn.mock.calls[0][0] as string;
    expect(message).toContain('cannot be undone');
  });

  it('stays blocked (declined) on consent:false, a missing value, a refusal, or a throw', async () => {
    const cases: Array<() => Promise<unknown>> = [
      async () => ({ ok: true, value: { consent: false } }),
      async () => ({ ok: true, value: {} }),
      async () => ({ ok: false }),
      async () => { throw new Error('rpc-failed'); },
    ];
    for (const elicitFn of cases) {
      const decision = await elicitHighImpactConsent('delete asset', { hasElicitation: true }, elicitFn, 1000, logger);
      expect(decision).toEqual({ granted: false, reason: 'declined' });
    }
  });
});

describe('Task 38 lane D — direct-call-removal migration receipt (current TS behavior)', () => {
  it('steers an unknown tool to search with bounded suggestions', () => {
    const receipt = buildDirectCallMigration('not_a_tool', {});
    expect(receipt.success).toBe(false);
    expect(receipt.operation).toBe('search');
    expect(receipt.errorCode).toBe('DIRECT_TOOL_CALL_REMOVED');
    expect(receipt.nextCall).toEqual({ operation: 'search' });
    expect(Array.isArray(receipt.suggestions)).toBe(true);
    expect((receipt.suggestions ?? []).length).toBeLessThanOrEqual(3);
  });

  it('steers a known tool with no action to describe', () => {
    const receipt = buildDirectCallMigration('manage_asset', {});
    expect(receipt.operation).toBe('describe');
    expect(receipt.nextCall).toEqual({ operation: 'describe', tool: 'manage_asset' });
  });

  it('steers a known tool with an action to a runnable execute call, stripping control fields', () => {
    const receipt = buildDirectCallMigration('manage_asset', {
      action: 'import_asset', subAction: 'ignored', operation: 'nope',
      params: { sourcePath: '/x' }, destinationPath: '/Game/y',
    });
    expect(receipt.operation).toBe('execute');
    expect(receipt.nextCall).toEqual({
      operation: 'execute', tool: 'manage_asset', action: 'import_asset',
      params: { sourcePath: '/x', destinationPath: '/Game/y' },
    });
  });

  it('does not mutate the caller arguments while migrating', () => {
    const args = { action: 'import_asset', params: { sourcePath: '/x' } };
    const snapshot = JSON.parse(JSON.stringify(args));
    buildDirectCallMigration('manage_asset', args);
    expect(args).toEqual(snapshot);
  });
});
