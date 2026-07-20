import { LegacyActionNameSchema, LegacyToolNameSchema } from '../identifiers.js';
import {
  isNonTranslatable,
  resolveMigrationEntry
} from './migration-map.js';
import { buildReplacementGuidance, findLossyRule } from './lossy-translations.js';
import {
  NonTranslatableMigrationError,
  UnknownLegacyCallError
} from './types.js';
import type { LegacyKey, TranslateResult } from './types.js';

/**
 * Translator for the `unreal.execute` compatibility path.
 *
 * A legacy `{tool, action, params}` call is resolved to its canonical
 * capability id. Translation is LOSSLESS-FIRST: when the legacy call maps
 * cleanly to a canonical capability (including aliases) the params pass through
 * (or are transformed) and the canonical id is returned for the receipt.
 *
 * When the legacy call is a LOSSY mismatch — distinct semantics that the old
 * handler silently coerced — translation REFUSES with a typed error carrying
 * exact replacement guidance. No silent semantic coercion happens here.
 *
 * This module is new code only. It does NOT modify the existing gateway execute
 * path (that is Task 26); it is the generated compatibility translator the
 * migration map drives.
 */

function applyTransform(
  params: Readonly<Record<string, unknown>>,
  transform: TranslateResult['entry']['argumentTransform']
): Readonly<Record<string, unknown>> {
  if (!transform || transform.kind === 'identity') return params;
  if (transform.kind === 'rename') {
    const out: Record<string, unknown> = {};
    for (const [key, value] of Object.entries(params)) {
      const renamed = transform.renames[key];
      out[renamed ?? key] = value;
    }
    return out;
  }
  if (transform.kind === 'drop-unsupported') {
    const out: Record<string, unknown> = {};
    for (const [key, value] of Object.entries(params)) {
      if (transform.dropped.includes(key)) continue;
      out[key] = value;
    }
    return out;
  }
  return params;
}

export function translateExecute(input: {
  readonly tool: string;
  readonly action: string;
  readonly params?: Readonly<Record<string, unknown>>;
}): TranslateResult {
  const tool = LegacyToolNameSchema.parse(input.tool);
  const action = LegacyActionNameSchema.parse(input.action);
  const params = input.params ?? {};
  const key = `${tool}::${action}` as LegacyKey;

  // Lossy mismatch: refuse before any coercion.
  const lossy = findLossyRule(key, params);
  if (lossy) {
    throw new NonTranslatableMigrationError(key, buildReplacementGuidance(lossy));
  }

  const entry = resolveMigrationEntry(tool, action);
  if (!entry) {
    throw new UnknownLegacyCallError(key);
  }

  if (entry.disposition === 'removed') {
    // A removed verb is an explicit typed removal; surface it as non-translatable
    // with the recorded replacement guidance when available.
    throw new NonTranslatableMigrationError(key, {
      canonicalId: entry.removal?.replacement,
      reason: entry.removal?.guidance ?? 'This legacy verb was removed.',
      nextCall: { operation: 'execute', tool: entry.tool, action: entry.action }
    });
  }

  if (entry.disposition === 'non-translatable') {
    if (!entry.nonTranslatable) throw new UnknownLegacyCallError(key);
    throw new NonTranslatableMigrationError(key, entry.nonTranslatable.guidance);
  }

  const canonicalId = entry.canonicalId;
  if (!canonicalId) throw new UnknownLegacyCallError(key);

  const transformedParams = applyTransform(params, entry.argumentTransform);
  return { canonicalId, transformedParams, entry };
}

export { isNonTranslatable };
