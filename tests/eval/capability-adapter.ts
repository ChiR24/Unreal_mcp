import type {
  CapabilityId,
  CapabilityRecord,
} from '../../src/tools/catalog/capabilities/index.js';
import {
  type CapabilityRuntimeProfile,
  filterCapabilityRecords,
  PILOT_CAPABILITY_CATALOG,
  PILOT_PARENT_CATEGORIES,
  parseCapabilityRuntimeProfile,
  retrieveCapabilities,
} from '../../src/tools/catalog/capabilities/retrieval/index.js';
import {
  canonicalCapabilityId,
  deriveAliasFold,
} from '../../src/tools/catalog/capabilities/retrieval/alias-fold.js';
import { corpus } from './corpus.js';
import { type CapabilityRef, TOP_K } from './types.js';

export type Task13RetrievalCaseScore = {
  readonly id: string;
  readonly expectedCapabilityId: string;
  readonly acceptedCapabilityIds: readonly string[];
  readonly rankedCapabilityIds: readonly string[];
  readonly strictTop1Correct: boolean;
  readonly top1Correct: boolean;
  readonly topKCorrect: boolean;
};

export type Task13RetrievalEvalReport = {
  readonly schema: 'omo.eval.capability-retrieval.v1';
  readonly corpusVersion: string;
  readonly eligibleCases: number;
  readonly strictTop1Accuracy: number;
  readonly top1Accuracy: number;
  readonly topKRecall: number;
  readonly perCase: readonly Task13RetrievalCaseScore[];
};

export type Task13UnavailableFilterReport = {
  readonly evaluatedUnavailableCandidates: number;
  readonly leakedCandidates: number;
  readonly filterRate: number;
};

const LEGACY_TO_CANONICAL = new Map<string, CapabilityId>();
for (const record of PILOT_CAPABILITY_CATALOG) {
  for (const legacy of record.legacyIds) {
    LEGACY_TO_CANONICAL.set(`${legacy.tool}::${legacy.action}`, record.id);
  }
}

const ALL_PLUGINS = [...new Set(
  PILOT_CAPABILITY_CATALOG.flatMap((record) => record.availability.requiredPlugins),
)].sort();
const ALL_PARENTS = [...new Set(
  PILOT_CAPABILITY_CATALOG.map((record) => record.routing.parentTool),
)].sort();
const ALL_CATEGORIES = [...new Set(
  Object.values(PILOT_PARENT_CATEGORIES),
)].sort();
const BASE_PROFILE = parseCapabilityRuntimeProfile({
  unrealVersion: { major: 5, minor: 7, patch: 4, channel: 'stable' },
  installedPlugins: ALL_PLUGINS,
  editorState: 'edit',
  enabledParents: ALL_PARENTS,
  enabledCategories: ALL_CATEGORIES,
  authorizedScopes: ['read', 'write', 'destructive', 'admin'],
  requestedEffects: ['read', 'write', 'destructive'],
  requiredOutputFields: [],
});

// Search answers in primary space, so a rationale-declared alias is resolved to
// the primary the catalog itself says it is. The relation is the registry's own
// fold, never the corpus.
const PILOT_ALIAS_FOLD = deriveAliasFold(PILOT_CAPABILITY_CATALOG);

function canonicalId(reference: CapabilityRef): CapabilityId | undefined {
  const resolved = LEGACY_TO_CANONICAL.get(`${reference.tool}::${reference.action}`);
  if (resolved === undefined) return undefined;
  return canonicalCapabilityId(PILOT_ALIAS_FOLD, String(resolved)) as CapabilityId;
}

function rate(
  scores: readonly Task13RetrievalCaseScore[],
  field: 'strictTop1Correct' | 'top1Correct' | 'topKCorrect',
): number {
  if (scores.length === 0) return 0;
  return scores.filter((score) => score[field]).length / scores.length;
}

export function evaluateTask13Retrieval(): Task13RetrievalEvalReport {
  const perCase: Task13RetrievalCaseScore[] = [];
  for (const entry of corpus.cases) {
    const expectedCapabilityId = canonicalId(entry.expected);
    if (expectedCapabilityId === undefined) continue;
    const acceptedCapabilityIds = [
      expectedCapabilityId,
      ...entry.allowedAlternatives
        .map(canonicalId)
        .filter((id): id is CapabilityId => id !== undefined),
    ];
    const result = retrieveCapabilities({
      query: entry.intent,
      limit: TOP_K,
      profile: BASE_PROFILE,
    });
    const rankedCapabilityIds = result.matches.map((match) => match.id);
    const top = rankedCapabilityIds[0];
    perCase.push({
      id: entry.id,
      expectedCapabilityId,
      acceptedCapabilityIds,
      rankedCapabilityIds,
      strictTop1Correct: top === expectedCapabilityId,
      top1Correct: top !== undefined && acceptedCapabilityIds.includes(top),
      topKCorrect: rankedCapabilityIds.includes(expectedCapabilityId),
    });
  }
  perCase.sort((left, right) => {
    if (left.id < right.id) return -1;
    if (left.id > right.id) return 1;
    return 0;
  });
  return Object.freeze({
    schema: 'omo.eval.capability-retrieval.v1',
    corpusVersion: corpus.version,
    eligibleCases: perCase.length,
    strictTop1Accuracy: rate(perCase, 'strictTop1Correct'),
    top1Accuracy: rate(perCase, 'top1Correct'),
    topKRecall: rate(perCase, 'topKCorrect'),
    perCase: Object.freeze(perCase),
  });
}

type UnavailableScenario = {
  readonly profile: CapabilityRuntimeProfile;
  readonly excludes: (record: CapabilityRecord) => boolean;
};

function profileWith(changes: Readonly<Partial<CapabilityRuntimeProfile>>): CapabilityRuntimeProfile {
  return parseCapabilityRuntimeProfile({ ...BASE_PROFILE, ...changes });
}

export function evaluateTask13UnavailableFiltering(): Task13UnavailableFilterReport {
  const scenarios: readonly UnavailableScenario[] = [
    {
      profile: profileWith({ unrealVersion: { major: 5, minor: 9, patch: 0, channel: 'stable' } }),
      excludes: () => true,
    },
    {
      profile: profileWith({ installedPlugins: [] }),
      excludes: (record) => record.availability.requiredPlugins.length > 0,
    },
    {
      profile: profileWith({ editorState: 'simulate' }),
      excludes: (record) => !record.availability.editorStates.includes('simulate'),
    },
    { profile: profileWith({ enabledParents: [] }), excludes: () => true },
    { profile: profileWith({ enabledCategories: [] }), excludes: () => true },
    {
      profile: profileWith({ authorizedScopes: ['read'] }),
      excludes: (record) => record.policy.requiredScope !== 'read',
    },
    {
      profile: profileWith({ requestedEffects: ['read'] }),
      excludes: (record) => record.behavior.effect !== 'read',
    },
    { profile: profileWith({ requiredOutputFields: ['__task13_missing_output__'] }), excludes: () => true },
  ];
  let evaluatedUnavailableCandidates = 0;
  let leakedCandidates = 0;
  for (const scenario of scenarios) {
    const returnedIds = new Set(
      filterCapabilityRecords(PILOT_CAPABILITY_CATALOG, scenario.profile).map((record) => record.id),
    );
    const unavailable = PILOT_CAPABILITY_CATALOG.filter(scenario.excludes);
    evaluatedUnavailableCandidates += unavailable.length;
    leakedCandidates += unavailable.filter((record) => returnedIds.has(record.id)).length;
  }
  return Object.freeze({
    evaluatedUnavailableCandidates,
    leakedCandidates,
    filterRate: evaluatedUnavailableCandidates === 0
      ? 1
      : (evaluatedUnavailableCandidates - leakedCandidates) / evaluatedUnavailableCandidates,
  });
}
