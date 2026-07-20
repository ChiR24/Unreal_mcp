/**
 * Procedural and misc family records (5 actions).
 *
 * Grounded in environment-procedural-actions.ts (create_procedural_terrain,
 * create_procedural_foliage) and environment-misc-actions.ts (bake_lightmap,
 * export_snapshot, import_snapshot, delete). create_procedural_terrain
 * dispatches to the 'create_procedural_terrain' bridge action.
 * bake_lightmap dispatches to 'bake_lightmap'. export_snapshot/import_snapshot
 * dispatch through build_environment. delete dispatches through build_environment.
 *
 * Snapshot light selectors are asymmetric: ExportSnapshot reads
 * directionalLightActorPath/skyLightActorPath from the request payload, while
 * McpApplyEnvironmentSnapshot reads them from the snapshot file being imported.
 * Both echo the resolved actor paths back on the response.
 *
 * Note: create_procedural_foliage is in the foliage family (foliage.data.ts)
 * because it creates a foliage volume. generate_lods is in the landscape family.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';

const F = 'procedural';

export const PROCEDURAL_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'build_environment.create_procedural_terrain', action: 'create_procedural_terrain', family: F,
    summary: 'Create a procedural terrain mesh actor.',
    whenToUse: ['A procedural terrain mesh must be generated.'],
    whenNotToUse: ['A heightmap-based landscape should be used instead.'],
    inputProps: { action: P.action, name: P.name, actorName: P.actorName, location: P.location,
      sizeX: P.sizeX, sizeY: P.sizeY, heightScale: P.heightScale, subdivisions: P.subdivisions,
      rotation: P.rotation, material: P.material },
    required: ['action'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    dispatchAction: 'create_procedural_terrain', dispatchMode: 'action',
    exampleInput: { action: 'create_procedural_terrain', name: 'PTerrain_1', sizeX: 1000, sizeY: 1000 },
    exampleOutput: { success: true, message: 'Procedural terrain created' },
  }),
  buildRecord({
    id: 'build_environment.bake_lightmap', action: 'bake_lightmap', family: F,
    summary: 'Bake lightmaps for the current level.',
    whenToUse: ['Lightmaps must be baked for static lighting.'],
    whenNotToUse: ['Dynamic lighting is sufficient.'],
    inputProps: { action: P.action, quality: P.quality },
    required: ['action'],
    effect: 'write', behavior: { longRunning: true, idempotency: 'idempotent' },
    latency: 'long-running', resources: 'high',
    dispatchAction: 'bake_lightmap', dispatchMode: 'action',
    exampleInput: { action: 'bake_lightmap', quality: 'Preview' },
    exampleOutput: { success: true, message: 'Lightmaps baked' },
  }),
  buildRecord({
    id: 'build_environment.export_snapshot', action: 'export_snapshot', family: F,
    summary: 'Export an environment snapshot to a file.',
    whenToUse: ['An environment state must be exported for later restoration.'],
    whenNotToUse: ['The level should be saved directly.'],
    inputProps: { action: P.action, path: P.path, filename: P.filename,
      directionalLightActorPath: P.directionalLightActorPath, skyLightActorPath: P.skyLightActorPath },
    required: ['action'],
    outputProps: { directionalLightActorPath: P.directionalLightActorPath, skyLightActorPath: P.skyLightActorPath },
    outputRequired: [],
    effect: 'read', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'export_snapshot', path: '/Game/Snapshots', filename: 'env.json',
      directionalLightActorPath: '/Game/Maps/Main.Main:PersistentLevel.DirectionalLight_0' },
    exampleOutput: { success: true, message: 'Snapshot exported',
      directionalLightActorPath: '/Game/Maps/Main.Main:PersistentLevel.DirectionalLight_0',
      skyLightActorPath: '/Game/Maps/Main.Main:PersistentLevel.SkyLight_0' },
  }),
  buildRecord({
    id: 'build_environment.import_snapshot', action: 'import_snapshot', family: F,
    summary: 'Import an environment snapshot from a file.',
    whenToUse: ['A previously exported environment snapshot must be restored.'],
    whenNotToUse: ['The environment should be rebuilt from scratch.'],
    inputProps: { action: P.action, path: P.path, filename: P.filename },
    required: ['action'],
    outputProps: { directionalLightActorPath: P.directionalLightActorPath, skyLightActorPath: P.skyLightActorPath },
    outputRequired: [],
    effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'import_snapshot', path: '/Game/Snapshots', filename: 'env.json' },
    exampleOutput: { success: true, message: 'Snapshot imported',
      directionalLightActorPath: '/Game/Maps/Main.Main:PersistentLevel.DirectionalLight_0',
      skyLightActorPath: '/Game/Maps/Main.Main:PersistentLevel.SkyLight_0' },
  }),
  buildRecord({
    id: 'build_environment.delete', action: 'delete', family: F,
    summary: 'Delete environment actors by name or path.',
    whenToUse: ['Environment actors must be permanently removed.'],
    whenNotToUse: ['Actors should be hidden rather than deleted.'],
    inputProps: { action: P.action, names: P.names, name: P.name, actorPath: P.actorPath,
      actorName: P.actorName, targetActor: P.targetActor },
    required: ['action'],
    effect: 'destructive', behavior: { supportsUndo: false, safeToRetry: false },
    latency: 'interactive', resources: 'low',
    exampleInput: { action: 'delete', names: ['Landscape_1'] },
    exampleOutput: { success: true, message: 'Actors deleted' },
  }),
];
