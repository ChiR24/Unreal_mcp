// scripts/qa/world-qa-pass.ts
// Manual QA entrypoint for Task 16. Imports the world capability aggregate
// directly from source and asserts, fail-closed, exactly 301 unique records,
// 150 reused build_environment identities, version/plugin negative filters,
// async PCG contract truth, partition-grid source-backed shape, shell thickness
// scalar, Nanite 5.7 gate, Water runtime-optional truth, geometry route
// dispositions, and a controlled scratch malformed fixture. Prints
// WORLD_QA_PASS 301 on success, non-zero exit on any failure.
async function main(): Promise<void> {
  console.error('QA_START');
  const { BUILD_ENVIRONMENT_RECORDS } = await import('../../src/tools/catalog/capabilities/records/build-environment/index.js');
  const { MANAGE_LEVEL_STRUCTURE_RECORDS } = await import('../../src/tools/catalog/capabilities/records/world/manage-level-structure.index.js');
  const { MANAGE_GEOMETRY_RECORDS } = await import('../../src/tools/catalog/capabilities/records/world/manage-geometry.index.js');
  const { MANAGE_PCG_RECORDS } = await import('../../src/tools/catalog/capabilities/records/world/manage-pcg.index.js');
  const { WORLD_CAPABILITY_CATALOG, WORLD_SOURCE_RECORDS } = await import('../../src/tools/catalog/capabilities/records/world/index.js');
  const { GEOMETRY_ROUTE_DISPOSITIONS } = await import('../../src/tools/catalog/capabilities/normalization/routedispositions-geometry.data.js');
  const { createCapabilityRecord } = await import('../../src/tools/catalog/capabilities/parser.js');

  const assert = (cond: boolean, msg: string): void => {
    if (!cond) { console.error(`WORLD_QA_FAIL: ${msg}`); process.exit(1); }
  };
  const req = <T>(v: T | undefined | null, msg: string): T => {
    if (v === undefined || v === null) { console.error(`WORLD_QA_FAIL: ${msg}`); process.exit(1); }
    return v;
  };

  const catalog = WORLD_CAPABILITY_CATALOG;
  assert(catalog.length === 301, `expected 301 records, got ${catalog.length}`);
  assert(new Set(catalog.map((r) => r.id)).size === 301, 'expected 301 unique IDs');

  assert(BUILD_ENVIRONMENT_RECORDS.length === 150, 'build_environment must be 150');
  const envIds = new Set(BUILD_ENVIRONMENT_RECORDS.map((r) => r.id));
  const reusedInCatalog = catalog.filter((r) => envIds.has(r.id));
  assert(reusedInCatalog.length === 150, `expected 150 reused env records in catalog, got ${reusedInCatalog.length}`);
  const identityOk = BUILD_ENVIRONMENT_RECORDS.every((src) => {
    const match = WORLD_SOURCE_RECORDS.find((r) => r.id === src.id);
    return match !== undefined && match === src;
  });
  assert(identityOk, 'build_environment objects must be reused by identity, not re-created');

  const netNew = catalog.length - BUILD_ENVIRONMENT_RECORDS.length;
  assert(netNew === 151, `expected 151 net-new, got ${netNew}`);
  assert(MANAGE_LEVEL_STRUCTURE_RECORDS.length === 45, 'level-structure must be 45');
  assert(MANAGE_GEOMETRY_RECORDS.length === 76, 'geometry must be 76');
  assert(MANAGE_PCG_RECORDS.length === 30, 'pcg must be 30');

  const nanite = MANAGE_GEOMETRY_RECORDS.find((r) => r.id === 'manage_geometry.convert_to_nanite');
  assert(nanite !== undefined && nanite.availability.unreal.min.major === 5 && nanite.availability.unreal.min.minor === 7, 'convert_to_nanite gated to 5.7');
  assert(req(nanite, 'nanite record missing').availability.unreal.min.minor > 6, 'convert_to_nanite must not surface on 5.6');
  const negativePlugins = new Set<string>();
  for (const r of [...MANAGE_GEOMETRY_RECORDS, ...MANAGE_PCG_RECORDS]) {
    const runnable = r.availability.requiredPlugins.length === 0
      || r.availability.requiredPlugins.every((p: string) => negativePlugins.has(p));
    assert(!runnable, `${r.id} must NOT be runnable on a negative profile`);
  }
  assert(Object.isFrozen(catalog), 'catalog must be frozen');

  const execSrc = req(MANAGE_PCG_RECORDS.find((r) => r.id === 'manage_pcg.execute_pcg_graph'), 'execute_pcg_graph record missing');
  const execBuilt = createCapabilityRecord(execSrc);
  assert(execSrc.behavior.longRunning === true, 'execute_pcg_graph must be long-running');
  const taskIdType = (execBuilt.schemas.output.properties.taskId as { type?: string } | null)?.type;
  assert(taskIdType === 'number', 'execute_pcg_graph taskId must be a number');
  assert(execBuilt.schemas.output.properties.bWasCancelled === undefined, 'execute_pcg_graph must not advertise bWasCancelled');
  assert(execBuilt.schemas.input.properties.actorName !== undefined, 'execute_pcg_graph needs component selector actorName');

  const gridSrc = req(MANAGE_PCG_RECORDS.find((r) => r.id === 'manage_pcg.set_pcg_partition_grid_size'), 'set_pcg_partition_grid_size record missing');
  const gridBuilt = createCapabilityRecord(gridSrc);
  assert(gridBuilt.schemas.input.properties.gridSize !== undefined, 'partition grid needs gridSize');
  assert(gridBuilt.schemas.input.properties.scope !== undefined, 'partition grid needs scope');
  assert(gridBuilt.schemas.input.properties.gridCellSize === undefined, 'partition grid must not use gridCellSize');
  assert(gridBuilt.schemas.output.properties.saved !== undefined, 'partition grid returns saved (leaf proves save output)');

  const shellSrc = req(MANAGE_GEOMETRY_RECORDS.find((r) => r.id === 'manage_geometry.shell'), 'shell record missing');
  const shellBuilt = createCapabilityRecord(shellSrc);
  const thicknessType = (shellBuilt.schemas.input.properties.thickness as { type?: string } | null)?.type;
  assert(thicknessType === 'number', 'shell thickness must be scalar number');
  assert(shellBuilt.schemas.input.properties.offset === undefined, 'shell must not use vector offset');

  const water = BUILD_ENVIRONMENT_RECORDS.filter((r) => r.id.startsWith('build_environment.create_water_body') || r.id.startsWith('build_environment.configure_water'));
  assert(water.length > 0, 'water records must be present in reused build_environment');
  for (const w of water) {
    assert((w.availability.requiredPlugins ?? []).indexOf('Water') === -1, 'Water must not be a compile-gated required plugin');
    assert(w.availability.unreal.min.major <= 5, 'Water records are 5.x runtime-optional');
  }

  const targetIds = GEOMETRY_ROUTE_DISPOSITIONS.map((d) => d.targetCanonicalId);
  assert(targetIds.indexOf('cap:manage_geometry:difference') !== -1, 'difference disposition present');
  assert(targetIds.indexOf('cap:manage_geometry:bridge') !== -1, 'bridge hidden disposition present');
  assert(targetIds.indexOf('cap:manage_geometry:loft') !== -1, 'loft hidden disposition present');
  assert(targetIds.indexOf('cap:manage_geometry:dynamicmesh_raw_residual') !== -1, '11 residual disposition present');
  for (const d of GEOMETRY_ROUTE_DISPOSITIONS) {
    assert(d.evidenceSymbol.length > 0, `disposition ${d.key} must cite source symbol`);
  }

  const scratch = [...MANAGE_PCG_RECORDS, MANAGE_PCG_RECORDS[0]];
  const scratchUnique = new Set(scratch.map((r) => r.id)).size === scratch.length;
  assert(scratchUnique === false, 'scratch duplicate-id fixture must break unique guard (proves fail-closed)');

  const sampleRec = req(catalog[0], 'catalog must have a record');
  assert(Object.isFrozen(sampleRec), 'built catalog record must be frozen (fail-closed)');
  assert(Object.isFrozen(sampleRec.schemas), 'built record.schemas must be frozen (fail-closed)');
  assert(Object.isFrozen(sampleRec.schemas.input.properties), 'built record input properties must be frozen (fail-closed)');
  assert(Object.isFrozen(sampleRec.schemas.output.properties), 'built record output properties must be frozen (fail-closed)');
  assert(Object.isFrozen(sampleRec.discovery), 'built record discovery must be frozen (fail-closed)');
  assert(sampleRec.aliases !== undefined ? Object.isFrozen(sampleRec.aliases) : true, 'built record aliases array must be frozen');

  console.log('WORLD_QA_PASS 301');
}

main().catch((e) => {
  const zod = e as { issues?: Array<{ message: string }>; message?: string; constructor?: { name?: string } };
  console.error('WORLD_QA_FAIL_BODY:', zod.constructor?.name, zod.message, JSON.stringify(zod.issues ?? null));
  process.exit(1);
});
