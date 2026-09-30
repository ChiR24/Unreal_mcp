#!/usr/bin/env node

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/CoreAssets';
const ts = Date.now();

const MAIN_ACTOR = `MCP_CoreActor_${ts}`;
const DELETE_ACTOR = `MCP_DeleteActor_${ts}`;
const DESTROY_ACTOR = `MCP_DestroyActor_${ts}`;
const TAG_DELETE_ACTOR = `MCP_TagDeleteActor_${ts}`;
const DUPLICATE_ACTOR = `MCP_DuplicateActor_${ts}`;
const DUPLICATE_COPY = `MCP_DuplicateActorCopy_${ts}`;
const RENAMED_COPY = `MCP_RenamedCopy_${ts}`;
const MESH_ACTOR = `MCP_MeshActor_${ts}`;
const PARENT_ACTOR = `MCP_ParentActor_${ts}`;
const CHILD_ACTOR = `MCP_ChildActor_${ts}`;
const BP_NAME = `BP_ControlActor_${ts}`;
const BP_PATH = `${TEST_FOLDER}/${BP_NAME}`;
const BP_ACTOR = `MCP_BlueprintActor_${ts}`;
// Two cubes far from everything else, for the near queries: NEAR_ACTOR's bounds contain (30000, 30000, 100), and
// FAR_ACTOR's begin 250 units to the right of it (a cube is 100 units wide).
const NEAR_ACTOR = `MCP_NearActor_${ts}`;
const FAR_ACTOR = `MCP_NearFarActor_${ts}`;
const TAG = `MCPControlActorTag_${ts}`;
const DELETE_TAG = `MCPDeleteTag_${ts}`;
const BATCH_TAG = `MCPBatchTag_${ts}`;
const COMPONENT_NAME = `MCPPointLight_${ts}`;
const MESH_COMPONENT = `MCPConeMesh_${ts}`;
const ENGINE_BASIC_MATERIAL = '/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial';

const cubeSpawn = (scenario, actorName, location) => ({
  scenario,
  toolName: 'control_actor',
  arguments: {
    action: 'spawn',
    classPath: '/Engine/BasicShapes/Cube',
    actorName,
    location,
  },
  expected: 'success|already exists',
});

const actorArgs = (action, extra = {}) => ({ action, actorName: MAIN_ACTOR, ...extra });

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create actor blueprint', toolName: 'manage_blueprint', arguments: { action: 'create', name: BP_NAME, savePath: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists' },
  // A billboard is a primitive component a Blueprint actor makes without RF_Transactional: set_visibility below must still be undoable.
  { scenario: 'Setup: give the actor blueprint a billboard component', toolName: 'manage_blueprint', arguments: { action: 'add_scs_component', blueprintPath: BP_PATH, componentClass: 'BillboardComponent', componentName: `MCPBillboard_${ts}` }, expected: 'success|already exists' },
  cubeSpawn('Setup: spawn main test actor', MAIN_ACTOR, { x: 0, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn delete test actor', DELETE_ACTOR, { x: 120, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn destroy test actor', DESTROY_ACTOR, { x: 240, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn tag-delete test actor', TAG_DELETE_ACTOR, { x: 360, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn duplicate test actor', DUPLICATE_ACTOR, { x: 480, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn attach parent actor', PARENT_ACTOR, { x: 600, y: 0, z: 100 }),
  cubeSpawn('Setup: spawn the near-query actor', NEAR_ACTOR, { x: 30000, y: 30000, z: 100 }),
  cubeSpawn('Setup: spawn the actor 300 units from it', FAR_ACTOR, { x: 30300, y: 30000, z: 100 }),
  cubeSpawn('Setup: spawn attach child actor', CHILD_ACTOR, { x: 720, y: 0, z: 100 }),
  { scenario: 'Setup: tag actor for delete_by_tag', toolName: 'control_actor', arguments: { action: 'add_tag', actorName: TAG_DELETE_ACTOR, tag: DELETE_TAG }, expected: 'success|already exists' },

  // === SPAWN / DELETE ===
  { scenario: 'ACTION: spawn', toolName: 'control_actor', arguments: { action: 'spawn', classPath: '/Engine/BasicShapes/Sphere', actorName: `MCP_SpawnSphere_${ts}`, location: { x: 0, y: 160, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'CREATE: spawn_actor', toolName: 'control_actor', arguments: { action: 'spawn_actor', classPath: '/Engine/BasicShapes/Cylinder', actorName: `MCP_SpawnCylinder_${ts}`, location: { x: 120, y: 160, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'CREATE: spawn_actor with meshPath', toolName: 'control_actor', arguments: { action: 'spawn_actor', classPath: '/Script/Engine.StaticMeshActor', meshPath: '/Engine/BasicShapes/Cube.Cube', actorName: MESH_ACTOR, location: { x: 360, y: 160, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'CREATE: spawn_blueprint', toolName: 'control_actor', arguments: { action: 'spawn_blueprint', blueprintPath: BP_PATH, actorName: BP_ACTOR, location: { x: 240, y: 160, z: 120 } }, expected: 'success|already exists' },
  { scenario: 'CREATE: spawn_batch with shared defaults, a material and tags', toolName: 'control_actor', arguments: { action: 'spawn_batch', defaults: { classPath: '/Engine/BasicShapes/Cube', folder: 'MCPTest/Batch', tags: [BATCH_TAG] }, actors: [{ actorName: `MCP_BatchCube1_${ts}`, location: [0, 320, 50] }, { actorName: `MCP_BatchCube2_${ts}`, location: [120, 320, 50], materialPath: ENGINE_BASIC_MATERIAL }] }, expected: 'success' },
  { scenario: 'CREATE: spawn_batch reporting failures only (none here)', toolName: 'control_actor', arguments: { action: 'spawn_batch', report: 'failures', defaults: { classPath: '/Engine/BasicShapes/Cube', folder: 'MCPTest/Batch', tags: [BATCH_TAG] }, actors: [{ actorName: `MCP_BatchCube3_${ts}`, location: { x: 240, y: 320, z: 50 } }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.spawned', equals: 1, label: 'the item spawned' }, { path: 'structuredContent.result.results', length: 0, label: 'a clean batch lists no items' }] },
  { scenario: 'QUERY: list narrowed by tag, class and a parent outliner folder', toolName: 'control_actor', arguments: { action: 'list', tag: BATCH_TAG, className: 'StaticMeshActor', folder: 'MCPTest' }, expected: 'success', assertions: [{ path: 'structuredContent.result.totalCount', equals: 3, label: 'the three batch cubes, matched by tag, class and the folder above theirs' }] },
  { scenario: 'DELETE: spawn_batch actors by their shared tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: BATCH_TAG }, expected: 'success' },
  { scenario: 'DELETE: delete', toolName: 'control_actor', arguments: { action: 'delete', actorName: DELETE_ACTOR }, expected: 'success|not found' },
  { scenario: 'DELETE: destroy_actor', toolName: 'control_actor', arguments: { action: 'destroy_actor', actorName: DESTROY_ACTOR }, expected: 'success|not found' },
  { scenario: 'DELETE: delete_by_tag', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tag: DELETE_TAG }, expected: 'success|not found' },
  { scenario: 'DELETE: delete_by_tag with several tags under one consent', toolName: 'control_actor', arguments: { action: 'delete_by_tag', tags: [DELETE_TAG, BATCH_TAG] }, expected: 'success' },

  // === TRANSFORM / PHYSICS ===
  { scenario: 'ACTION: duplicate', toolName: 'control_actor', arguments: { action: 'duplicate', actorName: DUPLICATE_ACTOR, newName: DUPLICATE_COPY, offset: { x: 50, y: 0, z: 0 } }, expected: 'success|already exists' },
  { scenario: 'ACTION: rename relabels an actor and renames its object', toolName: 'control_actor', arguments: { action: 'rename', actorName: DUPLICATE_COPY, newName: RENAMED_COPY, renameObject: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.objectName', equals: RENAMED_COPY, label: 'the object name followed the label' }] },
  { scenario: 'ACTION: rename back, label only', toolName: 'control_actor', arguments: { action: 'rename', actorName: RENAMED_COPY, newName: DUPLICATE_COPY }, expected: 'success', assertions: [{ path: 'structuredContent.result.label', equals: DUPLICATE_COPY, label: 'the label changed back' }] },
  { scenario: 'CONFIG: set_transform', toolName: 'control_actor', arguments: actorArgs('set_transform', { location: { x: 10, y: 20, z: 130 }, rotation: { x: 0, y: 0, z: 15 }, scale: { x: 1.1, y: 1.1, z: 1.1 } }), expected: 'success' },
  { scenario: 'QUERY: list summary counts the level instead of listing it', toolName: 'control_actor', arguments: { action: 'list', summary: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.count', equals: 0, label: 'no per-actor rows in a summary' }] },
  { scenario: 'CONFIG: set_transform on many actors, each its own transform', toolName: 'control_actor', arguments: { action: 'set_transform', actors: [{ actorName: MAIN_ACTOR, location: { x: 10, y: 20, z: 130 } }, { actorName: MESH_ACTOR, location: { x: 360, y: 160, z: 120 } }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.movedActors', equals: 2, label: 'both actors moved' }, { path: 'structuredContent.result.results.1.location.2', approximately: 120, tolerance: 1, label: 'each item reports its read-back location' }] },
  { scenario: 'CONFIG: set_transform moves by an offset', toolName: 'control_actor', arguments: actorArgs('set_transform', { offset: [0, 0, 10] }), expected: 'success' },
  { scenario: 'CONFIG: set_transform shifts a group, each item by its offset', toolName: 'control_actor', arguments: { action: 'set_transform', actors: [{ actorName: MAIN_ACTOR, offset: [0, 0, -10] }, { actorName: MESH_ACTOR, offset: [0, 0, 5] }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.results.1.location.2', approximately: 125, tolerance: 1, label: 'the second item moved from 120 by +5' }] },
  { scenario: 'ERROR: set_transform refuses location together with offset', toolName: 'control_actor', arguments: actorArgs('set_transform', { location: { x: 10, y: 20, z: 130 }, offset: [0, 0, 10] }), expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'ACTION: teleport_actor', toolName: 'control_actor', arguments: actorArgs('teleport_actor', { location: { x: 20, y: 30, z: 140 } }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_location', toolName: 'control_actor', arguments: actorArgs('set_actor_location', { location: { x: 30, y: 40, z: 150 } }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_rotation', toolName: 'control_actor', arguments: actorArgs('set_actor_rotation', { rotation: { x: 0, y: 45, z: 0 } }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_scale', toolName: 'control_actor', arguments: actorArgs('set_actor_scale', { scale: { x: 1.25, y: 1.25, z: 1.25 } }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_transform', toolName: 'control_actor', arguments: actorArgs('set_actor_transform', { location: { x: 40, y: 50, z: 160 }, rotation: { x: 0, y: 0, z: 30 }, scale: { x: 1, y: 1, z: 1 } }), expected: 'success' },
  { scenario: 'INFO: get_transform', toolName: 'control_actor', arguments: actorArgs('get_transform'), expected: 'success' },
  { scenario: 'INFO: get_actor_transform', toolName: 'control_actor', arguments: actorArgs('get_actor_transform'), expected: 'success' },
  // The suite runs in the editor world, which never ticks: sample_motion must refuse it rather than return 20 identical samples.
  { scenario: 'ERROR: sample_motion refuses the editor world', toolName: 'control_actor', arguments: actorArgs('sample_motion', { durationSeconds: 0.2, intervalSeconds: 0, maxRealSeconds: 2, propertyNames: ['bHidden'] }), expected: 'error|NOT_SIMULATING' },
  // A timeline changes nothing about that refusal; inputs and startWhen still have to reach the plugin intact.
  { scenario: 'ERROR: sample_motion with inputs refuses the editor world', toolName: 'control_actor', arguments: actorArgs('sample_motion', { durationSeconds: 0.2, inputs: [{ key: 'SpaceBar', atSeconds: 0, holdSeconds: 0.1 }] }), expected: 'error|NOT_SIMULATING' },
  { scenario: 'ERROR: sample_motion with startWhen refuses the editor world', toolName: 'control_actor', arguments: actorArgs('sample_motion', { durationSeconds: 0.2, startWhen: { actorName: MAIN_ACTOR, propertyName: 'bHidden', equals: false, waitForChange: false, maxWaitSeconds: 1 } }), expected: 'error|NOT_SIMULATING' },
  { scenario: 'CONFIG: set_visibility', toolName: 'control_actor', arguments: actorArgs('set_visibility', { visible: true }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_visible', toolName: 'control_actor', arguments: actorArgs('set_actor_visible', { visible: true }), expected: 'success' },
  // actorNames: hiding seventeen actors took thirty-four calls; now one call, one undo step, and the misses come back.
  { scenario: 'CONFIG: set_visibility on several actors at once, listing names not found', toolName: 'control_actor', arguments: { action: 'set_visibility', actorNames: [MAIN_ACTOR, DUPLICATE_ACTOR, `MCP_MissingActor_${ts}`], visible: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.updatedActors', equals: 2, label: 'both actors that exist took the change' }, { path: 'structuredContent.result.missing.0', equals: `MCP_MissingActor_${ts}`, label: 'the name that matched no actor is listed back' }] },
  { scenario: 'ERROR: set_visibility with none of actorNames found', toolName: 'control_actor', arguments: { action: 'set_visibility', actorNames: [`MCP_MissingActor_${ts}`], visible: true }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'CONFIG: set_visibility on a Blueprint actor with a billboard is undoable', toolName: 'control_actor', arguments: { action: 'set_visibility', actorName: BP_ACTOR, visible: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.undo.undoable', equals: true, label: 'the billboard was flagged transactional, so the change reaches the undo buffer' }] },
  { scenario: 'ACTION: apply_force', toolName: 'control_actor', arguments: actorArgs('apply_force', { force: { x: 0, y: 0, z: 2500 } }), expected: 'success' },
  { scenario: 'CONFIG: set_material', toolName: 'control_actor', arguments: actorArgs('set_material', { materialPath: ENGINE_BASIC_MATERIAL, materialSlot: 0 }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_material', toolName: 'control_actor', arguments: actorArgs('set_actor_material', { materialPath: ENGINE_BASIC_MATERIAL, materialIndex: 0 }), expected: 'success' },
  { scenario: 'CONFIG: apply_material all components', toolName: 'control_actor', arguments: actorArgs('apply_material', { materialPath: ENGINE_BASIC_MATERIAL, materialSlot: 0, allComponents: true }), expected: 'success' },
  { scenario: 'CONFIG: set_material on several actors', toolName: 'control_actor', arguments: { action: 'set_material', actorNames: [MAIN_ACTOR, DUPLICATE_COPY], materialPath: ENGINE_BASIC_MATERIAL, materialSlot: 0 }, expected: 'success' },
  { scenario: 'ERROR: set_material names the actor it could not find', toolName: 'control_actor', arguments: { action: 'set_material', actorNames: [MAIN_ACTOR, `MCP_Missing_${ts}`], materialPath: ENGINE_BASIC_MATERIAL }, expected: 'error|MATERIAL_BATCH_INCOMPLETE' },

  // === COMPONENTS ===
  { scenario: 'ADD: add_component', toolName: 'control_actor', arguments: actorArgs('add_component', { componentType: '/Script/Engine.PointLightComponent', componentName: COMPONENT_NAME, properties: { Intensity: 1250 } }), expected: 'success|already exists' },
  { scenario: 'CONFIG: set_component_properties', toolName: 'control_actor', arguments: actorArgs('set_component_properties', { componentName: COMPONENT_NAME, properties: { Intensity: 1800 } }), expected: 'success' },
  { scenario: 'CONFIG: set_component_property', toolName: 'control_actor', arguments: actorArgs('set_component_property', { componentName: COMPONENT_NAME, propertyName: 'Intensity', value: 950 }), expected: 'success' },
  { scenario: 'INFO: get_component_property', toolName: 'control_actor', arguments: actorArgs('get_component_property', { componentName: COMPONENT_NAME, propertyName: 'Intensity' }), expected: 'success' },
  { scenario: 'INFO: get_component_property nested propertyPath', toolName: 'control_actor', arguments: actorArgs('get_component_property', { componentName: COMPONENT_NAME, propertyPath: 'AttenuationRadius' }), expected: 'success' },
  // One shared resolver for get and set: dotted struct paths at any depth, and a bare name that lives in one struct member.
  { scenario: 'CONFIG: set_component_property through a struct path', toolName: 'control_actor', arguments: actorArgs('set_component_property', { componentName: 'StaticMeshComponent0', propertyName: 'BodyInstance.CollisionEnabled', value: 'QueryOnly' }), expected: 'success' },
  { scenario: 'CONFIG: set_component_properties with a nested struct member', toolName: 'control_actor', arguments: actorArgs('set_component_properties', { componentName: 'StaticMeshComponent0', properties: { 'LightmassSettings.bShadowIndirectOnly': true, CollisionEnabled: 'QueryAndPhysics' } }), expected: 'success' },
  { scenario: 'INFO: get_component_property reads a bare nested name', toolName: 'control_actor', arguments: actorArgs('get_component_property', { componentName: 'StaticMeshComponent0', propertyName: 'bShadowIndirectOnly' }), expected: 'success', assertions: [{ path: 'structuredContent.result.value', equals: true, label: 'the nested member written through its dotted path reads back by its bare name' }] },
  { scenario: 'ADD: add_component with a mesh', toolName: 'control_actor', arguments: actorArgs('add_component', { componentType: '/Script/Engine.StaticMeshComponent', componentName: MESH_COMPONENT, meshPath: '/Engine/BasicShapes/Cone' }), expected: 'success|already exists' },
  { scenario: 'DELETE: remove the mesh component', toolName: 'control_actor', arguments: actorArgs('remove_component', { componentName: MESH_COMPONENT }), expected: 'success|not found' },
  { scenario: 'AUDIT: audit_placement sweeps the level', toolName: 'control_actor', arguments: { action: 'audit_placement' }, expected: 'success' },
  { scenario: 'AUDIT: audit_placement filtered and capped', toolName: 'control_actor', arguments: { action: 'audit_placement', nameFilter: 'MCP', limit: 5 }, expected: 'success' },
  { scenario: 'AUDIT: audit_placement drops findings below a severity floor', toolName: 'control_actor', arguments: { action: 'audit_placement', minSeverity: 50, limit: 5 }, expected: 'success' },
  { scenario: 'FIX: fix_coplanar previews its moves', toolName: 'control_actor', arguments: { action: 'fix_coplanar', dryRun: true }, expected: 'success' },
  { scenario: 'FIX: fix_coplanar limited by name with a distance', toolName: 'control_actor', arguments: { action: 'fix_coplanar', nameFilter: 'MCP', distance: 1, dryRun: true }, expected: 'success' },
  { scenario: 'AUDIT: audit_placement reports only the kinds asked for', toolName: 'control_actor', arguments: { action: 'audit_placement', kinds: ['coplanar'], limit: 5 }, expected: 'success' },
  { scenario: 'AUDIT: audit_placement reports actors leaning off vertical', toolName: 'control_actor', arguments: { action: 'audit_placement', maxTilt: 15, limit: 5 }, expected: 'success' },
  { scenario: 'INFO: get_components', toolName: 'control_actor', arguments: actorArgs('get_components'), expected: 'success' },
  { scenario: 'INFO: get_components narrowed by componentNames', toolName: 'control_actor', arguments: actorArgs('get_components', { componentNames: ['NoSuchComponent'] }), expected: 'success', assertions: [{ path: 'structuredContent.result.missingComponents.0', equals: 'NoSuchComponent', label: 'a name no component has is reported, not silently dropped' }] },
  { scenario: 'INFO: get_actor_components', toolName: 'control_actor', arguments: actorArgs('get_actor_components'), expected: 'success' },
  { scenario: 'INFO: get_actor_components narrowed by componentNames', toolName: 'control_actor', arguments: actorArgs('get_actor_components', { componentNames: ['NoSuchComponent'] }), expected: 'success' },
  { scenario: 'INFO: get_actor_bounds', toolName: 'control_actor', arguments: actorArgs('get_actor_bounds'), expected: 'success' },
  { scenario: 'DELETE: remove_component', toolName: 'control_actor', arguments: actorArgs('remove_component', { componentName: COMPONENT_NAME }), expected: 'success|not found' },

  // === TAGS / SEARCH ===
  { scenario: 'ADD: add_tag', toolName: 'control_actor', arguments: actorArgs('add_tag', { tag: TAG }), expected: 'success|already exists' },
  { scenario: 'ADD: add_tag to many actors at once, listing names not found', toolName: 'control_actor', arguments: { action: 'add_tag', actorNames: [MAIN_ACTOR, DUPLICATE_ACTOR, `MCP_MissingActor_${ts}`], tag: TAG }, expected: 'success' },
  { scenario: 'INFO: find_by_tag', toolName: 'control_actor', arguments: { action: 'find_by_tag', tag: TAG }, expected: 'success' },
  { scenario: 'INFO: find_actors_by_tag', toolName: 'control_actor', arguments: { action: 'find_actors_by_tag', tag: TAG }, expected: 'success' },
  { scenario: 'INFO: find_by_name', toolName: 'control_actor', arguments: { action: 'find_by_name', name: MAIN_ACTOR }, expected: 'success' },
  { scenario: 'INFO: find_by_name near miss offers similar labels', toolName: 'control_actor', arguments: { action: 'find_by_name', name: MAIN_ACTOR.toLowerCase().replace(/_/g, ' ') }, expected: 'success', assertions: [{ path: 'structuredContent.result.count', equals: 0, label: 'spaces for underscores match nothing' }, { path: 'structuredContent.result.similar', minLength: 1, label: 'the real label is offered' }] },
  { scenario: 'INFO: find_actors_by_name', toolName: 'control_actor', arguments: { action: 'find_actors_by_name', name: MAIN_ACTOR }, expected: 'success' },
  { scenario: 'INFO: find_by_class', toolName: 'control_actor', arguments: { action: 'find_by_class', className: 'StaticMeshActor' }, expected: 'success' },
  { scenario: 'INFO: find_by_class via class alias', toolName: 'control_actor', arguments: { action: 'find_by_class', class: 'StaticMeshActor' }, expected: 'success' },
  // A Blueprint named by its short class name used to answer CLASS_NOT_FOUND.
  { scenario: 'INFO: find_by_class with a short Blueprint class name', toolName: 'control_actor', arguments: { action: 'find_by_class', className: `${BP_NAME}_C` }, expected: 'success' },
  { scenario: 'INFO: find_actors_by_class', toolName: 'control_actor', arguments: { action: 'find_actors_by_class', className: 'StaticMeshActor' }, expected: 'success' },
  { scenario: 'DELETE: remove_tag', toolName: 'control_actor', arguments: actorArgs('remove_tag', { tag: TAG }), expected: 'success|not found' },
  { scenario: 'ACTION: list', toolName: 'control_actor', arguments: { action: 'list', limit: 20, filter: 'MCP_' }, expected: 'success', assertions: [{ path: 'structuredContent.result.actors.0.scale.x', gte: 0.0001, label: 'list rows carry each actor transform' }] },
  { scenario: 'INFO: list reads named properties on every row', toolName: 'control_actor', arguments: { action: 'list', limit: 5, filter: 'MCP_', propertyNames: ['bHidden'] }, expected: 'success', assertions: [{ path: 'structuredContent.result.actors.0.properties.bHidden', equals: 'False', label: 'each row carries the property asked for' }] },
  // A component's property is read the way sample_motion reads it: "Component.Property", the component by name.
  { scenario: 'INFO: list reads a component property as Component.Property', toolName: 'control_actor', arguments: { action: 'list', limit: 1, filter: MAIN_ACTOR, propertyNames: ['StaticMeshComponent.LDMaxDrawDistance'] }, expected: 'success', assertions: [{ path: 'structuredContent.result.actors.0.missingProperties', equals: undefined, label: 'the component property resolved, so nothing is missing' }] },
  { scenario: 'INFO: list names a component property that does not resolve as missing', toolName: 'control_actor', arguments: { action: 'list', limit: 1, filter: MAIN_ACTOR, propertyNames: ['NoSuchComponent.Foo'] }, expected: 'success', assertions: [{ path: 'structuredContent.result.actors.0.missingProperties.0', equals: 'NoSuchComponent.Foo', label: 'the unresolved name is reported as asked' }] },
  // near and radius find what is close to a world point, nearest first, each row with its distance to the actor's bounds.
  { scenario: 'INFO: list near a point inside an actor\'s bounds finds it at distance 0', toolName: 'control_actor', arguments: { action: 'list', filter: 'MCP_Near', near: [30000, 30000, 100], radius: 100 }, expected: 'success', assertions: [{ path: 'structuredContent.result.totalCount', equals: 1, label: 'only the actor whose bounds the point is in, within 100' }, { path: 'structuredContent.result.actors.0.distance', equals: 0, label: 'the point is inside its bounds' }] },
  { scenario: 'INFO: list near a point with a larger radius sorts nearest first', toolName: 'control_actor', arguments: { action: 'list', filter: 'MCP_Near', near: { x: 30000, y: 30000, z: 100 }, radius: 400 }, expected: 'success', assertions: [{ path: 'structuredContent.result.totalCount', equals: 2, label: 'both cubes are within 400' }, { path: 'structuredContent.result.actors.0.distance', equals: 0, label: 'the nearer is first' }, { path: 'structuredContent.result.actors.1.distance', equals: 250, label: 'the second is 250 from the point to its bounds' }] },
  { scenario: 'INFO: list near a point without a radius sorts every matching actor by distance', toolName: 'control_actor', arguments: { action: 'list', filter: 'MCP_Near', near: [30300, 30000, 100], limit: 1 }, expected: 'success', assertions: [{ path: 'structuredContent.result.actors.0.distance', equals: 0, label: 'nearest first: the cube at the point comes before the one 250 away' }, { path: 'structuredContent.result.totalCount', equals: 2, label: 'nothing is dropped without a radius' }] },
  { scenario: 'INFO: list near a distant point with a radius finds nothing', toolName: 'control_actor', arguments: { action: 'list', filter: 'MCP_Near', near: [-30000, -30000, 100], radius: 500 }, expected: 'success', assertions: [{ path: 'structuredContent.result.totalCount', equals: 0, label: 'no actor within 500 of that point' }] },
  { scenario: 'ERROR: list radius without near is refused', toolName: 'control_actor', arguments: { action: 'list', radius: 100 }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'INFO: list pages on with offset', toolName: 'control_actor', arguments: { action: 'list', limit: 1, offset: 1, filter: 'MCP_' }, expected: 'success', assertions: [{ path: 'structuredContent.result.count', equals: 1, label: 'the second page holds one actor' }] },

  // === MISC ===
  { scenario: 'CONFIG: set_blueprint_variables', toolName: 'control_actor', arguments: actorArgs('set_blueprint_variables', { variables: { InitialLifeSpan: 0 } }), expected: 'success' },
  { scenario: 'ERROR: set_blueprint_variables with no real variable sets nothing', toolName: 'control_actor', arguments: actorArgs('set_blueprint_variables', { variables: { NoSuchVariable: 1 } }), expected: 'error|PROPERTY_NOT_FOUND' },
  { scenario: 'CONFIG: set_blueprint_variables on many actors, each its own values', toolName: 'control_actor', arguments: { action: 'set_blueprint_variables', actors: [{ actorName: MAIN_ACTOR, variables: { InitialLifeSpan: 0 } }, { actorName: PARENT_ACTOR, variables: { InitialLifeSpan: 0 } }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.updatedActors', equals: 2, label: 'both actors took their variables' }] },
  { scenario: 'CREATE: create_snapshot', toolName: 'control_actor', arguments: actorArgs('create_snapshot', { snapshotName: `Snapshot_${ts}` }), expected: 'success|already exists' },
  { scenario: 'ACTION: attach', toolName: 'control_actor', arguments: { action: 'attach', childActor: CHILD_ACTOR, parentActor: PARENT_ACTOR }, expected: 'success' },
  { scenario: 'ACTION: detach', toolName: 'control_actor', arguments: { action: 'detach', actorName: CHILD_ACTOR }, expected: 'success' },
  { scenario: 'ACTION: attach_actor', toolName: 'control_actor', arguments: { action: 'attach_actor', childActor: CHILD_ACTOR, parentActor: PARENT_ACTOR }, expected: 'success' },
  { scenario: 'ACTION: detach_actor', toolName: 'control_actor', arguments: { action: 'detach_actor', actorName: CHILD_ACTOR }, expected: 'success' },
  { scenario: 'CONFIG: set_actor_collision', toolName: 'control_actor', arguments: actorArgs('set_actor_collision', { collisionEnabled: true }), expected: 'success' },
  { scenario: 'CONFIG: set_actor_collision on several actors at once, listing names not found', toolName: 'control_actor', arguments: { action: 'set_actor_collision', actorNames: [MAIN_ACTOR, DUPLICATE_ACTOR, `MCP_MissingActor_${ts}`], collisionEnabled: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.updatedActors', equals: 2, label: 'both actors that exist took the change' }, { path: 'structuredContent.result.missing.0', equals: `MCP_MissingActor_${ts}`, label: 'the name that matched no actor is listed back' }, { path: 'structuredContent.result.undo.undoable', equals: true, label: 'one transaction holds every actor, so the whole list is one undo step' }] },
  // The billboard is a primitive component a Blueprint actor makes without RF_Transactional: the change must still reach the undo buffer.
  { scenario: 'CONFIG: set_actor_collision on a Blueprint actor with a billboard is undoable', toolName: 'control_actor', arguments: { action: 'set_actor_collision', actorName: BP_ACTOR, collisionEnabled: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.undo.undoable', equals: true, label: 'the billboard was flagged transactional, so the change reaches the undo buffer' }] },
  { scenario: 'ERROR: set_actor_collision with none of actorNames found', toolName: 'control_actor', arguments: { action: 'set_actor_collision', actorNames: [`MCP_MissingActor_${ts}`], collisionEnabled: true }, expected: 'error|ACTOR_NOT_FOUND' },
  { scenario: 'ACTION: call_actor_function', toolName: 'control_actor', arguments: actorArgs('call_actor_function', { functionName: 'SetActorTickEnabled', arguments: [true] }), expected: 'success|FUNCTION_NOT_FOUND' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete spawned actors', toolName: 'control_actor', arguments: { action: 'delete', actorNames: [MAIN_ACTOR, DUPLICATE_ACTOR, DUPLICATE_COPY, MESH_ACTOR, PARENT_ACTOR, CHILD_ACTOR, BP_ACTOR, NEAR_ACTOR, FAR_ACTOR, `MCP_SpawnSphere_${ts}`, `MCP_SpawnCylinder_${ts}`] }, expected: 'success|not found' },
];

runToolTests('control-actor', testCases, { folder: TEST_FOLDER });
