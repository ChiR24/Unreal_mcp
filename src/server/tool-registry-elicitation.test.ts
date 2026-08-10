import { describe, expect, it, vi } from 'vitest';
import {
  collectSafeElicitableProps,
  elicitHighImpactConsent,
  isSafeToElicit,
} from './tool-registry-elicitation.js';

const noopLogger = { debug: () => undefined };

describe('safe elicitation policy — field gating', () => {
  it('never elicits secret/token/credential fields', () => {
    for (const field of [
      'token', 'capabilityToken', 'apiKey', 'api_key', 'secret', 'clientSecret',
      'password', 'passwd', 'credential', 'privateKey', 'bearer', 'authorization', 'accessKey',
    ]) {
      expect(isSafeToElicit(field)).toBe(false);
    }
  });

  it('never elicits destructive confirmation values', () => {
    for (const field of ['confirm', 'confirmDelete', 'force', 'forceDelete', 'destroy', 'purge', 'wipe', 'overwrite', 'drop']) {
      expect(isSafeToElicit(field)).toBe(false);
    }
  });

  it('elicits ordinary safe primitive fields', () => {
    for (const field of ['assetPath', 'name', 'count', 'destinationPath', 'rotationDegrees']) {
      expect(isSafeToElicit(field)).toBe(true);
    }
  });

  it('collects only safe missing required primitives from a schema', () => {
    const schema = {
      type: 'object',
      properties: {
        name: { type: 'string' },
        token: { type: 'string' },
        confirmDelete: { type: 'boolean' },
        count: { type: 'number' },
      },
      required: ['name', 'token', 'confirmDelete', 'count'],
    };
    const props = collectSafeElicitableProps(schema, {});
    expect(Object.keys(props).sort()).toEqual(['count', 'name']);
  });
});

describe('high-impact consent elicitation', () => {
  it('stays blocked (unsupported) when the client cannot elicit', async () => {
    const elicitFn = vi.fn();
    const decision = await elicitHighImpactConsent('delete asset', { hasElicitation: false }, elicitFn, 1000, noopLogger);
    expect(decision).toEqual({ granted: false, reason: 'unsupported' });
    expect(elicitFn).not.toHaveBeenCalled();
  });

  it('grants only on an explicit boolean consent and never solicits a secret', async () => {
    let capturedSchema: { properties: Record<string, { type: string }> } | undefined;
    const elicitFn = async (_message: string, schema: { properties: Record<string, { type: string }> }) => {
      capturedSchema = schema;
      return { ok: true, value: { consent: true } };
    };
    const decision = await elicitHighImpactConsent('delete asset', { hasElicitation: true }, elicitFn, 1000, noopLogger);
    expect(decision).toEqual({ granted: true, reason: 'granted' });
    expect(Object.keys(capturedSchema?.properties ?? {})).toEqual(['consent']);
    expect(capturedSchema?.properties.consent.type).toBe('boolean');
  });

  it('stays blocked (declined) when the user refuses or omits consent', async () => {
    const declined = await elicitHighImpactConsent('delete', { hasElicitation: true }, async () => ({ ok: false }), 1000, noopLogger);
    expect(declined).toEqual({ granted: false, reason: 'declined' });
    const consentFalse = await elicitHighImpactConsent('delete', { hasElicitation: true }, async () => ({ ok: true, value: { consent: false } }), 1000, noopLogger);
    expect(consentFalse).toEqual({ granted: false, reason: 'declined' });
  });
});
