/**
 * Foliage family records (14 actions).
 *
 * Grounded in environment-foliage-actions.ts and native
 * EnvironmentHandlers.cpp foliage dispatch. add_foliage dispatches to
 * add_foliage_type (when meshPath present) or paint_foliage (when
 * foliageType/locations present). create_foliage_type dispatches to
 * add_foliage_type. paint_foliage_instances and remove_foliage_instances
 * dispatch through build_environment. configure_foliage_* dispatch through
 * build_environment.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';

const F = 'foliage';
const WU = ['Foliage must be added, configured, or removed in the level.'];
const AREAS = {
  type: 'array', items: P.area,
  description: 'Several area boxes at once, each {min, max}: every instance inside any of them is removed under one consent (every type unless foliageType names one).',
};

export const FOLIAGE_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'build_environment.add_foliage', action: 'add_foliage', family: F,
    topics: ['foliage', 'vegetation', 'grass', 'trees', 'scatter foliage', 'plants'],
    summary: 'Add foliage: with only meshPath (+ name) create or update a foliage type asset; with a foliage type or a placement (locations, position, location + radius + count) scatter instances onto the ground, a bare meshPath standing in for its auto foliage type.',
    whenToUse: WU, whenNotToUse: ['A procedural foliage volume should be used for large areas.'],
    inputProps: { count: P.count, name: P.name, foliageType: P.foliageType, foliageTypePath: P.foliageTypePath,
      meshPath: P.meshPath, path: { ...P.path, description: 'Folder the new foliage type asset goes in (type creation only; default /Game/Foliage).' },
      density: { ...P.density, description: 'Type creation: instances per 1000x1000 units. Scatter: brush fill 0-1 (count overrides it).' },
      minScale: P.minScale, maxScale: P.maxScale,
      alignToNormal: P.alignToNormal, randomYaw: P.randomYaw, cullDistance: P.cullDistance,
      locations: P.locations, location: P.location, radius: P.radius, position: P.position },
    effect: 'write', latency: 'interactive', resources: 'medium',
    dispatchAction: 'add_foliage',
    exampleInput: { action: 'add_foliage', meshPath: '/Game/Meshes/SM_Bush', name: 'Bush_Type' },
  }),
  buildRecord({
    id: 'build_environment.add_foliage_instances', action: 'add_foliage_instances', family: F,
    summary: 'Add explicit foliage instances: full transforms, or bare locations varied by minScale/maxScale/randomYaw. One undo step removes them all; the reply\'s undo block says so.',
    whenToUse: WU, whenNotToUse: ['Random scatter via paint_foliage is sufficient.'],
    inputProps: { foliageType: P.foliageType, foliageTypePath: P.foliageTypePath,
      meshPath: { ...P.meshPath, description: 'StaticMesh to place when no foliage type is named; its /Game/Foliage/Auto_<mesh> type is made once and reused.' },
      locations: P.locations, transforms: P.transforms,
      minScale: P.minScale, maxScale: P.maxScale, randomYaw: P.randomYaw },
    effect: 'write', latency: 'interactive', resources: 'medium',
    dispatchAction: 'add_foliage_instances',
    exampleInput: { action: 'add_foliage_instances', foliageType: 'Bush_Type', transforms: [{ location: { x: 0, y: 0, z: 0 } }] },
  }),
  buildRecord({
    id: 'build_environment.get_foliage_instances', action: 'get_foliage_instances', family: F,
    topics: ['count per type', 'vegetation count', 'positions and scale', 'count trees', 'how many trees'],
    summary: 'Count foliage instances in the level, across every foliage actor (each loaded sublevel and World Partition cell keeps its own; foliageActors counts them), in total and per foliage type (byType), and list their positions; one type also returns rotation and scale ({x, y, z}: the whole scale, so a non-uniform one shows). The summary flag drops the list; limit caps it.',
    whenToUse: ['Existing foliage instances must be inspected.', 'What foliage a level holds must be counted before removing it (summary).'],
    whenNotToUse: ['Foliage should be removed rather than inspected.'],
    inputProps: {
      foliageType: P.foliageType, foliageTypePath: P.foliageTypePath,
      summary: { type: 'boolean', description: 'Count only: byType and count, no instance list. A level-wide listing runs to tens of KB.' },
      limit: { type: 'number', minimum: 0, description: 'Return at most this many instances; truncated says when more exist (count stays the total).' },
    },
    effect: 'read', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'get_foliage_instances', foliageType: 'Bush_Type' },
  }),
  buildRecord({
    id: 'build_environment.remove_foliage', action: 'remove_foliage', family: F,
    summary: 'Remove foliage from every foliage actor in the level (each loaded sublevel and World Partition cell keeps its own): every instance of foliageType, every type (removeAll true), or only the instances inside an area box or several (areas, each {min, max}; with foliageType, that type only). A whole type also leaves the level\'s foliage list. One undo step; instancesRemoved counts what went (0 when nothing matched), an unknown type is FOLIAGE_TYPE_NOT_FOUND, and a box without min and max removes nothing.',
    whenToUse: WU, whenNotToUse: ['Foliage should be hidden rather than removed.'],
    inputProps: { foliageType: P.foliageType, foliageTypePath: P.foliageTypePath, removeAll: P.removeAll, area: P.area, areas: AREAS },
    effect: 'destructive', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'remove_foliage', foliageType: 'Bush_Type' },
  }),
  buildRecord({
    id: 'build_environment.paint_foliage', action: 'paint_foliage', family: F,
    summary: 'Paint foliage over a brush disc (location + radius) or a box area: count instances dropped onto the ground, each with a random scale and yaw. One undo step removes the whole stroke; NO_SURFACE (no ground below any point) adds nothing.',
    whenToUse: WU, whenNotToUse: ['Explicit transforms should be used via add_foliage_instances.'],
    inputProps: { foliageType: P.foliageType, foliageTypePath: P.foliageTypePath,
      locations: P.locations, position: P.position, location: P.location, radius: P.radius,
      density: P.density, area: P.area, count: P.count, snapToSurface: P.snapToSurface,
      minScale: P.minScale, maxScale: P.maxScale, randomYaw: P.randomYaw, alignToNormal: P.alignToNormal },
    effect: 'write', latency: 'interactive', resources: 'low',
    dispatchAction: 'paint_foliage',
    exampleInput: { action: 'paint_foliage', foliageType: '/Game/Foliage/FT_Grass', area: { min: { x: 0, y: -150, z: 0 }, max: { x: 5000, y: -100, z: 0 } }, count: 40, minScale: 0.8, maxScale: 1.4, randomYaw: true },
  }),
  buildRecord({
    id: 'build_environment.paint_foliage_instances', action: 'paint_foliage_instances', family: F,
    summary: 'Paint foliage instances with brush-based placement: a disc or a box area, dropped onto the ground.',
    whenToUse: WU, whenNotToUse: ['Explicit locations should be used via add_foliage_instances.'],
    inputProps: { foliageType: P.foliageType, foliageTypePath: P.foliageTypePath,
      locations: P.locations, position: P.position, radius: P.radius, density: P.density,
      area: P.area, count: P.count, snapToSurface: P.snapToSurface,
      minScale: P.minScale, maxScale: P.maxScale, randomYaw: P.randomYaw, alignToNormal: P.alignToNormal },
    effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'paint_foliage_instances', foliageType: 'Bush_Type', position: { x: 0, y: 0, z: 0 } },
  }),
  buildRecord({
    id: 'build_environment.create_foliage_type', action: 'create_foliage_type', family: F,
    summary: 'Create a foliage type asset from a static mesh.',
    whenToUse: ['A new foliage type asset is needed.'],
    whenNotToUse: ['An existing foliage type should be reused.'],
    inputProps: { name: P.name, meshPath: P.meshPath, path: P.path, density: P.density, minScale: P.minScale, maxScale: P.maxScale,
      alignToNormal: P.alignToNormal, randomYaw: P.randomYaw, cullDistance: P.cullDistance },
    effect: 'write', latency: 'interactive', resources: 'low',
    dispatchAction: 'add_foliage_type',
    exampleInput: { action: 'create_foliage_type', name: 'Bush_Type', meshPath: '/Game/Meshes/SM_Bush' },
  }),
  buildRecord({
    id: 'build_environment.configure_foliage_mesh', action: 'configure_foliage_mesh', family: F,
    summary: 'Configure the mesh assignment for a foliage type.',
    whenToUse: ['A foliage type needs a different mesh.'],
    whenNotToUse: ['A new foliage type should be created instead.'],
    inputProps: { foliageType: P.foliageType, foliageTypePath: P.foliageTypePath,
      meshPath: P.meshPath, staticMesh: P.staticMesh },
    // The reply of every configure_foliage variant: what it wrote, and (mesh) what became of the placed instances.
    outputProps: {
      configuredProperties: { type: 'array', items: { type: 'string' }, description: 'Every setting this call wrote, by UFoliageType property name (Mesh, Density, ScaleX, CullDistance, ...), the reflected settings entries included.' },
      configuredPropertyCount: { type: 'number', description: 'How many properties configuredProperties lists.' },
      previousMesh: { type: 'string', description: 'Mesh assignment: the mesh the type used before this call (empty when it had none).' },
      placedInstances: { type: 'number', description: 'Mesh assignment: instances of this type placed in the level (its foliage actor).' },
      instancesShowingNewMesh: { type: 'number', description: 'Mesh assignment: how many of the placed instances now draw the new mesh, read back off their component; below placedInstances means some did not follow.' },
    },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_foliage_mesh', foliageType: 'Bush_Type', meshPath: '/Game/Meshes/SM_Bush_v2' },
  }),
  buildRecord({
    id: 'build_environment.configure_foliage_placement', action: 'configure_foliage_placement', family: F,
    summary: 'Configure placement parameters for a foliage type.',
    whenToUse: ['Foliage placement density or scale must be tuned.'],
    whenNotToUse: ['Foliage should be placed manually.'],
    inputProps: { foliageType: P.foliageType, foliageTypePath: P.foliageTypePath,
      density: P.density, minScale: P.minScale, maxScale: P.maxScale, alignToNormal: P.alignToNormal, randomYaw: P.randomYaw },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_foliage_placement', foliageType: 'Bush_Type', density: 0.5 },
  }),
  buildRecord({
    id: 'build_environment.configure_foliage_lod', action: 'configure_foliage_lod', family: F,
    summary: 'Configure LOD settings for a foliage type.',
    whenToUse: ['Foliage LOD distances must be tuned.'],
    whenNotToUse: ['LODs should be generated rather than configured.'],
    inputProps: { foliageType: P.foliageType, foliageTypePath: P.foliageTypePath, settings: P.settings },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_foliage_lod', foliageType: 'Bush_Type' },
  }),
  buildRecord({
    id: 'build_environment.configure_foliage_collision', action: 'configure_foliage_collision', family: F,
    summary: 'Configure collision settings for a foliage type.',
    whenToUse: ['Foliage collision must be enabled or disabled.'],
    whenNotToUse: ['Collision is not needed for decorative foliage.'],
    inputProps: { foliageType: P.foliageType, foliageTypePath: P.foliageTypePath,
      collisionEnabled: { ...P.collisionEnabled, description: 'Collision of placed instances: true = query and physics, false = none.' } },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_foliage_collision', foliageType: 'Bush_Type', collisionEnabled: true },
  }),
  buildRecord({
    id: 'build_environment.configure_foliage_culling', action: 'configure_foliage_culling', family: F,
    summary: 'Configure culling distance for a foliage type.',
    whenToUse: ['Foliage cull distance must be tuned.'],
    whenNotToUse: ['All foliage should remain visible at all distances.'],
    inputProps: { foliageType: P.foliageType, foliageTypePath: P.foliageTypePath,
      cullDistance: { ...P.cullDistance, description: 'Distance at which instances are culled (CullDistance max); the fade start is lowered to it if it was further.' } },
    effect: 'write', behavior: { idempotency: 'idempotent' }, latency: 'interactive', resources: 'low',
    exampleInput: { action: 'configure_foliage_culling', foliageType: 'Bush_Type', cullDistance: 5000 },
  }),
  buildRecord({
    id: 'build_environment.create_procedural_foliage', action: 'create_procedural_foliage', family: F,
    summary: 'Scatter foliage over an area in one call: make a Procedural Foliage Spawner (and one foliage type per foliageTypes entry) in path, place a Procedural Foliage Volume over bounds (a box of size, default 1000 on each axis, centred on location) and run the simulation as the Resimulate button does, which traces the instances onto the landscape, static mesh and BSP surfaces inside the volume. A mesh that does not load (MESH_NOT_FOUND) or an empty foliageTypes is refused before anything is created; a run that places no instance is NOTHING_SPAWNED (the assets and volume stay).',
    whenToUse: ['Large-area procedural foliage scatter is needed.'],
    whenNotToUse: ['Manual foliage placement is sufficient.'],
    inputProps: { name: P.name, volumeName: { ...P.volumeName, description: 'Label of the procedural foliage volume (name is accepted too).' },
      path: { ...P.path, description: 'Folder for the spawner and its foliage type assets (default /Game/ProceduralFoliage).' },
      foliageTypes: { ...P.foliageTypes, description: 'At least one {meshPath, density}: the static mesh to scatter and its density, the seeds per 10 m x 10 m the simulation starts from (default 10). Its steps then spread, age and thin them (instances keep a collision radius apart), so the count placed differs; get_foliage_instances reads it.' }, bounds: { ...P.bounds, description: 'The area {location, size}: a box of size (default 1000 on each axis) centred on location (default the origin); it must enclose a surface to place on.' }, seed: { ...P.seed, description: 'Random seed of the scatter (default 12345); the same seed gives the same placement.' }, tileSize: { ...P.tileSize, description: 'Side of one simulated tile in world units (default 1000); the scatter repeats 10 unique tiles across the area.' } },
    effect: 'write', latency: 'interactive', resources: 'medium',
    dispatchAction: 'create_procedural_foliage',
    exampleInput: { action: 'create_procedural_foliage', name: 'PFV_1', foliageTypes: [{ meshPath: '/Game/Meshes/SM_Bush', density: 0.5 }] },
  }),
];
