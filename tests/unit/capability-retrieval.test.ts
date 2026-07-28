import { describe, expect, it } from 'vitest';

import {
  type CapabilityDiscovery,
  type CapabilityRecord,
  createCapabilityRecord,
} from '../../src/tools/catalog/capabilities/index.js';
import {
  type CapabilityRetrievalRequest,
  createCapabilityRetrievalParityVector,
  createCapabilityRetriever,
  filterCapabilityRecords,
  MAX_MATCH_REASONS,
  NEAR_TIE_RATIO,
  PILOT_CAPABILITY_CATALOG,
  parseCapabilityRetrievalRequest,
  RETRIEVAL_FIELD_WEIGHTS,
  RETRIEVAL_SCORE_CONSTANTS,
  RETRIEVAL_TOKENIZATION,
  retrieveCapabilities,
  SCORE_TIE_EPSILON,
} from '../../src/tools/catalog/capabilities/retrieval/index.js';

const ALL_PLUGINS = [...new Set(
  PILOT_CAPABILITY_CATALOG.flatMap((record) => record.availability.requiredPlugins),
)].sort();

const BASE_REQUEST = {
  query: 'search material assets',
  limit: 5,
  profile: {
    unrealVersion: { major: 5, minor: 7, patch: 4, channel: 'stable' },
    installedPlugins: ALL_PLUGINS,
    editorState: 'edit',
    enabledParents: ['build_environment', 'manage_asset', 'manage_blueprint', 'manage_sequence'],
    enabledCategories: ['core', 'world', 'utility'],
    authorizedScopes: ['read', 'write', 'destructive', 'admin'],
    requestedEffects: ['read', 'write', 'destructive'],
    requiredOutputFields: [],
  },
} as const;

function requestWith(
  changes: Readonly<Partial<CapabilityRetrievalRequest['profile']>>,
  query: string = BASE_REQUEST.query,
): unknown {
  return {
    ...BASE_REQUEST,
    query,
    profile: { ...BASE_REQUEST.profile, ...changes },
  };
}

function byLegacyAction(tool: string, action: string): CapabilityRecord {
  const record = PILOT_CAPABILITY_CATALOG.find((candidate) =>
    candidate.legacyIds.some((legacy) => legacy.tool === tool && legacy.action === action),
  );
  if (record === undefined) throw new TypeError(`Missing pilot record ${tool}.${action}`);
  return record;
}

function withDiscovery(
  record: CapabilityRecord,
  discovery: CapabilityDiscovery,
): CapabilityRecord {
  const { hashes, ...source } = record;
  if (hashes.algorithm !== 'sha256') throw new TypeError('Expected parsed capability record');
  return createCapabilityRecord({ ...source, discovery });
}

describe('capability retrieval frozen contract', () => {
  it('Given the lexical configuration, When consumers inspect it, Then tokenization, weights, and tie constants are frozen and locale-invariant', () => {
    expect(Object.isFrozen(RETRIEVAL_TOKENIZATION)).toBe(true);
    expect(Object.isFrozen(RETRIEVAL_FIELD_WEIGHTS)).toBe(true);
    expect(Object.isFrozen(RETRIEVAL_SCORE_CONSTANTS)).toBe(true);
    expect(RETRIEVAL_TOKENIZATION.locale).toBe('invariant');
    expect(SCORE_TIE_EPSILON).toBeGreaterThan(0);
    expect(NEAR_TIE_RATIO).toBe(0.02);
  });

  it('Given a retrieval result, When a parity vector is materialized, Then Task 25 receives a frozen neutral JSON shape', () => {
    const result = retrieveCapabilities(BASE_REQUEST);
    const vector = createCapabilityRetrievalParityVector({
      name: 'asset-search',
      request: BASE_REQUEST,
      result,
    });

    expect(Object.isFrozen(vector)).toBe(true);
    expect(vector.schema).toBe('unreal.capability-retrieval.parity.v1');
    expect(vector.expected.rankedCapabilityIds).toEqual(result.matches.map((match) => match.id));
    expect(JSON.parse(JSON.stringify(vector))).toEqual(vector);
  });
});

describe('capability retrieval stage-one hard filters', () => {
  it('Given an engine version above a record maximum, When stage one runs, Then the record is unavailable', () => {
    const request = parseCapabilityRetrievalRequest(requestWith({
      unrealVersion: { major: 5, minor: 9, patch: 0, channel: 'stable' },
    }));

    expect(filterCapabilityRecords([byLegacyAction('manage_asset', 'search_assets')], request.profile)).toEqual([]);
  });

  it('Given a missing required plugin, When stage one runs, Then the record is unavailable', () => {
    const request = parseCapabilityRetrievalRequest(requestWith({
      installedPlugins: ['LevelSequenceEditor'],
    }, 'start render'));

    expect(filterCapabilityRecords([byLegacyAction('manage_sequence', 'start_render')], request.profile)).toEqual([]);
  });

  it('Given PIE state for an edit-only capability, When stage one runs, Then the record is unavailable', () => {
    const request = parseCapabilityRetrievalRequest(requestWith({ editorState: 'pie' }));

    expect(filterCapabilityRecords([byLegacyAction('manage_asset', 'search_assets')], request.profile)).toEqual([]);
  });

  it('Given a disabled parent or category, When stage one runs, Then both states fail closed', () => {
    const parentDisabled = parseCapabilityRetrievalRequest(requestWith({ enabledParents: [] }));
    const categoryDisabled = parseCapabilityRetrievalRequest(requestWith({ enabledCategories: ['world', 'utility'] }));
    const record = byLegacyAction('manage_asset', 'search_assets');

    expect(filterCapabilityRecords([record], parentDisabled.profile)).toEqual([]);
    expect(filterCapabilityRecords([record], categoryDisabled.profile)).toEqual([]);
  });

  it('Given insufficient scope or a disallowed side effect, When stage one runs, Then destructive records are excluded', () => {
    const unauthorized = parseCapabilityRetrievalRequest(requestWith({ authorizedScopes: ['read'] }, 'delete asset'));
    const readOnly = parseCapabilityRetrievalRequest(requestWith({ requestedEffects: ['read'] }, 'delete asset'));
    const record = byLegacyAction('manage_asset', 'delete_asset');

    expect(filterCapabilityRecords([record], unauthorized.profile)).toEqual([]);
    expect(filterCapabilityRecords([record], readOnly.profile)).toEqual([]);
  });

  it('Given a required output field absent from a record, When stage one runs, Then the record is excluded', () => {
    const request = parseCapabilityRetrievalRequest(requestWith({ requiredOutputFields: ['nodeGuid'] }));

    expect(filterCapabilityRecords([byLegacyAction('manage_asset', 'search_assets')], request.profile)).toEqual([]);
  });
});

describe('capability retrieval stage-two ranking and disclosure', () => {
  it('Given an available asset-search intent, When retrieval runs, Then the exact canonical match leads with bounded explainable disclosure', () => {
    const result = retrieveCapabilities(BASE_REQUEST);

    expect(result.matches[0]?.id).toBe('asset.search_assets');
    // Disclosure is BOUNDED by the 5-result cap, not required to fill it. The
    // alias fold removed material.rebuild_material as an independent document
    // because it is a declared alias of material.compile_material, which still
    // appears here; asserting exactly 5 would re-require that duplicate.
    expect(result.matches.length).toBeLessThanOrEqual(5);
    expect(result.matches.length).toBeGreaterThan(0);
    expect(new Set(result.matches.map((match) => match.id)).size).toBe(result.matches.length);
    expect(result.matches.every((match) => match.reasons.length <= MAX_MATCH_REASONS)).toBe(true);
    expect(result.matches.every((match) => match.confidence >= 0 && match.confidence <= 1)).toBe(true);
    expect(result.matches.every((match) => match.availability.status === 'available')).toBe(true);
    expect(result.matches[0]?.nextCall).toEqual({ operation: 'describe', capability: 'asset.search_assets' });
    expect(JSON.stringify(result)).not.toMatch(/schemas|properties|inputSchema|outputSchema/u);
  });

  it('Given a whitespace-only query, When retrieval runs, Then it returns no catalog dump or auto-selection', () => {
    const result = retrieveCapabilities({ ...BASE_REQUEST, query: ' \t\n ' });

    expect(result.matches).toEqual([]);
    expect(result.selection).toEqual({ kind: 'none', reason: 'empty_query' });
  });

  it('Given malformed limit and profile options, When the boundary parses them, Then both fail closed', () => {
    const invalidLimit = { ...BASE_REQUEST, limit: 0 };
    const unknownOption = {
      ...BASE_REQUEST,
      profile: { ...BASE_REQUEST.profile, unexpected: true },
    };

    expect(() => parseCapabilityRetrievalRequest(invalidLimit)).toThrow();
    expect(() => parseCapabilityRetrievalRequest(unknownOption)).toThrow();
  });

  it('Given repeated and deterministically shuffled catalogs, When the same request runs, Then ranking bytes are identical', () => {
    const shuffled = PILOT_CAPABILITY_CATALOG.map((_, index, records) =>
      records[(index * 197) % records.length],
    );
    const canonicalRetriever = createCapabilityRetriever(PILOT_CAPABILITY_CATALOG);
    const shuffledRetriever = createCapabilityRetriever(shuffled);
    const expected = JSON.stringify(canonicalRetriever.retrieve(BASE_REQUEST));

    expect(JSON.stringify(shuffledRetriever.retrieve(BASE_REQUEST))).toBe(expected);
    for (let run = 0; run < 10; run += 1) {
      expect(JSON.stringify(canonicalRetriever.retrieve(BASE_REQUEST))).toBe(expected);
    }
  });

  it('Given destructive candidates within the frozen near-tie ratio, When retrieval runs, Then no mutation is auto-selected', () => {
    const sharedDiscovery: CapabilityDiscovery = {
      domain: 'destructive_collision',
      family: 'destructive_collision',
      topics: ['obliterate target'],
      summary: 'Obliterate the selected target.',
      whenToUse: ['Use to obliterate a target.'],
      whenNotToUse: [],
    };
    const records = [
      withDiscovery(byLegacyAction('manage_asset', 'delete'), sharedDiscovery),
      withDiscovery(byLegacyAction('manage_sequence', 'delete'), sharedDiscovery),
    ];
    const retriever = createCapabilityRetriever(records);
    const result = retriever.retrieve({ ...BASE_REQUEST, query: 'delete obliterate target' });

    expect(result.matches[0]?.effect).toBe('destructive');
    expect(result.nearTieCapabilityIds.length).toBeGreaterThan(1);
    expect(result.selection).toEqual({ kind: 'none', reason: 'destructive_near_tie' });
  });
});
