// Task 39 — strict typed contract for the correlated receipt and the one typed
// error algebra. Written failing-first: the revision schemas, the new error
// kinds, and the enriched receipt fields do not exist yet, so every assertion
// below is a clean RED against the current semantic layer. Builder inputs are
// routed through `unknown`-cast helpers so the RED lives at the runtime
// assertion boundary rather than at tsc (the new fields are not yet typed).

import { describe, expect, it } from 'vitest';

import { CapabilityIdSchema } from '../identifiers.js';

import {
  buildErrorReceipt,
  buildSuccessReceipt,
  ReceiptSchema,
  serializeReceipt,
  type Receipt
} from './envelope.js';
import { CorrelationIdSchema, IdempotencyKeySchema } from './ids.js';
import * as ids from './ids.js';
import { SemanticErrorSchema } from './errors.js';
import { maskSecrets, maskSecretsDeep } from './receipt-redaction.js';

const CAP = CapabilityIdSchema.parse('asset.import');
const CORRELATION = CorrelationIdSchema.parse('gw-42');
const IDEMPOTENCY = IdempotencyKeySchema.parse('idem-1');
const HEX16 = '740752bc2cdcb7b9';
const HEX64 = 'a'.repeat(64);
const SCHEMA_HEX64 = 'b'.repeat(64);
const CATALOG_REVISION = ids.CatalogRevisionSchema.parse(HEX16);
const CAPABILITY_REVISION = ids.CapabilityRevisionSchema.parse(HEX64);
const SCHEMA_REVISION = ids.SchemaRevisionSchema.parse(SCHEMA_HEX64);
const REQUEST_ONE = ids.RequestIdSchema.parse('str:req-1');
const REQUEST_TWO = ids.RequestIdSchema.parse('str:req-2');
const LIVE_REVISIONS = { selection: 2, level: 3, assetRegistry: 4, package: 5 } as const;

const buildSuccess = (input: Parameters<typeof buildSuccessReceipt>[0]): Receipt =>
  buildSuccessReceipt(input);
const buildError = (input: Parameters<typeof buildErrorReceipt>[0]): Receipt =>
  buildErrorReceipt(input);

function schemaOf(name: string): { safeParse: (v: unknown) => { success: boolean } } {
  return (ids as Record<string, unknown>)[name] as { safeParse: (v: unknown) => { success: boolean } };
}

describe('task39 ids: revision fields are bounded, distinct, hex strings', () => {
  it('accepts the live catalog digest string on CatalogRevisionSchema (no longer a number brand)', () => {
    expect(ids.CatalogRevisionSchema.safeParse(HEX16).success).toBe(true);
  });

  it('rejects a numeric catalog revision (the old number branding is gone)', () => {
    expect(ids.CatalogRevisionSchema.safeParse(123).success).toBe(false);
  });

  it('mints a bounded CapabilityRevisionSchema over the record content hash', () => {
    const schema = schemaOf('CapabilityRevisionSchema');
    expect(schema).toBeDefined();
    expect(schema.safeParse(HEX64).success).toBe(true);
    expect(schema.safeParse('not-hex').success).toBe(false);
    expect(schema.safeParse('a'.repeat(200)).success).toBe(false);
  });

  it('mints a bounded SchemaRevisionSchema over the record schema hash', () => {
    const schema = schemaOf('SchemaRevisionSchema');
    expect(schema).toBeDefined();
    expect(schema.safeParse(SCHEMA_HEX64).success).toBe(true);
    expect(schema.safeParse('').success).toBe(false);
  });

  it('mints a bounded RequestIdSchema for the external MCP request id', () => {
    const schema = schemaOf('RequestIdSchema');
    expect(schema).toBeDefined();
    expect(schema.safeParse('str:abc-1').success).toBe(true);
    expect(schema.safeParse('').success).toBe(false);
    expect(schema.safeParse('x'.repeat(4000)).success).toBe(false);
  });
});

describe('task39 errors: the additive discriminated algebra covers every plan class', () => {
  const validErrorFixtures: readonly Record<string, unknown>[] = [
    { kind: 'validation', code: 'VALIDATION_ERROR', message: 'v' },
    { kind: 'capability', code: 'CAPABILITY_DISABLED', message: 'disabled', retryable: false },
    { kind: 'capability', code: 'CAPABILITY_UNAVAILABLE', message: 'gone', retryable: false },
    { kind: 'consent', code: 'CONSENT_REQUIRED', message: 'consent', scope: 'destructive' },
    { kind: 'staleState', code: 'STALE_STATE', message: 'stale', currentRevision: HEX16, expectedRevision: 'deadbeef' },
    { kind: 'conflict', code: 'STATE_CONFLICT', message: 'conflict' },
    { kind: 'cancellation', code: 'OPERATION_CANCELLED', message: 'cancelled' },
    { kind: 'dispatch', code: 'NOT_CONNECTED', message: 'not connected', retryable: true },
    { kind: 'dispatch', code: 'DISPATCH_ERROR', message: 'routing', retryable: false },
    { kind: 'output', code: 'OUTPUT_SCHEMA_VIOLATION', message: 'bad output', pointer: '/x' },
    { kind: 'output', code: 'RESULT_TOO_LARGE', message: 'too big', resultChars: 150_000 }
  ];

  const legacyErrorFixtures: readonly Record<string, unknown>[] = [
    { kind: 'path', code: 'PATH_TRAVERSAL', message: 't', input: '/x/..' },
    { kind: 'option', code: 'UNSUPPORTED_OPTION', option: 'o', supported: ['timeoutMs'], message: 'no' },
    { kind: 'handle', code: 'HANDLE_KIND_MISMATCH', expected: 'actor', received: 'component', message: 'k' },
    { kind: 'range', code: 'OUT_OF_RANGE', field: 'r', message: 'oob' },
    { kind: 'timeout', code: 'TIMEOUT_EXCEEDED', message: 'to', boundMs: 1000 },
    { kind: 'execution', code: 'EXECUTION_ERROR', message: 'x', retryable: false },
    { kind: 'unknown', code: 'UNKNOWN_ERROR', message: '?' }
  ];

  it('parses a valid instance of every plan error class', () => {
    for (const error of validErrorFixtures) {
      expect(SemanticErrorSchema.safeParse(error).success, `${String(error.kind)}/${String(error.code)}`).toBe(true);
    }
  });

  it('still parses every preserved legacy variant', () => {
    for (const error of legacyErrorFixtures) {
      expect(SemanticErrorSchema.safeParse(error).success, String(error.kind)).toBe(true);
    }
  });

  it('rejects a malformed instance of the new kinds', () => {
    expect(SemanticErrorSchema.safeParse({ kind: 'output', code: 'WRONG', message: 'x' }).success).toBe(false);
    expect(SemanticErrorSchema.safeParse({ kind: 'capability', code: 'CAPABILITY_DISABLED', message: 'x' }).success).toBe(false);
    expect(SemanticErrorSchema.safeParse({ kind: 'staleState', code: 'STALE_STATE' }).success).toBe(false);
    expect(SemanticErrorSchema.safeParse({ kind: 'bogus', code: 'X', message: 'x' }).success).toBe(false);
  });
});

describe('task39 envelope: the enriched receipt carries correlation, ids and revisions', () => {
  it('round-trips a fully enriched success receipt through ReceiptSchema', () => {
    const receipt = buildSuccess({
      capabilityId: CAP,
      data: { assetPath: '/Game/A' },
      correlationId: CORRELATION,
      requestId: REQUEST_ONE,
      idempotencyId: IDEMPOTENCY,
      catalogRevision: CATALOG_REVISION,
      capabilityRevision: CAPABILITY_REVISION,
      schemaRevision: SCHEMA_REVISION,
      timingMs: 12,
      validation: { outputSchema: 'passed', level: 'strict' }
    });

    expect(ReceiptSchema.safeParse(receipt).success).toBe(true);
    const view = receipt as Record<string, unknown>;
    expect(view.correlationId).toBe(CORRELATION);
    expect(view.requestId).toBe('str:req-1');
    expect(view.catalogRevision).toBe(HEX16);
    expect(view.capabilityRevision).toBe(HEX64);
    expect(view.schemaRevision).toBe(SCHEMA_HEX64);
    expect(view.timingMs).toBe(12);
    expect(view.validation).toEqual({ outputSchema: 'passed', level: 'strict' });
  });

  it('carries correlation, request id and revisions on an error receipt', () => {
    const receipt = buildError({
      capabilityId: CAP,
      error: { kind: 'output', code: 'OUTPUT_SCHEMA_VIOLATION', message: 'bad', pointer: '/x' },
      correlationId: CORRELATION,
      requestId: REQUEST_TWO,
      catalogRevision: CATALOG_REVISION,
      capabilityRevision: CAPABILITY_REVISION,
      schemaRevision: SCHEMA_REVISION,
      timingMs: 3
    });

    expect(ReceiptSchema.safeParse(receipt).success).toBe(true);
    const view = receipt as Record<string, unknown>;
    expect(view.requestId).toBe('str:req-2');
    expect(view.capabilityRevision).toBe(HEX64);
    expect(view.schemaRevision).toBe(SCHEMA_HEX64);
  });

  it('carries the same strict live revision snapshot on success and error receipts', () => {
    const success = buildSuccess({ capabilityId: CAP, data: {}, liveRevisions: LIVE_REVISIONS });
    const error = buildError({
      capabilityId: CAP,
      error: { kind: 'execution', code: 'UNREAL_ENGINE_ERROR', message: 'failed', retryable: false },
      liveRevisions: LIVE_REVISIONS
    });

    expect(ReceiptSchema.safeParse(success).success).toBe(true);
    expect(ReceiptSchema.safeParse(error).success).toBe(true);
    expect((success as Record<string, unknown>).liveRevisions).toEqual(LIVE_REVISIONS);
    expect((error as Record<string, unknown>).liveRevisions).toEqual(LIVE_REVISIONS);
  });

  it('rejects receipt snapshots that omit or add a live revision key', () => {
    const success = buildSuccess({ capabilityId: CAP, data: {} });
    expect(ReceiptSchema.safeParse({
      ...success,
      liveRevisions: { selection: 2, level: 3, assetRegistry: 4 }
    }).success).toBe(false);
    expect(ReceiptSchema.safeParse({
      ...success,
      liveRevisions: { ...LIVE_REVISIONS, futureState: 6 }
    }).success).toBe(false);
  });
});

describe('task39 envelope: adversarial bounds and secret redaction', () => {
  it('bounds an oversized changes array on the receipt', () => {
    const receipt = buildSuccess({
      capabilityId: CAP,
      data: {},
      changes: Array.from({ length: 5000 }, (_v, i) => `/Game/Actor_${i}`)
    });
    if (receipt.status !== 'success') throw new Error('expected success');
    expect(receipt.changes.length).toBeLessThanOrEqual(200);
  });

  it('redacts a secret-looking token in a warning instead of leaking it', () => {
    const receipt = buildSuccess({
      capabilityId: CAP,
      data: {},
      warnings: ['auth token=abcdef0123456789abcdef0123456789 was rotated']
    });
    const serialized = serializeReceipt(receipt);
    expect(serialized).not.toContain('abcdef0123456789abcdef0123456789');
    expect(serialized).toContain('[REDACTED]');
  });

  it('truncates an over-long warning string', () => {
    const receipt = buildSuccess({
      capabilityId: CAP,
      data: {},
      warnings: ['w'.repeat(10_000)]
    });
    if (receipt.status !== 'success') throw new Error('expected success');
    expect(receipt.warnings[0]?.length ?? 0).toBeLessThanOrEqual(2048);
  });
});

describe('task39 redaction: secret masking is identical for every credential shape (TS/native parity)', () => {
  const SECRET = 'sk-supersecret-abcdef0123456789';

  it('masks the TOKEN in an "Authorization: Bearer <token>" header, not just the scheme word', () => {
    const masked = maskSecrets(`Authorization: Bearer ${SECRET}`);
    expect(masked).not.toContain(SECRET);
    expect(masked).toContain('[REDACTED]');
  });

  it('masks a bare "Bearer <token>" outside an assignment', () => {
    const masked = maskSecrets(`Bearer ${SECRET}`);
    expect(masked).not.toContain(SECRET);
    expect(masked).toContain('Bearer [REDACTED]');
  });

  it('masks a JSON-like quoted assignment ("token":"<value>")', () => {
    const masked = maskSecrets(`{"token":"${SECRET}","keep":"public"}`);
    expect(masked).not.toContain(SECRET);
    expect(masked).toContain('[REDACTED]');
    expect(masked).toContain('public');
  });

  it('masks a JSON-like nested authorization Bearer value', () => {
    const masked = maskSecrets(`{"authorization":"Bearer ${SECRET}"}`);
    expect(masked).not.toContain(SECRET);
    expect(masked).toContain('[REDACTED]');
  });

  it('masks bare keyword assignments (token=, secret:, password=, api_key:)', () => {
    for (const line of [`token=${SECRET}`, `secret: ${SECRET}`, `password=${SECRET}`, `api_key: ${SECRET}`]) {
      const masked = maskSecrets(line);
      expect(masked, line).not.toContain(SECRET);
      expect(masked, line).toContain('[REDACTED]');
    }
  });

  it('deep-masks a secret in a nested object VALUE under a secret-shaped string leaf while preserving legitimate data', () => {
    const masked = maskSecretsDeep({
      outer: { note: `Authorization: Bearer ${SECRET}` },
      list: [`token=${SECRET}`],
      keep: '/Game/Meshes/SM_Rock'
    }) as Record<string, unknown>;
    const serialized = JSON.stringify(masked);
    expect(serialized).not.toContain(SECRET);
    expect(serialized).toContain('[REDACTED]');
    expect(serialized).toContain('/Game/Meshes/SM_Rock');
  });

  it('leaves ordinary prose untouched (no over-masking)', () => {
    const prose = 'The token bucket refilled and the password field is required.';
    expect(maskSecrets(prose)).toBe(prose);
  });
});

// Key-aware deep masking. The string-shaped masker only fires on `keyword<sep>value`
// INSIDE one string, so a credential that arrives as a real JSON value under a
// secret-named key carries no keyword/separator context and survives untouched.
// The invariant is absolute — a secret must never reach a receipt — so the key
// name alone has to be sufficient, whatever shape hangs beneath it.
describe('receipt redaction — a secret-named KEY masks its value whatever its shape', () => {
  const SECRET = 'sk-supersecret-abcdef0123456789';

  it('masks a plain string value under a secret-named key (no keyword in the value)', () => {
    const masked = maskSecretsDeep({ capabilityToken: SECRET, nested: { password: 'p' } });

    expect(JSON.stringify(masked)).not.toContain(SECRET);
    expect((masked as Record<string, unknown>).capabilityToken).toBe('[REDACTED]');
    expect((masked as { nested: Record<string, unknown> }).nested.password).toBe('[REDACTED]');
  });

  it('masks a STRUCTURED value (object) under a secret-named key', () => {
    const masked = maskSecretsDeep({ credentials: { user: 'u', password: SECRET } });

    expect(JSON.stringify(masked)).not.toContain(SECRET);
    expect((masked as Record<string, unknown>).credentials).toBe('[REDACTED]');
  });

  it('masks a STRUCTURED value (array) under a secret-named key', () => {
    const masked = maskSecretsDeep({ apiKeys: [SECRET, { rotated: SECRET }] });

    expect(JSON.stringify(masked)).not.toContain(SECRET);
    expect((masked as Record<string, unknown>).apiKeys).toBe('[REDACTED]');
  });

  it('masks a non-string leaf under a secret-named key (numbers leak too)', () => {
    const masked = maskSecretsDeep({ pwd: 8675309, authorization: null });

    expect((masked as Record<string, unknown>).pwd).toBe('[REDACTED]');
    expect((masked as Record<string, unknown>).authorization).toBe('[REDACTED]');
  });

  it('masks the whole subtree the moment a secret-named key appears at any depth', () => {
    const masked = maskSecretsDeep({
      a: { b: { c: { api_key: { primary: SECRET, backup: [SECRET] } } } },
      keep: '/Game/Meshes/SM_Rock',
    });

    expect(JSON.stringify(masked)).not.toContain(SECRET);
    expect(JSON.stringify(masked)).toContain('/Game/Meshes/SM_Rock');
  });

  it('does not over-mask keys that merely embed a secret word inside a longer word', () => {
    const masked = maskSecretsDeep({
      tokenizer: 'lexer',
      passwordless: true,
      unauthorized: 'no',
      keep: '/Game/Meshes/SM_Rock',
    }) as Record<string, unknown>;

    expect(masked.tokenizer).toBe('lexer');
    expect(masked.passwordless).toBe(true);
    expect(masked.unauthorized).toBe('no');
    expect(masked.keep).toBe('/Game/Meshes/SM_Rock');
  });
});

// The reflection handlers (get_object_property, set_object_property,
// array_get_element, map_get_value) answer in a SPLIT shape: the property the
// caller named goes in `propertyName`, and its value goes in `value`. Both halves
// of the key-name rule miss it — `value` names nothing, and `propertyName` holds
// a name rather than a credential — so a read-scoped caller could once export the
// plugin's own CapabilityToken through a fully redacted pipeline. The name-bearing
// sibling therefore decides for the generic carriers in the same object.
describe('receipt redaction — a credential NAMED by a sibling masks the generic value', () => {
  const SECRET = 'sk-supersecret-abcdef0123456789';

  it('masks `value` when `propertyName` names a credential', () => {
    const masked = maskSecretsDeep({
      propertyName: 'CapabilityToken',
      value: SECRET,
      objectPath: '/Script/McpAutomationBridge.Default__McpAutomationBridgeSettings',
    }) as Record<string, unknown>;

    expect(JSON.stringify(masked)).not.toContain(SECRET);
    expect(masked.value).toBe('[REDACTED]');
    // The NAME is the question, not the answer — masking it would hide which
    // property was read while telling the caller nothing they did not send.
    expect(masked.propertyName).toBe('CapabilityToken');
  });

  it('covers every generic carrier and every name-bearing field the handlers use', () => {
    for (const nameKey of ['propertyName', 'propertyPath', 'property', 'field', 'key', 'name']) {
      for (const valueKey of ['value', 'values', 'currentValue', 'previousValue', 'element', 'result']) {
        const masked = maskSecretsDeep({
          [nameKey]: 'ScopedCapabilityTokens',
          [valueKey]: SECRET,
        }) as Record<string, unknown>;
        expect(masked[valueKey], `${nameKey}/${valueKey}`).toBe('[REDACTED]');
      }
    }
  });

  it('masks a STRUCTURED value named by a sibling, not just a string', () => {
    const masked = maskSecretsDeep({
      propertyName: 'ScopedCapabilityTokens',
      value: [{ profile: 'reader', token: SECRET }],
    }) as Record<string, unknown>;

    expect(JSON.stringify(masked)).not.toContain(SECRET);
    expect(masked.value).toBe('[REDACTED]');
  });

  it('applies at depth, inside the result envelope the handlers actually emit', () => {
    const masked = maskSecretsDeep({
      success: true,
      data: { propertyName: 'CapabilityToken', value: SECRET },
    });

    expect(JSON.stringify(masked)).not.toContain(SECRET);
  });

  it('does NOT mask when the sibling names something ordinary (no over-masking)', () => {
    const masked = maskSecretsDeep({
      propertyName: 'StaticMesh',
      value: '/Game/Meshes/SM_Rock',
      name: 'Cube',
    }) as Record<string, unknown>;

    expect(masked.value).toBe('/Game/Meshes/SM_Rock');
    expect(masked.name).toBe('Cube');
  });

  it('does NOT let a non-string sibling or an unrelated key trigger masking', () => {
    const masked = maskSecretsDeep({
      propertyName: { nested: 'CapabilityToken' },
      description: 'CapabilityToken',
      value: 'PLAIN',
    }) as Record<string, unknown>;

    expect(masked.value).toBe('PLAIN');
  });

  it('spares a measurement named by a sibling, matching the key-name rule', () => {
    const masked = maskSecretsDeep({ propertyName: 'TokenCount', value: 7 }) as Record<
      string,
      unknown
    >;

    expect(masked.value).toBe(7);
  });

  it('spares a BOOLEAN or NUMBER sibling value (they cannot carry a credential)', () => {
    const masked = maskSecretsDeep({
      propertyName: 'CapabilityToken',
      value: false,
      name: 42,
    }) as Record<string, unknown>;

    expect(masked.value, 'a boolean sibling is not a credential carrier').toBe(false);
    expect(masked.name, 'a numeric sibling is not a credential carrier').toBe(42);
  });

  it('still masks a string sibling value when the name-bearing field names a credential', () => {
    const masked = maskSecretsDeep({
      propertyName: 'CapabilityToken',
      value: SECRET,
    }) as Record<string, unknown>;

    expect(masked.value).toBe('[REDACTED]');
  });

  it('classifies an over-long sibling name promptly instead of O(n^2)-splitting it', () => {
    // The compound splitter is O(n^2) on a camelCase-free run, and the sibling
    // name is caller-supplied; a 100 KB run of `a` must be skipped, not split.
    const longName = 'a'.repeat(100_000);
    const start = performance.now();
    const masked = maskSecretsDeep({
      propertyName: longName,
      value: SECRET,
    }) as Record<string, unknown>;

    expect(masked.value, 'an over-long name must not trigger sibling masking').toBe(SECRET);
    expect(performance.now() - start, 'a 100 KB name must not blow up the classifier').toBeLessThan(
      1000
    );
  });
});

// The sibling classifier delegates to the same key-name classifier a key would
// face, so TS and native must agree on which NAME-BEARING values classify as
// credentials — a divergence would mask a reflection reply on one transport and
// ship it in the clear on the other. This fixture pins the shared vocabulary
// (compound tails included) so the two mirrors cannot drift.
describe('receipt redaction — sibling-name classifier parity fixture (TS/native)', () => {
  const SECRET = 'sk-supersecret-abcdef0123456789';

  // Separator-less runs whose halves are NOT in the closed vocabulary
  // (`capabilitytoken`, `CAPABILITYTOKEN`) are deliberately absent here: no
  // transport's classifier recognises them, and the fix for that spelling is
  // the canonical-name ECHO in the reflection handlers (the resolved
  // `CapabilityToken` is what the classifier sees), not vocabulary expansion.
  const CREDENTIAL_NAMES = [
    'CapabilityToken', 'ScopedCapabilityTokens',
    'SecretKey', 'ApiAccessToken', 'apiaccesstoken', 'AccessKey',
    'passwordHash', 'passwordhash', 'credentialBytes', 'credentialbytes',
    'authorizationHeader', 'authorizationheader', 'secretBlob', 'secretblob',
    'signingKey', 'PrivateKey', 'ClientSecret', 'RefreshToken', 'OAuthToken',
  ] as const;

  const ORDINARY_NAMES = [
    'StaticMesh', 'ActorLocation', 'SkeletalMesh', 'MaterialSlot',
    'TokenCount', 'TokenLimit', 'TokenIndex', 'TokenBudget',
    'SecretName', 'SecretVersion', 'AuthorizationRequired', 'AuthorizationScheme',
  ] as const;

  for (const name of CREDENTIAL_NAMES) {
    it(`names \`${name}\` a credential (masks the sibling value)`, () => {
      const masked = maskSecretsDeep({ propertyName: name, value: SECRET }) as Record<
        string,
        unknown
      >;

      expect(masked.value, `${name} must mask its generic sibling`).toBe('[REDACTED]');
    });
  }

  for (const name of ORDINARY_NAMES) {
    it(`names \`${name}\` ordinary (spares the sibling value)`, () => {
      const masked = maskSecretsDeep({ propertyName: name, value: SECRET }) as Record<
        string,
        unknown
      >;

      expect(masked.value, `${name} must not mask its generic sibling`).toBe(SECRET);
    });
  }
});

// A head-word rule alone once un-masked every one of these: the head (`key`,
// `hash`, `header`, `bytes`) is not itself a secret word, yet each compound
// names the credential rather than a fact about it. Masking must therefore be
// fail-closed, and these shapes pin that direction so the two failure modes
// cannot be traded for one another again.
describe('receipt redaction — compounds whose head is a carrier, not a secret word', () => {
  const SECRET = 'sk-supersecret-abcdef0123456789';

  const CREDENTIAL_KEYS = [
    'secretKey', 'secret_key', 'SecretAccessKey', 'secretAccessKey',
    'accessKey', 'ACCESS_KEY', 'privateKey', 'private_key', 'signingKey',
    'passwordHash', 'PASSWORD_HASH', 'passwordDigest', 'passwordBytes',
    'authorizationHeader', 'apiKeyHash', 'credentialBytes', 'tokenPayload',
    'tokenBase64', 'secretBlob', 'privateKeyBytes',
  ] as const;

  for (const key of CREDENTIAL_KEYS) {
    it(`masks \`${key}\``, () => {
      const masked = maskSecretsDeep({ [key]: SECRET }) as Record<string, unknown>;

      expect(JSON.stringify(masked)).not.toContain(SECRET);
      expect(masked[key]).toBe('[REDACTED]');
    });
  }

  const MEASUREMENT_KEYS = [
    'tokenCount', 'tokenLimit', 'tokenTotal', 'tokenIndex', 'tokenBudget',
    'secretsFound', 'secretStatus', 'secretName', 'secretVersion',
    'authorizationRequired', 'authorizationScheme', 'credentialType',
    'passwordPolicy', 'passwordMinLength', 'accessKeyCount',
  ] as const;

  for (const key of MEASUREMENT_KEYS) {
    it(`leaves \`${key}\` intact`, () => {
      const masked = maskSecretsDeep({ [key]: 'PLAIN' }) as Record<string, unknown>;

      expect(masked[key]).toBe('PLAIN');
    });
  }

  it('still peels a transparent head to reach the secret beneath it', () => {
    const masked = maskSecretsDeep({
      secretValue: SECRET,
      tokenString: SECRET,
      passwordData: SECRET,
    }) as Record<string, unknown>;

    expect(JSON.stringify(masked)).not.toContain(SECRET);
    expect(masked.secretValue).toBe('[REDACTED]');
    expect(masked.tokenString).toBe('[REDACTED]');
    expect(masked.passwordData).toBe('[REDACTED]');
  });
});

// A run that lost its separators is the case the compound splitter exists for:
// `secretKey` and `SECRET_KEY` were always masked while `SECRETKEY` escaped.
// Three-part runs and carrier tails are pinned here because a two-way split
// over the old vocabulary could reach neither.
describe('receipt redaction — separator-less credential runs', () => {
  const SECRET = 'sk-supersecret-abcdef0123456789';

  const CONCATENATED_KEYS = [
    'SECRETKEY', 'ACCESSTOKEN', 'PASSWORDHASH', 'passwordhash',
    'SECRETBYTES', 'secretdigest', 'PRIVATEKEYBYTES', 'APIACCESSTOKEN',
    // Three-part runs whose credential word is the LEADING part and whose tail is
    // two qualifiers/transparent heads. The predicate must judge the whole triple
    // (never a sub-split), or the native surface masks less than stdio.
    'SECRETUSERDATA', 'SECRETACCESSAPI', 'TOKENAPPOAUTH',
  ] as const;

  for (const key of CONCATENATED_KEYS) {
    it(`masks \`${key}\``, () => {
      const masked = maskSecretsDeep({ [key]: SECRET }) as Record<string, unknown>;

      expect(JSON.stringify(masked)).not.toContain(SECRET);
      expect(masked[key]).toBe('[REDACTED]');
    });
  }

  // The closed vocabulary and the measurement head are the only things standing
  // between a wider split and over-masking, so both directions are pinned.
  const SPARED_KEYS = [
    'TOKENCOUNT', 'tokencount', 'tokenizer', 'passwordless', 'unauthorized',
  ] as const;

  for (const key of SPARED_KEYS) {
    it(`leaves \`${key}\` intact`, () => {
      const masked = maskSecretsDeep({ [key]: 'PLAIN' }) as Record<string, unknown>;

      expect(masked[key]).toBe('PLAIN');
    });
  }
});
