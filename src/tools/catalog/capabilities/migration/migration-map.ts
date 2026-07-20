import inventory from '../normalization-inventory.json' with { type: 'json' };
import { LegacyActionNameSchema, LegacyToolNameSchema } from '../identifiers.js';
import type { LegacyActionName, LegacyToolName } from '../identifiers.js';
import type {
  ArgumentTransform,
  LegacyKey,
  MigrationDisposition,
  MigrationEntry,
  MigrationMap
} from './types.js';
import { findLossyRule } from './lossy-translations.js';

/**
 * Builds the complete migration map from the audited normalization inventory
 * (Task 19 output, 1,335 leaf-backed occurrences). Every shipped legacy
 * `{tool, action}` resolves to exactly one canonical capability, an explicit
 * typed removal, or is marked non-translatable at translate time for lossy
 * mismatches. Nothing here is hand-written into handlers; the map is derived
 * from the inventory and the curated lossy rules.
 */

const DEPRECATION_WINDOW = 'until next major (v1.0) gateway surface';

function legacyKey(tool: LegacyToolName, action: LegacyActionName): LegacyKey {
  return `${tool}::${action}`;
}

function identityTransform(): ArgumentTransform {
  return { kind: 'identity', renames: {}, dropped: [] };
}

type InventoryOccurrence = {
  readonly occurrenceKey: string;
  readonly tool: string;
  readonly action: string;
  readonly canonicalId: string;
  readonly disposition: string;
  readonly classification: string;
};

type RouteDisposition = {
  readonly dispositionKey: string;
  readonly route: string;
  readonly domain: string;
  readonly status: string;
  readonly evidence?: { readonly tool?: string; readonly source?: string; readonly symbol?: string };
  readonly targetCanonicalId?: string;
  readonly disposition: string;
  readonly rationale: string;
  readonly removalGuidance?: string;
  readonly resolved: boolean;
};

function dispositionFromInventory(occurrenceDisposition: string): MigrationDisposition {
  switch (occurrenceDisposition) {
    case 'alias':
      return 'alias';
    case 'keep':
    default:
      return 'canonical';
  }
}

function buildEntryFromOccurrence(occurrence: InventoryOccurrence): MigrationEntry {
  const tool = LegacyToolNameSchema.parse(occurrence.tool);
  const action = LegacyActionNameSchema.parse(occurrence.action);
  const key = legacyKey(tool, action);
  const canonicalId = occurrence.canonicalId;
  const disposition = dispositionFromInventory(occurrence.disposition);

  const entry: MigrationEntry = {
    legacyKey: key,
    tool,
    action,
    disposition,
    canonicalId,
    argumentTransform: identityTransform(),
    deprecation: {
      status: disposition === 'alias' ? 'deprecated' : 'active',
      window: DEPRECATION_WINDOW
    }
  };
  return entry;
}

function buildRemovalFromRoute(route: RouteDisposition): MigrationEntry | undefined {
  if (route.disposition !== 'remove') return undefined;
  // Native raw routes key by evidence.tool + route.action (not tool:action).
  const tool = route.evidence?.tool;
  const action = route.route;
  const toolName = LegacyToolNameSchema.safeParse(tool ?? '');
  const actionName = LegacyActionNameSchema.safeParse(action ?? '');
  if (!toolName.success || !actionName.success) return undefined;
  const key = legacyKey(toolName.data, actionName.data);
  return {
    legacyKey: key,
    tool: toolName.data,
    action: actionName.data,
    disposition: 'removed',
    removal: {
      since: '5.0',
      guidance: route.removalGuidance ?? route.rationale,
      replacement: route.targetCanonicalId ? route.targetCanonicalId : undefined
    },
    deprecation: { status: 'removed', window: DEPRECATION_WINDOW }
  };
}

export function buildMigrationMap(): MigrationMap {
  const entries = new Map<LegacyKey, MigrationEntry>();

  for (const occurrence of inventory.occurrences as ReadonlyArray<InventoryOccurrence>) {
    const entry = buildEntryFromOccurrence(occurrence);
    entries.set(entry.legacyKey, entry);
  }

  for (const route of inventory.routeDispositions as ReadonlyArray<RouteDisposition>) {
    const removal = buildRemovalFromRoute(route);
    if (removal) {
      // A removed route shadows any kept occurrence with the same key.
      entries.set(removal.legacyKey, removal);
    } else if (route.disposition === 'map' || route.disposition === 'promote') {
      const [tool, action] = route.route.split(':') as [string, string];
      const toolName = LegacyToolNameSchema.safeParse(tool);
      const actionName = LegacyActionNameSchema.safeParse(action);
      if (toolName.success && actionName.success && route.targetCanonicalId) {
        const key = legacyKey(toolName.data, actionName.data);
        // Only override if not already a kept canonical (avoid clobbering).
        if (!entries.has(key)) {
          entries.set(key, {
            legacyKey: key,
            tool: toolName.data,
            action: actionName.data,
            disposition: 'canonical',
            canonicalId: route.targetCanonicalId,
            argumentTransform: identityTransform(),
            deprecation: { status: 'active', window: DEPRECATION_WINDOW }
          });
        }
      }
    }
  }

  return {
    schemaVersion: 'task20.migration.v1',
    generatedAt: '2026-07-17',
    occurrenceCount: inventory.occurrences.length,
    entries
  };
}

export const migrationMap: MigrationMap = buildMigrationMap();

/**
 * Resolve a legacy call to its migration entry (no lossy refusal here — that is
 * the translator's job, which also inspects params). Returns undefined when the
 * legacy key is unknown.
 */
export function resolveMigrationEntry(
  tool: string,
  action: string
): MigrationEntry | undefined {
  const toolName = LegacyToolNameSchema.safeParse(tool);
  const actionName = LegacyActionNameSchema.safeParse(action);
  if (!toolName.success || !actionName.success) return undefined;
  return migrationMap.entries.get(legacyKey(toolName.data, actionName.data));
}

/** True when a legacy call is non-translatable for the given params. */
export function isNonTranslatable(
  tool: LegacyToolName,
  action: LegacyActionName,
  params: Readonly<Record<string, unknown>>
): boolean {
  return findLossyRule(legacyKey(tool, action), params) !== undefined;
}
