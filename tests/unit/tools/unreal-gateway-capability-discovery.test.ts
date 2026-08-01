// Task 24 — exact, bounded TypeScript `search` and `describe`.
//
// Discovery is served from the Task 23 generated canonical registry (1,335
// records) through the Task 13 retrieval ranker. The parent-tool manifest union
// is no longer a contract source: describing an action returns THAT action's
// exact schema, never the union of every action on its parent tool.
//
// Concrete anchor for the whole task: `manage_asset:import` carried a
// 161-parameter union catalog before this change; the canonical `asset.import`
// record declares exactly 4 parameters.

import { afterEach, describe, expect, it } from 'vitest';
import {
  describeGatewayCapability,
  searchGatewayCatalog
} from '../../../src/server/tool-registry-gateway.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import { dynamicToolManager } from '../../../src/tools/dynamic/dynamic-tool-manager.js';
import {
  CATALOG_REVISION,
  CANONICAL_CAPABILITY_RECORD_COUNT
} from '../../../src/tools/catalog/capabilities/generated/canonical-registry.generated.js';

const ASSET_IMPORT = 'asset.import';
const ASSET_IMPORT_SCHEMA_HASH = '8145032abab044ee8e4ea3960cc422e219c02ceed47171dd60a12b9fbbf71478';
const ASSET_IMPORT_CONTENT_HASH = 'f13b1339db9c5def8c8bc323d5fdf499db2af36b1abf5ecf4b70cd2651d39747';

type Row = Record<string, unknown>;

function search(args: Row): Row {
  return searchGatewayCatalog(args) as Row;
}

function describeCapability(args: Row): Row {
  return describeGatewayCapability(args) as Row;
}

function rows(result: Row): Row[] {
  return (result.results ?? []) as Row[];
}

afterEach(() => {
  dynamicToolManager.reset();
});

describe('search returns canonical capabilities, not parent-tool manifest rows', () => {
  it('stamps the generated catalog revision on every response', () => {
    const result = search({ query: 'import asset' });
    expect(result.success).toBe(true);
    expect(result.operation).toBe('search');
    expect(result.catalogRevision).toBe(CATALOG_REVISION);
  });

  it('finds the exact canonical capability for a natural query', () => {
    const result = search({ query: 'import asset' });
    const ids = rows(result).map((row) => row.capability);
    expect(ids).toContain(ASSET_IMPORT);
  });

  it('carries the routing, discovery, effect and hash facts for each hit', () => {
    const hit = rows(search({ query: 'import asset' })).find((row) => row.capability === ASSET_IMPORT);
    expect(hit).toBeDefined();
    expect(hit?.parentTool).toBe('manage_asset');
    expect(hit?.action).toBe('import');
    expect(hit?.category).toBe('core');
    expect(hit?.domain).toBe('asset');
    expect(hit?.family).toBe('lifecycle');
    expect(hit?.effect).toBe('write');
    expect(hit?.hashes).toEqual({
      algorithm: 'sha256',
      schema: ASSET_IMPORT_SCHEMA_HASH,
      content: ASSET_IMPORT_CONTENT_HASH
    });
  });

  it('reports bounded, deterministic match reasons', () => {
    const hit = rows(search({ query: 'import asset' })).find((row) => row.capability === ASSET_IMPORT);
    const reasons = hit?.reasons as Array<Row> | undefined;
    expect(Array.isArray(reasons)).toBe(true);
    expect(reasons?.length).toBeGreaterThan(0);
    expect(reasons?.length).toBeLessThanOrEqual(3);
    for (const reason of reasons ?? []) {
      expect(typeof reason.field).toBe('string');
      expect(Array.isArray(reason.matchedTokens)).toBe(true);
      expect((reason.matchedTokens as string[]).length).toBeLessThanOrEqual(3);
    }
  });

  it('reports availability and policy without dumping a schema', () => {
    const hit = rows(search({ query: 'import asset' })).find((row) => row.capability === ASSET_IMPORT);
    const availability = hit?.availability as Row | undefined;
    expect(availability?.status).toBe('available');
    expect(Array.isArray(availability?.editorStates)).toBe(true);
    expect(isRecord(hit?.policy)).toBe(true);
    expect(Array.isArray(hit?.outputs)).toBe(true);
    expect(hit?.inputSchema).toBeUndefined();
    expect(hit?.outputSchema).toBeUndefined();
    expect(hit?.parameters).toBeUndefined();
  });

  it('hands back an executable describe nextCall per hit', () => {
    const hit = rows(search({ query: 'import asset' })).find((row) => row.capability === ASSET_IMPORT);
    expect(hit?.nextCall).toEqual({ operation: 'describe', capability: ASSET_IMPORT });
  });

  it('is deterministic across repeated identical queries', () => {
    const first = JSON.stringify(search({ query: 'spawn actor' }));
    const second = JSON.stringify(search({ query: 'spawn actor' }));
    expect(first).toBe(second);
  });
});

describe('search filters bound the catalog by domain, family, parent and effect', () => {
  it('filters to a single domain', () => {
    const result = search({ domain: 'asset', limit: 10 });
    expect(rows(result).every((row) => row.domain === 'asset')).toBe(true);
    expect((result.total as number) > 0).toBe(true);
  });

  it('filters to a single family inside a domain', () => {
    const result = search({ domain: 'tools', family: 'status', limit: 10 });
    expect(rows(result).length).toBeGreaterThan(0);
    expect(rows(result).every((row) => row.domain === 'tools' && row.family === 'status')).toBe(true);
  });

  it('filters to a legacy parent tool as a migration view', () => {
    const result = search({ tool: 'manage_tools', limit: 10 });
    expect(rows(result).length).toBeGreaterThan(0);
    expect(rows(result).every((row) => row.parentTool === 'manage_tools')).toBe(true);
  });

  it('resolves an exact legacy tool+action pair to one capability', () => {
    const result = search({ tool: 'manage_asset', action: 'import' });
    expect(rows(result)).toHaveLength(1);
    expect(rows(result)[0].capability).toBe(ASSET_IMPORT);
  });

  it('filters by behavior effect', () => {
    const result = search({ domain: 'asset', effect: 'read', limit: 25 });
    expect(rows(result).every((row) => row.effect === 'read')).toBe(true);
  });

  it('rejects an unknown domain with bounded suggestions and an executable nextCall', () => {
    const result = search({ domain: 'assets' });
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_DOMAIN');
    expect(result.suggestions).toContain('asset');
    expect((result.suggestions as string[]).length).toBeLessThanOrEqual(3);
    expect(isRecord(result.nextCall)).toBe(true);
    expect((result.nextCall as Row).operation).toBe('search');
  });

  it('rejects an unknown family with bounded suggestions', () => {
    const result = search({ domain: 'tools', family: 'statuses' });
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_FAMILY');
    expect(result.suggestions).toContain('status');
  });
});

describe('search stays inside its result, cursor and byte budgets', () => {
  it('never exceeds the maximum result limit even when asked to', () => {
    const result = search({ domain: 'asset', limit: 10_000 });
    expect(rows(result).length).toBeLessThanOrEqual(result.limit as number);
    expect(result.limit as number).toBeLessThanOrEqual(25);
  });

  it('pages deterministically and issues a resumable cursor', () => {
    const first = search({ domain: 'asset', limit: 5, offset: 0 });
    expect(rows(first)).toHaveLength(5);
    expect(first.hasMore).toBe(true);
    expect(typeof first.nextCursor).toBe('string');

    const second = search({ domain: 'asset', cursor: first.nextCursor });
    const firstIds = rows(first).map((row) => row.capability);
    const secondIds = rows(second).map((row) => row.capability);
    expect(secondIds.some((id) => firstIds.includes(id as string))).toBe(false);
  });

  it('drops the cursor once the page set is exhausted', () => {
    const result = search({ domain: 'tools', family: 'status', limit: 25 });
    expect(result.hasMore).toBe(false);
    expect(result.nextCursor).toBeUndefined();
  });

  it('rejects a malformed cursor rather than silently restarting', () => {
    const result = search({ domain: 'asset', cursor: 'not-a-cursor' });
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('INVALID_CURSOR');
  });

  it('honours an explicit byte budget and reports the truncation honestly', () => {
    const result = search({ domain: 'asset', limit: 25, maxBytes: 1500 });
    expect(result.success).toBe(true);
    expect(JSON.stringify(result).length).toBeLessThanOrEqual(1500);
    expect(result.truncated).toBe(true);
    expect(result.hasMore).toBe(true);
  });

  it('keeps an unbudgeted full-catalog browse inside the default byte ceiling', () => {
    const result = search({ limit: 25 });
    expect(result.success).toBe(true);
    expect(JSON.stringify(result).length).toBeLessThanOrEqual(result.maxBytes as number);
    expect(result.total).toBe(CANONICAL_CAPABILITY_RECORD_COUNT);
  });
});

describe('describe browses the catalog by domain then family', () => {
  it('lists bounded domains when nothing is selected', () => {
    const result = describeCapability({});
    expect(result.success).toBe(true);
    expect(result.scope).toBe('catalog');
    expect(result.catalogRevision).toBe(CATALOG_REVISION);
    const domains = result.domains as Row[];
    expect(Array.isArray(domains)).toBe(true);
    expect(domains.length).toBeGreaterThan(0);
    expect(domains.some((row) => row.domain === 'asset')).toBe(true);
    for (const row of domains) {
      expect(typeof row.capabilityCount).toBe('number');
      expect((row.nextCall as Row).operation).toBe('describe');
    }
  });

  it('lists the families of a domain', () => {
    const result = describeCapability({ domain: 'tools' });
    expect(result.scope).toBe('domain');
    expect(result.domain).toBe('tools');
    const families = (result.families as Row[]).map((row) => row.family);
    expect(families).toContain('status');
  });

  it('lists the capabilities of a family without any schema body', () => {
    const result = describeCapability({ domain: 'tools', family: 'status' });
    expect(result.scope).toBe('family');
    const capabilities = result.capabilities as Row[];
    expect(capabilities.length).toBeGreaterThan(0);
    for (const row of capabilities) {
      expect(typeof row.capability).toBe('string');
      expect(row.inputSchema).toBeUndefined();
      expect((row.nextCall as Row).operation).toBe('describe');
      expect(typeof (row.nextCall as Row).capability).toBe('string');
    }
  });
});

describe('describe returns one capability contract with its EXACT action schema', () => {
  const result = describeCapability({ capability: ASSET_IMPORT });

  it('identifies the capability and its legacy routing', () => {
    expect(result.success).toBe(true);
    expect(result.operation).toBe('describe');
    expect(result.scope).toBe('capability');
    expect(result.capability).toBe(ASSET_IMPORT);
    expect(result.parentTool).toBe('manage_asset');
    expect(result.action).toBe('import');
    expect(result.domain).toBe('asset');
    expect(result.family).toBe('lifecycle');
    expect(result.catalogRevision).toBe(CATALOG_REVISION);
  });

  it('returns the action-exact input schema, never the parent union', () => {
    expect(result.perActionSchemas).toBe(true);
    expect(result.scope).not.toBe('union');
    const input = result.inputSchema as Row;
    expect(isRecord(input)).toBe(true);
    expect(Object.keys(input.properties as Row).sort()).toEqual([
      'destinationPath',
      'overwrite',
      'save',
      'sourcePath'
    ]);
    expect(input.required).toEqual(['sourcePath', 'destinationPath']);
    expect(input.additionalProperties).toBe(false);
  });

  it('projects the same four parameters into the compact list', () => {
    const parameters = result.parameters as Row[];
    expect(parameters.map((row) => row.name).sort()).toEqual([
      'destinationPath',
      'overwrite',
      'save',
      'sourcePath'
    ]);
    const sourcePath = parameters.find((row) => row.name === 'sourcePath');
    expect(sourcePath?.required).toBe(true);
    expect(parameters.find((row) => row.name === 'save')?.required).toBe(false);
  });

  it('never leaks a union-sized parameter catalog', () => {
    // The pre-Task-24 union catalog for manage_asset carried 161 parameters.
    expect((result.parameters as Row[]).length).toBe(4);
    expect(result.parameterCount).toBe(4);
  });

  it('publishes the output contract and the per-record hashes', () => {
    expect(isRecord(result.outputSchema)).toBe(true);
    expect(result.hashes).toEqual({
      algorithm: 'sha256',
      schema: ASSET_IMPORT_SCHEMA_HASH,
      content: ASSET_IMPORT_CONTENT_HASH
    });
  });

  it('publishes behavior, policy, cost and availability', () => {
    expect((result.behavior as Row).effect).toBe('write');
    expect(isRecord(result.policy)).toBe(true);
    expect(isRecord(result.cost)).toBe(true);
    expect((result.availability as Row).status).toBe('available');
    expect((result.deprecation as Row).status).toBe('active');
  });

  it('marks an available capability runnable with an executable execute nextCall', () => {
    expect(result.runnable).toBe(true);
    // `capability` leads because it is the ONLY selector guaranteed to resolve:
    // execute matches a tool+action pair against the legacy-pair index, so a
    // nextCall built from `routing.dispatchAction` was unresolvable for 181
    // capabilities and resolved to the WRONG capability for 26 more. The legacy
    // pair is still published for callers that address by tool+action, but it
    // now comes from `record.legacyIds` rather than the native dispatch verb.
    expect(result.nextCall).toEqual({
      operation: 'execute',
      capability: ASSET_IMPORT,
      tool: 'manage_asset',
      action: 'import',
      params: {}
    });
  });
});

describe('describe drills into exactly one parameter', () => {
  it('returns a single parameter schema scoped to the action, not the union', () => {
    const result = describeCapability({ capability: ASSET_IMPORT, param: 'sourcePath' });
    expect(result.success).toBe(true);
    expect(result.scope).toBe('parameter');
    expect(result.capability).toBe(ASSET_IMPORT);
    expect(result.param).toBe('sourcePath');
    expect(result.required).toBe(true);
    expect(result.perActionSchemas).toBe(true);
    expect((result.schema as Row).type).toBe('string');
    expect(result.parameters).toBeUndefined();
  });

  it('rejects a parameter that belongs to the parent union but not to this action', () => {
    // `assetPath` exists on the manage_asset union; `asset.import` does not declare it.
    const result = describeCapability({ capability: ASSET_IMPORT, param: 'assetPath' });
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_PARAM');
    expect(result.availableParameters).toEqual([
      'destinationPath',
      'overwrite',
      'save',
      'sourcePath'
    ]);
  });

  it('suggests the closest declared parameter on a typo', () => {
    const result = describeCapability({ capability: ASSET_IMPORT, param: 'sorcePath' });
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_PARAM');
    expect(result.suggestions).toContain('sourcePath');
    expect((result.nextCall as Row).operation).toBe('describe');
    expect((result.nextCall as Row).capability).toBe(ASSET_IMPORT);
    expect((result.nextCall as Row).param).toBe('sourcePath');
  });
});

describe('describe accepts the legacy tool+action pair as a migration view', () => {
  it('resolves the legacy pair to the canonical capability contract', () => {
    const result = describeCapability({ tool: 'manage_asset', action: 'import' });
    expect(result.success).toBe(true);
    expect(result.scope).toBe('capability');
    expect(result.capability).toBe(ASSET_IMPORT);
    expect(result.migratedFrom).toEqual({ tool: 'manage_asset', action: 'import' });
  });

  it('gives the legacy pair the exact same contract as the canonical id', () => {
    const legacy = describeCapability({ tool: 'manage_asset', action: 'import' });
    const canonical = describeCapability({ capability: ASSET_IMPORT });
    expect(legacy.inputSchema).toEqual(canonical.inputSchema);
    expect(legacy.hashes).toEqual(canonical.hashes);
  });

  it('resolves a declared alias to its canonical capability', () => {
    // The registry declares exactly three aliases; this is one of them.
    const result = describeCapability({ capability: 'blueprint.create_widget' });
    expect(result.success).toBe(true);
    expect(result.scope).toBe('capability');
    expect(result.resolvedFromAlias).toBe('blueprint.create_widget');
    expect(result.capability).toBe('blueprint.create_widget_blueprint');
  });
});

describe('unknown, disabled and unavailable discovery is guided and never runnable', () => {
  it('rejects an unknown capability with bounded suggestions and an executable nextCall', () => {
    const result = describeCapability({ capability: 'asset.improt' });
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_CAPABILITY');
    expect(result.suggestions).toContain(ASSET_IMPORT);
    expect((result.suggestions as string[]).length).toBeLessThanOrEqual(3);
    expect((result.nextCall as Row).operation).toBe('describe');
    expect((result.nextCall as Row).capability).toBe(ASSET_IMPORT);
    expect(result.inputSchema).toBeUndefined();
  });

  it('does not dump a schema catalog on a miss', () => {
    const result = describeCapability({ capability: 'totally.bogus' });
    expect(result.success).toBe(false);
    expect(JSON.stringify(result).length).toBeLessThan(2000);
    expect(result.capabilities).toBeUndefined();
    expect(result.parameters).toBeUndefined();
  });

  it('describes a disabled capability as not runnable and points at configure', () => {
    dynamicToolManager.disableTools(['manage_asset']);
    const result = describeCapability({ capability: ASSET_IMPORT });
    expect(result.success).toBe(true);
    expect((result.availability as Row).status).toBe('disabled');
    expect(result.runnable).toBe(false);
    expect(result.nextCall).toEqual({ operation: 'configure', tool: 'manage_asset' });
  });

  it('marks a disabled capability disabled in search results too', () => {
    dynamicToolManager.disableTools(['manage_asset']);
    const hit = rows(search({ tool: 'manage_asset', action: 'import' }))[0];
    expect((hit.availability as Row).status).toBe('disabled');
    expect(hit.nextCall).toEqual({ operation: 'configure', tool: 'manage_asset' });
  });

  it('states why a capability is unavailable rather than hiding the reason', () => {
    dynamicToolManager.disableTools(['manage_asset']);
    const result = describeCapability({ capability: ASSET_IMPORT });
    const reasons = (result.availability as Row).reasons as string[];
    expect(Array.isArray(reasons)).toBe(true);
    expect(reasons).toContain('parent_tool_disabled');
  });
});
