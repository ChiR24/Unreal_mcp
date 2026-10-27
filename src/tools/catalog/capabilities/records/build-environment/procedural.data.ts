/**
 * Procedural and misc family records (6 actions).
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
import { bool, int, num, str } from '../shared/schema-props.js';

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
  buildRecord({
    id: 'build_environment.assemble_modules', action: 'assemble_modules', family: F,
    topics: ['wave function collapse', 'modular kit layout', 'dungeon from tiles', 'assemble modular pieces', 'pieces that join up', 'dungeon or maze generator', 'modular building kit', 'snap modular pieces together'],
    summary: 'Lay a modular kit out over a grid of cells so that every face where two pieces meet fits, by wave function collapse, and spawn the layout as one actor (an instanced static mesh component per piece, one undo step). Each piece is a static mesh made to fill one cell of cellSize; pivot says where its own origin sits in the cell (corner: the cell\'s lowest corner; center; bottom_center: the middle of its floor), and a piece reaching outside that box is refused. Each face of each piece, at each quarter turn about Z when rotations is on (half turns when the cell is not square), is read from the mesh\'s source geometry as 16 x 16 squares (each read by 4 x 4 rays cast in from just outside, so a wall as thin as 1/40 of the cell shows): two cells sit side by side only where the faces between them match, both empty or covering the same squares to 90% (a corridor 3 squares wide never meets one 4 wide). emptyWeight adds an empty cell (0 forbids it). The grid\'s outer faces must be empty except on openSides (default -Z: the pieces stand on the ground). A piece whose face nothing in the kit can meet is never placed and unmatchedFaces names it: a wall reaching the top of its cell needs +Z open or a piece to stack on it, and an outer wall needs a piece to stand back to back with or that side open. Instances collide where the piece\'s mesh has collision. dryRun lays the kit out and spawns nothing. Refused, placing nothing: NO_SOLUTION (no layout: why, and the faces nothing meets), MESH_NOT_FOUND (not a static mesh with source geometry), INVALID_ARGUMENT (a piece outside its cell box, more than 4096 cells, or pieces whose faces take more than 10 s to read).',
    whenToUse: ['A dungeon, building, maze or town block should be assembled from a modular kit so the pieces join up.', 'Many variations of one layout from the same kit (seed).'],
    whenNotToUse: ['A few pieces placed by hand: control_actor spawn.', 'Props scattered with no joins between them: the foliage or scatter tools.'],
    inputProps: {
      modules: { type: 'array', minItems: 1, maxItems: 32, description: 'The kit: 1-32 pieces, each a static mesh made to fill one cell.', items: { type: 'object', additionalProperties: false, required: ['meshPath'], properties: {
        meshPath: { type: 'string', description: 'The piece\'s static mesh.' },
        weight: { type: 'number', minimum: 0.001, description: 'How often it is chosen against the others (default 1), shared among its turns.' } } } },
      cellSize: { type: 'object', additionalProperties: false, required: ['x', 'y', 'z'], properties: { x: { type: 'number', minimum: 1 }, y: { type: 'number', minimum: 1 }, z: { type: 'number', minimum: 1 } }, description: 'One cell, in cm: every piece fits inside one.' },
      pivot: { type: 'string', enum: ['corner', 'center', 'bottom_center'], description: 'Where each piece\'s own origin sits in its cell: corner (the cell\'s lowest corner), center, or bottom_center (the middle of its floor).' },
      size: { type: 'object', additionalProperties: false, required: ['x', 'y', 'z'], properties: { x: { type: 'integer', minimum: 1 }, y: { type: 'integer', minimum: 1 }, z: { type: 'integer', minimum: 1 } }, description: 'Cells along X, Y and Z, at most 4096 in all.' },
      location: { ...P.location, description: 'The grid\'s lowest corner in world cm (default the origin).' },
      rotations: bool('Also try each piece turned about Z: quarter turns in a square cell, half turns otherwise (default true).'),
      emptyWeight: { type: 'number', minimum: 0, description: 'How often a cell is left empty against the pieces (default 1; 0: every cell holds a piece).' },
      openSides: { type: 'array', items: { type: 'string', enum: ['+X', '-X', '+Y', '-Y', '+Z', '-Z'] }, description: 'Sides of the grid a piece may face out of with anything (default ["-Z"]: the floor); on the others only an empty face may.' },
      seed: int('Seed (default 1): the same kit, size and seed give the same layout.'),
      name: str('Label of the spawned actor (default ModuleAssembly).'),
      dryRun: bool('Lay the kit out and report it, spawning nothing (default false).'),
    },
    outputProps: {
      actorName: str('Label of the spawned actor (absent under dryRun).'),
      placed: { type: 'array', description: 'Each piece and how many cells it fills.', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: '{meshPath, count}.' } },
      cells: int('Cells in the grid.'),
      emptyCells: int('Cells left empty.'),
      variants: int('Pieces and their turns tried, the empty cell included.'),
      openFaces: int('Faces where the layout meets a neighbour or the outside it does not match: always 0 on success.'),
      backtracks: int('Choices undone on the way.'),
      seedsTried: int('Seeds tried: after 1000 backtracks a seed is given up and the next one tried, up to 5.'),
      unmatchedFaces: { type: 'array', items: { type: 'string' }, description: 'Pieces never placed and the face that nothing in the kit or the outside on that side takes.' },
      seconds: num('Time the call took.'),
      undo: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true, description: 'What editor undo takes back: {undoable, transactionScope}; the whole layout is one step. Absent under dryRun.' },
    },
    required: ['modules', 'cellSize', 'pivot', 'size'],
    effect: 'write', latency: 'interactive', resources: 'medium',
    exampleInput: { action: 'assemble_modules', modules: [{ meshPath: '/Game/Kit/SM_Corridor_Straight' }, { meshPath: '/Game/Kit/SM_Corridor_Corner' }], cellSize: { x: 400, y: 400, z: 300 }, pivot: 'bottom_center', size: { x: 12, y: 8, z: 1 }, seed: 3 },
  }),
];
