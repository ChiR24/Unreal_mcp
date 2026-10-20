#!/usr/bin/env node
/**
 * manage_blueprint Tool Integration Tests
 * Covers all 36 actions with proper setup/teardown sequencing.
 *
 * Every action that operates on a blueprint must include a valid
 * blueprintPath pointing to the blueprint created during setup.
 *
 * captureResult is used to capture real nodeIds from node creation
 * so that subsequent node operations (delete, connect, etc.) use
 * real GUIDs that exist in the blueprint graph.
 */

import { runToolTests } from '../../test-runner.mjs';

const TEST_FOLDER = '/Game/MCPTest/AuthoringAssets';
const ts = Date.now();
const BP_NAME = `BP_Test_${ts}`;
const BP_PATH = `${TEST_FOLDER}/${BP_NAME}`;
const INPUT_ACTION_NAME = `IA_Blueprint_${ts}`;
const INPUT_ACTION_PATH = `${TEST_FOLDER}/${INPUT_ACTION_NAME}`;
const ENGINE_CUBE_MESH = '/Engine/BasicShapes/Cube.Cube';
const ENGINE_BASIC_MATERIAL = '/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial';
const ENGINE_DEFAULT_TEXTURE = '/Engine/EngineResources/DefaultTexture.DefaultTexture';
const ENGINE_SOUND = '/Engine/EditorSounds/Notifications/CompileSuccess.CompileSuccess';
const SOUND_BP_NAME = `BP_SoundProbe_${ts}`;

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test blueprint', toolName: 'manage_blueprint', arguments: { action: 'create_blueprint', name: BP_NAME, savePath: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists', assertions: [{ path: 'structuredContent.result.assetPath', equals: BP_PATH, label: 'create_blueprint uses the requested folder' }] },
  { scenario: 'Setup: create input action asset', toolName: 'manage_networking', arguments: { action: 'create_input_action', name: INPUT_ACTION_NAME, path: TEST_FOLDER }, expected: 'success|already exists' },

  // === ACTION: create (requires name + path/blueprintPath) ===
  { scenario: 'ACTION: create', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_Create_${ts}`, savePath: TEST_FOLDER, blueprintType: 'Actor', properties: { bReplicates: true } }, expected: 'success|already exists', assertions: [{ path: 'structuredContent.result.appliedProperties.0', equals: 'bReplicates', label: 'the property that was set is listed' }] },
  { scenario: 'ACTION: create names a property it could not set', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_CreateBadProp_${ts}`, savePath: TEST_FOLDER, blueprintType: 'Actor', properties: { NoSuchProperty: 1 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.failedProperties.0', includes: 'NoSuchProperty', label: 'the miss is named, not dropped' }] },
  { scenario: 'ACTION: create answers a Blueprint already at the path', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_Create_${ts}`, savePath: TEST_FOLDER, blueprintType: 'Actor' }, expected: 'success', assertions: [{ path: 'structuredContent.result.alreadyExists', equals: true, label: 'the existing Blueprint is answered, not rebuilt' }] },
  { scenario: 'ERROR: create refuses a Blueprint already at the path with another parent', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_Create_${ts}`, savePath: TEST_FOLDER, parentClass: '/Script/Engine.Character' }, expected: 'error|ASSET_ALREADY_EXISTS' },
  // A dotted name reaches a component of the class defaults, by property name or by the component's object name.
  { scenario: 'ACTION: create sets component members through dotted names', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_CreateDotted_${ts}`, savePath: TEST_FOLDER, parentClass: '/Script/Engine.Character', properties: { 'CollisionCylinder.CapsuleRadius': 30, 'CharacterMovement.MaxWalkSpeed': 420 } }, expected: 'success', assertions: [{ path: 'structuredContent.result.appliedProperties', length: 2, label: 'both component members were set' }] },

  // === INFO: get_blueprint (uses blueprintPath) ===
  { scenario: 'INFO: get_blueprint', toolName: 'manage_blueprint', arguments: { action: 'get_blueprint', blueprintPath: BP_PATH }, expected: 'success' },

  // === ACTION: get (uses blueprintPath via name fallback) ===
  { scenario: 'ACTION: get', toolName: 'manage_blueprint', arguments: { action: 'get', blueprintPath: BP_PATH }, expected: 'success', timeoutMs: 5000 },

  // === ACTION: compile (uses blueprintPath) ===
  { scenario: 'ACTION: compile', toolName: 'manage_blueprint', arguments: { action: 'compile', blueprintPath: BP_PATH, saveAfterCompile: false }, expected: 'success', assertions: [{ path: 'structuredContent.receipt.changes', length: 1, label: 'the receipt lists the compiled asset, not the words compiled and saved' }, { path: 'structuredContent.receipt.changes', notIncludes: 'compiled', label: 'no status word among the changes' }] },

  // === ADD: add_component (blueprintPath + componentClass + componentName) ===
  { scenario: 'ADD: add_component', toolName: 'manage_blueprint', arguments: { action: 'add_component', blueprintPath: BP_PATH, componentType: 'PointLightComponent', componentName: 'TestLight', attachTo: 'DefaultSceneRoot' }, expected: 'success|already exists' },
  { scenario: 'ADD: add_component placed with mesh and material', toolName: 'manage_blueprint', arguments: { action: 'add_component', blueprintPath: BP_PATH, componentClass: 'StaticMeshComponent', componentName: 'TestPlacedMesh', meshPath: ENGINE_CUBE_MESH, materialPath: ENGINE_BASIC_MATERIAL, location: { x: 0, y: 0, z: 80 }, rotation: { pitch: 0, yaw: 90, roll: 0 }, scale: { x: 0.5, y: 0.5, z: 0.5 } }, expected: 'success|already exists' },
  { scenario: 'ADD: add_scs_component overlap box placed and configured', toolName: 'manage_blueprint', arguments: { action: 'add_scs_component', blueprintPath: BP_PATH, componentClass: 'BoxComponent', componentName: 'TestOverlapBox', location: { x: 0, y: 0, z: 40 }, rotation: { pitch: 0, yaw: 0, roll: 0 }, scale: { x: 2, y: 2, z: 1 }, properties: { bGenerateOverlapEvents: true } }, expected: 'success|already exists' },

// === CONFIG: set_default (blueprintPath + propertyName + value/propertyValue) ===
// bGenerateOverlapEvents is on UPrimitiveComponent; this Actor BP root is SceneComponent.
// Use a property that exists on AActor CDO directly.
// A placed actor still holding the old default takes the new one: set_default used to change only the class
// default, and every actor already in the level kept the old value under a success reply.
{ scenario: 'Setup: place an actor of the test Blueprint before its default changes', toolName: 'control_actor', arguments: { action: 'spawn_blueprint', blueprintPath: BP_PATH, actorName: `BPDefaultProbe_${ts}`, location: { x: 0, y: 900, z: 200 } }, expected: 'success|already exists' },
{ scenario: 'CONFIG: set_default', toolName: 'manage_blueprint', arguments: { action: 'set_default', blueprintPath: BP_PATH, propertyName: 'bReplicates', propertyValue: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.instancesUpdated', equals: 1, label: 'the placed actor took the new default' }] },
{ scenario: 'VERIFY: the placed actor reads the new default', toolName: 'control_actor', arguments: { action: 'list', filter: `BPDefaultProbe_${ts}`, propertyNames: ['bReplicates'] }, expected: 'success', assertions: [{ path: 'structuredContent.result.actors.0.properties.bReplicates', equals: true, label: 'the instance followed the class default' }] },
{ scenario: 'Cleanup: delete the placed probe actor', toolName: 'control_actor', arguments: { action: 'delete', actorName: `BPDefaultProbe_${ts}` }, expected: 'success|not found' },
{ scenario: 'VERIFY: get_blueprint reads one default without the whole summary', toolName: 'manage_blueprint', arguments: { action: 'get_blueprint', blueprintPath: BP_PATH, propertyName: 'bReplicates' }, expected: 'success', assertions: [{ path: 'structuredContent.result.propertyValue', equals: 'True', label: 'the value set above' }, { path: 'structuredContent.result.variables', equals: undefined, label: 'no summary alongside a single-property read' }] },

  // === CONFIG: modify_scs (blueprintPath + operations) ===
  { scenario: 'CONFIG: modify_scs', toolName: 'manage_blueprint', arguments: { action: 'modify_scs', blueprintPath: BP_PATH, operations: [{ type: 'add_component', componentName: 'TestModSCSComp', componentClass: 'SceneComponent' }], applyAndSave: true }, expected: 'success|already exists' },
  { scenario: 'CONFIG: modify_scs one component at the top level', toolName: 'manage_blueprint', arguments: { action: 'modify_scs', blueprintPath: BP_PATH, componentName: 'TestPlacedMesh', location: { x: 0, y: 0, z: 120 }, rotation: { pitch: 0, yaw: 45, roll: 0 }, scale: { x: 1, y: 1, z: 1 }, meshPath: ENGINE_CUBE_MESH, materialPath: ENGINE_BASIC_MATERIAL, properties: { bCastShadow: false }, compile: true, save: true }, expected: 'success' },

  // location, rotation and scale sit on an operation itself as well as inside `transform`: the rotation given
  // on the operation used to be dropped while its properties applied, and the batch answered success.
  { scenario: 'CONFIG: modify_scs operation with rotation given directly on it', toolName: 'manage_blueprint', arguments: { action: 'modify_scs', blueprintPath: BP_PATH, operations: [{ type: 'modify_component', componentName: 'TestPlacedMesh', rotation: { pitch: -8, yaw: -90, roll: 0 }, properties: { bCastShadow: true } }], compile: true, save: true }, expected: 'success' },
  { scenario: 'VERIFY: the rotation given on the operation reached the component', toolName: 'manage_blueprint', arguments: { action: 'get_scs', blueprintPath: BP_PATH, componentClass: 'StaticMeshComponent' }, expected: 'success', assertions: [{ path: 'structuredContent.result.components.0.componentName', equals: 'TestPlacedMesh', label: 'the mesh component the operation named' }, { path: 'structuredContent.result.components.0.transform.rotation.pitch', approximately: -8, tolerance: 0.01, label: 'the pitch given on the operation' }, { path: 'structuredContent.result.components.0.transform.rotation.yaw', approximately: -90, tolerance: 0.01, label: 'the yaw given on the operation' }, { path: 'structuredContent.result.components.0.transform.location.z', approximately: 120, tolerance: 0.01, label: 'the location left out kept its value' }] },
  { scenario: 'CONFIG: modify_scs names a transform part it could not read', toolName: 'manage_blueprint', arguments: { action: 'modify_scs', blueprintPath: BP_PATH, operations: [{ type: 'modify_component', componentName: 'TestPlacedMesh', rotation: 'sideways', properties: { bCastShadow: true } }], compile: true, save: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.operations.0.rejectedProperties.0', includes: 'rotation', label: 'the part that did not apply is named on its operation' }, { path: 'structuredContent.result.warnings', minLength: 1, label: 'and repeated in the batch warnings, not skipped' }] },

  // An add given only a mesh and a material (no transform, no properties) used to create an empty component and answer success.
  { scenario: 'CONFIG: modify_scs add_component with only a mesh and a material applies both', toolName: 'manage_blueprint', arguments: { action: 'modify_scs', blueprintPath: BP_PATH, operations: [{ type: 'add_component', componentName: 'TestBatchMesh', componentClass: 'StaticMeshComponent', meshPath: ENGINE_CUBE_MESH, materialPath: ENGINE_BASIC_MATERIAL }], compile: true, save: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.operations.0.success', equals: true, label: 'the add reports the mesh and material it applied' }] },
  { scenario: 'VERIFY: the mesh the add named is on the component', toolName: 'manage_blueprint', arguments: { action: 'get_blueprint', blueprintPath: BP_PATH, propertyName: 'TestBatchMesh.StaticMesh' }, expected: 'success', assertions: [{ path: 'structuredContent.result.propertyValue', includes: 'Cube', label: 'StaticMesh is the cube, not None' }] },
  { scenario: 'CONFIG: modify_scs names a mesh path that does not load', toolName: 'manage_blueprint', arguments: { action: 'modify_scs', blueprintPath: BP_PATH, operations: [{ type: 'add_component', componentName: 'TestBatchBadMesh', componentClass: 'StaticMeshComponent', meshPath: `${TEST_FOLDER}/NoSuchMesh_${ts}`, properties: { bCastShadow: true } }], compile: true, save: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.operations.0.rejectedProperties.0', includes: 'meshPath', label: 'the mesh that did not load is named on its operation' }] },

  // componentType is the spelling add_component declares for the class; the batch add read only componentClass and answered
  // "Component class not found", and so did a single-component payload the promotion had already counted as an add.
  { scenario: 'CONFIG: modify_scs add_component names its class as componentType', toolName: 'manage_blueprint', arguments: { action: 'modify_scs', blueprintPath: BP_PATH, operations: [{ type: 'add_component', componentName: 'TestTypeAlias', componentType: 'SceneComponent' }], compile: true, save: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.operations.0.success', equals: true, label: 'the class was found under componentType' }] },
  { scenario: 'CONFIG: modify_scs one component at the top level is an add when it names componentType', toolName: 'manage_blueprint', arguments: { action: 'modify_scs', blueprintPath: BP_PATH, componentName: 'TestTypeAliasTop', componentType: 'SceneComponent', location: { x: 0, y: 0, z: 10 }, compile: true, save: true }, expected: 'success' },

  // The class and attach parameters are declared for every variant that reads them: the top-level add modify_scs promotes, and both
  // adds (which share one handler), so none of these calls draws a receipt warning that the parameter "was not used".
  // A trigger under a base flattened to (1.6, 1.6, 0.1) is stretched the same way in the world; the edit says so.
  { scenario: 'CONFIG: modify_scs says a part takes the uneven scale of the part it is attached to', toolName: 'manage_blueprint', arguments: { action: 'modify_scs', blueprintPath: BP_PATH, operations: [{ type: 'add_component', componentName: 'TestFlatBase', componentClass: 'StaticMeshComponent', meshPath: ENGINE_CUBE_MESH, transform: { location: { x: 0, y: 0, z: -300 }, scale: { x: 1.6, y: 1.6, z: 0.1 } } }, { type: 'add_component', componentName: 'TestFlatTrigger', componentClass: 'BoxComponent', attachTo: 'TestFlatBase', transform: { location: { x: 0, y: 0, z: 400 } } }], compile: true, save: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.warnings.0', includes: 'scaled unevenly', label: 'the part under the flattened base is named' }] },
  { scenario: 'CONFIG: modify_scs one component at the top level is an add when it names componentClass, attached under attachTo', toolName: 'manage_blueprint', arguments: { action: 'modify_scs', blueprintPath: BP_PATH, componentName: 'TestClassTop', componentClass: 'StaticMeshComponent', attachTo: 'DefaultSceneRoot', meshPath: ENGINE_CUBE_MESH, compile: true, save: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.operations.0.attachedTo', equals: 'DefaultSceneRoot', label: 'attachTo reached the add the payload became' }] },
  { scenario: 'ADD: add_scs_component names its class as componentType and its parent as attachTo', toolName: 'manage_blueprint', arguments: { action: 'add_scs_component', blueprintPath: BP_PATH, componentType: 'SceneComponent', componentName: 'TestTypeAttachSCS', attachTo: 'DefaultSceneRoot' }, expected: 'success|already exists' },
  { scenario: 'ADD: add_component names its parent as parentComponent', toolName: 'manage_blueprint', arguments: { action: 'add_component', blueprintPath: BP_PATH, componentClass: 'SceneComponent', componentName: 'TestParentAdd', parentComponent: 'DefaultSceneRoot' }, expected: 'success|already exists' },

  // === INFO: get_scs (blueprintPath) ===
  { scenario: 'INFO: get_scs', toolName: 'manage_blueprint', arguments: { action: 'get_scs', blueprintPath: BP_PATH }, expected: 'success' },
  { scenario: 'INFO: get_scs filtered to one component class', toolName: 'manage_blueprint', arguments: { action: 'get_scs', blueprintPath: BP_PATH, componentClass: 'SceneComponent' }, expected: 'success' },
  { scenario: 'INFO: get_scs scans a folder for a component class', toolName: 'manage_blueprint', arguments: { action: 'get_scs', path: TEST_FOLDER, componentClass: 'SceneComponent' }, expected: 'success' },

  // === ADD: add_scs_component (blueprint_path + component_class + component_name) ===
  { scenario: 'ADD: add_scs_component', toolName: 'manage_blueprint', arguments: { action: 'add_scs_component', blueprintPath: BP_PATH, componentClass: 'PointLightComponent', componentName: 'TestSCSComp', parentComponent: 'DefaultSceneRoot' }, expected: 'success|already exists' },

  // === ADD: add_scs_component with mesh/material assignment ===
  { scenario: 'ADD: add_scs_component with mesh material', toolName: 'manage_blueprint', arguments: { action: 'add_scs_component', blueprintPath: BP_PATH, componentClass: 'StaticMeshComponent', componentName: 'TestStaticMeshSCSComp', parentComponent: 'DefaultSceneRoot', meshPath: ENGINE_CUBE_MESH, materialPath: ENGINE_BASIC_MATERIAL }, expected: 'success|already exists', assertions: [{ path: 'structuredContent.result.mesh_applied', equals: true, label: 'SCS static mesh assignment applied' }, { path: 'structuredContent.result.material_applied', equals: true, label: 'SCS material assignment applied' }] },
  { scenario: 'ERROR: add_scs_component refuses a mesh that does not load', toolName: 'manage_blueprint', arguments: { action: 'add_scs_component', blueprintPath: BP_PATH, componentClass: 'StaticMeshComponent', componentName: 'TestMissingMeshComp', meshPath: '/Game/MCPTest/NoSuchMesh' }, expected: 'error|ASSET_NOT_FOUND' },
  { scenario: 'ERROR: add_scs_component refuses a mesh for a component that shows none', toolName: 'manage_blueprint', arguments: { action: 'add_scs_component', blueprintPath: BP_PATH, componentClass: 'PointLightComponent', componentName: 'TestLightWithMesh', meshPath: ENGINE_CUBE_MESH }, expected: 'error|INVALID_ARGUMENT' },

  // === DELETE: remove_scs_component (blueprintPath + componentName) ===
  { scenario: 'DELETE: remove_scs_component', toolName: 'manage_blueprint', arguments: { action: 'remove_scs_component', blueprintPath: BP_PATH, componentName: 'TestSCSComp' }, expected: 'success|not found' },
  { scenario: 'DELETE: remove_scs_static_mesh_component', toolName: 'manage_blueprint', arguments: { action: 'remove_scs_component', blueprintPath: BP_PATH, componentName: 'TestStaticMeshSCSComp' }, expected: 'success|not found' },
  // Several removals under one consent (componentNames).
  { scenario: 'ADD: two SCS components for a batch remove', toolName: 'manage_blueprint', arguments: { action: 'modify_scs', blueprintPath: BP_PATH, operations: [{ type: 'add_component', componentName: 'TestBatchA', componentClass: 'SceneComponent' }, { type: 'add_component', componentName: 'TestBatchB', componentClass: 'SceneComponent' }], applyAndSave: true }, expected: 'success|already exists' },
  { scenario: 'DELETE: remove_scs_component componentNames', toolName: 'manage_blueprint', arguments: { action: 'remove_scs_component', blueprintPath: BP_PATH, componentNames: ['TestBatchA', 'TestBatchB'] }, expected: 'success' },

  // === ACTION: reparent_scs_component (blueprintPath + componentName + newParent) ===
  { scenario: 'ACTION: reparent_scs_component', toolName: 'manage_blueprint', arguments: { action: 'reparent_scs_component', blueprintPath: BP_PATH, componentName: 'TestModSCSComp', newParent: 'DefaultSceneRoot' }, expected: 'success' },

  // === CONFIG: set_scs_transform (blueprintPath + componentName + location/rotation/scale) ===
  { scenario: 'CONFIG: set_scs_transform', toolName: 'manage_blueprint', arguments: { action: 'set_scs_transform', blueprintPath: BP_PATH, componentName: 'TestModSCSComp', location: { x: 100, y: 0, z: 50 }, rotation: { pitch: 0, yaw: 45, roll: 0 }, scale: { x: 1.1, y: 1.1, z: 1.1 } }, expected: 'success' },

  // === CONFIG: set_scs_property (blueprintPath + componentName + propertyName + propertyValue) ===
  // C++ ApplyJsonValueToProperty supports struct (FVector) via array format [x, y, z].
  { scenario: 'CONFIG: set_scs_property', toolName: 'manage_blueprint', arguments: { action: 'set_scs_property', blueprintPath: BP_PATH, componentName: 'TestModSCSComp', propertyName: 'RelativeLocation', propertyValue: [100, 0, 50] }, expected: 'success' },
  // The same resolver as control_actor: a struct path, and a bare name living in one struct member.
  { scenario: 'ADD: static mesh SCS component for nested property writes', toolName: 'manage_blueprint', arguments: { action: 'add_scs_component', blueprintPath: BP_PATH, componentClass: 'StaticMeshComponent', componentName: 'TestNestedMesh', parentComponent: 'DefaultSceneRoot' }, expected: 'success|already exists' },
  { scenario: 'CONFIG: set_scs_property through a struct path', toolName: 'manage_blueprint', arguments: { action: 'set_scs_property', blueprintPath: BP_PATH, componentName: 'TestNestedMesh', propertyName: 'LightmassSettings.bShadowIndirectOnly', propertyValue: true }, expected: 'success' },
  { scenario: 'CONFIG: set_scs_property collision by its bare name', toolName: 'manage_blueprint', arguments: { action: 'set_scs_property', blueprintPath: BP_PATH, componentName: 'TestNestedMesh', propertyName: 'CollisionEnabled', propertyValue: 'QueryOnly' }, expected: 'success' },
  { scenario: 'DELETE: remove the nested-property mesh component', toolName: 'manage_blueprint', arguments: { action: 'remove_scs_component', blueprintPath: BP_PATH, componentName: 'TestNestedMesh' }, expected: 'success|not found' },

  // === ACTION: ensure_exists (blueprintPath) ===
  // name + savePath outside /Game's root: the check looks where the create would land.
  { scenario: 'ACTION: ensure_exists', toolName: 'manage_blueprint', arguments: { action: 'ensure_exists', name: BP_NAME, savePath: TEST_FOLDER, parentClass: 'Actor', createIfMissing: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.blueprintPath', equals: BP_PATH, label: 'the existing Blueprint under savePath is found' }] },

  // === ACTION: probe_handle (no blueprint needed - uses componentClass) ===
  { scenario: 'ACTION: probe_handle', toolName: 'manage_blueprint', arguments: { action: 'probe_handle', blueprintPath: BP_PATH }, expected: 'success', assertions: [{ path: 'structuredContent.result.reachable', equals: true, label: 'probe_handle found the Blueprint' }] },

  // === ADD: add_variable (blueprintPath + variableName + variableType) ===
  // This variable will be renamed in the next step — do NOT delete it before rename.
  { scenario: 'ADD: add_variable', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: BP_PATH, variableName: 'TestVariable', variableType: 'Boolean', category: 'MCP', isReplicated: true, isPublic: true }, expected: 'success|already exists' },
  { scenario: 'ADD: add_variable exposed on spawn', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: BP_PATH, variableName: 'SpawnPower', variableType: 'Float', exposeOnSpawn: true }, expected: 'success|already exists' },
  { scenario: 'ADD: add_variable of an enum type', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: BP_PATH, variableName: 'MoveMode', variableType: 'Enum:/Script/Engine.EMovementMode' }, expected: 'success' },
  { scenario: 'VERIFY: the enum variable compiles as that enum, not an int', toolName: 'inspect', arguments: { action: 'inspect_class', className: `${BP_PATH}.${BP_NAME}_C` }, expected: 'success', assertions: [{ path: 'structuredContent.result.properties', includesObject: { name: 'MoveMode', type: 'Enum(EMovementMode)' }, label: 'enum property' }] },
  { scenario: 'ERROR: add_variable colliding with a parent-class property', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: BP_PATH, variableName: 'bReplicates', variableType: 'Boolean' }, expected: 'error|VARIABLE_NAME_CONFLICT' },
  // The compiler only warns on a default it cannot parse; add_variable refuses it and adds nothing.
  { scenario: 'ERROR: add_variable with a default its type cannot take', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: BP_PATH, variableName: 'BadDefaultVar', variableType: 'Int', defaultValue: 'NotANumberAtAll' }, expected: 'error|DEFAULT_NOT_APPLIED' },
  { scenario: 'VERIFY: the refused add_variable left no variable behind', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: BP_PATH, variableName: 'BadDefaultVar', variableType: 'Int', defaultValue: 7 }, expected: 'success', assertions: [{ path: 'structuredContent.result.saved', equals: true, label: 'the variable is added anew (saved), not found already existing' }] },
  { scenario: 'ADD: add_variable for member metadata', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: BP_PATH, variableName: 'MetaVariable', variableType: 'Float' }, expected: 'success|already exists' },
  { scenario: 'CONFIG: set_metadata on a member variable', toolName: 'manage_blueprint', arguments: { action: 'set_metadata', blueprintPath: BP_PATH, propertyName: 'MetaVariable', metadata: { tooltip: 'Member metadata' } }, expected: 'success', assertions: [{ path: 'structuredContent.result.variableName', equals: 'MetaVariable', label: 'routed to the variable' }] },

  // === ACTION: rename_variable (blueprintPath + oldName + newName) ===
  // Renames the variable added above (NOT deleted).
  { scenario: 'ACTION: rename_variable', toolName: 'manage_blueprint', arguments: { action: 'rename_variable', blueprintPath: BP_PATH, oldName: 'TestVariable', newName: 'RenamedVariable' }, expected: 'success' },
  { scenario: 'ACTION: refresh_blueprints after the rename', toolName: 'manage_asset', arguments: { action: 'refresh_blueprints', assetPaths: [BP_PATH] }, expected: 'success' },

  // === CONFIG: set_variable_metadata (blueprintPath + variableName + metadata) ===
  // Operates on the RENAMED variable from the previous step.
  { scenario: 'CONFIG: set_variable_metadata', toolName: 'manage_blueprint', arguments: { action: 'set_variable_metadata', blueprintPath: BP_PATH, variableName: 'RenamedVariable', metadata: { tooltip: 'Test variable tooltip' } }, expected: 'success' },
  { scenario: 'CONFIG: set_variable_metadata on several variables', toolName: 'manage_blueprint', arguments: { action: 'set_variable_metadata', blueprintPath: BP_PATH, variableNames: ['RenamedVariable'], metadata: { ExposeOnSpawn: 'true' } }, expected: 'success', assertions: [{ path: 'structuredContent.result.variableNames', length: 1, label: 'every named variable reported' }] },

  // === DELETE: remove_variable (blueprintPath + variableName) ===
  // Now remove the renamed variable after metadata was set.
  { scenario: 'DELETE: remove_variable', toolName: 'manage_blueprint', arguments: { action: 'remove_variable', blueprintPath: BP_PATH, variableName: 'RenamedVariable' }, expected: 'success|not found' },
  { scenario: 'ADD: add_variable ScratchA for a list removal', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: BP_PATH, variableName: 'ScratchA', variableType: 'Float' }, expected: 'success|already exists' },
  { scenario: 'ADD: add_variable ScratchB for a list removal', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: BP_PATH, variableName: 'ScratchB', variableType: 'Int' }, expected: 'success|already exists' },
  { scenario: 'DELETE: remove_variable takes variableNames', toolName: 'manage_blueprint', arguments: { action: 'remove_variable', blueprintPath: BP_PATH, variableNames: ['ScratchA', 'scratchb'] }, expected: 'success', assertions: [{ path: 'structuredContent.result.variableNames', includes: 'ScratchB', label: 'removed in its own spelling' }] },
  { scenario: 'ERROR: remove_variable refuses a list with a name it lacks', toolName: 'manage_blueprint', arguments: { action: 'remove_variable', blueprintPath: BP_PATH, variableNames: ['ScratchA'] }, expected: 'error|not found' },

  // === ADD: add_function (blueprintPath + functionName) ===
  { scenario: 'ADD: add_function', toolName: 'manage_blueprint', arguments: { action: 'add_function', blueprintPath: BP_PATH, functionName: 'TestFunction', inputs: [{ name: 'InputValue', type: 'Float' }], outputs: [{ name: 'ReturnValue', type: 'Float' }], isPublic: true }, expected: 'success|already exists' },
  { scenario: 'CREATE: a Blueprint Interface', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BPI_Test_${ts}`, savePath: TEST_FOLDER, blueprintType: 'Interface' }, expected: 'success|already exists' },
  { scenario: 'ADD: an interface function that is an event', toolName: 'manage_blueprint', arguments: { action: 'add_function', blueprintPath: `${TEST_FOLDER}/BPI_Test_${ts}`, functionName: 'Interact', inputs: [{ name: 'Instigator', type: 'Object' }] }, expected: 'success|already exists' },
  { scenario: 'ADD: an interface function with a return value', toolName: 'manage_blueprint', arguments: { action: 'add_function', blueprintPath: `${TEST_FOLDER}/BPI_Test_${ts}`, functionName: 'CanInteract', outputs: [{ name: 'Allowed', type: 'Boolean' }] }, expected: 'success|already exists' },
  { scenario: 'CONFIG: implement_interface on a Blueprint', toolName: 'manage_blueprint', arguments: { action: 'implement_interface', blueprintPath: BP_PATH, interfacePath: `${TEST_FOLDER}/BPI_Test_${ts}` }, expected: 'success', assertions: [{ path: 'structuredContent.result.changed', equals: true, label: 'the interface was added' }, { path: 'structuredContent.result.events.0', equals: 'Interact', label: 'a function without outputs is an event' }, { path: 'structuredContent.result.functionGraphs.0', equals: 'CanInteract', label: 'a function with outputs is a graph to fill' }] },
  { scenario: 'ADD: add_event handles the interface event, not a same-named custom one', toolName: 'manage_blueprint', arguments: { action: 'add_event', blueprintPath: BP_PATH, eventName: 'Interact' }, expected: 'success', assertions: [{ path: 'structuredContent.result.overrides', includes: 'BPI_Test_', label: 'the interface event, not a custom event' }] },
  { scenario: 'CONFIG: implement_interface again changes nothing', toolName: 'manage_blueprint', arguments: { action: 'implement_interface', blueprintPath: BP_PATH, interfacePath: `${TEST_FOLDER}/BPI_Test_${ts}` }, expected: 'success', assertions: [{ path: 'structuredContent.result.changed', equals: false, label: 'already implemented' }] },
  { scenario: 'CONFIG: implement_interface remove drops it', toolName: 'manage_blueprint', arguments: { action: 'implement_interface', blueprintPath: BP_PATH, interfacePath: `${TEST_FOLDER}/BPI_Test_${ts}`, remove: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.changed', equals: true, label: 'the interface was removed' }] },
  { scenario: 'ERROR: implement_interface with a class that is no interface', toolName: 'manage_blueprint', arguments: { action: 'implement_interface', blueprintPath: BP_PATH, interfacePath: BP_PATH }, expected: 'error|not a Blueprint Interface' },
  { scenario: 'ADD: add_function private', toolName: 'manage_blueprint', arguments: { action: 'add_function', blueprintPath: BP_PATH, functionName: 'TestPrivateFunction', isPublic: false }, expected: 'success|already exists' },
  // pure: its call nodes get no exec pins. Declared outputs make a return node, named by resultNodeGuid.
  { scenario: 'ADD: add_function pure with a return node', toolName: 'manage_blueprint', arguments: { action: 'add_function', blueprintPath: BP_PATH, functionName: 'TestPureFunction', inputs: [{ name: 'Value', type: 'Float' }], outputs: [{ name: 'Result', type: 'Float' }], pure: true }, expected: 'success', captureResult: { key: 'pureReturnId', fromField: 'result.resultNodeGuid' } },
  { scenario: 'VERIFY: add_function resultNodeGuid names the return node', toolName: 'manage_blueprint', arguments: { action: 'get_node_details', blueprintPath: BP_PATH, nodeGuid: '${captured:pureReturnId}', graphName: 'TestPureFunction' }, expected: 'success', assertions: [{ path: 'structuredContent.result.pins', includesObject: { pinName: 'Result' }, label: 'the declared output is an input pin of the return node' }] },
  { scenario: 'ERROR: add_function refuses an existing function with another signature', toolName: 'manage_blueprint', arguments: { action: 'add_function', blueprintPath: BP_PATH, functionName: 'TestPureFunction', pure: false }, expected: 'error|FUNCTION_EXISTS' },
  { scenario: 'ERROR: add_function on a missing Blueprint', toolName: 'manage_blueprint', arguments: { action: 'add_function', blueprintPath: `${TEST_FOLDER}/BP_Missing_${ts}`, functionName: 'Nowhere', pure: true }, expected: 'error|BLUEPRINT_NOT_FOUND' },
  { scenario: 'ERROR: add_function refuses an input type no pin takes', toolName: 'manage_blueprint', arguments: { action: 'add_function', blueprintPath: BP_PATH, functionName: 'TestBadTypeFunction', inputs: [{ name: 'Speed', type: 'flaot' }] }, expected: 'error|TYPE_RESOLUTION_FAILED' },
  { scenario: 'ERROR: add_function refuses a nameless output', toolName: 'manage_blueprint', arguments: { action: 'add_function', blueprintPath: BP_PATH, functionName: 'TestBadTypeFunction', outputs: [{ type: 'Float' }] }, expected: 'error|TYPE_RESOLUTION_FAILED' },

  // === DELETE: remove_function (blueprintPath + functionName) ===
  // Removes the function added directly above; safe because no later case reuses TestFunction.
  { scenario: 'DELETE: remove_function', toolName: 'manage_blueprint', arguments: { action: 'remove_function', blueprintPath: BP_PATH, functionName: 'TestFunction' }, expected: 'success|not found' },

  // === ADD: add_event (blueprintPath + eventType) ===
  { scenario: 'ADD: add_event', toolName: 'manage_blueprint', arguments: { action: 'add_event', blueprintPath: BP_PATH, eventType: 'Custom', customEventName: 'TestEvent', parameters: [{ name: 'Payload', type: 'String' }] }, expected: 'success|already exists' },

  // === DELETE: remove_event (blueprintPath + eventName) ===
  { scenario: 'DELETE: remove_event', toolName: 'manage_blueprint', arguments: { action: 'remove_event', blueprintPath: BP_PATH, eventName: 'TestEvent', graphName: 'EventGraph' }, expected: 'success|not found' },
  { scenario: 'ERROR: add_event into a missing event graph page', toolName: 'manage_blueprint', arguments: { action: 'add_event', blueprintPath: BP_PATH, graphName: 'NoSuchGraph', customEventName: 'Nowhere' }, expected: 'error|GRAPH_NOT_FOUND' },
  { scenario: 'ERROR: add_event refuses a parameter type no pin takes', toolName: 'manage_blueprint', arguments: { action: 'add_event', blueprintPath: BP_PATH, eventType: 'Custom', customEventName: 'TestBadTypeEvent', parameters: [{ name: 'Payload', type: 'Strnig' }] }, expected: 'error|TYPE_RESOLUTION_FAILED' },
  { scenario: 'ADD: add_event with array and map parameters', toolName: 'manage_blueprint', arguments: { action: 'add_event', blueprintPath: BP_PATH, eventType: 'Custom', customEventName: 'TestContainerEvent', parameters: [{ name: 'Scores', type: 'Array<Int>' }, { name: 'Owners', type: 'Map<Name,Actor>' }] }, expected: 'success', captureResult: { key: 'containerEventId', fromField: 'result.nodeGuid' } },
  { scenario: 'VERIFY: an array and a map pin read as containers', toolName: 'manage_blueprint', arguments: { action: 'get_node_details', blueprintPath: BP_PATH, nodeGuid: '${captured:containerEventId}', graphName: 'EventGraph' }, expected: 'success', assertions: [{ path: 'structuredContent.result.pins', includesObject: { pinName: 'Scores', container: 'array' }, label: 'array pin' }, { path: 'structuredContent.result.pins', includesObject: { pinName: 'Owners', container: 'map', valueType: 'Actor' }, label: 'map pin' }] },
  { scenario: 'ADD: add_event with an enum parameter', toolName: 'manage_blueprint', arguments: { action: 'add_event', blueprintPath: BP_PATH, eventType: 'Custom', customEventName: 'TestEnumEvent', parameters: [{ name: 'Mode', type: 'Enum:/Script/Engine.EMovementMode' }] }, expected: 'success', captureResult: { key: 'enumEventId', fromField: 'result.nodeGuid' } },
  { scenario: 'VERIFY: the enum parameter is a byte pin naming its enum', toolName: 'manage_blueprint', arguments: { action: 'get_node_details', blueprintPath: BP_PATH, nodeGuid: '${captured:enumEventId}', graphName: 'EventGraph' }, expected: 'success', assertions: [{ path: 'structuredContent.result.pins', includesObject: { pinName: 'Mode', pinType: 'byte', pinSubType: 'EMovementMode' }, label: 'byte enum pin' }] },
  { scenario: 'ADD: add_function takes a bare enum name', toolName: 'manage_blueprint', arguments: { action: 'add_function', blueprintPath: BP_PATH, functionName: 'TestPickMode', inputs: [{ name: 'Wanted', type: 'EMovementMode' }], outputs: [{ name: 'Chosen', type: 'Enum:/Script/Engine.EMovementMode' }] }, expected: 'success' },
  // Component-bound event: a real K2Node_ComponentBoundEvent on the box's delegate.
  { scenario: 'ADD: add_event bound to a component delegate', toolName: 'manage_blueprint', arguments: { action: 'add_event', blueprintPath: BP_PATH, componentName: 'TestOverlapBox', eventName: 'OnComponentBeginOverlap', graphName: 'EventGraph', posX: 0, posY: 900 }, expected: 'success', captureResult: { key: 'overlapEventId', fromField: 'result.nodeGuid' }, assertions: [{ path: 'structuredContent.result.eventName', includes: 'TestOverlapBox', label: 'bound to the box' }] },
  { scenario: 'VERIFY: component-bound event has its delegate pins', toolName: 'manage_blueprint', arguments: { action: 'get_node_details', blueprintPath: BP_PATH, nodeGuid: '${captured:overlapEventId}', graphName: 'EventGraph' }, expected: 'success', assertions: [{ path: 'structuredContent.result.pins', includesObject: { pinName: 'OtherActor' }, label: 'the overlap signature pins exist' }] },
  { scenario: 'DELETE: remove_event component-bound by node id', toolName: 'manage_blueprint', arguments: { action: 'remove_event', blueprintPath: BP_PATH, nodeId: '${captured:overlapEventId}', graphName: 'EventGraph' }, expected: 'success' },

  // === ADD: add_construction_script (blueprintPath) ===
  { scenario: 'ADD: add_construction_script', toolName: 'manage_blueprint', arguments: { action: 'add_construction_script', blueprintPath: BP_PATH }, expected: 'success|already exists' },

  // === CONFIG: set_metadata (assetPath/blueprintPath + metadata) ===
  { scenario: 'CONFIG: set_metadata', toolName: 'manage_blueprint', arguments: { action: 'set_metadata', blueprintPath: BP_PATH, metadata: { comment: 'Test metadata' } }, expected: 'success' },

  // === CREATE: create_node (blueprintPath as assetPath + nodeType + graphName) ===
  // Capture the nodeId for use in subsequent node operations.
  { scenario: 'CREATE: create_node', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'Sequence', graphName: 'EventGraph', posX: -240, posY: 120 }, expected: 'success|already exists', captureResult: { key: 'seqNodeId', fromField: 'nodeId' } },

  // === CREATE: create_node variants with specialized metadata ===
  { scenario: 'CREATE: create_node call function metadata', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'CallFunction', memberName: 'PrintString', memberClass: 'KismetSystemLibrary', graphName: 'EventGraph', posX: -40, posY: 120 }, expected: 'success|already exists' },
  { scenario: 'CREATE: create_node cast target class', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'Cast', targetClass: 'Actor', graphName: 'EventGraph', posX: 160, posY: 120 }, expected: 'success|already exists' },
  // A placed actor's reference belongs in a level Blueprint; in an Actor Blueprint it used to come out an unbound "Unknown" node.
  { scenario: 'CREATE: create_node refuses an actor reference outside a level Blueprint', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'K2Node_Literal', actorName: 'AnyPlacedActor', graphName: 'EventGraph', posX: 400, posY: 120 }, expected: 'error|INVALID_ARGUMENT' },
  // A setter of a read-only property used to be made and broke the next compile.
  { scenario: 'CREATE: create_node refuses a setter of a read-only property', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'VariableSet', memberName: 'PointLightComponent', memberClass: 'PointLight', graphName: 'EventGraph', posX: 400, posY: 320 }, expected: 'error|VARIABLE_NOT_ACCESSIBLE' },
  { scenario: 'CREATE: create_node call function by functionName', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'CallFunction', functionName: 'PrintString', memberClass: 'KismetSystemLibrary', graphName: 'EventGraph', posX: 1160, posY: 120 }, expected: 'success' },
  { scenario: 'CREATE: create_node pure cast', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'Cast', targetClass: 'Pawn', pure: true, graphName: 'EventGraph', posX: 1360, posY: 120 }, expected: 'success' },
  { scenario: 'CREATE: create_node MakeStruct with structPath has the struct pins', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'MakeStruct', structPath: '/Script/CoreUObject.Vector', graphName: 'EventGraph', posX: 1560, posY: 120 }, expected: 'success' },
  { scenario: 'ERROR: create_node MakeStruct without structPath is refused', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'MakeStruct', graphName: 'EventGraph', posX: 1760, posY: 120 }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'CREATE: create_node SwitchOnEnum with enumPath has a case per value', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'SwitchOnEnum', enumPath: '/Script/Engine.ECollisionChannel', graphName: 'EventGraph', posX: 1960, posY: 120 }, expected: 'success' },
  { scenario: 'ERROR: create_node SwitchOnEnum without enumPath is refused', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'SwitchOnEnum', graphName: 'EventGraph', posX: 2160, posY: 120 }, expected: 'error|INVALID_ARGUMENT' },
  { scenario: 'CREATE: create_node CallFunction takes the title the editor shows (GetActorTransform)', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'CallFunction', memberName: 'GetActorTransform', graphName: 'EventGraph', posX: 2360, posY: 120 }, expected: 'success' },
  { scenario: 'ADD: add_node cast with targetClass', toolName: 'manage_blueprint', arguments: { action: 'add_node', blueprintPath: BP_PATH, nodeType: 'Cast', targetClass: 'Actor', graphName: 'EventGraph', posX: 1560, posY: 120 }, expected: 'success' },
  // A timeline keyed first, then the node that plays it: the node shows a pin per track.
  { scenario: 'ADD: edit_timeline makes a keyed timeline', toolName: 'manage_blueprint', arguments: { action: 'edit_timeline', blueprintPath: BP_PATH, timelineName: 'TL_Test', length: 2, useLastKeyframe: false, autoPlay: false, loop: true, ignoreTimeDilation: false, tracks: [{ name: 'Alpha', type: 'float', interpolation: 'linear', keys: [{ time: 0, value: 0 }, { time: 2, value: 1 }] }, { name: 'Offset', type: 'vector', keys: [{ time: 0, value: { x: 0, y: 0, z: 0 } }, { time: 2, value: { x: 0, y: 0, z: 100 } }] }, { name: 'Tint', type: 'color', keys: [{ time: 0, value: { r: 1, g: 0, b: 0 } }] }, { name: 'Halfway', type: 'event', keys: [{ time: 1 }] }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.created', equals: true, label: 'the timeline was made' }, { path: 'structuredContent.result.tracks', length: 4, label: 'every track was added' }] },
  { scenario: 'CREATE: create_node Timeline plays the keyed timeline', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'Timeline', timelineName: 'TL_Test', graphName: 'EventGraph', posX: 0, posY: 1400 }, expected: 'success', captureResult: { key: 'timelineNodeId', fromField: 'nodeId' }, assertions: [{ path: 'structuredContent.result.timelineName', equals: 'TL_Test', label: 'bound to the named timeline' }] },
  { scenario: 'VERIFY: the Timeline node has a pin per track', toolName: 'manage_blueprint', arguments: { action: 'get_node_details', blueprintPath: BP_PATH, nodeGuid: '${captured:timelineNodeId}', graphName: 'EventGraph' }, expected: 'success', assertions: [{ path: 'structuredContent.result.pins', includesObject: { pinName: 'Alpha' }, label: 'float track pin' }, { path: 'structuredContent.result.pins', includesObject: { pinName: 'Halfway' }, label: 'event track pin' }] },
  { scenario: 'ADD: edit_timeline replaces the keys of a track', toolName: 'manage_blueprint', arguments: { action: 'edit_timeline', blueprintPath: BP_PATH, timelineName: 'TL_Test', tracks: [{ name: 'Alpha', keys: [{ time: 0, value: 1 }, { time: 1, value: 0.5 }, { time: 2, value: 0 }] }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.created', equals: false, label: 'the same timeline' }, { path: 'structuredContent.result.tracks.0.keys', equals: 3, label: 'the keys were replaced' }] },
  { scenario: 'ERROR: edit_timeline refuses a track that changes type', toolName: 'manage_blueprint', arguments: { action: 'edit_timeline', blueprintPath: BP_PATH, timelineName: 'TL_Test', tracks: [{ name: 'Alpha', type: 'event', keys: [{ time: 0 }] }] }, expected: 'error|TRACK_TYPE_MISMATCH' },
  { scenario: 'ERROR: edit_timeline refuses a key of the wrong shape', toolName: 'manage_blueprint', arguments: { action: 'edit_timeline', blueprintPath: BP_PATH, timelineName: 'TL_Test', tracks: [{ name: 'Lift', type: 'vector', keys: [{ time: 0, value: 5 }] }] }, expected: 'error|tracks[0].keys[0]' },
  { scenario: 'ERROR: create_node refuses a second node for one timeline', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'Timeline', timelineName: 'TL_Test', graphName: 'EventGraph', posX: 600, posY: 1400 }, expected: 'error|already has its node' },
  { scenario: 'ADD: add_node Timeline makes its timeline', toolName: 'manage_blueprint', arguments: { action: 'add_node', blueprintPath: BP_PATH, nodeType: 'Timeline', timelineName: 'TL_Added', graphName: 'EventGraph', posX: 1200, posY: 1400 }, expected: 'success', assertions: [{ path: 'structuredContent.result.timelineName', equals: 'TL_Added', label: 'a new timeline' }] },
  // An undone batch removes the Timeline node it made, but not a timeline that was there before it.
  { scenario: 'ADD: edit_timeline keys a timeline before a batch plays it', toolName: 'manage_blueprint', arguments: { action: 'edit_timeline', blueprintPath: BP_PATH, timelineName: 'TL_Kept', tracks: [{ name: 'Alpha', keys: [{ time: 0, value: 0 }, { time: 1, value: 1 }] }] }, expected: 'success' },
  { scenario: 'ERROR: a failed build_graph undoes its Timeline node', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [{ edit: 'create_node', id: 'kept', nodeType: 'Timeline', timelineName: 'TL_Kept', posX: 1800, posY: 1400 }, { edit: 'connect_pins', from: '$kept.NoSuchPin', to: '$kept.Play' }] }, expected: 'error|NoSuchPin' },
  { scenario: 'VERIFY: the timeline keeps its keys after the undone batch', toolName: 'manage_blueprint', arguments: { action: 'edit_timeline', blueprintPath: BP_PATH, timelineName: 'TL_Kept' }, expected: 'success', assertions: [{ path: 'structuredContent.result.created', equals: false, label: 'not made again' }, { path: 'structuredContent.result.tracks.0.keys', equals: 2, label: 'its keys are there' }] },

  // === Gameplay behaviours written into the Blueprint: each must compile on every engine version ===
  { scenario: 'ADD: add_behaviour health', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'health', blueprintPath: BP_PATH, maxHealth: 50, destroyOnDeath: true, invulnerableTime: 0.5 }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the Blueprint compiles with it' }, { path: 'structuredContent.result.functions.0', equals: 'Heal', label: 'Heal is callable' }] },
  { scenario: 'ADD: add_behaviour damage on touch', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'damage_on_touch', blueprintPath: BP_PATH, damage: 15 }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the Blueprint compiles with it' }] },
  { scenario: 'ADD: add_behaviour pickup that heals', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'pickup', blueprintPath: BP_PATH, heal: 25, soundPath: '/Engine/EditorSounds/Notifications/CompileSuccess_Cue' }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the Blueprint compiles with it' }] },
  { scenario: 'ADD: add_behaviour pickup that counts', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'pickup', blueprintPath: BP_PATH, collectorClass: BP_PATH, variableName: 'SpawnPower', amount: 2 }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the Blueprint compiles with it' }] },
  { scenario: 'ERROR: add_behaviour pickup takes heal or a collector, not both', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'pickup', blueprintPath: BP_PATH, heal: 5, collectorClass: BP_PATH, variableName: 'SpawnPower' }, expected: 'error|two kinds of pickup' },
  { scenario: 'ERROR: add_behaviour sprint needs a Character', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'sprint', blueprintPath: BP_PATH }, expected: 'error|Character' },
  { scenario: 'SETUP: an input mapping context for the sprint key', toolName: 'manage_networking', arguments: { action: 'configure_input', setting: 'create_mapping_context', name: `IMC_Behaviour_${ts}`, path: TEST_FOLDER }, expected: 'success|already exists' },
  { scenario: 'ADD: add_behaviour sprint on a Character', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'sprint', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, sprintSpeed: 900, key: 'LeftShift', mappingContextPath: `${TEST_FOLDER}/IMC_Behaviour_${ts}` }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the Character compiles with it' }] },
  { scenario: 'ADD: add_behaviour sprint again with its own action replaces the first', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'sprint', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, inputActionPath: `${TEST_FOLDER}/Input/IA_Sprint`, mappingContextPath: `${TEST_FOLDER}/IMC_Behaviour_${ts}` }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the Character still compiles' }] },
  { scenario: 'ADD: add_behaviour shoot on a Character', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'shoot', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, projectileClass: '/Script/Engine.Actor', projectileSpeed: 2500, fireRate: 240, key: 'LeftMouseButton', mappingContextPath: `${TEST_FOLDER}/IMC_Behaviour_${ts}` }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the Character compiles with the shot' }] },
  { scenario: 'ERROR: add_behaviour shoot needs an actor class to shoot', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'shoot', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, projectileClass: '/Script/Engine.StaticMesh' }, expected: 'error|not an actor class' },
  { scenario: 'ERROR: add_behaviour shoot needs a pawn', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'shoot', blueprintPath: BP_PATH, projectileClass: '/Script/Engine.Actor' }, expected: 'error|Pawn' },
  { scenario: 'ADD: add_behaviour chase makes a Character chase the player under an AI controller', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'chase', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, chaseRange: 1500, stopDistance: 120, usePathfinding: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the Character compiles with the chase' }, { path: 'structuredContent.result.functions.0', equals: 'ChaseUpdate', label: 'the chase runs from ChaseUpdate' }, { path: 'structuredContent.result.autoPossessAI', equals: 'PlacedInWorldOrSpawned', label: 'placed or spawned, an AI controller possesses it' }] },
  { scenario: 'ERROR: add_behaviour chase needs a pawn', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'chase', blueprintPath: BP_PATH }, expected: 'error|Pawn' },
  { scenario: 'ADD: add_behaviour controls makes a Character playable', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'controls', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, jumpKey: 'SpaceBar', look: true, mappingContextPath: `${TEST_FOLDER}/IMC_Behaviour_${ts}` }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the Character compiles with its controls' }, { path: 'structuredContent.result.keysMapped', equals: true, label: 'WASD, the sticks, the mouse and the jump key are mapped' }, { path: 'structuredContent.result.moveAction', includes: '/Input/IA_Move', label: 'IA_Move sits next to the Blueprint' }, { path: 'structuredContent.receipt.changes.0', equals: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, label: 'the receipt names the Blueprint as changed, then its input assets' }] },
  { scenario: 'ADD: add_behaviour controls again replaces only its own nodes', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'controls', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}` }, expected: 'success', assertions: [{ path: 'structuredContent.result.previousRemoved', equals: 17, label: 'the jump, walk and look nodes (3, 9 and 5) were replaced, never the input registration' }] },
  { scenario: 'ERROR: add_behaviour controls needs a Character', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'controls', blueprintPath: BP_PATH }, expected: 'error|Character' },
  { scenario: 'ADD: add_behaviour respawn brings a pawn back when it falls', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'respawn', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, fallHeight: 1500, checkpointTag: 'Checkpoint' }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the Character compiles with it' }, { path: 'structuredContent.result.respawnOnDeath', equals: false, label: 'no health behaviour yet, so only falling respawns it' }] },
  { scenario: 'ADD: add_behaviour health on the respawning Character', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'health', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, maxHealth: 100 }, expected: 'success' },
  { scenario: 'ADD: add_behaviour respawn again respawns it when it dies too', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'respawn', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}` }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'it still compiles' }, { path: 'structuredContent.result.respawnOnDeath', equals: true, label: 'dying respawns it at full health' }] },
  { scenario: 'ADD: add_behaviour goal ends the level when the player reaches it', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'goal', blueprintPath: BP_PATH, nextLevel: 'L_Next', delay: 1, soundPath: '/Engine/EditorSounds/Notifications/CompileSuccess_Cue' }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the goal compiles' }] },
  { scenario: 'ERROR: add_behaviour goal with a level path that does not exist', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'goal', blueprintPath: BP_PATH, nextLevel: '/Game/MCPTest/NoSuchLevel' }, expected: 'error|not a level' },
  { scenario: 'ADD: add_behaviour spawner keeps spawning an actor class', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'spawner', blueprintPath: BP_PATH, spawnClass: '/Script/Engine.Actor', interval: 2, maxAlive: 3, radius: 200 }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the spawner compiles' }] },
  { scenario: 'ERROR: add_behaviour spawner needs an actor class', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'spawner', blueprintPath: BP_PATH, spawnClass: '/Script/Engine.StaticMesh' }, expected: 'error|not an actor class' },
  { scenario: 'ADD: add_behaviour dash on a Character', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'dash', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, dashSpeed: 1600, cooldown: 0.5, key: 'Q', mappingContextPath: `${TEST_FOLDER}/IMC_Behaviour_${ts}` }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the Character compiles with its dash' }] },
  { scenario: 'ERROR: add_behaviour dash needs a Character', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'dash', blueprintPath: BP_PATH }, expected: 'error|Character' },
  { scenario: 'ADD: add_behaviour health_hud shows the Character health on screen', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'health_hud', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}` }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the Character compiles with its health bar' }, { path: 'structuredContent.result.widgetPath', equals: `${TEST_FOLDER}/UI/WBP_HealthBar`, label: 'the health bar widget is made next to it' }, { path: 'structuredContent.result.createdAssets.0', equals: `${TEST_FOLDER}/UI/WBP_HealthBar`, label: 'and listed as made' }, { path: 'structuredContent.receipt.changes.0', equals: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, label: 'the receipt names the Blueprint as changed' }, { path: 'structuredContent.receipt.changes.1', equals: `${TEST_FOLDER}/UI/WBP_HealthBar`, label: 'and the widget it made' }] },
  { scenario: 'ADD: add_behaviour health_hud again with the widget and bar named', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'health_hud', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, widgetPath: `${TEST_FOLDER}/UI/WBP_HealthBar`, barName: 'HealthBar_Bar' }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the re-run compiles' }] },
  { scenario: 'ERROR: add_behaviour health_hud refuses a bar the widget does not have', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'health_hud', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, widgetPath: `${TEST_FOLDER}/UI/WBP_HealthBar`, barName: 'NoSuchBar' }, expected: 'error|ProgressBar' },
  { scenario: 'ERROR: add_behaviour health_hud needs a pawn', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'health_hud', blueprintPath: BP_PATH }, expected: 'error|Pawn' },
  { scenario: 'ADD: add_variable Coins for a counter on screen', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, variableName: 'Coins', variableType: 'Int' }, expected: 'success' },
  { scenario: 'ADD: add_behaviour counter_hud shows the Coins on screen', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'counter_hud', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, variableName: 'Coins' }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the Character compiles with its counter' }, { path: 'structuredContent.result.widgetPath', equals: `${TEST_FOLDER}/UI/WBP_CoinsCounter`, label: 'the counter widget is made next to it' }, { path: 'structuredContent.receipt.changes.1', equals: `${TEST_FOLDER}/UI/WBP_CoinsCounter`, label: 'and the receipt names it as changed' }] },
  { scenario: 'ADD: add_behaviour counter_hud again with its widget, text and label named', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'counter_hud', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, variableName: 'Coins', widgetPath: `${TEST_FOLDER}/UI/WBP_CoinsCounter`, textName: 'CoinsCounter', label: 'Gold' }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the re-run compiles' }] },
  { scenario: 'ERROR: add_behaviour counter_hud needs a number variable', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'counter_hud', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, variableName: 'bIsDead' }, expected: 'error|not a number' },
  { scenario: 'ERROR: add_behaviour counter_hud needs a pawn', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'counter_hud', blueprintPath: BP_PATH, variableName: 'Coins' }, expected: 'error|Pawn' },
  { scenario: 'ADD: add_behaviour patrol takes over the chase the Character has', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'patrol', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}` }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the Character compiles with its patrol' }, { path: 'structuredContent.result.replaced.0', equals: 'Chase', label: 'its chase was removed first' }] },
  { scenario: 'ERROR: add_behaviour chase refuses a pawn that patrols', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'chase', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}` }, expected: 'error|patrols' },
  { scenario: 'ADD: add_behaviour patrol again with its own route', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'patrol', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}`, points: [[300, 0, 0], [300, 300, 0], [0, 0, 0]], waitTime: 0.5, chaseRange: 800, stopDistance: 120, usePathfinding: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the re-run compiles' }] },
  { scenario: 'ERROR: add_behaviour patrol needs a pawn', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'patrol', blueprintPath: BP_PATH }, expected: 'error|Pawn' },
  { scenario: 'ADD: add_behaviour launch_pad makes an actor launch Characters', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'launch_pad', blueprintPath: BP_PATH, launchVelocity: [0, 0, 1500], overrideXY: false }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the actor compiles with its launch' }] },
  { scenario: 'ADD: add_behaviour teleporter makes an actor move pawns to its target', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'teleporter', blueprintPath: BP_PATH, cooldown: 0.5 }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the actor compiles with its teleport' }, { path: 'structuredContent.result.functions.0', equals: 'HoldTeleport', label: 'HoldTeleport is callable' }] },
  { scenario: 'ADD: create a pawn for a game over', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_GameOverPawn_${ts}`, savePath: TEST_FOLDER, blueprintType: 'Pawn' }, expected: 'success' },
  { scenario: 'ADD: add_behaviour health on the game over pawn', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'health', blueprintPath: `${TEST_FOLDER}/BP_GameOverPawn_${ts}`, maxHealth: 50 }, expected: 'success' },
  { scenario: 'ADD: add_behaviour game_over shows a game over and restarts the level', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'game_over', blueprintPath: `${TEST_FOLDER}/BP_GameOverPawn_${ts}`, message: 'YOU DIED', restartDelay: 4 }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the pawn compiles with its game over' }, { path: 'structuredContent.result.widgetPath', equals: `${TEST_FOLDER}/UI/WBP_GameOver`, label: 'the game over screen is made next to it' }, { path: 'structuredContent.result.functions.0', equals: 'RestartLevel', label: 'RestartLevel is callable' }, { path: 'structuredContent.result.warningCount', equals: 0, label: 'no node was pruned off the exec chain (GetCurrentLevelName has exec pins)' }] },
  { scenario: 'ADD: add_behaviour game_over again with its screen and text named', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'game_over', blueprintPath: `${TEST_FOLDER}/BP_GameOverPawn_${ts}`, widgetPath: `${TEST_FOLDER}/UI/WBP_GameOver`, textName: 'GameOver_Text', restartDelay: 0 }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the re-run compiles' }] },
  { scenario: 'ERROR: add_behaviour game_over refuses a pawn that respawns when it dies', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'game_over', blueprintPath: `${TEST_FOLDER}/BP_CreateDotted_${ts}` }, expected: 'error|respawns' },
  { scenario: 'ADD: add_behaviour damage_on_touch with knockback', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'damage_on_touch', blueprintPath: BP_PATH, damage: 15, knockback: 600 }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the actor compiles with its knockback' }] },
  { scenario: 'ADD: add_behaviour countdown gives the pawn a time limit', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'countdown', blueprintPath: `${TEST_FOLDER}/BP_GameOverPawn_${ts}`, seconds: 30, killAtZero: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the pawn compiles with its countdown' }, { path: 'structuredContent.result.dispatchers.0', equals: 'OnTimeUp', label: 'OnTimeUp can be bound' }] },
  { scenario: 'ERROR: add_behaviour countdown needs at least a second', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'countdown', blueprintPath: `${TEST_FOLDER}/BP_GameOverPawn_${ts}`, seconds: 0 }, expected: 'error|seconds' },
  { scenario: 'ADD: add_behaviour turret makes an actor fire at the player', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'turret', blueprintPath: BP_PATH, projectileClass: '/Script/Engine.Actor', projectileSpeed: 2000, fireRate: 30, range: 1500 }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the actor compiles with its turret' }, { path: 'structuredContent.result.warningCount', equals: 0, label: 'every node is on the exec chain' }] },
  { scenario: 'ADD: add_behaviour turret again with another projectile class', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'turret', blueprintPath: BP_PATH, projectileClass: '/Script/Engine.StaticMeshActor' }, expected: 'success', assertions: [{ path: 'structuredContent.result.variables.0.status', equals: 'redefaulted', label: 'ProjectileClass takes the new class' }] },
  { scenario: 'ERROR: add_behaviour turret needs a range above 0', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'turret', blueprintPath: BP_PATH, projectileClass: '/Script/Engine.Actor', range: 0 }, expected: 'error|range' },
  { scenario: 'ERROR: add_behaviour turret needs an actor class to shoot', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'turret', blueprintPath: BP_PATH, projectileClass: '/Script/Engine.StaticMesh' }, expected: 'error|not an actor class' },
  { scenario: 'ADD: add_behaviour falling_platform makes an actor drop under a pawn', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'falling_platform', blueprintPath: BP_PATH, fallDelay: 0.8, respawnTime: 4 }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the actor compiles with its fall' }, { path: 'structuredContent.result.functions.0', equals: 'PlatformFall', label: 'PlatformFall is callable' }, { path: 'structuredContent.result.warningCount', equals: 0, label: 'every node is on the exec chain' }] },
  { scenario: 'ERROR: add_behaviour falling_platform needs a delay of 0 or more', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'falling_platform', blueprintPath: BP_PATH, fallDelay: -1 }, expected: 'error|fallDelay' },
  { scenario: 'ADD: add_behaviour moving_platform makes an actor travel a route', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'moving_platform', blueprintPath: BP_PATH, points: [[0, 0, 300], [0, 0, 0]], speed: 250, waitTime: 1.5 }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the actor compiles with its route' }, { path: 'structuredContent.result.warningCount', equals: 0, label: 'every node is on the exec chain' }] },
  { scenario: 'ERROR: add_behaviour moving_platform needs a speed above 0', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'moving_platform', blueprintPath: BP_PATH, speed: 0 }, expected: 'error|speed' },
  { scenario: 'ADD: add_behaviour stomp takes over the damage on touch', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'stomp', blueprintPath: BP_PATH, bounceSpeed: 700, sideDamage: 15 }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the actor compiles with its stomp' }, { path: 'structuredContent.result.replaced.0', equals: 'DamageOnTouch', label: 'the damage on touch is taken over' }, { path: 'structuredContent.result.warningCount', equals: 0, label: 'every node is on the exec chain' }] },
  { scenario: 'ERROR: add_behaviour stomp needs a bounce above 0', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'stomp', blueprintPath: BP_PATH, bounceSpeed: 0 }, expected: 'error|bounceSpeed' },
  { scenario: 'ADD: add_behaviour hint shows a message while the player is in the area', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'hint', blueprintPath: BP_PATH, message: 'Press Space to jump', hideAfter: 3 }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the actor compiles with its hint' }, { path: 'structuredContent.result.widgetPath', equals: `${TEST_FOLDER}/UI/WBP_Hint`, label: 'the hint widget is made next to it' }, { path: 'structuredContent.result.functions.0', equals: 'HideHint', label: 'HideHint is callable' }, { path: 'structuredContent.result.warningCount', equals: 0, label: 'every node is on the exec chain' }] },
  { scenario: 'ADD: add_behaviour hint again with its widget and text named', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'hint', blueprintPath: BP_PATH, message: 'Mind the gap', widgetPath: `${TEST_FOLDER}/UI/WBP_Hint`, textName: 'Hint_Text' }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the re-run compiles' }] },
  { scenario: 'ERROR: add_behaviour hint needs a message', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'hint', blueprintPath: BP_PATH, message: ' ' }, expected: 'error|message' },
  { scenario: 'ADD: add_behaviour explosive makes an actor with health blow up when it dies', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'explosive', blueprintPath: BP_PATH, explosionDamage: 80, explosionRadius: 500 }, expected: 'success', assertions: [{ path: 'structuredContent.result.compiled', equals: true, label: 'the actor compiles with its blast' }, { path: 'structuredContent.result.warningCount', equals: 0, label: 'every node is on the exec chain' }] },
  { scenario: 'ERROR: add_behaviour explosive needs a Niagara system for its effect', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'explosive', blueprintPath: BP_PATH, effectPath: '/Engine/BasicShapes/Cube' }, expected: 'error|Niagara' },
  { scenario: 'ERROR: add_behaviour respawn needs a pawn', toolName: 'manage_blueprint', arguments: { action: 'add_behaviour', behaviour: 'respawn', blueprintPath: BP_PATH }, expected: 'error|Pawn' },
  { scenario: 'CREATE: GetAllActorsOfClass takes targetClass as the class it looks for', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'CallFunction', memberName: 'GetAllActorsOfClass', memberClass: 'GameplayStatics', targetClass: 'Pawn', graphName: 'EventGraph', posX: 0, posY: 2400 }, expected: 'success' },
  { scenario: 'CREATE: create_node takes a function by the name the editor shows (AddLocalRotation is K2_AddLocalRotation)', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'CallFunction', memberName: 'AddLocalRotation', memberClass: 'SceneComponent', graphName: 'EventGraph', posX: 1440, posY: 2400 }, expected: 'success' },
  { scenario: 'CREATE: GetAllActorsOfClass with targetClass alone finds the library function', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'CallFunction', memberName: 'GetAllActorsOfClass', targetClass: 'Pawn', graphName: 'EventGraph', posX: 480, posY: 2400 }, expected: 'success' },
  { scenario: 'ERROR: GetAllActorsOfClass refuses a targetClass that is not an Actor', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'CallFunction', memberName: 'GetAllActorsOfClass', memberClass: 'GameplayStatics', targetClass: 'KismetMathLibrary', graphName: 'EventGraph', posX: 960, posY: 2400 }, expected: 'error|CLASS_NOT_FOUND' },
  { scenario: 'CREATE: create_node input axis event', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'InputAxisEvent', inputAxisName: 'MoveForward', graphName: 'EventGraph', posX: 360, posY: 120 }, expected: 'success|already exists' },
  { scenario: 'CREATE: create_node enhanced input actionPath', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'K2Node_EnhancedInputAction', actionPath: INPUT_ACTION_PATH, graphName: 'EventGraph', posX: 560, posY: 120 }, expected: 'success|already exists' },
  { scenario: 'CREATE: create_node enhanced input inputActionPath', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'K2Node_EnhancedInputAction', inputActionPath: INPUT_ACTION_PATH, graphName: 'EventGraph', posX: 760, posY: 120 }, expected: 'success|already exists' },
  { scenario: 'CREATE: create_node enhanced input inputActionAssetPath', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'K2Node_EnhancedInputAction', inputActionAssetPath: INPUT_ACTION_PATH, graphName: 'EventGraph', posX: 960, posY: 120 }, expected: 'success|already exists' },

  // === ADD: add_node (blueprintPath as assetPath + nodeType + nodeName) ===
  // Capture a real PrintString node for connect/default/delete operations.
  { scenario: 'ADD: add_node', toolName: 'manage_blueprint', arguments: { action: 'add_node', blueprintPath: BP_PATH, nodeType: 'PrintString', functionName: 'PrintString', nodeName: 'TestPrintNode', graphName: 'EventGraph' }, expected: 'success|already exists', captureResult: { key: 'printNodeId', fromField: 'nodeId' } },

  // === CONNECT: connect_pins (blueprintPath + sourceNode/targetNode + sourcePin/targetPin) ===
  // Uses real nodeIds captured from node creation. Sequence output pins are then_0/then_1.
  { scenario: 'CONNECT: connect_pins', toolName: 'manage_blueprint', arguments: { action: 'connect_pins', blueprintPath: BP_PATH, sourceNode: '${captured:seqNodeId}', targetNode: '${captured:printNodeId}', sourcePin: 'then_0', targetPin: 'execute', graphName: 'EventGraph' }, expected: 'success' },

  // === VERIFY: connected pin state ===
  { scenario: 'VERIFY: connected pin state', toolName: 'manage_blueprint', arguments: { action: 'get_pin_details', blueprintPath: BP_PATH, nodeGuid: '${captured:seqNodeId}', pinName: 'then_0', graphName: 'EventGraph' }, expected: 'success', assertions: [{ path: 'structuredContent.result.pins', includesObject: { pinName: 'then_0', linkedTo: { length: 1 } }, label: 'then_0 has one real graph link after connect_pins' }] },

  // === ACTION: break_pin_links (blueprintPath + nodeGuid + pinName) ===
  // Uses the real Sequence output pin that was just connected.
  { scenario: 'ACTION: break_pin_links', toolName: 'manage_blueprint', arguments: { action: 'break_pin_links', blueprintPath: BP_PATH, nodeGuid: '${captured:seqNodeId}', pinName: 'then_0', graphName: 'EventGraph' }, expected: 'success' },

  // === CONNECT: connect_pins using native field names ===
  { scenario: 'CONNECT: connect_pins via native fields', toolName: 'manage_blueprint', arguments: { action: 'connect_pins', blueprintPath: BP_PATH, fromNodeId: '${captured:seqNodeId}', fromPinName: 'then_0', toNodeId: '${captured:printNodeId}', toPinName: 'execute', graphName: 'EventGraph' }, expected: 'success' },

  // === VERIFY: native-field connected pin state ===
  { scenario: 'VERIFY: native-field connected pin state', toolName: 'manage_blueprint', arguments: { action: 'get_pin_details', blueprintPath: BP_PATH, nodeId: '${captured:seqNodeId}', pinName: 'then_0', graphName: 'EventGraph' }, expected: 'success', assertions: [{ path: 'structuredContent.result.pins', includesObject: { pinName: 'then_0', linkedTo: { length: 1 } }, label: 'then_0 has one real graph link after native-field connect_pins' }] },
  { scenario: 'INFO: get_node_details followExec walks the exec wires', toolName: 'manage_blueprint', arguments: { action: 'get_node_details', blueprintPath: BP_PATH, nodeGuid: '${captured:seqNodeId}', graphName: 'EventGraph', followExec: 3 }, expected: 'success', assertions: [{ path: 'structuredContent.result.chain', includesObject: { nodeTitle: 'Print String' }, label: 'the Sequence reaches Print String through then_0' }] },
  { scenario: 'ACTION: arrange_nodes moves a node beside what runs it', toolName: 'manage_blueprint', arguments: { action: 'arrange_nodes', blueprintPath: BP_PATH, graphName: 'EventGraph', nodeIds: ['${captured:printNodeId}'] }, expected: 'success', assertions: [{ path: 'structuredContent.result.moved', includesObject: { nodeTitle: 'Print String' }, label: 'the Print String moved beside the Sequence that runs it' }] },

  // === ACTION: break_pin_links via nodeId alias ===
  { scenario: 'ACTION: break_pin_links via nodeId', toolName: 'manage_blueprint', arguments: { action: 'break_pin_links', blueprintPath: BP_PATH, nodeId: '${captured:seqNodeId}', pinName: 'then_0', graphName: 'EventGraph' }, expected: 'success' },

  // === CONFIG: set_node_property (blueprintPath + nodeGuid + propertyName + propertyValue) ===
  // Uses the real nodeId captured from the first Sequence node.
  { scenario: 'CONFIG: set_node_property', toolName: 'manage_blueprint', arguments: { action: 'set_node_property', blueprintPath: BP_PATH, nodeGuid: '${captured:seqNodeId}', propertyName: 'Comment', propertyValue: 'Test comment', graphName: 'EventGraph' }, expected: 'success' },
  { scenario: 'VERIFY: set_node_property wrote the comment', toolName: 'manage_blueprint', arguments: { action: 'get_node_details', blueprintPath: BP_PATH, nodeGuid: '${captured:seqNodeId}', graphName: 'EventGraph' }, expected: 'success', assertions: [{ path: 'structuredContent.result.nodeComment', equals: 'Test comment', label: 'propertyValue reached the node' }] },
  { scenario: 'CONFIG: set_node_property numeric position', toolName: 'manage_blueprint', arguments: { action: 'set_node_property', blueprintPath: BP_PATH, nodeId: '${captured:seqNodeId}', propertyName: 'NodePosY', propertyValue: 160, graphName: 'EventGraph' }, expected: 'success' },
  { scenario: 'CREATE: a Math Expression node', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'MathExpression', graphName: 'EventGraph', posX: 1440, posY: 2400 }, expected: 'success', captureResult: { key: 'mathNodeId', fromField: 'nodeId' } },
  { scenario: 'CONFIG: an Expression names its pins and the member variables it reads instead', toolName: 'manage_blueprint', arguments: { action: 'set_node_property', blueprintPath: BP_PATH, nodeId: '${captured:mathNodeId}', propertyName: 'Expression', propertyValue: 'max(SpawnPower, 0) * Gain', graphName: 'EventGraph' }, expected: 'success', assertions: [{ path: 'structuredContent.result.inputPins', length: 1, label: 'only Gain became a pin' }, { path: 'structuredContent.result.boundToMembers', length: 1, label: 'SpawnPower reads the member variable' }] },
  { scenario: 'ERROR: an Expression calling FMax is refused naming the spelling it takes', toolName: 'manage_blueprint', arguments: { action: 'set_node_property', blueprintPath: BP_PATH, nodeId: '${captured:mathNodeId}', propertyName: 'Expression', propertyValue: 'FMax(Gain, 0)', graphName: 'EventGraph' }, expected: 'error|EXPRESSION_INVALID' },
  { scenario: 'ERROR: an Expression using a comparison as a number is refused before it is set', toolName: 'manage_blueprint', arguments: { action: 'set_node_property', blueprintPath: BP_PATH, nodeId: '${captured:mathNodeId}', propertyName: 'Expression', propertyValue: '(Gain > 0) * 2', graphName: 'EventGraph' }, expected: 'error|EXPRESSION_INVALID' },
  { scenario: 'ERROR: an Expression with && on number inputs and a unary ! is refused naming both fixes', toolName: 'manage_blueprint', arguments: { action: 'set_node_property', blueprintPath: BP_PATH, nodeId: '${captured:mathNodeId}', propertyName: 'Expression', propertyValue: 'Crossed && !Ready', graphName: 'EventGraph' }, expected: 'error|EXPRESSION_INVALID' },
  { scenario: 'CONFIG: && and || take unbracketed comparisons of arithmetic', toolName: 'manage_blueprint', arguments: { action: 'set_node_property', blueprintPath: BP_PATH, nodeId: '${captured:mathNodeId}', propertyName: 'Expression', propertyValue: 'Gain - SpawnPower < 0 || Gain * 2 >= 1', graphName: 'EventGraph' }, expected: 'success' },
  { scenario: 'ERROR: arithmetic as an operand of && is refused before it is set', toolName: 'manage_blueprint', arguments: { action: 'set_node_property', blueprintPath: BP_PATH, nodeId: '${captured:mathNodeId}', propertyName: 'Expression', propertyValue: 'Gain + 1 && Gain > 0', graphName: 'EventGraph' }, expected: 'error|EXPRESSION_INVALID' },

  // === CREATE: create_reroute_node (blueprintPath + graphName) ===
  { scenario: 'CREATE: create_reroute_node', toolName: 'manage_blueprint', arguments: { action: 'create_reroute_node', blueprintPath: BP_PATH, graphName: 'EventGraph' }, expected: 'success|already exists', captureResult: { key: 'rerouteNodeId', fromField: 'nodeId' } },

  // === INFO: get_node_details (blueprintPath + nodeGuid + graphName) ===
  // Uses the real PrintString input data pin.
  { scenario: 'INFO: get_node_details', toolName: 'manage_blueprint', arguments: { action: 'get_node_details', blueprintPath: BP_PATH, nodeGuid: '${captured:seqNodeId}', graphName: 'EventGraph' }, expected: 'success' },

  // === INFO: get_graph_details (blueprintPath + graphName) ===
  { scenario: 'INFO: get_graph_details', toolName: 'manage_blueprint', arguments: { action: 'get_graph_details', blueprintPath: BP_PATH, graphName: 'EventGraph' }, expected: 'success' },

  // === INFO: get_graph_details with includePins (blueprintPath + graphName + includePins) ===
  // Exercises the optional includePins flag so each node also carries pin/linkedTo flow in one call.
  { scenario: 'INFO: get_graph_details with includePins', toolName: 'manage_blueprint', arguments: { action: 'get_graph_details', blueprintPath: BP_PATH, graphName: 'EventGraph', includePins: true }, expected: 'success' },
  { scenario: 'INFO: get_graph_details filtered and paged', toolName: 'manage_blueprint', arguments: { action: 'get_graph_details', blueprintPath: BP_PATH, graphName: 'EventGraph', filter: 'Event', offset: 0, limit: 5 }, expected: 'success' },

  // === INFO: get_pin_details (blueprintPath + nodeGuid + graphName) ===
  // Uses the real nodeId captured from the first Sequence node.
  { scenario: 'INFO: get_pin_details', toolName: 'manage_blueprint', arguments: { action: 'get_pin_details', blueprintPath: BP_PATH, nodeGuid: '${captured:seqNodeId}', graphName: 'EventGraph' }, expected: 'success', assertions: [{ path: 'structuredContent.result.pins', includesObject: { pinName: 'then_0', linkedTo: { length: 0 } }, label: 'then_0 links are removed after break_pin_links' }] },

  // === INFO: list_node_types (no blueprint needed) ===
  { scenario: 'INFO: list_node_types', toolName: 'manage_blueprint', arguments: { action: 'list_node_types' }, expected: 'success' },
  { scenario: 'INFO: list_node_types filter finds an animation node class', toolName: 'manage_blueprint', arguments: { action: 'list_node_types', filter: 'TwoBoneIK' }, expected: 'success', assertions: [{ path: 'structuredContent.result.count', gte: 1, label: 'AnimGraphNode_TwoBoneIK is listed' }] },
  { scenario: 'INFO: find_functions finds a function by what it does', toolName: 'manage_blueprint', arguments: { action: 'find_functions', words: 'game time' }, expected: 'success', assertions: [{ path: 'structuredContent.result.functions.0.memberName', equals: 'GetGameTimeInSeconds', label: 'the game clock comes first' }, { path: 'structuredContent.result.functions.0.memberClass', equals: '/Script/Engine.KismetSystemLibrary', label: 'with the class create_node takes' }] },
  { scenario: 'INFO: find_functions lists what a class can call on itself', toolName: 'manage_blueprint', arguments: { action: 'find_functions', words: 'jump', memberClass: 'Character' }, expected: 'success', assertions: [{ path: 'structuredContent.result.functions.0.target', equals: 'Character', label: 'a Character function' }] },
  { scenario: 'ERROR: find_functions needs words', toolName: 'manage_blueprint', arguments: { action: 'find_functions', words: 'the a' }, expected: 'error|words' },

  // === CONFIG: set_pin_default_value (blueprintPath + nodeGuid + pinName + value) ===
  // Uses the real nodeId captured from the first Sequence node.
  { scenario: 'CONFIG: set_pin_default_value', toolName: 'manage_blueprint', arguments: { action: 'set_pin_default_value', blueprintPath: BP_PATH, nodeGuid: '${captured:printNodeId}', pinName: 'InString', propertyValue: 'test', graphName: 'EventGraph' }, expected: 'success', assertions: [{ path: 'structuredContent.value', equals: 'test', label: 'set_pin_default_value returns applied value' }] },

  // === VERIFY: pin default persisted on PrintString node ===
  { scenario: 'VERIFY: pin default persisted', toolName: 'manage_blueprint', arguments: { action: 'get_pin_details', blueprintPath: BP_PATH, nodeGuid: '${captured:printNodeId}', pinName: 'InString', graphName: 'EventGraph' }, expected: 'success', assertions: [{ path: 'structuredContent.result.pins', includesObject: { pinName: 'InString', defaultValue: 'test' }, label: 'PrintString InString pin default is persisted' }] },

  // === INFO: the graph filter also reads what a node's pins hold (a literal default, or a default object's path) ===
  { scenario: 'CONFIG: set_pin_default_value a literal the graph filter can find', toolName: 'manage_blueprint', arguments: { action: 'set_pin_default_value', blueprintPath: BP_PATH, nodeGuid: '${captured:printNodeId}', pinName: 'InString', propertyValue: 'PinFilterNeedle', graphName: 'EventGraph' }, expected: 'success' },
  { scenario: 'INFO: get_graph_details filter matches a pin default value, case-insensitively', toolName: 'manage_blueprint', arguments: { action: 'get_graph_details', blueprintPath: BP_PATH, graphName: 'EventGraph', filter: 'pinfilterneedle' }, expected: 'success', assertions: [{ path: 'structuredContent.result.totalCount', equals: 1, label: 'the node holding the literal is found by it' }] },
  { scenario: 'CONFIG: wire a variable getter into a Delay for the graph filter', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [{ edit: 'create_node', id: 'pwr', nodeType: 'VariableGet', memberName: 'SpawnPower' }, { edit: 'create_node', id: 'wait', nodeType: 'CallFunction', memberName: 'Delay' }, { edit: 'connect_pins', from: '$pwr.SpawnPower', to: '$wait.Duration' }] }, expected: 'success' },
  { scenario: 'INFO: get_graph_details filter finds a node wired from a variable of that name', toolName: 'manage_blueprint', arguments: { action: 'get_graph_details', blueprintPath: BP_PATH, graphName: 'EventGraph', filter: 'SpawnPower' }, expected: 'success', assertions: [{ path: 'structuredContent.result.nodes', includesObject: { nodeTitle: 'Delay' }, label: 'the Delay fed by SpawnPower' }] },
  { scenario: 'INFO: get_graph_details unusedOnly lists a call no exec wire reaches', toolName: 'manage_blueprint', arguments: { action: 'get_graph_details', blueprintPath: BP_PATH, graphName: 'EventGraph', filter: 'Delay', unusedOnly: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.nodes', includesObject: { nodeTitle: 'Delay' }, label: 'the unreached Delay' }] },

  // === BATCH: build_graph creates, wires and defaults nodes in one call, $id linking steps ===
  { scenario: 'BATCH: build_graph', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'create_node', id: 'delay', nodeType: 'CallFunction', memberName: 'Delay', pinDefaults: { Duration: 0.25 } },
    { edit: 'create_node', id: 'print', nodeType: 'CallFunction', memberName: 'PrintString', pinDefaults: { InString: 'batched' } },
    { edit: 'connect_pins', from: '$delay.then', to: '$print.execute' },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.result.succeeded', equals: 3, label: 'build_graph ran all three steps' }, { path: 'structuredContent.receipt.handles.0.kind', equals: 'asset', label: 'the receipt names the Blueprint the batch edited' }, { path: 'structuredContent.receipt.changes', length: 1, label: 'and lists it once as changed' }] },

  // === BATCH: a link between nodes of a function made in the same batch needs no graphName ===
  { scenario: 'BATCH: build_graph links a new function body without naming its graph', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'add_function', id: 'fn', functionName: 'SayReady' },
    { edit: 'create_node', id: 'say', graphName: 'SayReady', nodeType: 'CallFunction', memberName: 'PrintString', pinDefaults: { InString: 'ready' } },
    { edit: 'connect_pins', from: '$fn.then', to: '$say.execute' },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.result.succeeded', equals: 3, label: 'the link was made in the function graph' }] },

  // === BATCH: graphs the compiler accepts that misbehave are named with the fix, each on its own Blueprint ===
  { scenario: 'ADD: create a Blueprint for the stale value check', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_StaleCheck_${ts}`, savePath: TEST_FOLDER, parentClass: '/Script/Engine.Actor' }, expected: 'success' },
  { scenario: 'BATCH: build_graph warns about a value worked on twice', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: `${TEST_FOLDER}/BP_StaleCheck_${ts}`, graphName: 'EventGraph', operations: [
    { edit: 'add_variable', variableName: 'StaleCount', variableType: 'int' },
    { edit: 'create_node', id: 'go', nodeType: 'CustomEvent', customEventName: 'CountUp' },
    { edit: 'create_node', id: 'get', nodeType: 'VariableGet', memberName: 'StaleCount' },
    { edit: 'create_node', id: 'add', nodeType: 'CallFunction', memberName: 'Add_IntInt', memberClass: 'KismetMathLibrary', pinDefaults: { B: 1 } },
    { edit: 'create_node', id: 'set', nodeType: 'VariableSet', memberName: 'StaleCount' },
    { edit: 'create_node', id: 'show', nodeType: 'CallFunction', memberName: 'Conv_IntToString', memberClass: 'KismetStringLibrary' },
    { edit: 'create_node', id: 'print', nodeType: 'CallFunction', memberName: 'PrintString' },
    { edit: 'connect_pins', from: '$get.StaleCount', to: '$add.A' },
    { edit: 'connect_pins', from: '$add.ReturnValue', to: '$set.StaleCount' },
    { edit: 'connect_pins', from: '$go.then', to: '$set.execute' },
    { edit: 'connect_pins', from: '$add.ReturnValue', to: '$show.InInt' },
    { edit: 'connect_pins', from: '$show.ReturnValue', to: '$print.InString' },
    { edit: 'connect_pins', from: '$set.then', to: '$print.execute' },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.receipt.warnings.0', includes: 'Output_Get', label: 'the receipt names the Set node pin to read instead' }] },
  { scenario: 'ADD: create a Blueprint for the random value check', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_RandomCheck_${ts}`, savePath: TEST_FOLDER, parentClass: '/Script/Engine.Actor' }, expected: 'success' },
  { scenario: 'BATCH: build_graph warns about one random value read twice', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: `${TEST_FOLDER}/BP_RandomCheck_${ts}`, graphName: 'EventGraph', operations: [
    { edit: 'add_variable', variableName: 'Roll', variableType: 'int' },
    { edit: 'create_node', id: 'go', nodeType: 'CustomEvent', customEventName: 'RollDie' },
    { edit: 'create_node', id: 'pick', nodeType: 'CallFunction', memberName: 'RandomInteger', memberClass: 'KismetMathLibrary', pinDefaults: { Max: 6 } },
    { edit: 'create_node', id: 'set', nodeType: 'VariableSet', memberName: 'Roll' },
    { edit: 'create_node', id: 'show', nodeType: 'CallFunction', memberName: 'Conv_IntToString', memberClass: 'KismetStringLibrary' },
    { edit: 'create_node', id: 'print', nodeType: 'CallFunction', memberName: 'PrintString' },
    { edit: 'connect_pins', from: '$go.then', to: '$set.execute' },
    { edit: 'connect_pins', from: '$pick.ReturnValue', to: '$set.Roll' },
    { edit: 'connect_pins', from: '$pick.ReturnValue', to: '$show.InInt' },
    { edit: 'connect_pins', from: '$show.ReturnValue', to: '$print.InString' },
    { edit: 'connect_pins', from: '$set.then', to: '$print.execute' },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.receipt.warnings.0', includes: 'store it with a Set node first', label: 'the receipt says each reader gets its own random value' }] },
  { scenario: 'ADD: create a Blueprint for the timer name check', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_TimerCheck_${ts}`, savePath: TEST_FOLDER, parentClass: '/Script/Engine.Actor' }, expected: 'success' },
  { scenario: 'BATCH: build_graph warns about a timer that names nothing', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: `${TEST_FOLDER}/BP_TimerCheck_${ts}`, graphName: 'EventGraph', operations: [
    { edit: 'create_node', id: 'go', nodeType: 'CustomEvent', customEventName: 'StartTicking' },
    { edit: 'create_node', id: 'tick', nodeType: 'CustomEvent', customEventName: 'Tick_Once' },
    { edit: 'create_node', id: 'timer', nodeType: 'CallFunction', memberName: 'K2_SetTimer', memberClass: 'KismetSystemLibrary', pinDefaults: { FunctionName: 'TickOnce', Time: 1, bLooping: true } },
    { edit: 'connect_pins', from: '$go.then', to: '$timer.execute' },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.receipt.warnings.0', includes: 'never runs', label: 'the receipt says the misnamed timer never runs' }] },
  { scenario: 'ADD: create a Blueprint for the in-place change check', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_MapCheck_${ts}`, savePath: TEST_FOLDER, parentClass: '/Script/Engine.Actor' }, expected: 'success' },
  { scenario: 'BATCH: build_graph warns about a map changed in place and read again', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: `${TEST_FOLDER}/BP_MapCheck_${ts}`, graphName: 'EventGraph', operations: [
    { edit: 'add_variable', variableName: 'Items', variableType: 'Map<Name,Integer>' },
    { edit: 'create_node', id: 'evt', nodeType: 'CustomEvent', customEventName: 'AddItem', parameters: [{ name: 'ItemName', type: 'name' }, { name: 'Count', type: 'int' }] },
    { edit: 'create_node', id: 'items', nodeType: 'VariableGet', memberName: 'Items' },
    { edit: 'create_node', id: 'find', nodeType: 'CallFunction', memberName: 'Map_Find', memberClass: '/Script/Engine.BlueprintMapLibrary' },
    { edit: 'create_node', id: 'sum', nodeType: 'CallFunction', memberName: 'Add_IntInt', memberClass: 'KismetMathLibrary' },
    { edit: 'create_node', id: 'put', nodeType: 'CallFunction', memberName: 'Map_Add', memberClass: '/Script/Engine.BlueprintMapLibrary' },
    { edit: 'create_node', id: 'show', nodeType: 'CallFunction', memberName: 'Conv_IntToString', memberClass: 'KismetStringLibrary' },
    { edit: 'create_node', id: 'print', nodeType: 'CallFunction', memberName: 'PrintString' },
    { edit: 'connect_pins', from: '$items.Items', to: '$find.TargetMap' },
    { edit: 'connect_pins', from: '$evt.ItemName', to: '$find.Key' },
    { edit: 'connect_pins', from: '$find.Value', to: '$sum.A' },
    { edit: 'connect_pins', from: '$evt.Count', to: '$sum.B' },
    { edit: 'connect_pins', from: '$evt.then', to: '$put.execute' },
    { edit: 'connect_pins', from: '$items.Items', to: '$put.TargetMap' },
    { edit: 'connect_pins', from: '$evt.ItemName', to: '$put.Key' },
    { edit: 'connect_pins', from: '$sum.ReturnValue', to: '$put.Value' },
    { edit: 'connect_pins', from: '$sum.ReturnValue', to: '$show.InInt' },
    { edit: 'connect_pins', from: '$show.ReturnValue', to: '$print.InString' },
    { edit: 'connect_pins', from: '$put.then', to: '$print.execute' },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.receipt.warnings.0', includes: 'changes Items in place', label: 'the receipt names the node that changes the map in place' }] },
  { scenario: 'ADD: create a Blueprint for the loop wait check', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_LoopCheck_${ts}`, savePath: TEST_FOLDER, parentClass: '/Script/Engine.Actor' }, expected: 'success' },
  { scenario: 'BATCH: build_graph warns about a delay inside a loop body', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: `${TEST_FOLDER}/BP_LoopCheck_${ts}`, graphName: 'EventGraph', operations: [
    { edit: 'create_node', id: 'go', nodeType: 'CustomEvent', customEventName: 'SpawnWave' },
    { edit: 'create_node', id: 'loop', nodeType: 'ForLoop', pinDefaults: { FirstIndex: 1, LastIndex: 3 } },
    { edit: 'create_node', id: 'wait', nodeType: 'CallFunction', memberName: 'Delay', memberClass: 'KismetSystemLibrary', pinDefaults: { Duration: 1 } },
    { edit: 'create_node', id: 'print', nodeType: 'CallFunction', memberName: 'PrintString' },
    { edit: 'connect_pins', from: '$go.then', to: '$loop.execute' },
    { edit: 'connect_pins', from: '$loop.LoopBody', to: '$wait.execute' },
    { edit: 'connect_pins', from: '$wait.then', to: '$print.execute' },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.receipt.warnings.0', includes: 'waits inside the loop body', label: 'the receipt says the delay in the loop waits once' }] },
  { scenario: 'ADD: create a Blueprint for the every-frame timer check', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_TickCheck_${ts}`, savePath: TEST_FOLDER, parentClass: '/Script/Engine.Actor' }, expected: 'success' },
  { scenario: 'BATCH: build_graph warns about a timer set on every frame', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: `${TEST_FOLDER}/BP_TickCheck_${ts}`, graphName: 'EventGraph', operations: [
    { edit: 'create_node', id: 'tick', nodeType: 'Event', eventName: 'Tick' },
    { edit: 'create_node', id: 'done', nodeType: 'CustomEvent', customEventName: 'Fired' },
    { edit: 'create_node', id: 'timer', nodeType: 'CallFunction', memberName: 'K2_SetTimerDelegate', memberClass: 'KismetSystemLibrary', pinDefaults: { Time: 1 } },
    { edit: 'connect_pins', from: '$tick.then', to: '$timer.execute' },
    { edit: 'connect_pins', from: '$done.OutputDelegate', to: '$timer.Delegate' },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.receipt.warnings.0', includes: 'runs on every frame', label: 'the receipt says the timer starts over every frame' }] },
  // An event's own value read after a wait holds whatever its latest run wrote; a variable set before the wait does not.
  { scenario: 'ADD: create a Blueprint for the value-after-a-wait check', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_WaitCheck_${ts}`, savePath: TEST_FOLDER, parentClass: '/Script/Engine.Actor' }, expected: 'success' },
  { scenario: 'BATCH: build_graph warns about an overlap value read after a delay', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: `${TEST_FOLDER}/BP_WaitCheck_${ts}`, graphName: 'EventGraph', operations: [
    { edit: 'create_node', id: 'hit', nodeType: 'Event', eventName: 'ActorBeginOverlap' },
    { edit: 'create_node', id: 'wait', nodeType: 'CallFunction', memberName: 'Delay', memberClass: 'KismetSystemLibrary', pinDefaults: { Duration: 1 } },
    { edit: 'create_node', id: 'kill', nodeType: 'CallFunction', memberName: 'K2_DestroyActor', memberClass: 'Actor' },
    { edit: 'connect_pins', from: '$hit.then', to: '$wait.execute' },
    { edit: 'connect_pins', from: '$wait.then', to: '$kill.execute' },
    { edit: 'connect_pins', from: '$hit.OtherActor', to: '$kill.self' },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.receipt.warnings.0', includes: 'after the wait', label: 'the receipt says the overlapping actor is read after the wait' }] },
  { scenario: 'ADD: create a Blueprint that stores the value before the wait', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_WaitStore_${ts}`, savePath: TEST_FOLDER, parentClass: '/Script/Engine.Actor' }, expected: 'success' },
  { scenario: 'BATCH: build_graph warns about a variable every run sets before the delay', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: `${TEST_FOLDER}/BP_WaitStore_${ts}`, graphName: 'EventGraph', operations: [
    { edit: 'add_variable', variableName: 'Target', variableType: 'Actor' },
    { edit: 'create_node', id: 'hit', nodeType: 'Event', eventName: 'ActorBeginOverlap' },
    { edit: 'create_node', id: 'keep', nodeType: 'VariableSet', memberName: 'Target' },
    { edit: 'create_node', id: 'wait', nodeType: 'CallFunction', memberName: 'Delay', memberClass: 'KismetSystemLibrary', pinDefaults: { Duration: 1 } },
    { edit: 'create_node', id: 'target', nodeType: 'VariableGet', memberName: 'Target' },
    { edit: 'create_node', id: 'kill', nodeType: 'CallFunction', memberName: 'K2_DestroyActor', memberClass: 'Actor' },
    { edit: 'connect_pins', from: '$hit.then', to: '$keep.execute' },
    { edit: 'connect_pins', from: '$hit.OtherActor', to: '$keep.Target' },
    { edit: 'connect_pins', from: '$keep.then', to: '$wait.execute' },
    { edit: 'connect_pins', from: '$wait.then', to: '$kill.execute' },
    { edit: 'connect_pins', from: '$target.Target', to: '$kill.self' },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.receipt.warnings.0', includes: 'reads Target after the wait', label: 'a variable every run sets is written again by the next run too' }] },
  { scenario: 'ADD: create a Blueprint that writes the value only in the run that starts the wait', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_WaitGate_${ts}`, savePath: TEST_FOLDER, parentClass: '/Script/Engine.Actor' }, expected: 'success' },
  { scenario: 'BATCH: build_graph does not warn when a flag keeps later runs from writing the value', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: `${TEST_FOLDER}/BP_WaitGate_${ts}`, graphName: 'EventGraph', operations: [
    { edit: 'add_variable', variableName: 'Target', variableType: 'Actor' },
    { edit: 'add_variable', variableName: 'bBusy', variableType: 'bool' },
    { edit: 'create_node', id: 'hit', nodeType: 'Event', eventName: 'ActorBeginOverlap' },
    { edit: 'create_node', id: 'busy', nodeType: 'VariableGet', memberName: 'bBusy' },
    { edit: 'create_node', id: 'gate', nodeType: 'Branch' },
    { edit: 'create_node', id: 'lock', nodeType: 'VariableSet', memberName: 'bBusy', pinDefaults: { bBusy: true } },
    { edit: 'create_node', id: 'keep', nodeType: 'VariableSet', memberName: 'Target' },
    { edit: 'create_node', id: 'wait', nodeType: 'CallFunction', memberName: 'Delay', memberClass: 'KismetSystemLibrary', pinDefaults: { Duration: 1 } },
    { edit: 'create_node', id: 'target', nodeType: 'VariableGet', memberName: 'Target' },
    { edit: 'create_node', id: 'kill', nodeType: 'CallFunction', memberName: 'K2_DestroyActor', memberClass: 'Actor' },
    { edit: 'connect_pins', from: '$hit.then', to: '$gate.execute' },
    { edit: 'connect_pins', from: '$busy.bBusy', to: '$gate.Condition' },
    { edit: 'connect_pins', from: '$gate.else', to: '$lock.execute' },
    { edit: 'connect_pins', from: '$lock.then', to: '$keep.execute' },
    { edit: 'connect_pins', from: '$hit.OtherActor', to: '$keep.Target' },
    { edit: 'connect_pins', from: '$keep.then', to: '$wait.execute' },
    { edit: 'connect_pins', from: '$wait.then', to: '$kill.execute' },
    { edit: 'connect_pins', from: '$target.Target', to: '$kill.self' },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.receipt.warnings', exists: false, label: 'only the run that starts the wait writes Target, so nothing is raised' }] },
  // A Set wired to a spawned copy writes the copy's variable: reading this one's after it is no second step.
  { scenario: 'ADD: create a Blueprint that copies a value into a spawned copy', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_CopyCheck_${ts}`, savePath: TEST_FOLDER, parentClass: '/Script/Engine.Actor' }, expected: 'success' },
  { scenario: 'BATCH: build_graph does not warn when the Set writes a spawned copy', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: `${TEST_FOLDER}/BP_CopyCheck_${ts}`, graphName: 'EventGraph', operations: [
    { edit: 'add_variable', variableName: 'HomeX', variableType: 'int' },
    { edit: 'create_node', id: 'go', nodeType: 'CustomEvent', customEventName: 'Split' },
    { edit: 'create_node', id: 'where', nodeType: 'CallFunction', memberName: 'GetTransform', memberClass: 'Actor' },
    { edit: 'create_node', id: 'spawn', nodeType: 'SpawnActorFromClass', targetClass: `${TEST_FOLDER}/BP_CopyCheck_${ts}.BP_CopyCheck_${ts}_C` },
    { edit: 'create_node', id: 'get', nodeType: 'VariableGet', memberName: 'HomeX' },
    { edit: 'create_node', id: 'copy', nodeType: 'VariableSet', memberName: 'HomeX' },
    { edit: 'create_node', id: 'show', nodeType: 'CallFunction', memberName: 'Conv_IntToString', memberClass: 'KismetStringLibrary' },
    { edit: 'create_node', id: 'print', nodeType: 'CallFunction', memberName: 'PrintString' },
    { edit: 'connect_pins', from: '$go.then', to: '$spawn.execute' },
    { edit: 'connect_pins', from: '$where.ReturnValue', to: '$spawn.SpawnTransform' },
    { edit: 'connect_pins', from: '$spawn.then', to: '$copy.execute' },
    { edit: 'connect_pins', from: '$spawn.ReturnValue', to: '$copy.self' },
    { edit: 'connect_pins', from: '$get.HomeX', to: '$copy.HomeX' },
    { edit: 'connect_pins', from: '$get.HomeX', to: '$show.InInt' },
    { edit: 'connect_pins', from: '$show.ReturnValue', to: '$print.InString' },
    { edit: 'connect_pins', from: '$copy.then', to: '$print.execute' },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.receipt.warnings', exists: false, label: "the Set writes the copy's HomeX, so the Print reads this one's unchanged" }] },
  // An audit: every Blueprint under the folder compiled in one call, the flagged ones listed with their diagnostics.
  { scenario: 'ACTION: compile a folder lists the Blueprints the checks flag', toolName: 'manage_blueprint', arguments: { action: 'compile', folder: TEST_FOLDER }, expected: 'success', assertions: [{ path: 'structuredContent.result.flaggedCount', gte: 3, label: 'the loop, timer and wait checks flag their Blueprints' }, { path: 'structuredContent.result.compiledCount', gte: 5, label: 'every Blueprint under the folder is compiled' }] },

  // === BATCH: "$entry" addresses the Construction Script's entry node without a lookup ===
  { scenario: 'BATCH: build_graph from the construction script entry', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'UserConstructionScript', operations: [
    { edit: 'create_node', id: 'print', nodeType: 'CallFunction', memberName: 'PrintString', pinDefaults: { InString: 'constructed' } },
    { edit: 'connect_pins', from: '$entry.then', to: '$print.execute' },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.result.succeeded', equals: 2, label: 'the entry node resolved' }] },

  // === BATCH: a create step's expression gives a Math Expression its formula and pins, before its pinDefaults ===
  { scenario: 'BATCH: build_graph creates a Math Expression with its formula and an input default', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'create_node', id: 'blend', nodeType: 'MathExpression', expression: 'Low + (High - Low) * Alpha', pinDefaults: { Alpha: 0.5 } },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.result.results.0.inputPins', length: 3, label: 'the formula made its three input pins' }] },
  { scenario: 'ERROR: build_graph refuses a create step expression that does not parse', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'create_node', nodeType: 'MathExpression', expression: 'FMax(Gain, 0)' },
  ] }, expected: 'error|EXPRESSION_INVALID' },

  // === BATCH: a step the batch does not run is refused before any step, not after the ones before it applied ===
  { scenario: 'ERROR: build_graph refuses a last arrange_nodes step before running any', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'create_node', id: 'unrun', nodeType: 'CallFunction', memberName: 'PrintString' },
    { edit: 'arrange_nodes' },
  ] }, expected: 'error|INVALID_OPERATION' },

  // === BATCH: a step whose explicit posX/posY overlaps a node is moved to a free slot and says so, instead of stopping the batch ===
  { scenario: 'BATCH: build_graph moves a create step off an overlapping position', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'create_node', id: 'first', nodeType: 'CallFunction', memberName: 'PrintString', posX: 4000, posY: 4000 },
    { edit: 'create_node', id: 'second', nodeType: 'CallFunction', memberName: 'PrintString', posX: 4000, posY: 4000 },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.result.succeeded', equals: 2, label: 'both steps ran' }, { path: 'structuredContent.result.results.1.placementWarning', includes: 'was placed at', label: 'the second step names where it went' }] },

  // === BATCH: a compile names a sound played at a location with no attenuation, heard at any distance ===
  { scenario: 'Setup: create a sound probe blueprint', toolName: 'manage_blueprint', arguments: { action: 'create_blueprint', name: SOUND_BP_NAME, savePath: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists' },
  { scenario: 'BATCH: a compile warns about a sound played at a location with no attenuation', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: `${TEST_FOLDER}/${SOUND_BP_NAME}`, graphName: 'EventGraph', operations: [
    { edit: 'create_node', id: 'sound', nodeType: 'CallFunction', memberName: 'PlaySoundAtLocation', pinDefaults: { Sound: ENGINE_SOUND } },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.result.diagnostics.0.message', includes: 'no attenuation', label: 'the compile names the sound heard everywhere' }] },

  // === QUERY: find_text reads what the assets contain (search_assets matches names only) ===
  { scenario: 'QUERY: find_text finds a graph literal', toolName: 'manage_asset', arguments: { action: 'find_text', searchText: 'CONSTRUCTED', packagePaths: [TEST_FOLDER], includeLevel: false, limit: 5 }, expected: 'success', assertions: [{ path: 'structuredContent.result.matches', includesObject: { field: 'InString', text: 'constructed' }, label: 'PrintString literal found' }] },
  { scenario: 'QUERY: find_text honours caseSensitive', toolName: 'manage_asset', arguments: { action: 'find_text', searchText: 'CONSTRUCTED', packagePaths: [TEST_FOLDER], caseSensitive: true, includeLevel: true }, expected: 'success', assertions: [{ path: 'structuredContent.result.matchCount', equals: 0, label: 'no upper-case literal' }] },

  // === BATCH: member steps. A function is declared, filled and called in one batch ===
  // "$fn" is the function's entry node and "$fn_return" its return node.
  { scenario: 'BATCH: build_graph declares a pure function, fills its body and calls it', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'add_function', id: 'fn', functionName: 'BatchPassThrough', inputs: [{ name: 'Value', type: 'Float' }], outputs: [{ name: 'Result', type: 'Float' }], pure: true },
    { edit: 'connect_pins', graphName: 'BatchPassThrough', from: '$fn.Value', to: '$fn_return.Result' },
    { edit: 'create_node', id: 'call', nodeType: 'CallFunction', memberName: 'BatchPassThrough' },
  ] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.succeeded', equals: 3, label: 'all three steps ran' },
    { path: 'structuredContent.result.compiled', equals: true, label: 'the Blueprint compiles with the batch-built function' },
    { path: 'structuredContent.result.results', length: 2, label: 'the plain connect is counted in succeeded, not listed' },
    { path: 'structuredContent.result.results.1.pins', notIncludes: 'execute', label: 'a pure function is called without exec pins' },
  ] },
  { scenario: 'BATCH: build_graph declares an event dispatcher and calls it', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'add_event_dispatcher', dispatcherName: 'OnBatchPing', parameters: [{ name: 'Count', type: 'Int' }] },
    { edit: 'create_node', id: 'ping', nodeType: 'CallDelegate', memberName: 'OnBatchPing' },
  ] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.compiled', equals: true, label: 'the Blueprint compiles with the new dispatcher' },
    { path: 'structuredContent.result.results.0.pins', includes: 'Count', label: 'the call carries the dispatcher parameter (the dispatcher step itself is only counted)' },
  ] },
  // Bind Event to a dispatcher: the custom event it wires is "$id_event" and runs when the dispatcher fires.
  { scenario: 'BATCH: build_graph binds an event to a dispatcher', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'create_node', id: 'bindPing', nodeType: 'BindEvent', memberName: 'OnBatchPing', eventName: 'OnBatchPinged' },
    { edit: 'create_node', id: 'pingSay', nodeType: 'CallFunction', memberName: 'PrintString' },
    { edit: 'connect_pins', from: '$bindPing_event.then', to: '$pingSay.execute' },
  ] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.compiled', equals: true, label: 'the Blueprint compiles with the bound event' },
    { path: 'structuredContent.result.results.0.eventName', equals: 'OnBatchPinged', label: 'the custom event takes the name asked for' },
    { path: 'structuredContent.result.results.0.pins', includes: 'Delegate', label: 'the step made the Bind Event node' },
  ] },
  { scenario: 'ERROR: build_graph refuses a Bind Event on a dispatcher nothing declares', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, operations: [
    { edit: 'create_node', nodeType: 'BindEvent', memberName: 'OnNoSuchDispatcher' },
  ] }, expected: 'error|DISPATCHER_NOT_FOUND' },
  // A second return node mirrors the first one's outputs, and its pins take defaults.
  { scenario: 'BATCH: build_graph gives a function a second return node', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'add_function', id: 'find', functionName: 'BatchFindSomething', outputs: [{ name: 'Found', type: 'Bool' }] },
    { edit: 'create_node', id: 'fail', graphName: 'BatchFindSomething', nodeType: 'FunctionResult', pinDefaults: { Found: false } },
    { edit: 'create_node', id: 'branch', graphName: 'BatchFindSomething', nodeType: 'Branch' },
    { edit: 'connect_pins', graphName: 'BatchFindSomething', from: '$find.then', to: '$branch.execute' },
    { edit: 'connect_pins', graphName: 'BatchFindSomething', from: '$branch.then', to: '$find_return.execute' },
    { edit: 'connect_pins', graphName: 'BatchFindSomething', from: '$branch.else', to: '$fail.execute' },
  ] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.succeeded', equals: 6, label: 'all six steps ran' },
    { path: 'structuredContent.result.compiled', equals: true, label: 'a function with two return nodes compiles' },
    { path: 'structuredContent.result.results.1.pins', includes: 'Found', label: 'the second return node has the declared output' },
  ] },
  { scenario: 'BATCH: build_graph places an async task node and an interface message', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'create_node', id: 'load', nodeType: 'AsyncTask', memberClass: 'AsyncActionHandleSaveGame', memberName: 'AsyncLoadGameFromSlot' },
    { edit: 'create_node', id: 'tagged', nodeType: 'Message', memberClass: 'GameplayTagAssetInterface', memberName: 'HasMatchingGameplayTag' },
  ] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.results.0.pins', includes: 'Completed', label: 'the async node has its delegate exec pin' },
    { path: 'structuredContent.result.results.1.pins', includes: 'TagToCheck', label: 'the message node has the interface function parameter' },
    { path: 'structuredContent.result.compiled', equals: true, label: 'the Blueprint compiles with both nodes' },
  ] },
  // "$ev" is the custom event node itself, not a call node with the same title.
  { scenario: 'BATCH: build_graph declares a custom event and wires from it', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'add_event', id: 'ev', customEventName: 'BatchCustomEvent' },
    { edit: 'create_node', id: 'print', nodeType: 'CallFunction', memberName: 'PrintString', pinDefaults: { InString: 'custom event' } },
    { edit: 'connect_pins', from: '$ev.then', to: '$print.execute' },
  ] }, expected: 'success', captureResult: { key: 'batchEventId', fromField: 'result.nodeIds.ev' }, assertions: [
    { path: 'structuredContent.result.succeeded', equals: 3, label: 'all three steps ran' },
    { path: 'structuredContent.result.compiled', equals: true, label: 'the Blueprint compiles with the wired custom event' },
  ] },
  { scenario: 'VERIFY: the add_event step id names the custom event node', toolName: 'manage_blueprint', arguments: { action: 'get_node_details', blueprintPath: BP_PATH, nodeGuid: '${captured:batchEventId}', graphName: 'EventGraph' }, expected: 'success', assertions: [{ path: 'structuredContent.result.nodeName', includes: 'K2Node_CustomEvent', label: 'nodeIds.ev is a K2Node_CustomEvent' }] },
  // add_function and add_event_dispatcher also take their name as memberName; a later step can call it.
  { scenario: 'BATCH: build_graph declares a function under memberName and calls it', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'add_function', memberName: 'BatchAliasFunction' },
    { edit: 'create_node', nodeType: 'CallFunction', memberName: 'BatchAliasFunction' },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.result.succeeded', equals: 2, label: 'the pre-check saw the memberName declaration' }] },
  { scenario: 'ERROR: build_graph refuses an async task in a function graph', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'create_node', graphName: 'BatchPassThrough', nodeType: 'AsyncTask', memberClass: 'AsyncActionHandleSaveGame', memberName: 'AsyncLoadGameFromSlot' },
  ] }, expected: 'error|LATENT_NODE_IN_FUNCTION' },
  // The pre-check holds these to the same rules as node creation, so nothing runs.
  { scenario: 'ERROR: build_graph refuses a message on a class that is not an interface', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'create_node', nodeType: 'Message', memberClass: 'Actor', memberName: 'K2_DestroyActor' },
  ] }, expected: 'error|FUNCTION_NOT_FOUND' },
  { scenario: 'ERROR: build_graph refuses an async task whose factory is not static', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'create_node', nodeType: 'AsyncTask', memberClass: 'Actor', memberName: 'K2_DestroyActor' },
  ] }, expected: 'error|FUNCTION_NOT_FOUND' },
  { scenario: 'ERROR: build_graph add_variable with a default its type cannot take', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'add_variable', variableName: 'BatchBadDefault', variableType: 'Int', defaultValue: 'NotANumberAtAll' },
  ] }, expected: 'error|DEFAULT_NOT_APPLIED' },
  { scenario: 'ERROR: build_graph refuses pinDefaults on a member step', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, operations: [
    { edit: 'add_function', functionName: 'NeverMade', pinDefaults: { Value: 1 } },
  ] }, expected: 'error|INVALID_OPERATION' },
  { scenario: 'ERROR: build_graph refuses a dispatcher call nothing declares', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, operations: [
    { edit: 'create_node', nodeType: 'CallDelegate', memberName: 'OnNoSuchDispatcher' },
  ] }, expected: 'error|DISPATCHER_NOT_FOUND' },
  // The failing step ran in a function graph; it must take its node with it there too.
  { scenario: 'ERROR: build_graph whose last step fails in a function graph', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'add_variable', variableName: 'BatchStopFlag', variableType: 'Bool' },
    { edit: 'create_node', graphName: 'BatchPassThrough', nodeType: 'CallFunction', memberName: 'PrintString', pinDefaults: { NoSuchPin: 'x' } },
  ] }, expected: 'error|PIN_DEFAULT_FAILED' },
  { scenario: 'VERIFY: the failed step left no node behind', toolName: 'manage_blueprint', arguments: { action: 'get_graph_details', blueprintPath: BP_PATH, graphName: 'BatchPassThrough', filter: 'Print' }, expected: 'success', assertions: [{ path: 'structuredContent.result.totalCount', equals: 0, label: 'the PrintString node of the failed step is gone' }] },
  // A failed batch is all or nothing: the add_variable step before the failing one is undone too.
  { scenario: 'VERIFY: the failed batch took back the variable its first step added', toolName: 'manage_blueprint', arguments: { action: 'get_blueprint', blueprintPath: BP_PATH, propertyName: 'BatchStopFlag' }, expected: 'error|PROPERTY_NOT_FOUND' },

  // === NODE: create_node CustomEvent with typed parameters ===
  { scenario: 'NODE: create_node custom event with parameters', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, graphName: 'EventGraph', nodeType: 'CustomEvent', eventName: 'AddScore', parameters: [{ name: 'Points', type: 'int' }], posX: 2400, posY: 1200 }, expected: 'success' },
  { scenario: 'ERROR: create_node custom event refuses a type it cannot build', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, graphName: 'EventGraph', nodeType: 'CustomEvent', eventName: 'AddScore', parameters: [{ name: 'Points', type: 'flaot' }], posX: 2400, posY: 1200 }, expected: 'error|TYPE_RESOLUTION_FAILED' },
  { scenario: 'VERIFY: the refused custom event kept the one before it', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, graphName: 'EventGraph', nodeType: 'CustomEvent', eventName: 'AddScore', posX: 2400, posY: 1200 }, expected: 'success', assertions: [{ path: 'structuredContent.result.details.reusedExistingNode', equals: true, label: 'AddScore still there' }] },
  { scenario: 'NODE: create_node custom event with byte and object parameters', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, graphName: 'EventGraph', nodeType: 'CustomEvent', eventName: 'TakeFlags', parameters: [{ name: 'Flags', type: 'byte' }, { name: 'Source', type: 'object' }], posX: 2400, posY: 1500 }, expected: 'success', captureResult: { key: 'takeFlagsId', fromField: 'result.nodeGuid' } },
  { scenario: 'VERIFY: the byte and object parameters are pins', toolName: 'manage_blueprint', arguments: { action: 'get_node_details', blueprintPath: BP_PATH, nodeGuid: '${captured:takeFlagsId}', graphName: 'EventGraph' }, expected: 'success', assertions: [{ path: 'structuredContent.result.pins', includesObject: { pinName: 'Flags' }, label: 'byte pin' }, { path: 'structuredContent.result.pins', includesObject: { pinName: 'Source' }, label: 'object pin' }] },

  // === DELETE: delete_node (blueprintPath + nodeGuid) ===
  // Delete the PrintString node after all pin operations have used it.
  { scenario: 'DELETE: delete_node', toolName: 'manage_blueprint', arguments: { action: 'delete_node', blueprintPath: BP_PATH, nodeGuid: '${captured:printNodeId}', graphName: 'EventGraph' }, expected: 'success' },

  // === STRUCT MAKE/BREAK NODES (issue #510) ===
  { scenario: 'STRUCT NODE: create_struct for node test', toolName: 'manage_asset', arguments: { action: 'create_struct', name: `S_MCP_Node_${ts}`, path: TEST_FOLDER, save: true }, expected: 'success', captureResult: { key: 'nodeStructPath', fromField: 'result.assetPath' } },
  { scenario: 'STRUCT NODE: add_struct_member', toolName: 'manage_asset', arguments: { action: 'add_struct_member', structPath: '${captured:nodeStructPath}', memberName: 'Amount', memberType: 'Float', save: true }, expected: 'success' },
  { scenario: 'STRUCT NODE: make_struct node', toolName: 'manage_blueprint', arguments: { action: 'create_struct_make_break_nodes', blueprintPath: BP_PATH, structPath: '${captured:nodeStructPath}', nodeType: 'make' }, expected: 'success', captureResult: { key: 'makeNodeId', fromField: 'result.nodeGuid' }, assertions: [{ path: 'structuredContent.result.nodeType', equals: 'make', label: 'make node reported' }] },
  { scenario: 'STRUCT NODE: break_struct node', toolName: 'manage_blueprint', arguments: { action: 'create_struct_make_break_nodes', blueprintPath: BP_PATH, structPath: '${captured:nodeStructPath}', nodeType: 'break' }, expected: 'success', captureResult: { key: 'breakNodeId', fromField: 'result.nodeGuid' }, assertions: [{ path: 'structuredContent.result.nodeType', equals: 'break', label: 'break node reported' }] },
  { scenario: 'STRUCT NODE: delete make and break nodes in one call', toolName: 'manage_blueprint', arguments: { action: 'delete_node', blueprintPath: BP_PATH, nodeIds: ['${captured:makeNodeId}', '${captured:breakNodeId}'], graphName: 'EventGraph' }, expected: 'success', assertions: [{ path: 'structuredContent.result.removedCount', equals: 2, label: 'both nodes removed in one call' }] },
  { scenario: 'STRUCT NODE: delete refuses a list with an unknown id', toolName: 'manage_blueprint', arguments: { action: 'delete_node', blueprintPath: BP_PATH, nodeIds: ['${captured:printNodeId}', 'NoSuchNode_Zz'], graphName: 'EventGraph' }, expected: 'error|NODE_NOT_FOUND' },

  // === CLEANUP ===
  { scenario: 'Cleanup: delete test blueprint', toolName: 'manage_asset', arguments: { action: 'delete', path: BP_PATH, force: true }, expected: 'success|not found' },
];

// === WIDGET AUTHORING ACTIONS ===
{
  const TEST_FOLDER = '/Game/MCPTest/AuthoringAssets';
  const ts = Date.now();
  const WIDGET_NAME = `WBP_WidgetAuthoring_${ts}`;
  const CREATED_WIDGET_PATH = '${captured:widgetPath}';
  const ANIMATION_NAME = `IntroFade_${ts}`;

  const widgetArgs = (action, extra = {}) => ({ action, widgetPath: CREATED_WIDGET_PATH, ...extra });

  const addWidgetCases = [
    ['ADD: add_horizontal_box', 'add_horizontal_box', 'MainHorizontalBox'],
    ['ADD: add_vertical_box', 'add_vertical_box', 'MainVerticalBox'],
    ['ADD: add_overlay', 'add_overlay', 'MainOverlay'],
    ['ADD: add_grid_panel', 'add_grid_panel', 'InventoryGrid', { columnCount: 2, rowCount: 2 }],
    ['ADD: add_uniform_grid', 'add_uniform_grid', 'UniformGrid', { slotPadding: { left: 4, top: 4, right: 4, bottom: 4 }, minDesiredSlotWidth: 32, minDesiredSlotHeight: 32 }],
    ['ADD: add_wrap_box', 'add_wrap_box', 'TagWrap', { innerSlotPadding: { left: 2, top: 2, right: 2, bottom: 2 }, wrapWidth: 256, explicitWrapWidth: true }],
    ['ADD: add_scroll_box', 'add_scroll_box', 'OptionsScroll', { orientation: 'Vertical', scrollBarVisibility: 'Visible', alwaysShowScrollbar: true }],
    ['ADD: add_size_box', 'add_size_box', 'SizedPanel', { widthOverride: 300, heightOverride: 120, minDesiredWidth: 200, minDesiredHeight: 80, maxDesiredWidth: 400, maxDesiredHeight: 160 }],
    ['ADD: add_scale_box', 'add_scale_box', 'ScaledPanel', { stretch: 'UserSpecified', stretchDirection: 'Both', userSpecifiedScale: 0.85 }],
    ['ADD: add_border', 'add_border', 'FramedBorder', { brushColor: { r: 0.1, g: 0.2, b: 0.8, a: 1 }, contentColorAndOpacity: { r: 1, g: 1, b: 1, a: 0.9 } }],
    ['ADD: add_text_block', 'add_text_block', 'TitleText', { text: 'Widget Authoring Test', fontSize: 24, colorAndOpacity: { r: 1, g: 0.9, b: 0.6, a: 1 }, autoWrap: true }],
    ['ADD: add_rich_text_block', 'add_rich_text_block', 'RichBodyText', { text: '<Rich>Body</>' }],
    ['ADD: add_image', 'add_image', 'LogoImage', { texturePath: ENGINE_DEFAULT_TEXTURE, brushSize: { x: 64, y: 64 } }],
    ['ADD: add_button', 'add_button', 'PlayButton', { isEnabled: true, text: 'Play', colorAndOpacity: { r: 1, g: 1, b: 1, a: 1 } }],
    ['ADD: add_check_box', 'add_check_box', 'OptionCheckBox', { isChecked: true }],
    ['ADD: add_slider', 'add_slider', 'VolumeSlider', { value: 0.5, minValue: 0, maxValue: 1, stepSize: 0.1 }],
    ['ADD: add_progress_bar', 'add_progress_bar', 'LoadingProgress', { percent: 0.75, fillColorAndOpacity: { r: 0.2, g: 0.8, b: 0.3, a: 1 }, isMarquee: false }],
    ['ADD: add_text_input', 'add_text_input', 'NameInput', { hintText: 'Name', inputType: 'single' }],
    ['ADD: add_combo_box', 'add_combo_box', 'QualityCombo', { options: ['Low', 'High'], selectedOption: 'High' }],
    ['ADD: add_spin_box', 'add_spin_box', 'AmountSpinBox', { value: 5, minValue: 0, maxValue: 10, delta: 0.5 }],
    ['ADD: add_list_view', 'add_list_view', 'InventoryList', { orientation: 'Vertical' }],
    ['ADD: add_tree_view', 'add_tree_view', 'QuestTree', { orientation: 'Horizontal' }],
  ].map(([scenario, action, slotName, extra = {}]) => ({
    scenario,
    toolName: 'manage_blueprint',
    arguments: widgetArgs(action, { slotName, parentSlot: 'RootCanvas', ...extra }),
    expected: 'success|already exists',
  }));

  const layoutCases = [
    ['CONFIG: set_anchor', 'set_anchor', { preset: 'TopCenter' }],
    ['CONFIG: set_anchor by corners', 'set_anchor', { anchorMin: { x: 0.5, y: 0 }, anchorMax: { x: 0.5, y: 0 } }],
    ['CONFIG: set_alignment', 'set_alignment', { alignment: { x: 0.5, y: 0 } }],
    ['CONFIG: set_position', 'set_position', { position: { x: 80, y: 40 } }],
    ['CONFIG: set_size', 'set_size', { size: { x: 420, y: 72 } }],
    ['CONFIG: set_padding', 'set_padding', { padding: { left: 8, top: 8, right: 8, bottom: 8 } }],
    ['CONFIG: set_z_order', 'set_z_order', { zOrder: 10 }],
    ['CONFIG: set_render_transform', 'set_render_transform', { translation: { x: 4, y: 2 }, scale: { x: 1, y: 1 }, shear: { x: 0.05, y: 0 }, angle: 0 }],
    ['CONFIG: set_visibility', 'set_visibility', { visibility: 'Visible' }],
    ['CONFIG: set_style', 'set_style', { propertyName: 'RenderOpacity', value: '0.9' }],
    // The convenience fields of set_style, each of which reaches a different
    // branch of McpApplyWidgetStyleConvenience. They went unexercised when they
    // were added, which is how renderOpacity and text stayed implemented-but-
    // undeclared long enough to look like missing features.
    ['CONFIG: set_style text', 'set_style', { text: 'Title', fontSize: 24, renderOpacity: 1 }],
    ['CONFIG: set_style justification', 'set_style', { justification: 'center' }],
    ['CONFIG: set_style font face and spacing', 'set_style', { fontFamily: '/Engine/EngineFonts/Roboto', typeface: 'Bold', letterSpacing: 50 }],
    ['CONFIG: set_style copies a text block look', 'set_style', { copyStyleFrom: 'TitleText' }],
    ['CONFIG: set_style rounding', 'set_style', { cornerRadius: 18, outlineColor: { r: 1, g: 1, b: 1, a: 0.25 }, outlineWidth: 2 }],
    ['CONFIG: set_clipping', 'set_clipping', { clipping: 'Inherit' }],
    // Button sounds live in the button's style; an empty path clears them, so
    // this needs no sound asset in the test project.
    ['CONFIG: set_style button sounds', 'set_style', { slotName: 'PlayButton', hoverSoundPath: '', pressSoundPath: '' }],
  ].map(([scenario, action, extra]) => ({
    scenario,
    toolName: 'manage_blueprint',
    arguments: widgetArgs(action, { slotName: 'TitleText', ...extra }),
    expected: 'success',
    // Every layout and style reply names the widget it saved into, so its receipt lists it once as changed.
    assertions: [{ path: 'structuredContent.receipt.changes', length: 1, label: 'the receipt lists the widget the call changed' }],
  }));

  const bindingCases = [
    // Property bindings read a variable of the widget; the getter that converts it is generated.
    ['CONNECT: bind_text', 'bind_text', 'TitleText', { bindingSource: 'Score' }],
    ['CONNECT: bind_visibility', 'bind_visibility', 'TitleText', { bindingSource: 'bShowTitle' }],
    ['CONNECT: bind_color', 'bind_color', 'LogoImage', { bindingSource: 'LogoTint' }],
    ['CONNECT: bind_enabled', 'bind_enabled', 'PlayButton', { bindingSource: 'bCanPlay' }],
    // Event bindings call the named function, created with the event inputs when missing.
    ['CONNECT: bind_on_clicked', 'bind_on_clicked', 'PlayButton', { bindingSource: 'HandlePlayClicked' }],
    ['CONNECT: bind_on_hovered', 'bind_on_hovered', 'PlayButton', { onHoveredFunction: 'HandlePlayHovered', onUnhoveredFunction: 'HandlePlayUnhovered' }],
    ['CONNECT: bind_on_value_changed', 'bind_on_value_changed', 'VolumeSlider', { bindingSource: 'HandleVolumeChanged' }],
  ].map(([scenario, action, slotName, extra]) => ({
    scenario,
    toolName: 'manage_blueprint',
    arguments: widgetArgs(action, { slotName, ...extra }),
    expected: 'success',
  }));

  const hudCases = [
    ['HUD: add_health_bar', 'add_health_bar', { slotName: 'PlayerHealth', percent: 0.6, fillColorAndOpacity: { r: 0.1, g: 0.9, b: 0.3, a: 1 }, text: 'HP' }],
    ['HUD: add_ammo_counter', 'add_ammo_counter', { text: '8 / 24', fontSize: 30, colorAndOpacity: { r: 1, g: 0.8, b: 0.2, a: 1 } }],
    ['HUD: add_crosshair', 'add_crosshair', { text: 'o', fontSize: 28, colorAndOpacity: { r: 1, g: 1, b: 1, a: 0.8 } }],
    ['HUD: add_minimap', 'add_minimap', { mapSize: 180, texturePath: ENGINE_DEFAULT_TEXTURE }],
    ['HUD: add_compass', 'add_compass', { text: 'NE', positionX: 400, positionY: 12, sizeX: 360, sizeY: 36 }],
    ['HUD: add_damage_indicator', 'add_damage_indicator', { colorAndOpacity: { r: 0.9, g: 0, b: 0, a: 0.5 }, fadeTime: 0.4 }],
    ['HUD: add_interaction_prompt', 'add_interaction_prompt', { text: 'Open', keyLabel: 'F' }],
    ['HUD: add_objective_tracker', 'add_objective_tracker', { title: 'GOALS', items: ['Find the key', 'Open the gate', 'Escape'], maxVisibleObjectives: 2 }],
    ['HUD: add_quest_tracker', 'add_quest_tracker', { title: 'The Relic', items: ['Reach the temple'], parentSlot: 'RootCanvas' }],
  ].map(([scenario, action, extra]) => ({
    scenario,
    toolName: 'manage_blueprint',
    arguments: widgetArgs(action, extra),
    expected: 'success',
  }));

  const screenCases = [
    ['SCREEN: create_main_menu', 'create_main_menu', 'Main', { title: 'Test Game', buttons: ['Start', 'Options', 'Quit'] }],
    ['SCREEN: create_pause_menu', 'create_pause_menu', 'Pause', { buttons: ['Resume', 'Quit'] }],
    ['SCREEN: create_settings_menu', 'create_settings_menu', 'Settings', { settingsType: 'audio' }],
    ['SCREEN: create_loading_screen', 'create_loading_screen', 'Loading', { includeProgressBar: true, fadeTime: 0.5 }],
    ['SCREEN: create_hud_widget', 'create_hud_widget', 'Hud', { elements: ['health_bar', 'crosshair', 'damage_indicator'] }],
    ['SCREEN: create_dialog_widget', 'create_dialog_widget', 'Dialog', { showSpeakerName: false, responseCount: 2 }],
    ['SCREEN: create_inventory_ui', 'create_inventory_ui', 'Inventory', { columns: 5, rows: 3 }],
    ['SCREEN: create_radial_menu', 'create_radial_menu', 'Radial', { segmentCount: 6 }],
    ['SCREEN: create_credits_screen', 'create_credits_screen', 'Credits', { title: 'Thanks', entries: [{ title: 'Design', name: 'Ada' }, { title: 'Music', name: 'Grace' }] }],
    ['SCREEN: create_shop_ui', 'create_shop_ui', 'Shop', { columns: 3, itemCount: 6 }],
  ].map(([scenario, action, stem, extra]) => ({
    scenario,
    toolName: 'manage_blueprint',
    arguments: { action, name: `WBP_Tpl${stem}_${ts}`, path: TEST_FOLDER, ...extra },
    expected: 'success',
  }));
  screenCases.push({ scenario: 'SCREEN: create_shop_ui with its own items, prices and currency', toolName: 'manage_blueprint', arguments: { action: 'create_shop_ui', name: `WBP_TplWares_${ts}`, path: TEST_FOLDER, currency: 'Coins', items: [{ name: 'Sword', price: 250 }, { name: 'Shield', price: 180 }] }, expected: 'success', assertions: [{ path: 'structuredContent.result.buttons', length: 3, label: 'a Buy button per item, and Close' }, { path: 'structuredContent.result.buttons.1', equals: 'Item_2_Buy', label: 'the buttons count the items in order' }] });
  screenCases.push({ scenario: 'SCREEN: create_dialog_widget with its own line and choices', toolName: 'manage_blueprint', arguments: { action: 'create_dialog_widget', name: `WBP_TplChoice_${ts}`, path: TEST_FOLDER, text: 'Open the gate?', buttons: ['Yes', 'No'] }, expected: 'success', assertions: [{ path: 'structuredContent.result.buttons.0', equals: 'YesButton', label: 'each response is named after its label' }, { path: 'structuredContent.result.buttons', length: 2, label: 'one button per label' }] });

  testCases.push(
    // === SETUP ===
    { scenario: 'Setup: clear stale widget test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|ASSET_NOT_FOUND|not found' },
    { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
    { scenario: 'Setup: create test widget blueprint', toolName: 'manage_blueprint', arguments: { action: 'create_widget_blueprint', name: WIDGET_NAME, path: TEST_FOLDER, parentClass: 'UserWidget' }, expected: 'success', assertions: [{ path: 'structuredContent.result.compileSucceeded', equals: true, label: 'created widget blueprint compiles before reuse' }, { path: 'structuredContent.result.saveSucceeded', equals: true, label: 'created widget blueprint saves before reuse' }], captureResult: { key: 'widgetPath', fromField: 'result.widgetPath' } },

    // === CREATE ===
    { scenario: 'CREATE: create_widget_blueprint', toolName: 'manage_blueprint', arguments: { action: 'create_widget_blueprint', name: `WBP_CreateWidget_${ts}`, path: TEST_FOLDER, parentClass: 'UserWidget' }, expected: 'success|already exists' },
    { scenario: 'CREATE: create_widget_blueprint named by its widgetPath', toolName: 'manage_blueprint', arguments: { action: 'create_widget_blueprint', widgetPath: `${TEST_FOLDER}/WBP_ByPath_${ts}` }, expected: 'success|already exists' },
    { scenario: 'CREATE: create_widget_blueprint into a folder', toolName: 'manage_blueprint', arguments: { action: 'create_widget_blueprint', name: `WBP_InFolder_${ts}`, folder: TEST_FOLDER }, expected: 'success|already exists' },
    { scenario: 'CONFIG: set_widget_parent_class', toolName: 'manage_blueprint', arguments: widgetArgs('set_widget_parent_class', { parentClass: 'UserWidget' }), expected: 'success' },

    // === ADD ===
    { scenario: 'ADD: add_canvas_panel', toolName: 'manage_blueprint', arguments: widgetArgs('add_canvas_panel', { slotName: 'RootCanvas' }), expected: 'success|already exists' },
    ...addWidgetCases,

    // === CONFIG ===
    ...layoutCases,
    // A box slot reads alignment by name; a JSON number used to be taken for an
    // unknown word, so only canvas children could be centred.
    { scenario: 'ADD: text block inside a vertical box', toolName: 'manage_blueprint', arguments: widgetArgs('add_text_block', { slotName: 'BoxedText', parentSlot: 'MainVerticalBox', text: 'Boxed' }), expected: 'success|already exists' },
    { scenario: 'CONFIG: set_alignment numeric on a box slot', toolName: 'manage_blueprint', arguments: widgetArgs('set_alignment', { slotName: 'BoxedText', alignment: { x: 0.5, y: 0.5 } }), expected: 'success' },
    // A box child sizes by a rule, not a canvas {x,y}: two buttons in a row that share it evenly are both Fill.
    { scenario: 'CONFIG: set_size sizeRule Fill with a fillValue weight on a box slot', toolName: 'manage_blueprint', arguments: widgetArgs('set_size', { slotName: 'BoxedText', sizeRule: 'Fill', fillValue: 2 }), expected: 'success', assertions: [{ path: 'structuredContent.result.applied.sizeRule', equals: 'Fill', label: 'the rule reads back' }, { path: 'structuredContent.result.applied.fillValue', equals: 2, label: 'the weight reads back' }] },
    { scenario: 'CONFIG: set_size fillValue alone makes the box child Fill', toolName: 'manage_blueprint', arguments: widgetArgs('set_size', { slotName: 'BoxedText', fillValue: 3 }), expected: 'success', assertions: [{ path: 'structuredContent.result.applied.sizeRule', equals: 'Fill', label: 'a weight implies Fill' }, { path: 'structuredContent.result.applied.fillValue', equals: 3, label: 'the weight reads back' }] },
    { scenario: 'CONFIG: set_size sizeRule Auto is case-insensitive', toolName: 'manage_blueprint', arguments: widgetArgs('set_size', { slotName: 'BoxedText', sizeRule: 'auto' }), expected: 'success', assertions: [{ path: 'structuredContent.result.applied.sizeRule', equals: 'Auto', label: 'the child fits its content again' }] },
    { scenario: 'CONFIG: set_size on a box slot without a rule is refused', toolName: 'manage_blueprint', arguments: widgetArgs('set_size', { slotName: 'BoxedText', size: { x: 100, y: 40 } }), expected: 'error|MISSING_PARAMETER' },
    { scenario: 'CONFIG: set_size with an unknown sizeRule is refused', toolName: 'manage_blueprint', arguments: widgetArgs('set_size', { slotName: 'BoxedText', sizeRule: 'Stretch' }), expected: 'error|INVALID_ARGUMENT' },
    { scenario: 'CONFIG: set_size sizeRule on a canvas child is refused', toolName: 'manage_blueprint', arguments: widgetArgs('set_size', { slotName: 'TitleText', sizeRule: 'Fill' }), expected: 'error|INVALID_ARGUMENT' },

    // === CONNECT ===
    { scenario: 'Setup: Score variable to bind', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: CREATED_WIDGET_PATH, variableName: 'Score', variableType: 'Integer' }, expected: 'success|already exists' },
    { scenario: 'Setup: bShowTitle variable to bind', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: CREATED_WIDGET_PATH, variableName: 'bShowTitle', variableType: 'Boolean' }, expected: 'success|already exists' },
    { scenario: 'Setup: LogoTint variable to bind', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: CREATED_WIDGET_PATH, variableName: 'LogoTint', variableType: 'LinearColor' }, expected: 'success|already exists' },
    { scenario: 'Setup: bCanPlay variable to bind', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: CREATED_WIDGET_PATH, variableName: 'bCanPlay', variableType: 'Boolean' }, expected: 'success|already exists' },
    { scenario: 'Setup: HealthPct variable to bind', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: CREATED_WIDGET_PATH, variableName: 'HealthPct', variableType: 'Float' }, expected: 'success|already exists' },
    ...bindingCases,
    // A Widget Blueprint widget's event binds like a component's: the widget is a variable, so the generated class has its property.
    { scenario: 'CONNECT: add_event binds a widget event (componentName is the widget)', toolName: 'manage_blueprint', arguments: { action: 'add_event', blueprintPath: CREATED_WIDGET_PATH, componentName: 'OptionCheckBox', eventName: 'OnCheckStateChanged', graphName: 'EventGraph' }, expected: 'success' },
    { scenario: 'CONNECT: a build_graph add_event step binds a widget event', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: CREATED_WIDGET_PATH, graphName: 'EventGraph', operations: [
      { edit: 'add_event', id: 'sliderChanged', componentName: 'VolumeSlider', eventName: 'OnValueChanged' },
    ] }, expected: 'success', assertions: [{ path: 'structuredContent.result.succeeded', equals: 1, label: 'the widget event step ran' }] },
    { scenario: 'CONNECT: bind_percent drives a Progress Bar fill', toolName: 'manage_blueprint', arguments: widgetArgs('bind_percent', { slotName: 'LoadingProgress', bindingSource: 'HealthPct' }), expected: 'success', assertions: [{ path: 'structuredContent.result.property', equals: 'Percent', label: 'the Percent delegate is bound' }, { path: 'structuredContent.result.generatedGetter', equals: true, label: 'a getter reads the variable' }] },
    { scenario: 'CONNECT: bind_percent on a text block (no Percent or Value) is refused', toolName: 'manage_blueprint', arguments: widgetArgs('bind_percent', { slotName: 'TitleText', bindingSource: 'HealthPct' }), expected: 'error|NOT_BINDABLE' },
    { scenario: 'CONNECT: bind_text to a missing source is refused', toolName: 'manage_blueprint', arguments: widgetArgs('bind_text', { slotName: 'TitleText', bindingSource: 'NoSuchVariable' }), expected: 'error|SOURCE_NOT_FOUND' },

    // === READY-MADE HUD PIECES (add_game_widget) ===
    ...hudCases,
    { scenario: 'HUD: a slotName already in the tree is refused', toolName: 'manage_blueprint', arguments: widgetArgs('add_health_bar', { slotName: 'PlayerHealth' }), expected: 'error|SLOT_EXISTS' },

    // === READY-MADE SCREENS (create_widget_template) ===
    ...screenCases,
    { scenario: 'SCREEN: an existing asset is refused', toolName: 'manage_blueprint', arguments: { action: 'create_main_menu', name: `WBP_TplMain_${ts}`, path: TEST_FOLDER }, expected: 'error|ALREADY_EXISTS' },
    { scenario: 'SCREEN: a bad knob is refused before the asset exists', toolName: 'manage_blueprint', arguments: { action: 'create_radial_menu', name: `WBP_TplBadRadial_${ts}`, path: TEST_FOLDER, segmentCount: 40 }, expected: 'error|INVALID_ARGUMENT' },

    // === ANIMATION ===
    { scenario: 'CREATE: create_widget_animation', toolName: 'manage_blueprint', arguments: widgetArgs('create_widget_animation', { animationName: ANIMATION_NAME, duration: 1.25 }), expected: 'success|already exists' },
    { scenario: 'ADD: add_animation_track', toolName: 'manage_blueprint', arguments: widgetArgs('add_animation_track', { animationName: ANIMATION_NAME, slotName: 'TitleText', trackType: 'opacity' }), expected: 'success|already exists' },
    { scenario: 'ADD: add_animation_keyframe', toolName: 'manage_blueprint', arguments: widgetArgs('add_animation_keyframe', { animationName: ANIMATION_NAME, slotName: 'TitleText', time: 0.25, value: 0.5, interpolation: 'linear' }), expected: 'success' },
    { scenario: 'ERROR: a scale key without an {x,y} pair adds nothing', toolName: 'manage_blueprint', arguments: widgetArgs('add_animation_keyframe', { animationName: ANIMATION_NAME, slotName: 'PlayButton', trackType: 'scale', time: 0, value: 1 }), expected: 'error|INVALID_ARGUMENT' },
    { scenario: 'ADD: the first valid key on that widget creates its binding', toolName: 'manage_blueprint', arguments: widgetArgs('add_animation_keyframe', { animationName: ANIMATION_NAME, slotName: 'PlayButton', trackType: 'scale', time: 0, propertyValue: { x: 1, y: 1 } }), expected: 'success', assertions: [{ path: 'structuredContent.result.createdBinding', equals: true, label: 'the refused key left no binding behind' }] },
    { scenario: 'ADD: keys writes a whole animation across two widgets in one call', toolName: 'manage_blueprint', arguments: widgetArgs('add_animation_keyframe', { animationName: ANIMATION_NAME, trackType: 'opacity', interpolation: 'cubic', keys: [{ slotName: 'TitleText', time: 0, value: 0 }, { slotName: 'TitleText', time: 0.5, value: 1 }, { slotName: 'PlayButton', trackType: 'scale', time: 0.5, propertyValue: { x: 1.1, y: 1.1 } }] }), expected: 'success', assertions: [{ path: 'structuredContent.result.keysAdded', equals: 3, label: 'every key was written' }] },
    { scenario: 'ERROR: a keys batch with one bad key adds nothing', toolName: 'manage_blueprint', arguments: widgetArgs('add_animation_keyframe', { animationName: ANIMATION_NAME, keys: [{ slotName: 'TitleText', time: 1, value: 1 }, { slotName: 'PlayButton', trackType: 'scale', time: 1, value: 2 }] }), expected: 'error|keys[1]' },

    // === INFO ===
    { scenario: 'INFO: get_widget_info', toolName: 'manage_blueprint', arguments: widgetArgs('get_widget_info'), expected: 'success' },
    { scenario: 'INFO: get_widget_animation lists the animations with their timing', toolName: 'manage_blueprint', arguments: widgetArgs('get_widget_animation'), expected: 'success' },
    { scenario: 'INFO: get_widget_animation reads the keys each widget got', toolName: 'manage_blueprint', arguments: widgetArgs('get_widget_animation', { animationName: ANIMATION_NAME }), expected: 'success', assertions: [{ path: 'structuredContent.result.widgets', minLength: 2, label: 'TitleText and PlayButton are both driven' }] },
    { scenario: 'ACTION: preview_widget', toolName: 'manage_blueprint', arguments: widgetArgs('preview_widget'), expected: 'success' },
    { scenario: 'ACTION: preview_widget drawn for a small screen, editor opened', toolName: 'manage_blueprint', arguments: widgetArgs('preview_widget', { resolution: '640x360', openEditor: true }), expected: 'success', assertions: [{ path: 'structuredContent.result.width', equals: 640, label: 'drawn at the asked width' }] },
    { scenario: 'ACTION: preview_widget drawn once shaders finish', toolName: 'manage_blueprint', arguments: widgetArgs('preview_widget', { waitForShaders: true }), expected: 'success', timeoutMs: 30000, assertions: [{ path: 'structuredContent.result.shadersCompiling', gte: 0, label: 'shader jobs reported' }] },

    // === CLEANUP ===
    { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  );
}

runToolTests('manage-blueprint', testCases, { folder: TEST_FOLDER });
