/**
 * PCG graph/sampling family records: the 29 synchronous actions. The single
 * async action, execute_pcg_graph, lives in manage-pcg.async.data.ts.
 *
 * Grounded in manage-pcg-tool.ts (PCG_ACTIONS) and native PCG domain dispatch.
 * Every PCG action requires the PCG optional plugin (compiled for source
 * projects when enabled). Graph/subgraph/node/pin/settings and
 * data/sampler/filter/spawner nodes are synchronous authoring operations;
 * execute_pcg_graph is the async task entry (see async data file).
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildWorldRecord } from './builder.js';
import { P } from './properties.js';
import { withPCGEditContract } from './manage-pcg.edit-contract.js';

const F = 'pcg';
const PLUGIN = ['PCG'] as const;
const PCG_SAVE = { type: 'boolean', description: 'Save the PCG graph asset after the edit (default true).' };

export const PCG_GRAPH_RECORDS: readonly CapabilityRecordSource[] = [
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'sync_pcg_editor_graph', plugins: PLUGIN, family: F,
    summary: 'Synchronize an existing PCG editor representation using native schema-created nodes and public virtual APIs.',
    topics: ['repair pcg editor graph', 'sync pcg nodes', 'missing pcg editor node'],
    whenToUse: ['Backend edits left the graph editor inconsistent.'], whenNotToUse: ['Only inspect state; use get_pcg_editor_graph.'],
    inputProps: { graphPath: P.graphPath, save: PCG_SAVE,
      openEditor: { type: 'boolean', description: 'Open the graph editor when no representation exists, default false.' },
      expectedGraphRevision: { type: 'string', description: 'Optional authored graph revision; stale edits are refused.' } },
    required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    outputProps: { revision: { type: 'string', description: 'Authored graph revision after synchronization.' },
      consistent: { type: 'boolean', description: 'Visible model nodes, pins, positions and edges map consistently.' },
      available: { type: 'boolean', description: 'An editor representation exists.' },
      committed: { type: 'boolean', description: 'In-memory synchronization committed.' },
      saved: { type: 'boolean', description: 'The graph asset was saved.' },
      rolledBack: { type: 'boolean', description: 'Explicit failure recovery restored the previous state.' } },
    exampleInput: { action: 'sync_pcg_editor_graph', graphPath: '/Game/PCG/Graph', save: false },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'create_pcg_graph', plugins: PLUGIN,
    topics: ['pcg graph', 'procedural generation', 'pcg asset', 'new pcg graph'],
    family: F, summary: 'Create a PCG graph asset at graphPath (or assetPath, or name in path, default /Game/PCG), saved unless save is false. A PCG graph already at the path is refused (ASSET_ALREADY_EXISTS) unless overwrite is true, which returns that graph unchanged (created false); another kind of asset there is refused. The reply gives graphPath, name, created and saved. Needs the PCG plugin.', whenToUse: ['Procedural content (scattered rocks, trees, props along splines) needs a graph to build it.'], whenNotToUse: ['A graph that runs inside another one (create_pcg_subgraph with parentGraphPath).'],
    inputProps: { assetPath: { ...P.assetPath, description: 'Alias of graphPath.' }, path: { ...P.path, description: 'Folder for name (default /Game/PCG).' }, overwrite: P.overwrite, graphPath: P.graphPath, name: { ...P.name, description: 'Name of the new graph, created in path; used when graphPath and assetPath are absent.' }, save: PCG_SAVE }, required: [], requiredOneOf: ['graphPath', 'assetPath', 'name'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    exampleInput: { action: 'create_pcg_graph', graphPath: '/Game/PCG/PCG_MyGraph' }, exampleOutput: { success: true, message: 'PCG graph created', graphPath: '/Game/PCG/PCG_MyGraph' },
    outputProps: { graphPath: { type: 'string', description: 'Created or reused PCG graph asset path.' },
      name: { type: 'string', description: 'The asset name.' },
      created: { type: 'boolean', description: 'False when overwrite handed back a graph already there.' },
      committed: { type: 'boolean', description: 'The requested in-memory creation/reuse completed; save failure can leave a created graph in memory.' },
      saved: { type: 'boolean', description: 'This request confirmed a durable graph-package save.' },
      rolledBack: { type: 'boolean', description: 'Whether explicit recovery was performed; failed disk writes are not represented as rollback.' },
      residualChanges: { type: 'boolean', description: 'A failed request left an owned in-memory asset to inspect or explicitly remove.' } },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'create_pcg_subgraph', plugins: PLUGIN,
    family: F, summary: 'Create a PCG graph at subgraphPath to run inside other graphs (the same rules as create_pcg_graph: one already there is refused unless overwrite is true, which reuses it), and with parentGraphPath add a Subgraph node running it to that graph, titled nodeName and placed at x, y. The reply gives subgraphPath, created and saved, and with a parent also parentGraphPath, nodeId (the new node, for connect_pcg_pins) and parentSaved. A parent that does not load is ASSET_NOT_FOUND. Saved unless save is false.', whenToUse: ['Steps several PCG graphs share (scatter rules, cleanup) should live in one graph run from each.'], whenNotToUse: ['A standalone graph (create_pcg_graph).'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, overwrite: P.overwrite, subgraphPath: P.subgraphPath, parentGraphPath: P.parentGraphPath, name: P.name, save: PCG_SAVE }, required: ['subgraphPath'], effect: 'write', costLatency: 'interactive', costResources: 'medium',
    outputProps: {
      graphPath: { type: 'string', description: 'Resolved child graph path.' },
      subgraphPath: { type: 'string', description: 'Resolved reusable child graph path.' },
      parentGraphPath: { type: 'string', description: 'Resolved parent graph when attaching a node.' },
      nodeId: { type: 'string', description: 'New parent node, present only after attachment commits.' },
      committed: { type: 'boolean', description: 'All requested in-memory graph changes completed.' },
      saved: { type: 'boolean', description: 'Both requested child and parent package saves were confirmed in this request.' },
      subgraphSaved: { type: 'boolean', description: 'Child package save completed before parent attachment/save.' },
      parentCommitted: { type: 'boolean', description: 'Parent attachment remains committed in memory.' },
      parentSaved: { type: 'boolean', description: 'Parent package save was confirmed.' },
      partialSave: { type: 'boolean', description: 'A package was saved while another requested package write did not complete; inspect the per-package fields.' },
      rolledBack: { type: 'boolean', description: 'Explicit recovery restored the parent authored state only.' },
      residualChanges: { type: 'boolean', description: 'A failed request left a child asset or un-restored parent mutation.' },
      atomic: { type: 'boolean', description: 'False: cross-package disk writes are not an atomic transaction.' },
      rollbackScope: { type: 'string', description: 'Parent authored-state recovery never claims cross-package disk rollback.' },
      revision: { type: 'string', description: 'Complete parent graph revision after attachment.' },
    },
    exampleInput: { action: 'create_pcg_subgraph', subgraphPath: '/Game/PCG/PCG_Sub' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_pcg_node', plugins: PLUGIN,
    family: F, summary: 'Add a node to a PCG graph by settings class or alias.', whenToUse: ['A node must be added to a PCG graph.'], whenNotToUse: ['Pins must be connected; use connect_pcg_pins.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, posX: P.posX, posY: P.posY, settings: P.settings, graphPath: P.graphPath, nodeType: P.nodeType, settingsClass: P.settingsClass, parameterName: { type: 'string', description: 'For PCGUserParameterGetSettings, bind this current graph parameter by its stable ID.' }, name: P.name, save: PCG_SAVE }, required: ['graphPath'], requiredOneOf: ['nodeType', 'settingsClass'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_pcg_node', graphPath: '/Game/PCG/PCG_MyGraph', nodeType: 'PCGStaticMeshSpawner' }, exampleOutput: { success: true, message: 'PCG node added', nodeId: 'Node_3' },
    outputProps: { nodeId: { type: 'string', description: 'Created PCG node identifier.' } },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'connect_pcg_pins', plugins: PLUGIN,
    family: F, summary: 'Wire sourcePin (or outputName) of sourceNodeId to targetPin (or inputName) of targetNodeId in a PCG graph (graphPath or assetPath). A node is found by its nodeId (object name), its title, its index in the graph, or input and output for the graph\'s own ends (NODE_NOT_FOUND). A pin left out is the node\'s Out or In pin, else its first; a label it lacks is PIN_NOT_FOUND listing the labels. An edge already there is kept, not doubled. The reply gives both node ids, both pin labels and saved; saved unless save is false.', whenToUse: ['Points must flow from one PCG node into the next, such as a sampler into a spawner.'], whenNotToUse: ['A node is added (add_pcg_node).'],
    inputProps: { inputName: P.inputName, outputName: P.outputName, graphPath: P.graphPath, assetPath: { ...P.assetPath, description: 'Alias of graphPath.' }, sourceNodeId: P.sourceNodeId, sourcePin: P.sourcePin, targetNodeId: P.targetNodeId, targetPin: P.targetPin, save: PCG_SAVE }, required: ['sourceNodeId', 'targetNodeId'], requiredOneOf: ['graphPath', 'assetPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'connect_pcg_pins', graphPath: '/Game/PCG/PCG_MyGraph', sourceNodeId: 'Node_1', targetNodeId: 'Node_2' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'set_pcg_node_settings', plugins: PLUGIN,
    family: F, summary: 'Change a node of a PCG graph, found by nodeId or nodeName (an object name, title, index, or input and output; NODE_NOT_FOUND): settings sets properties by name (INVALID_SETTINGS for one it lacks), texturePath, meshPath and actorClass set the texture, mesh or spawned class of nodes that take them (each checked to load), title renames it and x, y move it. An edit that changes nothing, or sends only a meshPath or class the node has no slot for, is refused. The reply gives nodeId, title, nodeType, settingsApplied and saved; saved unless save is false.', whenToUse: ['A PCG node must scatter, filter or spawn differently, such as a denser sampler or another mesh.'], whenNotToUse: ['A new node (add_pcg_node).'],
    inputProps: { nodeName: P.nodeName, title: P.title, classPath: P.classPath, texturePath: P.texturePath, meshPath: P.meshPath, actorClass: P.actorClass, x: P.x, y: P.y, settings: P.settings, graphPath: P.graphPath, assetPath: { ...P.assetPath, description: 'Alias of graphPath.' }, nodeId: { ...P.nodeId, description: 'The node to change; required, or send it as nodeName.' }, parameterName: { type: 'string', description: 'For a parameter getter, select a current graph parameter while preserving native GUID binding.' }, save: PCG_SAVE }, required: [], requiredOneOf: ['graphPath', 'assetPath'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_pcg_node_settings', graphPath: '/Game/PCG/PCG_MyGraph', nodeId: 'Node_3' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_landscape_data_node', plugins: PLUGIN,
    family: F, summary: 'Add a Landscape data source node to a PCG graph.', whenToUse: ['A PCG graph must sample landscape data.'], whenNotToUse: ['A spline data node is needed; use add_spline_data_node.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_landscape_data_node', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_spline_data_node', plugins: PLUGIN,
    family: F, summary: 'Add a Spline data source node to a PCG graph.', whenToUse: ['A PCG graph must sample a spline.'], whenNotToUse: ['A landscape data node is needed; use add_landscape_data_node.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_spline_data_node', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_volume_data_node', plugins: PLUGIN,
    family: F, summary: 'Add a Volume data source node to a PCG graph.', whenToUse: ['A PCG graph must sample a volume.'], whenNotToUse: ['A texture data node is needed; use add_texture_data_node.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_volume_data_node', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_actor_data_node', plugins: PLUGIN,
    family: F, summary: 'Add an Actor data source node to a PCG graph.', whenToUse: ['A PCG graph must sample existing actors.'], whenNotToUse: ['A mesh sampler is needed; use add_mesh_sampler.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_actor_data_node', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_texture_data_node', plugins: PLUGIN,
    family: F, summary: 'Add a Texture data source node to a PCG graph.', whenToUse: ['A PCG graph must sample a texture.'], whenNotToUse: ['A volume data node is needed; use add_volume_data_node.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, texturePath: P.texturePath, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_texture_data_node', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_surface_sampler', plugins: PLUGIN,
    family: F, summary: 'Add a Surface sampler node to a PCG graph.', whenToUse: ['Points must be sampled on surfaces.'], whenNotToUse: ['A mesh sampler is needed; use add_mesh_sampler.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_surface_sampler', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_mesh_sampler', plugins: PLUGIN,
    family: F, summary: 'Add a Mesh sampler node to a PCG graph.', whenToUse: ['Points must be sampled on a mesh.'], whenNotToUse: ['A surface sampler is needed; use add_surface_sampler.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name, meshPath: { ...P.meshPath, description: 'Static mesh (/Game path): the one mesh_sampler samples, or the one static_mesh_spawner places. Omitted, the node is added with no mesh, to be set later.' } }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_mesh_sampler', graphPath: '/Game/PCG/PCG_MyGraph', meshPath: '/Game/Meshes/SM_Rock' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_spline_sampler', plugins: PLUGIN,
    family: F, summary: 'Add a Spline sampler node to a PCG graph.', whenToUse: ['Points must be sampled along a spline.'], whenNotToUse: ['A volume sampler is needed; use add_volume_sampler.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_spline_sampler', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_volume_sampler', plugins: PLUGIN,
    family: F, summary: 'Add a Volume sampler node to a PCG graph.', whenToUse: ['Points must be sampled within a volume.'], whenNotToUse: ['A spline sampler is needed; use add_spline_sampler.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_volume_sampler', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_bounds_modifier', plugins: PLUGIN,
    family: F, summary: 'Add a Bounds modifier node to a PCG graph.', whenToUse: ['Point bounds must be modified.'], whenNotToUse: ['A density filter is needed; use add_density_filter.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_bounds_modifier', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_density_filter', plugins: PLUGIN,
    family: F, summary: 'Add a Density filter node to a PCG graph.', whenToUse: ['Points must be filtered by density.'], whenNotToUse: ['A height filter is needed; use add_height_filter.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_density_filter', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_height_filter', plugins: PLUGIN,
    family: F, summary: 'Add a Height filter node to a PCG graph.', whenToUse: ['Points must be filtered by height.'], whenNotToUse: ['A slope filter is needed; use add_slope_filter.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_height_filter', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_slope_filter', plugins: PLUGIN,
    family: F, summary: 'Add a Slope filter node to a PCG graph.', whenToUse: ['Points must be filtered by slope.'], whenNotToUse: ['A distance filter is needed; use add_distance_filter.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_slope_filter', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_distance_filter', plugins: PLUGIN,
    family: F, summary: 'Add a Distance filter node to a PCG graph.', whenToUse: ['Points must be filtered by distance.'], whenNotToUse: ['A bounds filter is needed; use add_bounds_filter.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_distance_filter', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_bounds_filter', plugins: PLUGIN,
    family: F, summary: 'Add a Bounds filter node to a PCG graph.', whenToUse: ['Points must be filtered by bounds.'], whenNotToUse: ['A self-pruning node is needed; use add_self_pruning.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_bounds_filter', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_self_pruning', plugins: PLUGIN,
    family: F, summary: 'Add a Self-pruning node to a PCG graph.', whenToUse: ['Overlapping points must be pruned.'], whenNotToUse: ['Points must be transformed; use add_transform_points.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_self_pruning', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_transform_points', plugins: PLUGIN,
    family: F, summary: 'Add a Transform Points node to a PCG graph.', whenToUse: ['Points must be transformed.'], whenNotToUse: ['Points must be projected to a surface; use add_project_to_surface.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_transform_points', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_project_to_surface', plugins: PLUGIN,
    family: F, summary: 'Add a Project to Surface node to a PCG graph.', whenToUse: ['Points must be projected onto a surface.'], whenNotToUse: ['Points must be copied; use add_copy_points.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_project_to_surface', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_copy_points', plugins: PLUGIN,
    family: F, summary: 'Add a Copy Points node to a PCG graph.', whenToUse: ['Points must be copied from one set to another.'], whenNotToUse: ['Points must be merged; use add_merge_points.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_copy_points', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_merge_points', plugins: PLUGIN,
    family: F, summary: 'Add a Merge Points node to a PCG graph.', whenToUse: ['Multiple point sets must be merged.'], whenNotToUse: ['A static mesh spawner is needed; use add_static_mesh_spawner.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_merge_points', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_static_mesh_spawner', plugins: PLUGIN,
    family: F, summary: 'Add a Static Mesh spawner node to a PCG graph.', whenToUse: ['Static meshes must be spawned at points.'], whenNotToUse: ['An actor spawner is needed; use add_actor_spawner.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name, meshPath: { ...P.meshPath, description: 'Static mesh the spawner places. Omitted, the spawner is added with no mesh, to be set later.' } }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_static_mesh_spawner', graphPath: '/Game/PCG/PCG_MyGraph', meshPath: '/Game/Meshes/SM_Rock' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_actor_spawner', plugins: PLUGIN,
    family: F, summary: 'Add an Actor spawner node to a PCG graph.', whenToUse: ['Actors must be spawned at points.'], whenNotToUse: ['A static mesh spawner is needed; use add_static_mesh_spawner.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, classPath: P.classPath, graphPath: P.graphPath, name: P.name, actorClass: { ...P.actorClass, description: 'Actor class the spawner places (classPath is read in its place). Omitted, the spawner is added with no template, to be set later.' } }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_actor_spawner', graphPath: '/Game/PCG/PCG_MyGraph', actorClass: '/Game/Blueprints/BP_Tree' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'add_spline_spawner', plugins: PLUGIN,
    family: F, summary: 'Add a Spline spawner node to a PCG graph.', whenToUse: ['Actors/meshes must be spawned along a spline.'], whenNotToUse: ['A static mesh spawner is needed; use add_static_mesh_spawner.'],
    inputProps: { nodeName: P.nodeName, x: P.x, y: P.y, settings: P.settings, save: PCG_SAVE, graphPath: P.graphPath, name: P.name }, required: ['graphPath'], effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'add_spline_spawner', graphPath: '/Game/PCG/PCG_MyGraph' },
  }),
  buildWorldRecord({
    parentTool: 'manage_pcg', action: 'set_pcg_partition_grid_size', plugins: PLUGIN,
    family: F, summary: 'Set the size of the cells partitioned PCG generates in (gridSize, a whole number of world units above 0): scope world (the default) sets the PCG world actor\'s partition grid, scope component one PCG component\'s generation grid (actorName, componentName or componentPath; COMPONENT_NOT_FOUND). The reply gives previousGridSize and gridSize. Saved with the level unless save is false, which saves every unsaved change in that level too.',
    whenToUse: ['Partitioned PCG makes too many small cells, or cells too large to stream well.'], whenNotToUse: ['A node is added (add_pcg_node).'],
    inputProps: {
      scope: P.pcgScope, gridSize: P.pcgGridSize,
      actorName: P.actorName, componentName: P.componentName, componentPath: P.componentPath, save: P.save,
    }, required: ['gridSize'], effect: 'write', behavior: { idempotency: 'idempotent' }, costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_pcg_partition_grid_size', scope: 'world', gridSize: 12800, save: true },
    exampleOutput: { success: true, message: 'PCG partition grid size set', scope: 'world', previousGridSize: 6400, gridSize: 12800, saved: true },
    outputProps: {
      scope: { type: 'string', description: "Resolved scope: 'world' or 'component'." },
      previousGridSize: { type: 'number', description: 'Previous partition grid cell size.' },
      gridSize: { type: 'number', description: 'New partition grid cell size.' },
      saved: { type: 'boolean', description: 'Whether the level/asset was saved.' },
    },
  }),
].map(withPCGEditContract);
