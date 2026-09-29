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

const testCases = [
  // === SETUP ===
  { scenario: 'Setup: create test blueprint', toolName: 'manage_blueprint', arguments: { action: 'create_blueprint', name: BP_NAME, path: TEST_FOLDER, parentClass: 'Actor' }, expected: 'success|already exists', assertions: [{ path: 'structuredContent.result.assetPath', equals: BP_PATH, label: 'create_blueprint path alias uses requested folder' }] },
  { scenario: 'Setup: create input action asset', toolName: 'manage_networking', arguments: { action: 'create_input_action', name: INPUT_ACTION_NAME, path: TEST_FOLDER }, expected: 'success|already exists' },

  // === ACTION: create (requires name + path/blueprintPath) ===
  { scenario: 'ACTION: create', toolName: 'manage_blueprint', arguments: { action: 'create', name: `BP_Create_${ts}`, savePath: TEST_FOLDER, blueprintType: 'Actor', properties: { bReplicates: true } }, expected: 'success|already exists' },

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
{ scenario: 'CONFIG: set_default', toolName: 'manage_blueprint', arguments: { action: 'set_default', blueprintPath: BP_PATH, propertyName: 'bReplicates', propertyValue: true }, expected: 'success' },
{ scenario: 'VERIFY: get_blueprint reads one default without the whole summary', toolName: 'manage_blueprint', arguments: { action: 'get_blueprint', blueprintPath: BP_PATH, propertyName: 'bReplicates' }, expected: 'success', assertions: [{ path: 'structuredContent.result.propertyValue', equals: 'True', label: 'the value set above' }, { path: 'structuredContent.result.variables', equals: undefined, label: 'no summary alongside a single-property read' }] },

  // === CONFIG: modify_scs (blueprintPath + operations) ===
  { scenario: 'CONFIG: modify_scs', toolName: 'manage_blueprint', arguments: { action: 'modify_scs', blueprintPath: BP_PATH, operations: [{ type: 'add_component', componentName: 'TestModSCSComp', componentClass: 'SceneComponent' }], applyAndSave: true }, expected: 'success|already exists' },
  { scenario: 'CONFIG: modify_scs one component at the top level', toolName: 'manage_blueprint', arguments: { action: 'modify_scs', blueprintPath: BP_PATH, componentName: 'TestPlacedMesh', location: { x: 0, y: 0, z: 120 }, rotation: { pitch: 0, yaw: 45, roll: 0 }, scale: { x: 1, y: 1, z: 1 }, meshPath: ENGINE_CUBE_MESH, materialPath: ENGINE_BASIC_MATERIAL, properties: { bCastShadow: false }, compile: true, save: true }, expected: 'success' },

  // === INFO: get_scs (blueprintPath) ===
  { scenario: 'INFO: get_scs', toolName: 'manage_blueprint', arguments: { action: 'get_scs', blueprintPath: BP_PATH }, expected: 'success' },

  // === ADD: add_scs_component (blueprint_path + component_class + component_name) ===
  { scenario: 'ADD: add_scs_component', toolName: 'manage_blueprint', arguments: { action: 'add_scs_component', blueprintPath: BP_PATH, componentClass: 'PointLightComponent', componentName: 'TestSCSComp', parentComponent: 'DefaultSceneRoot' }, expected: 'success|already exists' },

  // === ADD: add_scs_component with mesh/material assignment ===
  { scenario: 'ADD: add_scs_component with mesh material', toolName: 'manage_blueprint', arguments: { action: 'add_scs_component', blueprintPath: BP_PATH, componentClass: 'StaticMeshComponent', componentName: 'TestStaticMeshSCSComp', parentComponent: 'DefaultSceneRoot', meshPath: ENGINE_CUBE_MESH, materialPath: ENGINE_BASIC_MATERIAL }, expected: 'success|already exists', assertions: [{ path: 'structuredContent.result.mesh_applied', equals: true, label: 'SCS static mesh assignment applied' }, { path: 'structuredContent.result.material_applied', equals: true, label: 'SCS material assignment applied' }] },

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
  { scenario: 'ERROR: add_variable colliding with a parent-class property', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: BP_PATH, variableName: 'bReplicates', variableType: 'Boolean' }, expected: 'error|VARIABLE_NAME_CONFLICT' },
  // The compiler only warns on a default it cannot parse; add_variable refuses it and adds nothing.
  { scenario: 'ERROR: add_variable with a default its type cannot take', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: BP_PATH, variableName: 'BadDefaultVar', variableType: 'Int', defaultValue: 'NotANumberAtAll' }, expected: 'error|DEFAULT_NOT_APPLIED' },
  { scenario: 'VERIFY: the refused add_variable left no variable behind', toolName: 'manage_blueprint', arguments: { action: 'add_variable', blueprintPath: BP_PATH, variableName: 'BadDefaultVar', variableType: 'Int', defaultValue: 7 }, expected: 'success', assertions: [{ path: 'structuredContent.result.replicated', equals: false, label: 'the variable is added anew, not found already existing' }] },
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

  // === ADD: add_function (blueprintPath + functionName) ===
  { scenario: 'ADD: add_function', toolName: 'manage_blueprint', arguments: { action: 'add_function', blueprintPath: BP_PATH, memberName: 'TestFunction', inputs: [{ name: 'InputValue', type: 'Float' }], outputs: [{ name: 'ReturnValue', type: 'Float' }], isPublic: true }, expected: 'success|already exists' },
  { scenario: 'ADD: add_function private', toolName: 'manage_blueprint', arguments: { action: 'add_function', blueprintPath: BP_PATH, functionName: 'TestPrivateFunction', isPublic: false }, expected: 'success|already exists' },
  // pure: its call nodes get no exec pins. Declared outputs make a return node, named by resultNodeGuid.
  { scenario: 'ADD: add_function pure with a return node', toolName: 'manage_blueprint', arguments: { action: 'add_function', blueprintPath: BP_PATH, functionName: 'TestPureFunction', inputs: [{ name: 'Value', type: 'Float' }], outputs: [{ name: 'Result', type: 'Float' }], pure: true }, expected: 'success', captureResult: { key: 'pureReturnId', fromField: 'result.resultNodeGuid' } },
  { scenario: 'VERIFY: add_function resultNodeGuid names the return node', toolName: 'manage_blueprint', arguments: { action: 'get_node_details', blueprintPath: BP_PATH, nodeGuid: '${captured:pureReturnId}', graphName: 'TestPureFunction' }, expected: 'success', assertions: [{ path: 'structuredContent.result.pins', includesObject: { pinName: 'Result' }, label: 'the declared output is an input pin of the return node' }] },
  { scenario: 'ERROR: add_function refuses an existing function with another signature', toolName: 'manage_blueprint', arguments: { action: 'add_function', blueprintPath: BP_PATH, functionName: 'TestPureFunction', pure: false }, expected: 'error|FUNCTION_EXISTS' },
  { scenario: 'ERROR: add_function on a missing Blueprint', toolName: 'manage_blueprint', arguments: { action: 'add_function', blueprintPath: `${TEST_FOLDER}/BP_Missing_${ts}`, functionName: 'Nowhere', pure: true }, expected: 'error|BLUEPRINT_NOT_FOUND' },

  // === DELETE: remove_function (blueprintPath + functionName) ===
  // Removes the function added directly above; safe because no later case reuses TestFunction.
  { scenario: 'DELETE: remove_function', toolName: 'manage_blueprint', arguments: { action: 'remove_function', blueprintPath: BP_PATH, functionName: 'TestFunction' }, expected: 'success|not found' },

  // === ADD: add_event (blueprintPath + eventType) ===
  { scenario: 'ADD: add_event', toolName: 'manage_blueprint', arguments: { action: 'add_event', blueprintPath: BP_PATH, eventType: 'Custom', customEventName: 'TestEvent', parameters: [{ name: 'Payload', type: 'String' }] }, expected: 'success|already exists' },

  // === DELETE: remove_event (blueprintPath + eventName) ===
  { scenario: 'DELETE: remove_event', toolName: 'manage_blueprint', arguments: { action: 'remove_event', blueprintPath: BP_PATH, eventName: 'TestEvent', graphName: 'EventGraph' }, expected: 'success|not found' },
  { scenario: 'ERROR: add_event into a missing event graph page', toolName: 'manage_blueprint', arguments: { action: 'add_event', blueprintPath: BP_PATH, graphName: 'NoSuchGraph', customEventName: 'Nowhere' }, expected: 'error|GRAPH_NOT_FOUND' },
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
  { scenario: 'CREATE: create_node call function by functionName', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'CallFunction', functionName: 'PrintString', memberClass: 'KismetSystemLibrary', graphName: 'EventGraph', posX: 1160, posY: 120 }, expected: 'success' },
  { scenario: 'CREATE: create_node pure cast', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, nodeType: 'Cast', targetClass: 'Pawn', pure: true, graphName: 'EventGraph', posX: 1360, posY: 120 }, expected: 'success' },
  { scenario: 'ADD: add_node cast with targetClass', toolName: 'manage_blueprint', arguments: { action: 'add_node', blueprintPath: BP_PATH, nodeType: 'Cast', targetClass: 'Actor', graphName: 'EventGraph', posX: 1560, posY: 120 }, expected: 'success' },
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

  // === ACTION: break_pin_links via nodeId alias ===
  { scenario: 'ACTION: break_pin_links via nodeId', toolName: 'manage_blueprint', arguments: { action: 'break_pin_links', blueprintPath: BP_PATH, nodeId: '${captured:seqNodeId}', pinName: 'then_0', graphName: 'EventGraph' }, expected: 'success' },

  // === CONFIG: set_node_property (blueprintPath + nodeGuid + propertyName + propertyValue) ===
  // Uses the real nodeId captured from the first Sequence node.
  { scenario: 'CONFIG: set_node_property', toolName: 'manage_blueprint', arguments: { action: 'set_node_property', blueprintPath: BP_PATH, nodeGuid: '${captured:seqNodeId}', propertyName: 'Comment', propertyValue: 'Test comment', graphName: 'EventGraph' }, expected: 'success' },
  { scenario: 'VERIFY: set_node_property wrote the comment', toolName: 'manage_blueprint', arguments: { action: 'get_node_details', blueprintPath: BP_PATH, nodeGuid: '${captured:seqNodeId}', graphName: 'EventGraph' }, expected: 'success', assertions: [{ path: 'structuredContent.result.comment', equals: 'Test comment', label: 'propertyValue reached the node' }] },
  { scenario: 'CONFIG: set_node_property numeric position', toolName: 'manage_blueprint', arguments: { action: 'set_node_property', blueprintPath: BP_PATH, nodeId: '${captured:seqNodeId}', propertyName: 'NodePosY', propertyValue: 160, graphName: 'EventGraph' }, expected: 'success' },

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

  // === CONFIG: set_pin_default_value (blueprintPath + nodeGuid + pinName + value) ===
  // Uses the real nodeId captured from the first Sequence node.
  { scenario: 'CONFIG: set_pin_default_value', toolName: 'manage_blueprint', arguments: { action: 'set_pin_default_value', blueprintPath: BP_PATH, nodeGuid: '${captured:printNodeId}', pinName: 'InString', defaultValue: 'test', graphName: 'EventGraph' }, expected: 'success', assertions: [{ path: 'structuredContent.value', equals: 'test', label: 'set_pin_default_value returns applied value' }] },

  // === VERIFY: pin default persisted on PrintString node ===
  { scenario: 'VERIFY: pin default persisted', toolName: 'manage_blueprint', arguments: { action: 'get_pin_details', blueprintPath: BP_PATH, nodeGuid: '${captured:printNodeId}', pinName: 'InString', graphName: 'EventGraph' }, expected: 'success', assertions: [{ path: 'structuredContent.result.pins', includesObject: { pinName: 'InString', defaultValue: 'test' }, label: 'PrintString InString pin default is persisted' }] },

  // === INFO: the graph filter also reads what a node's pins hold (a literal default, or a default object's path) ===
  { scenario: 'CONFIG: set_pin_default_value a literal the graph filter can find', toolName: 'manage_blueprint', arguments: { action: 'set_pin_default_value', blueprintPath: BP_PATH, nodeGuid: '${captured:printNodeId}', pinName: 'InString', defaultValue: 'PinFilterNeedle', graphName: 'EventGraph' }, expected: 'success' },
  { scenario: 'INFO: get_graph_details filter matches a pin default value, case-insensitively', toolName: 'manage_blueprint', arguments: { action: 'get_graph_details', blueprintPath: BP_PATH, graphName: 'EventGraph', filter: 'pinfilterneedle' }, expected: 'success', assertions: [{ path: 'structuredContent.result.totalCount', equals: 1, label: 'the node holding the literal is found by it' }] },

  // === BATCH: build_graph creates, wires and defaults nodes in one call, $id linking steps ===
  { scenario: 'BATCH: build_graph', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'create_node', id: 'delay', nodeType: 'CallFunction', memberName: 'Delay', pinDefaults: { Duration: 0.25 } },
    { edit: 'create_node', id: 'print', nodeType: 'CallFunction', memberName: 'PrintString', pinDefaults: { InString: 'batched' } },
    { edit: 'connect_pins', from: '$delay.then', to: '$print.execute' },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.result.succeeded', equals: 3, label: 'build_graph ran all three steps' }, { path: 'structuredContent.receipt.handles.0.kind', equals: 'asset', label: 'the receipt names the Blueprint the batch edited' }, { path: 'structuredContent.receipt.changes', length: 1, label: 'and lists it once as changed' }] },

  // === BATCH: "$entry" addresses the Construction Script's entry node without a lookup ===
  { scenario: 'BATCH: build_graph from the construction script entry', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'UserConstructionScript', operations: [
    { edit: 'create_node', id: 'print', nodeType: 'CallFunction', memberName: 'PrintString', pinDefaults: { InString: 'constructed' } },
    { edit: 'connect_pins', from: '$entry.then', to: '$print.execute' },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.result.succeeded', equals: 2, label: 'the entry node resolved' }] },

  // === BATCH: a step whose explicit posX/posY overlaps a node is moved to a free slot and says so, instead of stopping the batch ===
  { scenario: 'BATCH: build_graph moves a create step off an overlapping position', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'create_node', id: 'first', nodeType: 'CallFunction', memberName: 'PrintString', posX: 4000, posY: 4000 },
    { edit: 'create_node', id: 'second', nodeType: 'CallFunction', memberName: 'PrintString', posX: 4000, posY: 4000 },
  ] }, expected: 'success', assertions: [{ path: 'structuredContent.result.succeeded', equals: 2, label: 'both steps ran' }, { path: 'structuredContent.result.results.1.placementWarning', includes: 'was placed at', label: 'the second step names where it went' }] },

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
    { path: 'structuredContent.result.results.2.pins', notIncludes: 'execute', label: 'a pure function is called without exec pins' },
  ] },
  { scenario: 'BATCH: build_graph declares an event dispatcher and calls it', toolName: 'manage_blueprint', arguments: { action: 'build_graph', blueprintPath: BP_PATH, graphName: 'EventGraph', operations: [
    { edit: 'add_event_dispatcher', dispatcherName: 'OnBatchPing', parameters: [{ name: 'Count', type: 'Int' }] },
    { edit: 'create_node', id: 'ping', nodeType: 'CallDelegate', memberName: 'OnBatchPing' },
  ] }, expected: 'success', assertions: [
    { path: 'structuredContent.result.compiled', equals: true, label: 'the Blueprint compiles with the new dispatcher' },
    { path: 'structuredContent.result.results.1.pins', includes: 'Count', label: 'the call carries the dispatcher parameter' },
  ] },
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

  // === NODE: create_node CustomEvent with typed parameters ===
  { scenario: 'NODE: create_node custom event with parameters', toolName: 'manage_blueprint', arguments: { action: 'create_node', blueprintPath: BP_PATH, graphName: 'EventGraph', nodeType: 'CustomEvent', eventName: 'AddScore', parameters: [{ name: 'Points', type: 'int' }], posX: 2400, posY: 1200 }, expected: 'success' },

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

  testCases.push(
    // === SETUP ===
    { scenario: 'Setup: clear stale widget test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|ASSET_NOT_FOUND|not found' },
    { scenario: 'Setup: create test folder', toolName: 'manage_asset', arguments: { action: 'create_folder', path: TEST_FOLDER }, expected: 'success|already exists' },
    { scenario: 'Setup: create test widget blueprint', toolName: 'manage_blueprint', arguments: { action: 'create_widget_blueprint', name: WIDGET_NAME, path: TEST_FOLDER, parentClass: 'UserWidget' }, expected: 'success', assertions: [{ path: 'structuredContent.result.compileSucceeded', equals: true, label: 'created widget blueprint compiles before reuse' }, { path: 'structuredContent.result.saveSucceeded', equals: true, label: 'created widget blueprint saves before reuse' }], captureResult: { key: 'widgetPath', fromField: 'result.widgetPath' } },

    // === CREATE ===
    { scenario: 'CREATE: create_widget_blueprint', toolName: 'manage_blueprint', arguments: { action: 'create_widget_blueprint', name: `WBP_CreateWidget_${ts}`, path: TEST_FOLDER, parentClass: 'UserWidget' }, expected: 'success|already exists' },
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

    // === INFO ===
    { scenario: 'INFO: get_widget_info', toolName: 'manage_blueprint', arguments: widgetArgs('get_widget_info'), expected: 'success' },
    { scenario: 'INFO: get_widget_info by name + folder', toolName: 'manage_blueprint', arguments: { action: 'get_widget_info', name: WIDGET_NAME, folder: TEST_FOLDER }, expected: 'success' },
    { scenario: 'ACTION: preview_widget', toolName: 'manage_blueprint', arguments: widgetArgs('preview_widget'), expected: 'success' },
    { scenario: 'ACTION: preview_widget drawn for a small screen, editor opened', toolName: 'manage_blueprint', arguments: widgetArgs('preview_widget', { resolution: '640x360', openEditor: true }), expected: 'success', assertions: [{ path: 'structuredContent.result.width', equals: 640, label: 'drawn at the asked width' }] },

    // === CLEANUP ===
    { scenario: 'Cleanup: delete test folder', toolName: 'manage_asset', arguments: { action: 'delete', path: TEST_FOLDER, force: true }, expected: 'success|not found' },
  );
}

runToolTests('manage-blueprint', testCases, { folder: TEST_FOLDER });
