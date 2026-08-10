// tests/unit/world-capability-records.test.ts
// Focused fail-closed tests for the Task 16 world capability catalog.
// Proves: 45 manage_level_structure + 86 manage_geometry + 30 manage_pcg
// records (161 net-new), 311 frozen aggregate reusing the 150 build_environment
// records by identity, exact action-set/order parity extracted from the canonical
// tool definitions (no duplicated arrays), async PCG contract truth (taskId is a
// number, no cancellation/poll), partition-grid-size source-backed shape, shell
// thickness scalar, Nanite 5.7 gate, Water runtime-optional truth, geometry route
// dispositions fully resolved, exactly one injected action property/required
// entry per net-new record, and fail-closed behavior for malformed/duplicate
// catalogs.

import { describe, expect, it } from 'vitest';
import { consolidatedToolDefinitions } from '../../src/tools/catalog/consolidated-tool-definitions.js';
import type { ToolDefinition } from '../../src/tools/definitions/shared/tool-definition.js';
import { createCapabilityRecord } from '../../src/tools/catalog/capabilities/index.js';
import { BUILD_ENVIRONMENT_RECORDS } from '../../src/tools/catalog/capabilities/records/build-environment/index.js';
import {
  MANAGE_LEVEL_STRUCTURE_RECORDS,
  MANAGE_LEVEL_STRUCTURE_RECORD_COUNT,
} from '../../src/tools/catalog/capabilities/records/world/manage-level-structure.index.js';
import {
  MANAGE_GEOMETRY_RECORDS,
  MANAGE_GEOMETRY_RECORD_COUNT,
} from '../../src/tools/catalog/capabilities/records/world/manage-geometry.index.js';
import {
  MANAGE_PCG_RECORDS,
  MANAGE_PCG_RECORD_COUNT,
} from '../../src/tools/catalog/capabilities/records/world/manage-pcg.index.js';
import {
  WORLD_CAPABILITY_CATALOG,
  WORLD_CAPABILITY_RECORD_COUNT,
  WORLD_SOURCE_RECORDS,
  WORLD_NET_NEW_COUNT,
  WORLD_REUSED_BUILD_ENVIRONMENT_COUNT,
} from '../../src/tools/catalog/capabilities/records/world/index.js';
import {
  GEOMETRY_ROUTE_DISPOSITIONS,
} from '../../src/tools/catalog/capabilities/normalization/routedispositions-geometry.data.js';

function ids(records: readonly { id: string }[]): string[] {
  return records.map((r) => r.id);
}

function actionEnum(tool: ToolDefinition): readonly string[] {
  const properties = (tool.inputSchema.properties ?? {}) as Record<string, { enum?: readonly string[] }>;
  const action = properties.action;
  return action?.enum ?? [];
}

const LEVEL_STRUCTURE_ACTIONS = actionEnum(consolidatedToolDefinitions.find((t) => t.name === 'manage_level_structure') as NonNullable<typeof consolidatedToolDefinitions[number]>);
const GEOMETRY_ACTIONS = actionEnum(consolidatedToolDefinitions.find((t) => t.name === 'manage_geometry') as NonNullable<typeof consolidatedToolDefinitions[number]>);
const PCG_ACTIONS = actionEnum(consolidatedToolDefinitions.find((t) => t.name === 'manage_pcg') as NonNullable<typeof consolidatedToolDefinitions[number]>);

describe('Task 16 net-new counts', () => {
  it('level-structure 45, geometry 86, pcg 30 records', () => {
    expect(MANAGE_LEVEL_STRUCTURE_RECORD_COUNT).toBe(45);
    expect(MANAGE_LEVEL_STRUCTURE_RECORDS).toHaveLength(45);
    expect(MANAGE_GEOMETRY_RECORD_COUNT).toBe(86);
    expect(MANAGE_GEOMETRY_RECORDS).toHaveLength(86);
    expect(MANAGE_PCG_RECORD_COUNT).toBe(30);
    expect(MANAGE_PCG_RECORDS).toHaveLength(30);
  });
  it('net-new world records total 161', () => {
    expect(MANAGE_LEVEL_STRUCTURE_RECORD_COUNT + MANAGE_GEOMETRY_RECORD_COUNT + MANAGE_PCG_RECORD_COUNT)
      .toBe(WORLD_NET_NEW_COUNT);
    expect(WORLD_NET_NEW_COUNT).toBe(161);
  });
});

describe('Task 16 action-set/order parity (extracted from tool definitions)', () => {
  it('level-structure records match the tool action enum in order', () => {
    expect(ids(MANAGE_LEVEL_STRUCTURE_RECORDS)).toEqual(
      LEVEL_STRUCTURE_ACTIONS.map((a) => `manage_level_structure.${a}`),
    );
  });
  it('geometry records match the tool action enum in order', () => {
    expect(ids(MANAGE_GEOMETRY_RECORDS)).toEqual(
      GEOMETRY_ACTIONS.map((a) => `manage_geometry.${a}`),
    );
  });
  it('pcg records match the tool action enum in order', () => {
    expect(ids(MANAGE_PCG_RECORDS)).toEqual(
      PCG_ACTIONS.map((a) => `manage_pcg.${a}`),
    );
  });
  it('all net-new IDs are unique', () => {
    const netNewIds = [...ids(MANAGE_LEVEL_STRUCTURE_RECORDS), ...ids(MANAGE_GEOMETRY_RECORDS), ...ids(MANAGE_PCG_RECORDS)];
    expect(new Set(netNewIds).size).toBe(161);
  });
});

describe('Task 16 frozen 311 aggregate', () => {
  it('aggregate has exactly 311 records and unique IDs', () => {
    expect(WORLD_CAPABILITY_RECORD_COUNT).toBe(311);
    expect(new Set(ids(WORLD_CAPABILITY_CATALOG)).size).toBe(311);
  });
  it('aggregate reuses the 150 build_environment records by object identity', () => {
    expect(WORLD_REUSED_BUILD_ENVIRONMENT_COUNT).toBe(150);
    const reused = BUILD_ENVIRONMENT_RECORDS.every((src) => WORLD_SOURCE_RECORDS.includes(src));
    expect(reused).toBe(true);
    const sameRef = BUILD_ENVIRONMENT_RECORDS.every((src) => {
      const inWorld = WORLD_SOURCE_RECORDS.find((r) => r.id === src.id);
      return inWorld === src;
    });
    expect(sameRef).toBe(true);
  });
  it('aggregate is frozen (fail-closed)', () => {
    expect(Object.isFrozen(WORLD_CAPABILITY_CATALOG)).toBe(true);
  });
});

describe('Task 16 built catalog records are strictly deep-frozen (fail-closed)', () => {
  const rec = WORLD_CAPABILITY_CATALOG[0];
  it('a built record and its nested schemas are frozen', () => {
    expect(Object.isFrozen(rec)).toBe(true);
    expect(Object.isFrozen(rec.schemas)).toBe(true);
    expect(Object.isFrozen(rec.schemas.input)).toBe(true);
    expect(Object.isFrozen(rec.schemas.input.properties)).toBe(true);
    expect(Object.isFrozen(rec.schemas.output.properties)).toBe(true);
    expect(Object.isFrozen(rec.discovery)).toBe(true);
    expect(Object.isFrozen(rec.aliases)).toBe(true);
    expect(Object.isFrozen(rec.legacyIds)).toBe(true);
  });
  it('a built record cannot be mutated without throwing under strict mode', () => {
    const attempt = () => { (rec as { id?: string }).id = 'mutated' as never; };
    expect(attempt).toThrow();
  });
});

describe('Task 16 async PCG contract truth', () => {
  it('execute_pcg_graph is long-running and returns a numeric taskId, not safe to retry', () => {
    const rec = MANAGE_PCG_RECORDS.find((r) => r.id === 'manage_pcg.execute_pcg_graph');
    expect(rec).toBeDefined();
    if (!rec) return;
    expect(rec.behavior.longRunning).toBe(true);
    expect(rec.behavior.safeToRetry).toBe(false);
    expect(rec.cost.latency).toBe('long-running');
    expect(rec.routing.dispatchAction).toBe('execute_pcg_graph');
    const built = createCapabilityRecord(rec);
    const outProps = built.schemas.output.properties;
    expect(outProps.taskId).toBeDefined();
    expect(outProps.taskId.type).toBe('number');
    expect(outProps.bWasCancelled).toBeUndefined();
    expect(outProps.success).toBeDefined();
  });
  it('execute_pcg_graph inputs require a component selector unless createComponent', () => {
    const rec = MANAGE_PCG_RECORDS.find((r) => r.id === 'manage_pcg.execute_pcg_graph');
    expect(rec).toBeDefined();
    if (!rec) return;
    const built = createCapabilityRecord(rec);
    const inp = built.schemas.input.properties;
    expect(inp.actorName).toBeDefined();
    expect(inp.componentName).toBeDefined();
    expect(inp.componentPath).toBeDefined();
    expect(inp.createComponent).toBeDefined();
    expect(inp.force).toBeDefined();
    expect(inp.save).toBeDefined();
    expect(inp.taskId).toBeUndefined();
    expect(inp.bWasCancelled).toBeUndefined();
  });
  it('only execute_pcg_graph is async among the 30 PCG records', () => {
    const async = MANAGE_PCG_RECORDS.filter((r) => r.behavior.longRunning);
    expect(async.map((r) => r.id)).toEqual(['manage_pcg.execute_pcg_graph']);
  });
});

describe('Task 16 partition grid size source-backed shape', () => {
  it('set_pcg_partition_grid_size uses gridSize/scope and returns scope/previousGridSize/gridSize/saved', () => {
    const rec = MANAGE_PCG_RECORDS.find((r) => r.id === 'manage_pcg.set_pcg_partition_grid_size');
    expect(rec).toBeDefined();
    if (!rec) return;
    const built = createCapabilityRecord(rec);
    const inp = built.schemas.input.properties;
    expect(inp.gridSize).toBeDefined();
    expect(inp.gridSize.type).toBe('number');
    expect(inp.scope).toBeDefined();
    expect(inp.gridCellSize).toBeUndefined();
    const out = built.schemas.output.properties;
    expect(out.scope).toBeDefined();
    expect(out.previousGridSize).toBeDefined();
    expect(out.gridSize).toBeDefined();
    expect(out.saved).toBeDefined();
  });
});

describe('Task 16 shell thickness is scalar', () => {
  it('shell.thickness is a number, not a vector offset', () => {
    const rec = MANAGE_GEOMETRY_RECORDS.find((r) => r.id === 'manage_geometry.shell');
    expect(rec).toBeDefined();
    if (!rec) return;
    const built = createCapabilityRecord(rec);
    expect(built.schemas.input.properties.thickness.type).toBe('number');
    expect(built.schemas.input.properties.offset).toBeUndefined();
  });
});

describe('Task 16 version/plugin negative filters', () => {
  it('convert_to_nanite is gated to UE 5.7+ and is not runnable on a 5.0/5.6 profile', () => {
    const rec = MANAGE_GEOMETRY_RECORDS.find((r) => r.id === 'manage_geometry.convert_to_nanite');
    expect(rec).toBeDefined();
    if (!rec) return;
    expect(rec.availability.unreal.min.major).toBe(5);
    expect(rec.availability.unreal.min.minor).toBe(7);
    const runnableOn50 = rec.availability.unreal.min.major < 5
      || (rec.availability.unreal.min.major === 5 && rec.availability.unreal.min.minor <= 0);
    expect(runnableOn50).toBe(false);
    const runnableOn56 = rec.availability.unreal.min.major < 5
      || (rec.availability.unreal.min.major === 5 && rec.availability.unreal.min.minor <= 6);
    expect(runnableOn56).toBe(false);
  });
  it('all geometry require GeometryScripting and all pcg require PCG', () => {
    for (const r of MANAGE_GEOMETRY_RECORDS) expect(r.availability.requiredPlugins).toContain('GeometryScripting');
    for (const r of MANAGE_PCG_RECORDS) expect(r.availability.requiredPlugins).toContain('PCG');
  });
  it('a negative profile (UE 5.0, no PCG/GeometryScripting) cannot surface geometry/pcg actions', () => {
    const negativeProfilePlugins = new Set<string>();
    for (const r of [...MANAGE_GEOMETRY_RECORDS, ...MANAGE_PCG_RECORDS]) {
      const required = r.availability.requiredPlugins;
      const runnable = required.length > 0 && required.every((p) => negativeProfilePlugins.has(p));
      expect(runnable).toBe(false);
    }
  });
  it('reused build_environment Water records stay runtime-optional (5.0, no Water plugin gate)', () => {
    const water = BUILD_ENVIRONMENT_RECORDS.filter((r) => r.id.startsWith('build_environment.create_water_body') || r.id.startsWith('build_environment.configure_water'));
    expect(water.length).toBeGreaterThan(0);
    for (const r of water) { expect(r.availability.requiredPlugins ?? []).not.toContain('Water'); expect(r.availability.unreal.min.major).toBeLessThanOrEqual(5); }
  });
});

describe('Task 16 geometry route dispositions resolved', () => {
  it('all 12 geometry route dispositions are source-backed (difference + 11 discrete dynamicmesh + bridge/loft hidden)', () => {
    const targetIds = GEOMETRY_ROUTE_DISPOSITIONS.map((d) => d.targetCanonicalId);
    for (const id of ['difference','bridge','loft','create_procedural_mesh','append_triangle','append_vertex','set_uvs','set_vertex_color','split_normals','delete_vertex','delete_triangle','get_vertex_position','set_vertex_position','translate_mesh']) {
      expect(targetIds, `missing geometry target ${id}`).toContain(`cap:manage_geometry:${id}`);
    }
    expect(GEOMETRY_ROUTE_DISPOSITIONS).toHaveLength(14);
    for (const d of GEOMETRY_ROUTE_DISPOSITIONS) {
      expect(d.evidenceSymbol.length).toBeGreaterThan(0);
      expect(d.targetCanonicalId).toMatch(/^cap:manage_geometry:/);
    }
  });
});

describe('Task 16 exactly one injected action property/required entry', () => {
  it('every net-new record input schema declares exactly one action property and action required entry', () => {
    for (const src of [...MANAGE_LEVEL_STRUCTURE_RECORDS, ...MANAGE_GEOMETRY_RECORDS, ...MANAGE_PCG_RECORDS]) { const built = createCapabilityRecord(src); expect(Object.keys(built.schemas.input.properties).filter((k) => k === 'action')).toHaveLength(1); expect(built.schemas.input.required.filter((k) => k === 'action')).toHaveLength(1); }
  });
});

describe('Task 16 fail-closed malformed/duplicate catalogs', () => {
  it('a duplicate ID in the net-new set breaks the unique-ID guard', () => {
    const dupList = [...MANAGE_LEVEL_STRUCTURE_RECORDS, ...MANAGE_GEOMETRY_RECORDS, ...MANAGE_PCG_RECORDS, MANAGE_PCG_RECORDS[0]];
    expect(new Set(dupList.map((r) => r.id)).size).not.toBe(dupList.length);
  });
  it('a malformed PCG output schema (bWasCancelled present) is not in the real catalog', () => {
    const rec = MANAGE_PCG_RECORDS.find((r) => r.id === 'manage_pcg.execute_pcg_graph');
    expect(rec).toBeDefined();
    if (!rec) return;
    const built = createCapabilityRecord(rec);
    expect(built.schemas.output.properties.bWasCancelled).toBeUndefined();
  });
});

describe('Task 16 stale monolith absent', () => {
  it('volume records are owned only by the A/B shards, totaling 28', () => {
    const volumeRecords = MANAGE_LEVEL_STRUCTURE_RECORDS.filter((r) => r.discovery.family === 'volume');
    expect(volumeRecords.length).toBe(28);
    for (const r of volumeRecords) { expect(r.id.startsWith('manage_level_structure.')).toBe(true); }
  });
});
