// src/tools/catalog/capabilities/records/shared/record-presets.ts
// The presets every domain record builder shares: the schema envelope, the
// output header, and the effect-derived policy/behavior. `policy` encodes a
// security decision (which effect class demands consent), so one copy keeps
// every domain on the same contract. `routing` and the availability blocks
// legitimately differ per domain and stay with their builders.

import type { CapabilityBehaviorSource, CapabilityPolicy, Draft202012ObjectSchema, JsonObject } from '../../model.js';

/** JSON Schema dialect every record schema declares. */
export const SCHEMA_URI = 'https://json-schema.org/draft/2020-12/schema' as const;

/** Closes a property map into the object schema every record side declares. */
export function schema(
  properties: JsonObject,
  required: readonly string[] = [],
  requiredOneOf?: readonly string[],
): Draft202012ObjectSchema {
  return {
    $schema: SCHEMA_URI,
    type: 'object',
    properties,
    required: [...required],
    additionalProperties: false,
    ...(requiredOneOf === undefined ? {} : { requiredOneOf: [...requiredOneOf] }),
  };
}

const ACTION_PROP: JsonObject = { type: 'string', description: 'The action to execute on the parent tool.' };

/** Input schema led by the parent tool's `action` discriminator, always required. */
export function actionInputSchema(
  inputProps: JsonObject,
  required: readonly string[] = [],
  requiredOneOf?: readonly string[],
): Draft202012ObjectSchema {
  return schema({ action: ACTION_PROP, ...inputProps }, [...new Set(['action', ...required])], requiredOneOf);
}

// Every output carries a `details` reflection boundary: both gateways fold handler
// fields the contract does not name into it, so a read's payload survives projection.
export const OUTPUT_HEADER: Readonly<Record<'success' | 'message' | 'details', JsonObject>> = Object.freeze({
  success: { type: 'boolean', description: 'Whether the action succeeded.' },
  message: { type: 'string', description: 'Human-readable result message.' },
  details: { type: 'object', 'x-unreal-reflection-boundary': true, description: 'Additional handler result fields not named by the contract.' },
});

export function outputSchema(props: JsonObject, required: readonly string[] = []): Draft202012ObjectSchema {
  return schema({ ...OUTPUT_HEADER, ...props }, ['success', ...required]);
}

export const EMPTY_OUTPUT = outputSchema({});

/** The effect classes a capability record may declare. */
export type EffectType = 'read' | 'write' | 'destructive';

/**
 * Scope, consent and data access derived from the effect class.
 * Only `destructive` demands an explicit consent grant.
 */
export function policy(effect: EffectType): CapabilityPolicy {
  return {
    requiredScope: effect,
    consent: effect === 'destructive' ? 'explicit' : 'none',
    dataAccess: effect === 'read' ? 'project-read' : 'project-write',
  };
}

/**
 * Retry defaults derived from the effect class, with per-record
 * overrides. Retry safety follows the resolved idempotency rather than the
 * effect, so a record that declares itself idempotent is not also published as
 * unsafe to retry; destructive stays opt-in.
 */
export function behavior(
  effect: EffectType,
  opts: Partial<CapabilityBehaviorSource> = {}
): CapabilityBehaviorSource {
  const idempotency = opts.idempotency ?? (effect === 'read' ? 'idempotent' : 'non-idempotent');
  return {
    effect,
    idempotency,
    longRunning: opts.longRunning ?? false,
    safeToRetry: opts.safeToRetry ?? (idempotency === 'idempotent' && effect !== 'destructive'),
  };
}
