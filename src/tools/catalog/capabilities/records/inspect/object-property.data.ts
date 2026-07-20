/**
 * Object, property, and class introspection records (12 actions).
 *
 * Grounded in:
 * - src/tools/handlers/inspect/inspect-actions.ts: get_actor_details,
 *   get_material_details, get_texture_details, get_mesh_details alias to
 *   inspect_object; get_level_details aliases to get_world_settings.
 * - src/tools/handlers/inspect/inspect-object-actions.ts: inspect_object
 *   dispatches to the inspect bridge route; get_blueprint_details re-routes
 *   to the separate blueprint_get bridge route (mismatch surfaced).
 * - src/tools/handlers/inspect/inspect-property-actions.ts: get_property/
 *   set_property dispatch to the inspect bridge route.
 * - src/tools/handlers/inspect/inspect-global-actions.ts: inspect_class,
 *   inspect_cdo dispatch to the inspect bridge route.
 * - native Private/Domains/Environment/Inspection/McpAutomationBridge_EnvironmentHandlersInspect.cpp:
 *   inspect sub-actions route through HandleInspectAction; inspect_cdo and
 *   inspect_struct have dedicated native handlers.
 */
import type { CapabilityRecordSource } from '../../index.js';
import { buildCoreRecord } from '../core/builder.js';
import { P } from './properties.js';

const D = 'inspect';
const NR = 'Distinct inspect verb and target; no cross-tool duplicate.';

export const OBJECT_PROPERTY_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'inspect', action: 'inspect_object', dispatchAction: 'inspect_object', domain: D, family: 'object',
    summary: 'Inspect a world actor or asset object by path, returning detailed properties.',
    whenToUse: ['An object\'s properties and structure must be read.'],
    whenNotToUse: ['A Blueprint CDO without a spawned actor is needed; use inspect_cdo.'],
    inputProps: { objectPath: P.objectPath, actorName: P.actorName, name: P.name, componentName: P.componentName, detailed: P.detailed, propertyNames: P.propertyNames },
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'inspect_object', objectPath: '/Game/Maps/Demo.Demo_PersistentLevel.PlayerStart_1' },
    exampleOutput: { success: true, message: 'Object inspected' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_actor_details', dispatchAction: 'inspect_object', domain: D, family: 'object',
    summary: 'Inspect a world actor (alias of inspect_object).',
    whenToUse: ['A world actor\'s details must be read using the get_actor_details verb.'],
    whenNotToUse: ['Prefer the canonical inspect_object verb.'],
    inputProps: { objectPath: P.objectPath, actorName: P.actorName, name: P.name, detailed: P.detailed },
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_actor_details', actorName: 'PlayerStart_1' },
    exampleOutput: { success: true, message: 'Object inspected' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_blueprint_details', dispatchAction: 'blueprint_get', dispatchMode: 'action', domain: D, family: 'object',
    summary: 'Inspect a Blueprint asset via the separate blueprint_get bridge route.',
    whenToUse: ['A Blueprint asset\'s structure must be read without spawning an actor.'],
    whenNotToUse: ['A world actor is in scope; use inspect_object.'],
    inputProps: { objectPath: P.objectPath, blueprintPath: P.blueprintPath },
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_blueprint_details', blueprintPath: '/Game/Blueprints/BP_Test' },
    exampleOutput: { success: true, message: 'Blueprint inspected' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'TS normalizes get_blueprint_details to inspect_object in the switch but inspect-object-actions.ts re-routes to the separate blueprint_get bridge route, not the inspect parent; mismatch surfaced, not normalized away.',
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_mesh_details', dispatchAction: 'inspect_object', domain: D, family: 'object',
    summary: 'Inspect a mesh asset (alias of inspect_object).',
    whenToUse: ['A mesh asset\'s details must be read.'],
    whenNotToUse: ['Prefer the canonical inspect_object verb.'],
    inputProps: { objectPath: P.objectPath, actorName: P.actorName, name: P.name },
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_mesh_details', objectPath: '/Game/Meshes/SM_Cube' },
    exampleOutput: { success: true, message: 'Object inspected' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_texture_details', dispatchAction: 'inspect_object', domain: D, family: 'object',
    summary: 'Inspect a texture asset (alias of inspect_object).',
    whenToUse: ['A texture asset\'s details must be read.'],
    whenNotToUse: ['Prefer the canonical inspect_object verb.'],
    inputProps: { objectPath: P.objectPath, actorName: P.actorName, name: P.name },
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_texture_details', objectPath: '/Game/Textures/T_Base' },
    exampleOutput: { success: true, message: 'Object inspected' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_material_details', dispatchAction: 'inspect_object', domain: D, family: 'object',
    summary: 'Inspect a material asset (alias of inspect_object).',
    whenToUse: ['A material asset\'s details must be read.'],
    whenNotToUse: ['Prefer the canonical inspect_object verb.'],
    inputProps: { objectPath: P.objectPath, actorName: P.actorName, name: P.name },
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_material_details', objectPath: '/Game/Materials/M_Base' },
    exampleOutput: { success: true, message: 'Object inspected' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_level_details', dispatchAction: 'get_world_settings', domain: D, family: 'object',
    summary: 'Inspect the current level/world summary (TS normalizes to get_world_settings).',
    whenToUse: ['The current level\'s world settings summary must be read.'],
    whenNotToUse: ['A specific actor is in scope; use inspect_object.'],
    inputProps: { objectPath: P.objectPath, actorName: P.actorName, name: P.name },
    required: [],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_level_details' },
    exampleOutput: { success: true, message: 'World settings', worldName: 'Demo' },
    outputProps: { worldName: { type: 'string', description: 'Current world name.' } },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET',
    normalizationRationale: 'A level/world has no objectPath, so inspect-actions.ts aliases get_level_details to get_world_settings; the record dispatches the get_world_settings action through the inspect parent, surfacing the normalization alias rather than hiding it.',
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'get_property', dispatchAction: 'get_property', domain: D, family: 'property',
    summary: 'Read a property value from a world actor, asset, or Blueprint CDO.',
    whenToUse: ['A single property value must be read.'],
    whenNotToUse: ['All properties are needed; use inspect_object or inspect_cdo.'],
    inputProps: { objectPath: P.objectPath, actorName: P.actorName, name: P.name, blueprintPath: P.blueprintPath, propertyName: P.propertyName, propertyPath: P.propertyPath },
    required: ['propertyName'],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'get_property', objectPath: '/Game/Maps/Demo.Demo_PersistentLevel.PlayerStart_1', propertyName: 'ActorLabel' },
    exampleOutput: { success: true, message: 'Property read', value: 'PlayerStart_1' },
    outputProps: { value: P.value },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'set_property', dispatchAction: 'set_property', domain: D, family: 'property',
    summary: 'Write a property value on a world actor, asset, or Blueprint CDO.',
    whenToUse: ['A single property value must be written.'],
    whenNotToUse: ['The property is read-only or the target is a packed asset.'],
    inputProps: { objectPath: P.objectPath, actorName: P.actorName, name: P.name, blueprintPath: P.blueprintPath, propertyName: P.propertyName, propertyPath: P.propertyPath, value: P.value },
    required: ['propertyName'],
    effect: 'write', costLatency: 'interactive', costResources: 'low',
    exampleInput: { action: 'set_property', objectPath: '/Game/Maps/Demo.Demo_PersistentLevel.PlayerStart_1', propertyName: 'ActorLabel', value: 'Spawn_01' },
    exampleOutput: { success: true, message: 'Property set' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'inspect_class', dispatchAction: 'inspect_class', domain: D, family: 'class',
    summary: 'Inspect a UClass: metadata, parent, default CDO properties.',
    whenToUse: ['A class\'s hierarchy and defaults must be read.'],
    whenNotToUse: ['A specific instance is in scope; use inspect_object.'],
    inputProps: { className: P.className, classPath: P.classPath },
    required: ['className'],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'inspect_class', className: 'PointLight' },
    exampleOutput: { success: true, message: 'Class inspected', className: '/Script/Engine.PointLight' },
    outputProps: { className: P.className },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'inspect_cdo', dispatchAction: 'inspect_cdo', domain: D, family: 'class',
    summary: 'Inspect a Blueprint Class Default Object (CDO) and its default components without spawning an actor.',
    whenToUse: ['A Blueprint\'s default properties and components must be read.'],
    whenNotToUse: ['A spawned world actor is in scope; use inspect_object.'],
    inputProps: { blueprintPath: P.blueprintPath, objectPath: P.objectPath, componentName: P.componentName, propertyNames: P.propertyNames, detailed: P.detailed },
    required: ['blueprintPath'],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'inspect_cdo', blueprintPath: '/Game/Blueprints/BP_Test' },
    exampleOutput: { success: true, message: 'CDO inspected' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
  buildCoreRecord({
    parentTool: 'inspect', action: 'inspect_struct', dispatchAction: 'inspect_struct', domain: D, family: 'class',
    summary: 'Inspect a UserDefinedStruct layout (member names, types, defaults) read-only.',
    whenToUse: ['A Blueprint Struct\'s member layout must be read.'],
    whenNotToUse: ['Struct values must be read or written; use manage_asset struct actions.'],
    inputProps: { structPath: P.structPath },
    required: ['structPath'],
    effect: 'read', costLatency: 'instant', costResources: 'low',
    exampleInput: { action: 'inspect_struct', structPath: '/Game/Structs/S_Test' },
    exampleOutput: { success: true, message: 'Struct inspected' },
    normalizationClass: 'C_SAME_VERB_DIFFERENT_TARGET', normalizationRationale: NR,
  }),
];
