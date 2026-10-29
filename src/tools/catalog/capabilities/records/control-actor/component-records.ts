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
    actorNames: { type: 'array', items: { type: 'string' }, description: 'Several actors to give the same material in one call and one undo step, in place of actorName; each is reported under results, and the actors changed under affectedActors (which the receipt lists as changes, with an actor handle each); the call fails naming any that did not take it.' },
    materialPath: P.materialPath, componentName: P.componentName, materialSlot: P.materialSlot, materialIndex: P.materialIndex, allComponents: P.allComponents,
    materials: { type: 'array', items: { type: 'string' }, minItems: 1, maxItems: 64, description: 'One material per slot in place of materialPath and materialSlot: entry i goes into slot i, "" leaves that slot as it is. Every slot is reported under slots, in one undo step; the call fails naming any slot that did not take its material. A mesh made with several material ids (an SDF part) needs this once instead of one call per slot.' },
    color: { type: 'string', maxLength: 32, description: 'A flat colour in place of materialPath: "#rrggbb", "#rgb" or a name (red, orange, yellow, green, blue, cyan, teal, purple, pink, magenta, brown, gray or grey, white, black). It becomes the material instance /Game/Materials/Colors/MI_Color_RRGGBB, the engine\'s BasicShapeMaterial with that Color, made and saved the first time and reused after (the reply\'s materialPath names it). It replaces the slot\'s material, so a textured mesh turns one flat colour. A colour it cannot read, or color with materialPath or materials, is INVALID_ARGUMENT; no instance is made when no actor named resolves (ACTOR_NOT_FOUND).' },
  },
  // materialPath, materials or color; the handler refuses a call with none of them.
  requiredOneOf: ['actorName', 'actorNames'],
} as const;
// set_component_property and its alias take one actor or many (a dozen fire lights each took a call for one property).
const COMPONENT_PROPERTY_INPUT = {
  inputProps: {
    actorName: P.actorName,
    actorNames: { type: 'array', items: { type: 'string' }, description: 'Several actors whose component of componentName takes the same properties in one call and one undo step, in place of actorName (a row of point lights, each LightComponent0). The actors changed are counted in updatedActors and listed under affectedActors (which the receipt lists as changes), names that matched no actor come back in missing, and an actor whose component did not take everything is a results row {actorName, error} that fails the call PARTIAL_FAILURE.' },
    componentName: P.componentName, properties: P.properties, propertyName: P.propertyName, value: P.value,
  },
  required: ['componentName'],
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
    whenToUse: ['A component must be attached to an existing actor.', 'A placed actor must spin or travel by itself, like a platform: add a RotatingMovementComponent (properties RotationRate {pitch, yaw, roll} in degrees per second) or an InterpToMovementComponent (ControlPoints: two or more {PositionControlPoint {x, y, z}, bPositionIsRelative}, Duration in seconds, and BehaviourType PingPong or Loop: the default OneShot stops at the last point).'],
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
    summary: 'Remove a component that was added to this actor (add_component, or by hand in the editor), by its exact name, in one undo step, leaving the level unsaved. A near miss is refused with COMPONENT_NOT_FOUND naming the closest component; a component of the actor class or its Blueprint, or the root, is refused with COMPONENT_NOT_REMOVABLE (a Blueprint one comes out with edit_scs).',
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
    whenToUse: ['Component properties must be changed via a properties object or propertyName/value pair.', 'Several placed actors need the same component values (a row of lights\' colour or light function): actorNames does them in one call.'],
    whenNotToUse: ['The property should only be read (use get_component_property).'],
    ...COMPONENT_PROPERTY_INPUT,
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
    ...COMPONENT_PROPERTY_INPUT,
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    exampleInput: { action: 'set_component_properties', actorName: 'Cube1', componentName: 'Light1', properties: { Intensity: 5.0 } },
  }),
  buildCoreRecord({
    parentTool: 'control_actor',
    action: 'get_component_property',
    domain: DOMAIN,
    family: FAMILY_COMPONENT,
    topics: ['value of a setting', 'nested struct member', 'no instance needed', 'blueprint component property'],
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
    // The colour topics hold the colour names and paint/tint: an alias set_color took "change material color" from
    // set_material_parameter, and "make a cube" in a topic took "make a cube" from create_primitive.
    topics: ['assign material to mesh', 'apply material to actor', 'change actor material', 'change the floor material', 'put material on mesh', 'replace mesh material', 'set material slot',
      'make the cube or the wall red', 'color an actor red, blue or green', 'white, black, yellow, orange, purple, pink, brown or gray', 'change the color of an actor', 'paint or tint a mesh a colour'],
    summary: 'Apply a material asset to a mesh component on an actor, optionally per slot, or a material to every slot in one call (materials).',
    whenToUse: ['A material must be assigned to an actor mesh component.', 'A mesh with several material slots (an SDF part) needs a material in each: materials sets them all in one call.', 'An actor must take a plain colour (red, "#1e88e5") with no material made first: color.'],
    whenNotToUse: ['The mesh has no material slots (the assignment is a no-op).'],
    ...MATERIAL_INPUT,
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    exampleInput: { action: 'set_material', actorName: 'Cube1', materialPath: '/Game/Materials/M_Glow', materialSlot: 0 },
  }),
];
