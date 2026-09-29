/**
 * Component and material records: add_component, remove_component,
 * set_component_properties/set_component_property, get_component_property,
 * set_material/set_actor_material/apply_material.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { buildCoreRecord } from '../core/builder.js';
import { DOMAIN, P } from './properties.js';

const FAMILY_COMPONENT = 'component';
const FAMILY_MATERIAL = 'material';
// set_material and its two aliases take one actor or many (every step of a staircase).
const MATERIAL_INPUT = {
  inputProps: {
    actorName: P.actorName,
    actorNames: { type: 'array', items: { type: 'string' }, description: 'Several actors to give the same material in one call, in place of actorName; each is reported, and the call fails naming any that did not take it.' },
    materialPath: P.materialPath, componentName: P.componentName, materialSlot: P.materialSlot, materialIndex: P.materialIndex, allComponents: P.allComponents,
  },
  required: ['materialPath'],
  requiredOneOf: ['actorName', 'actorNames'],
} as const;

export const COMPONENT_RECORDS: readonly CapabilityRecordSource[] = [
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'add_component',
    domain: DOMAIN,
    family: FAMILY_COMPONENT,
    topics: ['add component to actor', 'actor component', 'attach component to actor'],
    summary: 'Add a new component of a given type to an actor. If any requested property (or meshPath) does not apply, the call fails naming what did not, although the component was added.',
    whenToUse: ['A component must be attached to an existing actor.'],
    whenNotToUse: ['An existing component should be configured (use set_component_property).'],
    inputProps: {
      actorName: P.actorName, componentType: P.componentType, componentName: P.componentName, properties: P.properties,
      meshPath: { type: 'string', description: 'Static mesh asset (/Game path) to show on the new component; only a StaticMeshComponent takes one. A mesh that does not load fails the call, naming the component that was still added.' },
    },
    required: ['actorName', 'componentType'],
    effect: 'write',
    costLatency: 'interactive',
    exampleInput: { action: 'add_component', actorName: 'Cube1', componentType: 'PointLightComponent', componentName: 'Light1' },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'remove_component',
    domain: DOMAIN,
    family: FAMILY_COMPONENT,
    summary: 'Remove a named component from an actor.',
    whenToUse: ['A component must be permanently detached and removed from an actor.'],
    whenNotToUse: ['The component should only be reconfigured (use set_component_property).'],
    inputProps: { actorName: P.actorName, componentName: P.componentName },
    required: ['actorName', 'componentName'],
    effect: 'write',
    costLatency: 'interactive',
    exampleInput: { action: 'remove_component', actorName: 'Cube1', componentName: 'Light1' },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'set_component_property',
    domain: DOMAIN,
    family: FAMILY_COMPONENT,
    summary: 'Set one or more properties on a named actor component. For RelativeScale3D/RelativeLocation, note that a child inherits its parent\'s scale: Unreal multiplies parent and child scale COMPONENT-WISE in the child\'s own local axes, and rotation does NOT permute which axis each factor lands on. World size = ParentScale * ChildScale * MeshExtent per local axis, and world offset = ParentScale * RelativeLocation. Under a parent scaled non-uniformly, divide the offset you want by the parent scale on that axis.',
    whenToUse: ['Component properties must be changed via a properties object or propertyName/value pair.'],
    whenNotToUse: ['The property should only be read (use get_component_property).'],
    inputProps: { actorName: P.actorName, componentName: P.componentName, properties: P.properties, propertyName: P.propertyName, value: P.value },
    required: ['actorName', 'componentName'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    exampleInput: { action: 'set_component_property', actorName: 'Cube1', componentName: 'Light1', properties: { Intensity: 5.0 } },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'set_component_properties',
    domain: DOMAIN,
    family: FAMILY_COMPONENT,
    summary: 'Alias of set_component_property. The bridge dispatches both names to the same handler.',
    whenToUse: ['Preferred when callers use the plural set_component_properties verb.'],
    whenNotToUse: ['Use set_component_property to avoid alias normalization.'],
    inputProps: { actorName: P.actorName, componentName: P.componentName, properties: P.properties, propertyName: P.propertyName, value: P.value },
    required: ['actorName', 'componentName'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    exampleInput: { action: 'set_component_properties', actorName: 'Cube1', componentName: 'Light1', properties: { Intensity: 5.0 } },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'get_component_property',
    domain: DOMAIN,
    family: FAMILY_COMPONENT,
    topics: ['value of a setting', 'nested struct member', 'no instance needed'],
    summary: 'Read one property\'s value and type from a named component on a live actor, or from a Blueprint\'s own component (blueprintPath, no instance needed); dotted paths reach into structs.',
    whenToUse: ['A component property value must be inspected.', 'A component template on a Blueprint with no instance in the level must be read.'],
    whenNotToUse: ['The property should be changed (use set_component_property).'],
    // The handler reads blueprintPath and propertyPath as well, and its twin
    // inspect.get_component_details info:"property" already declares both. Leaving
    // them off here made the same handler strictly weaker through the spelling that
    // sits next to set_component_property, for no reason the handler enforces.
    inputProps: { actorName: P.actorName, blueprintPath: P.templateBlueprintPath, componentName: P.componentName, propertyName: P.propertyName, propertyPath: P.propertyPath },
    required: ['componentName'],
    outputProps: { value: P.value },
    outputRequired: [],
    effect: 'read',
    exampleInput: { action: 'get_component_property', actorName: 'Cube1', componentName: 'Light1', propertyName: 'Intensity' },
    exampleOutput: { success: true, message: 'Intensity = 5.0', value: 5.0 },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'set_material',
    domain: DOMAIN,
    family: FAMILY_MATERIAL,
    summary: 'Apply a material asset to a mesh component on an actor, optionally per slot.',
    whenToUse: ['A material must be assigned to an actor mesh component.'],
    whenNotToUse: ['The mesh has no material slots (the assignment is a no-op).'],
    ...MATERIAL_INPUT,
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    exampleInput: { action: 'set_material', actorName: 'Cube1', materialPath: '/Game/Materials/M_Glow', materialSlot: 0 },
  }),
];
