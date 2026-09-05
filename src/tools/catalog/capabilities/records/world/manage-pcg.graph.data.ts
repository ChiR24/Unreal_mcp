/**
 * PCG graph/sampling family records (28 actions, minus the async execution
 * pair which lives in manage-pcg.async.data.ts).
 *
 * Grounded in manage-pcg-tool.ts (PCG_ACTIONS) and native PCG domain dispatch.
 * Every PCG action requires the PCG optional plugin (compiled for source
 * projects when enabled). Graph/subgraph/node/pin/settings and
 * data/sampler/filter/spawner nodes are synchronous authoring operations;
 * execute_pcg_graph is the async task entry (see async data file).
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';

const F = 'pcg';
const NR = 'Distinct manage_pcg verb and target; no cross-tool duplicate.';
const PLUGIN = ['PCG'] as const;

export const PCG_GRAPH_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'create_pcg_graph', plugins: PLUGIN,
    topics: ['pcg graph', 'procedural generation', 'pcg asset', 'new pcg graph'],
    family: F, summary: 'Create a new PCG graph asset.', whenToUse: ['A PCG graph asset must be created.'], whenNotToUse: ['A subgraph is needed; use create_pcg_subgraph.'],
    inputProps: { assetPath: P.assetPath, path: P.path, overwrite: P.overwrite, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'create_pcg_graph', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'PCG graph created', graphPath: '/Game/PCG/PCG_MyGraph' },
    outputProps: { graphPath: { type: 'string', description: 'Created PCG graph asset path.' } },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'create_pcg_subgraph', plugins: PLUGIN,
    family: F, summary: 'Create a PCG subgraph asset for reuse.', whenToUse: ['A reusable PCG subgraph must be created.'], whenNotToUse: ['A top-level graph is needed; use create_pcg_graph.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, overwrite: P.overwrite, subgraphPath: P.subgraphPath, parentGraphPath: P.parentGraphPath, name: P.name }, required: ['subgraphPath'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'create_pcg_subgraph', subgraphPath: '/Game/PCG/PCG_Sub' }, exampleOutput: { success: true, message: 'PCG subgraph created' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_pcg_node', plugins: PLUGIN,
    family: F, summary: 'Add a node to a PCG graph by settings class or alias.', whenToUse: ['A node must be added to a PCG graph.'], whenNotToUse: ['Pins must be connected; use connect_pcg_pins.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, posX: P.posX, posY: P.posY, settings: P.settings, graphPath: P.graphPath, nodeType: P.nodeType, settingsClass: P.settingsClass, name: P.name }, required: ['graphPath', 'nodeType'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_pcg_node', graphPath: '/Game/PCG/PCG_MyGraph', nodeType: 'PCGStaticMeshSpawner' }, exampleOutput: { success: true, message: 'PCG node added', nodeId: 'Node_3' },
    outputProps: { nodeId: { type: 'string', description: 'Created PCG node identifier.' } },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'connect_pcg_pins', plugins: PLUGIN,
    family: F, summary: 'Connect an output pin of one PCG node to an input pin of another.', whenToUse: ['Two PCG nodes must be wired together.'], whenNotToUse: ['A node must be added; use add_pcg_node.'],
    inputProps: { inputName: P.inputName, outputName: P.outputName, graphPath: P.graphPath, sourceNodeId: P.sourceNodeId, sourcePin: P.sourcePin, targetNodeId: P.targetNodeId, targetPin: P.targetPin }, required: ['graphPath', 'sourceNodeId', 'targetNodeId'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'connect_pcg_pins', graphPath: '/Game/PCG/PCG_MyGraph', sourceNodeId: 'Node_1', targetNodeId: 'Node_2' }, exampleOutput: { success: true, message: 'PCG pins connected' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'set_pcg_node_settings', plugins: PLUGIN,
    family: F, summary: 'Set settings on an existing PCG node.', whenToUse: ['A PCG node settings must be updated.'], whenNotToUse: ['The node must be created; use add_pcg_node.'],
    inputProps: { nodeName: P.nodeName, title: P.title, classPath: P.classPath, texturePath: P.texturePath, settings: P.settings, graphPath: P.graphPath, nodeId: P.nodeId }, required: ['graphPath', 'nodeId'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_pcg_node_settings', graphPath: '/Game/PCG/PCG_MyGraph', nodeId: 'Node_3' }, exampleOutput: { success: true, message: 'PCG node settings set' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_landscape_data_node', plugins: PLUGIN,
    family: F, summary: 'Add a Landscape data source node to a PCG graph.', whenToUse: ['A PCG graph must sample landscape data.'], whenNotToUse: ['A spline data node is needed; use add_spline_data_node.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_landscape_data_node', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Landscape data node added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_spline_data_node', plugins: PLUGIN,
    family: F, summary: 'Add a Spline data source node to a PCG graph.', whenToUse: ['A PCG graph must sample a spline.'], whenNotToUse: ['A landscape data node is needed; use add_landscape_data_node.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_spline_data_node', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Spline data node added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_volume_data_node', plugins: PLUGIN,
    family: F, summary: 'Add a Volume data source node to a PCG graph.', whenToUse: ['A PCG graph must sample a volume.'], whenNotToUse: ['A texture data node is needed; use add_texture_data_node.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_volume_data_node', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Volume data node added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_actor_data_node', plugins: PLUGIN,
    family: F, summary: 'Add an Actor data source node to a PCG graph.', whenToUse: ['A PCG graph must sample existing actors.'], whenNotToUse: ['A mesh sampler is needed; use add_mesh_sampler.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_actor_data_node', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Actor data node added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_texture_data_node', plugins: PLUGIN,
    family: F, summary: 'Add a Texture data source node to a PCG graph.', whenToUse: ['A PCG graph must sample a texture.'], whenNotToUse: ['A volume data node is needed; use add_volume_data_node.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, texturePath: P.texturePath, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_texture_data_node', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Texture data node added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_surface_sampler', plugins: PLUGIN,
    family: F, summary: 'Add a Surface sampler node to a PCG graph.', whenToUse: ['Points must be sampled on surfaces.'], whenNotToUse: ['A mesh sampler is needed; use add_mesh_sampler.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_surface_sampler', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Surface sampler added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_mesh_sampler', plugins: PLUGIN,
    family: F, summary: 'Add a Mesh sampler node to a PCG graph.', whenToUse: ['Points must be sampled on a mesh.'], whenNotToUse: ['A surface sampler is needed; use add_surface_sampler.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name, meshPath: P.meshPath }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_mesh_sampler', graphPath: '/Game/PCG/PCG_MyGraph', meshPath: '/Game/Meshes/SM_Rock' }, exampleOutput: { success: true, message: 'Mesh sampler added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_spline_sampler', plugins: PLUGIN,
    family: F, summary: 'Add a Spline sampler node to a PCG graph.', whenToUse: ['Points must be sampled along a spline.'], whenNotToUse: ['A volume sampler is needed; use add_volume_sampler.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_spline_sampler', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Spline sampler added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_volume_sampler', plugins: PLUGIN,
    family: F, summary: 'Add a Volume sampler node to a PCG graph.', whenToUse: ['Points must be sampled within a volume.'], whenNotToUse: ['A spline sampler is needed; use add_spline_sampler.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_volume_sampler', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Volume sampler added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_bounds_modifier', plugins: PLUGIN,
    family: F, summary: 'Add a Bounds modifier node to a PCG graph.', whenToUse: ['Point bounds must be modified.'], whenNotToUse: ['A density filter is needed; use add_density_filter.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_bounds_modifier', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Bounds modifier added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_density_filter', plugins: PLUGIN,
    family: F, summary: 'Add a Density filter node to a PCG graph.', whenToUse: ['Points must be filtered by density.'], whenNotToUse: ['A height filter is needed; use add_height_filter.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_density_filter', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Density filter added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_height_filter', plugins: PLUGIN,
    family: F, summary: 'Add a Height filter node to a PCG graph.', whenToUse: ['Points must be filtered by height.'], whenNotToUse: ['A slope filter is needed; use add_slope_filter.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_height_filter', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Height filter added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_slope_filter', plugins: PLUGIN,
    family: F, summary: 'Add a Slope filter node to a PCG graph.', whenToUse: ['Points must be filtered by slope.'], whenNotToUse: ['A distance filter is needed; use add_distance_filter.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_slope_filter', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Slope filter added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_distance_filter', plugins: PLUGIN,
    family: F, summary: 'Add a Distance filter node to a PCG graph.', whenToUse: ['Points must be filtered by distance.'], whenNotToUse: ['A bounds filter is needed; use add_bounds_filter.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_distance_filter', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Distance filter added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_bounds_filter', plugins: PLUGIN,
    family: F, summary: 'Add a Bounds filter node to a PCG graph.', whenToUse: ['Points must be filtered by bounds.'], whenNotToUse: ['A self-pruning node is needed; use add_self_pruning.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_bounds_filter', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Bounds filter added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_self_pruning', plugins: PLUGIN,
    family: F, summary: 'Add a Self-pruning node to a PCG graph.', whenToUse: ['Overlapping points must be pruned.'], whenNotToUse: ['Points must be transformed; use add_transform_points.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_self_pruning', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Self-pruning node added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_transform_points', plugins: PLUGIN,
    family: F, summary: 'Add a Transform Points node to a PCG graph.', whenToUse: ['Points must be transformed.'], whenNotToUse: ['Points must be projected to a surface; use add_project_to_surface.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_transform_points', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Transform points node added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_project_to_surface', plugins: PLUGIN,
    family: F, summary: 'Add a Project to Surface node to a PCG graph.', whenToUse: ['Points must be projected onto a surface.'], whenNotToUse: ['Points must be copied; use add_copy_points.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_project_to_surface', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Project to surface node added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_copy_points', plugins: PLUGIN,
    family: F, summary: 'Add a Copy Points node to a PCG graph.', whenToUse: ['Points must be copied from one set to another.'], whenNotToUse: ['Points must be merged; use add_merge_points.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_copy_points', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Copy points node added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_merge_points', plugins: PLUGIN,
    family: F, summary: 'Add a Merge Points node to a PCG graph.', whenToUse: ['Multiple point sets must be merged.'], whenNotToUse: ['A static mesh spawner is needed; use add_static_mesh_spawner.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_merge_points', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Merge points node added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_static_mesh_spawner', plugins: PLUGIN,
    family: F, summary: 'Add a Static Mesh spawner node to a PCG graph.', whenToUse: ['Static meshes must be spawned at points.'], whenNotToUse: ['An actor spawner is needed; use add_actor_spawner.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name, meshPath: P.meshPath }, required: ['graphPath', 'meshPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_static_mesh_spawner', graphPath: '/Game/PCG/PCG_MyGraph', meshPath: '/Game/Meshes/SM_Rock' }, exampleOutput: { success: true, message: 'Static mesh spawner added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_actor_spawner', plugins: PLUGIN,
    family: F, summary: 'Add an Actor spawner node to a PCG graph.', whenToUse: ['Actors must be spawned at points.'], whenNotToUse: ['A static mesh spawner is needed; use add_static_mesh_spawner.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, classPath: P.classPath, graphPath: P.graphPath, name: P.name, actorClass: P.actorClass }, required: ['graphPath', 'actorClass'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_actor_spawner', graphPath: '/Game/PCG/PCG_MyGraph', actorClass: '/Game/Blueprints/BP_Tree' }, exampleOutput: { success: true, message: 'Actor spawner added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_spline_spawner', plugins: PLUGIN,
    family: F, summary: 'Add a Spline spawner node to a PCG graph.', whenToUse: ['Actors/meshes must be spawned along a spline.'], whenNotToUse: ['A static mesh spawner is needed; use add_static_mesh_spawner.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_spline_spawner', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'Spline spawner added' },
    normalizationRationale: NR,
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'set_pcg_partition_grid_size', plugins: PLUGIN,
    family: F, summary: 'Set the PCG partition grid cell size for a graph or component.',
    whenToUse: ['PCG partition resolution must be tuned.'], whenNotToUse: ['A node must be added; use add_pcg_node.'],
    inputProps: {
      graphPath: P.graphPath, scope: P.pcgScope, gridSize: P.pcgGridSize,
      actorName: P.actorName, componentName: P.componentName, componentPath: P.componentPath, save: P.save,
    }, required: ['graphPath', 'gridSize'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_pcg_partition_grid_size', graphPath: '/Game/PCG/PCG_MyGraph', scope: 'world', gridSize: 12800, save: true },
    exampleOutput: { success: true, message: 'PCG partition grid size set', scope: 'world', previousGridSize: 6400, gridSize: 12800, saved: true },
    outputProps: {
      scope: { type: 'string', description: "Resolved scope: 'world' or 'component'." },
      previousGridSize: { type: 'number', description: 'Previous partition grid cell size.' },
      gridSize: { type: 'number', description: 'New partition grid cell size.' },
      saved: { type: 'boolean', description: 'Whether the level/asset was saved.' },
    },
    normalizationRationale: NR,
  }),
];
