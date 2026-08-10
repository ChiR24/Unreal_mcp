import { LegacyActionNameSchema, LegacyToolNameSchema } from '../identifiers.js';
import type { LegacyActionName, LegacyToolName } from '../identifiers.js';
import { requireCanonicalTarget } from './canonical-targets.js';
import type { CanonicalCapabilityRef, LegacyKey, ReplacementGuidance } from './types.js';

/**
 * Curated LOSSY translation rules.
 *
 * These are legacy `{tool, action}` calls whose historical handler behavior
 * silently coerced distinct semantics into a single downstream action. The
 * migration map MUST refuse to translate them losslessly and instead return
 * exact replacement guidance, so clients are not surprised by dropped data.
 *
 * Example: a legacy `set_volume_bounds` call historically collapsed to
 * `set_volume_extent` in the runtime handler, dropping the `bounds.origin`
 * (the volume's position). Any legacy call that carries an `origin` cannot be
 * losslessly represented by `set_volume_extent` (extent only). Translation
 * refuses and points the caller at the correct canonical capability.
 */

export type LossyRule = {
  readonly legacyKey: LegacyKey;
  readonly canonicalId: CanonicalCapabilityRef;
  /** Human reason the legacy call is non-translatable as-is. */
  readonly reason: string;
  /** Detection: when this predicate is true on the legacy params, refuse. */
  readonly refusesWhen: (params: Readonly<Record<string, unknown>>) => boolean;
};

function legacyKey(tool: LegacyToolName, action: LegacyActionName): LegacyKey {
  return `${tool}::${action}`;
}

function hasOrigin(params: Readonly<Record<string, unknown>>): boolean {
  const bounds = params['bounds'];
  if (bounds === null || typeof bounds !== 'object') return false;
  return 'origin' in (bounds as Record<string, unknown>);
}

const VOLUME_TOOL = LegacyToolNameSchema.parse('manage_level_structure');
const VOLUME_EXTENT_ACTION = LegacyActionNameSchema.parse('set_volume_extent');

export const LOSSY_RULES: ReadonlyArray<LossyRule> = [
  {
    legacyKey: legacyKey(VOLUME_TOOL, LegacyActionNameSchema.parse('set_volume_bounds')),
    canonicalId: requireCanonicalTarget(legacyKey(VOLUME_TOOL, VOLUME_EXTENT_ACTION)),
    reason:
      'Legacy set_volume_bounds carried a bounds.origin (volume position) that the ' +
      'extent-only canonical path drops. Translating silently would lose the position.',
    refusesWhen: hasOrigin
  }
] as const;

export function findLossyRule(
  key: LegacyKey,
  params: Readonly<Record<string, unknown>>
): LossyRule | undefined {
  for (const rule of LOSSY_RULES) {
    if (rule.legacyKey === key && rule.refusesWhen(params)) return rule;
  }
  return undefined;
}

export function buildReplacementGuidance(rule: LossyRule): ReplacementGuidance {
  const [tool] = rule.legacyKey.split('::') as [LegacyToolName, LegacyActionName];
  return {
    canonicalId: rule.canonicalId,
    reason: rule.reason,
    nextCall: {
      operation: 'execute',
      tool,
      action: VOLUME_EXTENT_ACTION
    }
  };
}
