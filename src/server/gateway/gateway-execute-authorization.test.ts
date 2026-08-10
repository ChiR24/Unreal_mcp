import { describe, expect, it } from 'vitest';

import type { BridgeAuthority } from '../../automation/message-schema.js';
import type { ConsentGrant } from '../../tools/catalog/capabilities/semantic/authorization.js';
import { checkConsentAuthorization, checkScopeAuthorization } from './gateway-execute-policy.js';
import type { ExecuteTarget } from './gateway-execute-resolve.js';

const makeTarget = (requiredScope: string): ExecuteTarget =>
  ({
    record: {
      id: 'manage_asset.delete_asset',
      policy: { requiredScope },
      routing: { parentTool: 'manage_asset' }
    },
    legacy: { action: 'delete_asset' }
  }) as unknown as ExecuteTarget;

const authority = (scopes: string[] | undefined): BridgeAuthority => ({ scopes });

describe('checkScopeAuthorization — gated gateway scope fail-fast', () => {
  it('skips (admin) when no authority descriptor is present (no-token / old plugin)', () => {
    expect(checkScopeAuthorization(makeTarget('destructive'), undefined)).toBeUndefined();
  });

  it('skips (admin) when the descriptor advertises no scope set', () => {
    expect(checkScopeAuthorization(makeTarget('destructive'), authority(undefined))).toBeUndefined();
  });

  it('admin wildcard authorizes any required scope', () => {
    expect(checkScopeAuthorization(makeTarget('destructive'), authority(['admin']))).toBeUndefined();
  });

  it('exact membership authorizes the held scope', () => {
    expect(checkScopeAuthorization(makeTarget('read'), authority(['read', 'write']))).toBeUndefined();
  });

  it('refuses with SCOPE_NOT_GRANTED when the exact scope is missing (write does not imply destructive)', () => {
    const failure = checkScopeAuthorization(makeTarget('destructive'), authority(['read', 'write']));
    expect(failure?.errorCode).toBe('SCOPE_NOT_GRANTED');
    expect(failure?.requiredScope).toBe('destructive');
    expect(failure?.grantedScopes).toEqual(['read', 'write']);
    expect(failure?.nextCall).toBeDefined();
    expect(JSON.stringify(failure)).not.toContain('token');
  });

  it('refuses a read principal attempting a write-scoped action', () => {
    const failure = checkScopeAuthorization(makeTarget('write'), authority(['read']));
    expect(failure?.errorCode).toBe('SCOPE_NOT_GRANTED');
    expect(failure?.requiredScope).toBe('write');
  });
});

const makeConsentTarget = (consent: string): ExecuteTarget =>
  ({
    record: {
      id: 'manage_asset.delete_asset',
      policy: { requiredScope: 'destructive', consent },
      routing: { parentTool: 'manage_asset' }
    },
    legacy: { action: 'delete_asset' }
  }) as unknown as ExecuteTarget;

const grant = (capability: string, acknowledge: 'explicit' | 'elevated'): ConsentGrant =>
  ({ capability, acknowledge }) as unknown as ConsentGrant;

describe('checkConsentAuthorization — gated, capability-bound, no inference', () => {
  it('skips when no scoped authority descriptor is present', () => {
    expect(checkConsentAuthorization(makeConsentTarget('explicit'), undefined, undefined)).toBeUndefined();
  });

  it('policy none passes without any grant', () => {
    expect(checkConsentAuthorization(makeConsentTarget('none'), authority(['admin']), undefined)).toBeUndefined();
  });

  it('explicit policy refuses with CONSENT_REQUIRED when no grant is supplied', () => {
    const failure = checkConsentAuthorization(makeConsentTarget('explicit'), authority(['admin']), undefined);
    expect(failure?.errorCode).toBe('CONSENT_REQUIRED');
    expect(failure?.nextCall).toBeDefined();
  });

  it('explicit policy is satisfied by an explicit grant for THIS capability', () => {
    expect(
      checkConsentAuthorization(makeConsentTarget('explicit'), authority(['admin']), grant('manage_asset.delete_asset', 'explicit'))
    ).toBeUndefined();
  });

  it('a grant for a DIFFERENT capability never satisfies consent (no inference)', () => {
    const failure = checkConsentAuthorization(makeConsentTarget('explicit'), authority(['admin']), grant('manage_asset.rename_asset', 'explicit'));
    expect(failure?.errorCode).toBe('CONSENT_REQUIRED');
  });

  it('elevated policy requires elevated — explicit is insufficient', () => {
    const failure = checkConsentAuthorization(makeConsentTarget('elevated'), authority(['admin']), grant('manage_asset.delete_asset', 'explicit'));
    expect(failure?.errorCode).toBe('CONSENT_REQUIRED');
  });

  it('elevated policy is satisfied by an elevated grant', () => {
    expect(
      checkConsentAuthorization(makeConsentTarget('elevated'), authority(['admin']), grant('manage_asset.delete_asset', 'elevated'))
    ).toBeUndefined();
  });
});
