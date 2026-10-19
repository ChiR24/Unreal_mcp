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
import type { CapabilityRecordSource } from '../../model.js';
import { buildRecord } from './helpers.js';
import { P } from './properties.js';

const F = 'procedural';

export const PROCEDURAL_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'build_environment.create_procedural_terrain', action: 'create_procedural_terrain', family: F,
    summary: 'Create a procedural-mesh terrain actor (not a Landscape, so no sculpting or painting applies): a grid of sizeX by sizeY cells of spacing units each (default 100 x 100 of 100), subdivisions per side (2-200, default 50), raised by a fixed wave pattern whose peaks reach about 0.7 of heightScale (default 500), with collision and an optional material. Placed in the editor level, unsaved.',
    whenToUse: ['A procedural terrain mesh must be generated.'],
    whenNotToUse: ['A heightmap-based landscape should be used instead.'],
    inputProps: { name: { ...P.name, description: 'Label of the terrain actor (default ProceduralTerrain; actorName is accepted too).' },
      actorName: P.actorName, location: P.location,
      sizeX: P.sizeX, sizeY: P.sizeY, heightScale: P.heightScale, subdivisions: P.subdivisions,
      spacing: { ...P.spacing, description: 'World units per sizeX and sizeY unit: the terrain spans sizeX*spacing by sizeY*spacing (default 100).' },
      rotation: P.rotation, material: { ...P.material, description: 'Material for the terrain; a path that does not load fails the call.' } },
    effect: 'write', latency: 'interactive', resources: 'medium',
    dispatchAction: 'create_procedural_terrain',
    exampleInput: { action: 'create_procedural_terrain', name: 'PTerrain_1', sizeX: 1000, sizeY: 1000 },
  }),
  buildRecord({
    id: 'build_environment.bake_lightmap', action: 'bake_lightmap', family: F,
    summary: 'Build the level\'s static lighting (Lightmass) at quality (default Preview; Medium, High, Production): the same build as build_lighting, which defaults to Production. Reflection captures are not rebuilt, and a level that forces no precomputed lighting answers skipped. Can take minutes; nothing is saved.',
    whenToUse: ['Lightmaps must be baked for static lighting.'],
    whenNotToUse: ['Dynamic lighting is sufficient.'],
    inputProps: { quality: P.quality },
    effect: 'write', behavior: { longRunning: true, idempotency: 'idempotent' },
    latency: 'long-running', resources: 'high',
    dispatchAction: 'bake_lightmap',
    exampleInput: { action: 'bake_lightmap', quality: 'Preview' },
  }),
  buildRecord({
    id: 'build_environment.export_snapshot', action: 'export_snapshot', topics: ['save environment snapshot'], family: F,
    summary: 'Write the sun\'s rotation and intensity and the sky light\'s intensity (time of day included) to a snapshot file, for import_snapshot to apply again.',
    whenToUse: ['An environment state must be exported for later restoration.'],
    whenNotToUse: ['The level should be saved directly.'],
    inputProps: { path: { type: 'string', description: 'Snapshot file, or a folder for env_snapshot.json, relative to the project and under Saved/unreal-mcp or tmp/unreal-mcp; a file must end .json (default tmp/unreal-mcp/env_snapshot.json).' }, filename: P.filename,
      directionalLightActorPath: P.directionalLightActorPath, skyLightActorPath: P.skyLightActorPath },
    outputProps: { directionalLightActorPath: P.directionalLightActorPath, skyLightActorPath: P.skyLightActorPath },
    outputRequired: [],
    effect: 'read', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'export_snapshot', path: 'Saved/unreal-mcp', filename: 'env.json',
      directionalLightActorPath: '/Game/Maps/Main.Main:PersistentLevel.DirectionalLight_0' },
    exampleOutput: { success: true, message: 'Snapshot exported',
      directionalLightActorPath: '/Game/Maps/Main.Main:PersistentLevel.DirectionalLight_0',
      skyLightActorPath: '/Game/Maps/Main.Main:PersistentLevel.SkyLight_0' },
  }),
  buildRecord({
    id: 'build_environment.import_snapshot', action: 'import_snapshot', topics: ['load an environment snapshot'], family: F,
    summary: 'Apply a snapshot file export_snapshot wrote: the sun\'s rotation and intensity and the sky light\'s intensity, on the lights the file names. The file lives under Saved/unreal-mcp (or tmp/unreal-mcp) in the project and must end .json; one that is not there still answers success, applying nothing, so check the path.',
    whenToUse: ['A previously exported environment snapshot must be restored.'],
    whenNotToUse: ['The environment should be rebuilt from scratch.'],
    inputProps: { path: { type: 'string', description: 'Snapshot file, or a folder for env_snapshot.json, relative to the project and under Saved/unreal-mcp or tmp/unreal-mcp; a file must end .json (default tmp/unreal-mcp/env_snapshot.json).' }, filename: P.filename },
    outputProps: { directionalLightActorPath: P.directionalLightActorPath, skyLightActorPath: P.skyLightActorPath },
    outputRequired: [],
    effect: 'write', latency: 'interactive', resources: 'low',
    exampleInput: { action: 'import_snapshot', path: 'Saved/unreal-mcp', filename: 'env.json' },
    exampleOutput: { success: true, message: 'Snapshot imported',
      directionalLightActorPath: '/Game/Maps/Main.Main:PersistentLevel.DirectionalLight_0',
      skyLightActorPath: '/Game/Maps/Main.Main:PersistentLevel.SkyLight_0' },
  }),
  buildRecord({
    id: 'build_environment.delete', action: 'delete', topics: ['delete water, ocean, lake, river or landscape'], family: F,
    summary: 'Delete actors from the editor level by label, name or path (names, or one of actorPath, name, actorName, targetActor): any actor, not only environment ones. Every name must match exactly one actor or nothing is deleted (ACTOR_NOT_FOUND; AMBIGUOUS_ACTOR for a label several actors share), and a failed delete is rolled back. Not saved.',
    whenToUse: ['Environment actors must be permanently removed.'],
    whenNotToUse: ['Actors should be hidden rather than deleted.'],
    inputProps: { names: P.names, name: P.name, actorPath: P.actorPath,
      actorName: P.actorName, targetActor: P.targetActor },
    effect: 'destructive', behavior: { safeToRetry: false },
    latency: 'interactive', resources: 'low',
    exampleInput: { action: 'delete', names: ['Landscape_1'] },
  }),
];
