// Task 27 — executable specification of the canonical gateway execute pipeline.
//
// This is the reference the native `/mcp` implementation must agree with. The
// stage order is normative: form -> resolve -> availability -> options ->
// defaults -> input schema -> dispatch -> output schema -> receipt. Any stage
// that fails returns an error receipt and the request never reaches the queue.

import { CATALOG_REVISION } from '../../../src/tools/catalog/capabilities/generated/canonical-registry.generated.js';

import {
  applyDefaults,
  validateAgainstSubset,
  VIOLATION_GATEWAY_CODES,
  type SchemaViolation
} from './schema-subset.js';

// Mirrors src/tools/catalog/capabilities/semantic/execution-options.ts (Task 3).
export const EXECUTION_OPTION_KEYS = [
  'idempotencyKey',
  'expectedCatalogRevision',
  'preview',
  'savePolicy',
  'timeoutMs',
  'validationLevel',
  'taskPreference'
] as const;

export const MAX_TIMEOUT_MS = 600_000;

export type SemanticErrorKind = 'validation' | 'option' | 'range' | 'execution';

export type SemanticFailure = {
  readonly kind: SemanticErrorKind;
  readonly code: string;
  readonly message: string;
  readonly pointer?: string;
  readonly option?: string;
  readonly field?: string;
  readonly gatewayCode: string;
};

export type ExecuteReceipt =
  | {
      readonly status: 'success';
      readonly capabilityId: string;
      readonly catalogRevision: string;
      readonly dispatch: { readonly action: string; readonly params: Record<string, unknown> };
      readonly data: unknown;
    }
  | {
      readonly status: 'error';
      readonly capabilityId: string;
      readonly catalogRevision: string;
      readonly error: SemanticFailure;
      readonly reachedQueue: false;
    };

export type CapabilityLike = {
  readonly id: string;
  readonly aliases: readonly string[];
  readonly legacyIds: readonly { readonly tool: string; readonly action: string }[];
  readonly schemas: { readonly input: unknown; readonly output: unknown };
  readonly routing: { readonly parentTool: string; readonly dispatchAction: string };
};

export type ResolverIndex = {
  readonly byId: ReadonlyMap<string, CapabilityLike>;
  readonly byAlias: ReadonlyMap<string, readonly string[]>;
  readonly byLegacy: ReadonlyMap<string, CapabilityLike>;
};

export const legacyKey = (tool: string, action: string): string => `${tool}\u0000${action}`;

export function buildResolverIndex(records: readonly CapabilityLike[]): ResolverIndex {
  const byId = new Map<string, CapabilityLike>();
  const byAlias = new Map<string, string[]>();
  const byLegacy = new Map<string, CapabilityLike>();
  for (const record of records) {
    byId.set(record.id, record);
    for (const alias of record.aliases) {
      const owners = byAlias.get(alias) ?? [];
      owners.push(record.id);
      byAlias.set(alias, owners);
    }
    for (const legacy of record.legacyIds) {
      byLegacy.set(legacyKey(legacy.tool, legacy.action), record);
    }
  }
  return { byId, byAlias, byLegacy };
}

const fail = (
  kind: SemanticErrorKind,
  code: string,
  gatewayCode: string,
  message: string,
  extra: { pointer?: string; option?: string; field?: string } = {}
): SemanticFailure => ({ kind, code, gatewayCode, message, ...extra });

const isRecord = (value: unknown): value is Record<string, unknown> =>
  typeof value === 'object' && value !== null && !Array.isArray(value);

const declaresProperty = (schema: unknown, name: string): boolean =>
  isRecord(schema) && isRecord(schema.properties) && name in schema.properties;

export type ResolveOutcome =
  | { readonly ok: true; readonly record: CapabilityLike }
  | { readonly ok: false; readonly error: SemanticFailure };

// v2 `{capability}` and generated legacy `{tool, action}` are both accepted. When
// both are present they must designate the SAME capability; a disagreement is a
// typed conflict, never a silent precedence win.
export function resolveCapability(request: Record<string, unknown>, index: ResolverIndex): ResolveOutcome {
  const capability = typeof request.capability === 'string' ? request.capability : undefined;
  const tool = typeof request.tool === 'string' ? request.tool : undefined;
  const action = typeof request.action === 'string' ? request.action : undefined;

  let fromCapability: CapabilityLike | undefined;
  if (capability !== undefined) {
    fromCapability = index.byId.get(capability);
    if (!fromCapability) {
      const owners = index.byAlias.get(capability) ?? [];
      if (owners.length > 1) {
        return {
          ok: false,
          error: fail('validation', 'VALIDATION_ERROR', 'ALIAS_CONFLICT',
            `Alias '${capability}' resolves to ${owners.length} capabilities: ${[...owners].sort().join(', ')}`)
        };
      }
      const resolvedId = owners[0];
      fromCapability = resolvedId === undefined ? undefined : index.byId.get(resolvedId);
    }
    if (!fromCapability) {
      return {
        ok: false,
        error: fail('validation', 'VALIDATION_ERROR', 'UNKNOWN_CAPABILITY',
          `Unknown capability '${capability}'. Call search before execute.`)
      };
    }
  }

  let fromLegacy: CapabilityLike | undefined;
  if (tool !== undefined && action !== undefined) {
    fromLegacy = index.byLegacy.get(legacyKey(tool, action));
    if (!fromLegacy) {
      return {
        ok: false,
        error: fail('validation', 'VALIDATION_ERROR', 'UNKNOWN_ACTION',
          `Unknown action '${action}' for tool '${tool}'. Call describe before execute.`)
      };
    }
  }

  if (fromCapability && fromLegacy && fromCapability.id !== fromLegacy.id) {
    return {
      ok: false,
      error: fail('validation', 'VALIDATION_ERROR', 'FORM_CONFLICT',
        `capability '${fromCapability.id}' conflicts with tool/action '${fromLegacy.id}'`)
    };
  }

  const record = fromCapability ?? fromLegacy;
  if (!record) {
    return {
      ok: false,
      error: fail('validation', 'VALIDATION_ERROR', 'UNKNOWN_CAPABILITY',
        'execute requires either capability or tool + action.')
    };
  }
  return { ok: true, record };
}

export function validateOptions(raw: unknown): SemanticFailure | undefined {
  if (raw === undefined || raw === null) return undefined;
  if (!isRecord(raw)) {
    return fail('validation', 'VALIDATION_ERROR', 'INVALID_OPTIONS', 'options must be an object.');
  }
  const supported = new Set<string>(EXECUTION_OPTION_KEYS);
  for (const key of Object.keys(raw)) {
    if (!supported.has(key)) {
      return fail('option', 'UNSUPPORTED_OPTION', 'UNSUPPORTED_OPTION',
        `Unsupported execution option '${key}'. Supported: [${EXECUTION_OPTION_KEYS.join(', ')}]`,
        { option: key });
    }
  }
  const timeout = raw.timeoutMs;
  if (timeout !== undefined) {
    if (typeof timeout !== 'number' || !Number.isInteger(timeout) || timeout <= 0 || timeout > MAX_TIMEOUT_MS) {
      return fail('range', 'OUT_OF_RANGE', 'OUT_OF_RANGE',
        `options.timeoutMs must be an integer in 1..${MAX_TIMEOUT_MS}`, { field: 'timeoutMs' });
    }
  }
  const preview = raw.preview;
  if (preview !== undefined && typeof preview !== 'boolean') {
    return fail('validation', 'VALIDATION_ERROR', 'INVALID_OPTIONS', 'options.preview must be a boolean.',
      { pointer: '/options/preview' });
  }
  return undefined;
}

const violationToFailure = (violation: SchemaViolation, outputBoundary = false): SemanticFailure => {
  const gatewayCode = outputBoundary
    ? 'OUTPUT_SCHEMA_VIOLATION'
    : VIOLATION_GATEWAY_CODES[violation.reason];
  return violation.reason === 'range'
    ? fail('range', 'OUT_OF_RANGE', gatewayCode, violation.message, { field: violation.pointer })
    : fail('validation', 'VALIDATION_ERROR', gatewayCode, violation.message, { pointer: violation.pointer });
};

export type DispatchResult = { readonly ok: boolean; readonly data: unknown; readonly detail?: unknown };

export type ExecuteDeps = {
  readonly index: ResolverIndex;
  readonly isEnabled: (parentTool: string) => boolean;
  readonly dispatch: (record: CapabilityLike, params: Record<string, unknown>) => DispatchResult;
};

export function executeReference(request: Record<string, unknown>, deps: ExecuteDeps): ExecuteReceipt {
  const revision = CATALOG_REVISION;
  const resolved = resolveCapability(request, deps.index);
  if (!resolved.ok) {
    return { status: 'error', capabilityId: '', catalogRevision: revision, error: resolved.error, reachedQueue: false };
  }
  const record = resolved.record;
  const errorReceipt = (error: SemanticFailure): ExecuteReceipt => ({
    status: 'error', capabilityId: record.id, catalogRevision: revision, error, reachedQueue: false
  });

  if (!deps.isEnabled(record.routing.parentTool)) {
    return errorReceipt(fail('execution', 'EXECUTION_ERROR', 'TOOL_DISABLED',
      `Capability '${record.id}' is disabled or unavailable.`));
  }

  const rawParams = request.params;
  if (rawParams !== undefined && !isRecord(rawParams)) {
    return errorReceipt(fail('validation', 'VALIDATION_ERROR', 'INVALID_PARAMS', 'params must be an object.'));
  }
  const params = isRecord(rawParams) ? rawParams : {};

  for (const reserved of ['action', 'subAction'] as const) {
    if (reserved in params) {
      return errorReceipt(fail('validation', 'VALIDATION_ERROR', 'INVALID_PARAMS',
        'params must not override action or subAction. Supply the selected action at the gateway level.'));
    }
  }
  for (const control of EXECUTION_OPTION_KEYS) {
    if (control in params) {
      return errorReceipt(fail('option', 'UNSUPPORTED_OPTION', 'UNSUPPORTED_OPTION',
        `Gateway control '${control}' must not appear in action params`, { option: control }));
    }
  }

  const optionFailure = validateOptions(request.options);
  if (optionFailure) return errorReceipt(optionFailure);

  // Canonical per-action schemas mostly omit `action` (the action IS the
  // capability), so it is injected for validation only where declared.
  const withDefaults = applyDefaults(params, record.schemas.input);
  const declaresAction = declaresProperty(record.schemas.input, 'action');
  const toValidate = declaresAction
    ? { ...withDefaults, action: record.routing.dispatchAction }
    : withDefaults;
  const inputViolation = validateAgainstSubset(toValidate, record.schemas.input);
  if (inputViolation) return errorReceipt(violationToFailure(inputViolation));

  const dispatchParams = {
    ...withDefaults,
    action: record.routing.dispatchAction,
    subAction: record.routing.dispatchAction
  };
  const result = deps.dispatch(record, dispatchParams);
  if (!result.ok) {
    return {
      status: 'error',
      capabilityId: record.id,
      catalogRevision: revision,
      // Structured Unreal detail is preserved verbatim alongside the typed error.
      error: { ...fail('execution', 'UNREAL_ENGINE_ERROR', 'UNREAL_EXECUTION_ERROR', String(result.data)), field: undefined },
      reachedQueue: false
    };
  }

  const outputViolation = validateAgainstSubset(result.data, record.schemas.output);
  if (outputViolation) {
    return errorReceipt(violationToFailure(outputViolation, true));
  }

  return {
    status: 'success',
    capabilityId: record.id,
    catalogRevision: revision,
    dispatch: { action: record.routing.dispatchAction, params: dispatchParams },
    data: result.data
  };
}
