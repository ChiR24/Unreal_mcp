import type { CapabilityRecord } from '../model.js';
import { parseCapabilityCatalog } from '../parser.js';
import { PILOT_CAPABILITY_CATALOG } from './aggregate.js';
import { RETRIEVAL_SCORE_CONSTANTS } from './constants.js';
import { filterCapabilityRecords } from './filter.js';
import { parseCapabilityRetrievalRequest } from './request.js';
import {
  createCapabilitySearchIndex,
  isNearTieScore,
  type RankedCapability,
  rankCapabilityRecords,
} from './scoring.js';
import { uniqueCapabilityTokens } from './tokenize.js';
import type {
  CapabilityRetrievalMatch,
  CapabilityRetrievalResult,
  CapabilityRetriever,
  CapabilitySelection,
} from './types.js';

function compareAscii(left: string, right: string): number {
  if (left < right) return -1;
  if (left > right) return 1;
  return 0;
}

function toMatch(ranked: RankedCapability): CapabilityRetrievalMatch {
  const record = ranked.record;
  return Object.freeze({
    id: record.id,
    score: ranked.score,
    confidence: ranked.confidence,
    effect: record.behavior.effect,
    reasons: ranked.reasons,
    availability: Object.freeze({
      status: 'available' as const,
      unreal: Object.freeze({
        min: record.availability.unreal.min,
        max: record.availability.unreal.max,
      }),
      requiredPlugins: Object.freeze([...record.availability.requiredPlugins].sort(compareAscii)),
      editorStates: Object.freeze([...record.availability.editorStates].sort(compareAscii)),
    }),
    nextCall: Object.freeze({
      operation: 'describe' as const,
      capability: record.id,
    }),
  });
}

function selectCapability(
  ranked: readonly RankedCapability[],
  nearTies: readonly RankedCapability[],
): CapabilitySelection {
  const top = ranked[0];
  if (top === undefined) return Object.freeze({ kind: 'none', reason: 'no_match' });
  const destructiveNearTie = nearTies.length >= 2
    && nearTies.some((entry) => entry.record.behavior.effect === 'destructive');
  if (destructiveNearTie) {
    return Object.freeze({ kind: 'none', reason: 'destructive_near_tie' });
  }
  if (top.confidence < RETRIEVAL_SCORE_CONSTANTS.minimumAutoSelectConfidence) {
    return Object.freeze({ kind: 'none', reason: 'low_confidence' });
  }
  return Object.freeze({
    kind: 'selected',
    capability: top.record.id,
    requiresConfirmation: top.record.policy.consent !== 'none',
  });
}

export function createCapabilityRetriever(
  inputRecords: readonly CapabilityRecord[],
): CapabilityRetriever {
  const records = Object.freeze([...parseCapabilityCatalog([...inputRecords])]);
  const index = createCapabilitySearchIndex(records);
  return Object.freeze({
    retrieve(input: unknown): CapabilityRetrievalResult {
      const request = parseCapabilityRetrievalRequest(input);
      if (uniqueCapabilityTokens(request.query).length === 0) {
        return Object.freeze({
          matches: Object.freeze([]),
          nearTieCapabilityIds: Object.freeze([]),
          selection: Object.freeze({ kind: 'none', reason: 'empty_query' }),
        });
      }
      const available = filterCapabilityRecords(records, request.profile);
      const ranked = rankCapabilityRecords(index, available, request.query);
      const top = ranked[0];
      const nearTies = top === undefined
        ? []
        : ranked.filter((candidate) => isNearTieScore(top.score, candidate.score));
      return Object.freeze({
        matches: Object.freeze(ranked.slice(0, request.limit).map(toMatch)),
        nearTieCapabilityIds: Object.freeze(
          nearTies.slice(0, request.limit).map((entry) => entry.record.id),
        ),
        selection: selectCapability(ranked, nearTies),
      });
    },
  });
}

export const PILOT_CAPABILITY_RETRIEVER = createCapabilityRetriever(
  PILOT_CAPABILITY_CATALOG,
);

export function retrieveCapabilities(input: unknown): CapabilityRetrievalResult {
  return PILOT_CAPABILITY_RETRIEVER.retrieve(input);
}
