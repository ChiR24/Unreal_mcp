/**
 * SCS (Simple Construction Script) component records.
 *
 * add_component is a separate action that adds a component instance to the
 * Blueprint without SCS node ownership (no template). The SCS actions
 * construct component templates through SCS->CreateNode()/AddNode() so the
 * template is owned by the SCS node, per the UE 5.7 safety rules in
 * plugins/McpAutomationBridge/AGENTS.md.
 *
 * set_default applies property values to the CDO (Class Default Object) as a
 * fallback when SCS-owned template properties are not the right target.
 */
import type { CapabilityRecordSource } from '../../model.js';
import { BP_PLUGINS, buildRecord } from './helpers.js';
import { P } from './properties.js';

const FAMILY = 'scs';
const DOMAIN = 'blueprint';

export const SCS_COMPONENTS_RECORDS: readonly CapabilityRecordSource[] = [
  buildRecord({
    id: 'blueprint.add_component',
    action: 'add_component',
    family: FAMILY,
    domain: DOMAIN,
    // One native implementation serves add_component and add_scs_component
    // (ScsAddComponent.cpp): both add an SCS-owned template and read the same
    // fields, so both declare them.
    summary: 'Add a component to a Blueprint (the same SCS template add_scs_component adds), optionally attached, placed, meshed and configured.',
    whenToUse: ['A component must be added to a Blueprint under the add_component verb.'],
    whenNotToUse: ['Several components or other SCS edits must be batched (use modify_scs).'],
    inputProps: {
      blueprintPath: P.blueprintPath, componentClass: P.componentClass, componentType: P.componentType, componentName: P.componentName, attachTo: P.attachTo, properties: P.properties,
      meshPath: P.meshPath, materialPath: P.materialPath, location: P.location, rotation: P.rotation, scale: P.scale,
    },
    required: ['blueprintPath', 'componentClass', 'componentName'],
    outputProps: { componentName: P.componentName },
    outputRequired: ['componentName'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'add_component', blueprintPath: '/Game/Blueprints/BP_Test', componentClass: '/Script/Engine.StaticMeshComponent', componentName: 'Mesh' },
    exampleOutput: { success: true, componentName: 'Mesh' },
  }),
  buildRecord({
    id: 'blueprint.add_scs_component',
    action: 'add_scs_component',
    family: FAMILY,
    domain: DOMAIN,
    topics: ['add component', 'attach component', 'static mesh component', 'mesh component', 'component template', 'add mesh to blueprint', 'add camera component', 'add component to blueprint', 'blueprint component'],
    aliases: ['blueprint.add_component_to_blueprint'],
    summary: 'Add an SCS-owned component template node to the Blueprint\'s Simple Construction Script.',
    whenToUse: ['A component template must be owned by the SCS tree for instanced property overrides.'],
    whenNotToUse: ['A non-template component instance is sufficient (use add_component).'],
    inputProps: {
      blueprintPath: P.blueprintPath, componentClass: P.componentClass, componentName: P.componentName, parentComponent: P.parentComponent, meshPath: P.meshPath, materialPath: P.materialPath,
      location: P.location, rotation: P.rotation, scale: P.scale, properties: P.properties,
    },
    required: ['blueprintPath', 'componentClass', 'componentName'],
    outputProps: {
      componentName: P.componentName,
      componentClass: P.componentClass,
      parent: P.parentComponent,
      compiled: P.success,
      saved: P.success,
      scsVerification: { type: 'object', description: 'SCS node verification (exists, parent matches).', additionalProperties: true, 'x-unreal-reflection-boundary': true },
    },
    outputRequired: ['componentName'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'add_scs_component', blueprintPath: '/Game/Blueprints/BP_Test', componentClass: '/Script/Engine.StaticMeshComponent', componentName: 'SCS_Mesh', parentComponent: 'DefaultSceneRoot' },
    exampleOutput: { success: true, componentName: 'SCS_Mesh', componentClass: '/Script/Engine.StaticMeshComponent', parent: 'DefaultSceneRoot', compiled: true, saved: true, scsVerification: { exists: true } },
  }),
  buildRecord({
    id: 'blueprint.modify_scs',
    action: 'modify_scs',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Modify an existing SCS component template node (properties or transform).',
    whenToUse: ['An SCS-owned component template needs property or transform updates.'],
    whenNotToUse: ['Only a single property is needed (use set_scs_property).'],
    // Either an operations batch, or one component described at the top level
    // (componentName plus location, rotation, scale, meshPath, materialPath or
    // properties), which the handler runs as a single modify op.
    inputProps: {
      location: P.location, rotation: P.rotation, scale: P.scale, properties: P.properties, meshPath: P.meshPath, materialPath: P.materialPath,
      compile: { type: 'boolean', description: 'Compile the Blueprint after the operations (default false; applyAndSave also compiles).' },
      save: { type: 'boolean', description: 'Save the Blueprint after the operations (default true, so the edit survives an editor restart); applyAndSave overrides it.' },
      blueprintPath: P.blueprintPath, componentName: P.componentName, operations: { type: 'array', description: 'SCS operations applied in order. Each entry is an object with `type` plus that operation\'s own fields; `type: "add_component"` also takes componentName, componentClass, attachTo, transform, meshPath, materialPath and a nested properties bag; `type: "modify_component"` takes the same transform, meshPath, materialPath and properties for a component that already exists; `type: "attach_component"` (or "reparent") moves componentName under parentComponent (or attachTo/newParent). A failed operation is named in warnings, and the call fails when none applied.', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, 'x-unreal-reflection-boundary': true }, applyAndSave: P.applyAndSave },
    required: ['blueprintPath'],
    requiredOneOf: ['operations', 'componentName'],
    effect: 'write',
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'modify_scs', blueprintPath: '/Game/Blueprints/BP_Test', operations: [{ type: 'add_component', componentName: 'SCS_Mesh', componentClass: 'StaticMeshComponent', properties: { bCastShadow: true } }], applyAndSave: true },
  }),
  buildRecord({
    id: 'blueprint.get_scs',
    action: 'get_scs',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Read the SCS tree of a Blueprint plus the components it inherits from its parent class.',
    whenToUse: ['The SCS node hierarchy must be inspected before modifying components.', 'An inherited component must be named as an attach parent for add_scs_component.'],
    whenNotToUse: ['A single component property is needed (use get or set_scs_property).'],
    inputProps: { blueprintPath: P.blueprintPath },
    required: ['blueprintPath'],
    // inheritedComponents was invisible before: a Character Blueprint with no SCS
    // nodes reported "Retrieved 0 SCS components" while owning several inherited
    // ones, which are exactly the names add_scs_component wants as parentComponent.
    outputProps: { components: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'SCS node descriptors with name, class, and parent.', 'x-unreal-reflection-boundary': true }, componentCount: { type: 'number', description: 'Number of SCS-owned components.' }, inheritedComponents: { type: 'array', items: { type: 'object', additionalProperties: true, 'x-unreal-reflection-boundary': true }, description: 'Components inherited from the parent class: componentName, componentType, isSceneComponent, ownerClass. Any of these names is valid as parentComponent.', 'x-unreal-reflection-boundary': true }, inheritedComponentCount: { type: 'number', description: 'Number of inherited components.' } },
    outputRequired: ['components'],
    effect: 'read',
    latency: 'instant',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'get_scs', blueprintPath: '/Game/Blueprints/BP_Test' },
    exampleOutput: { success: true, components: [{ componentName: 'DefaultSceneRoot', componentType: 'SceneComponent' }], componentCount: 1, inheritedComponents: [{ componentName: 'CollisionCylinder', componentType: 'CapsuleComponent', isSceneComponent: true, ownerClass: 'Character' }], inheritedComponentCount: 1 },
  }),
  buildRecord({
    id: 'blueprint.remove_scs_component',
    action: 'remove_scs_component',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Remove one or several SCS-owned component template nodes from the Blueprint.',
    whenToUse: ['An SCS component node and its template must be permanently removed.'],
    whenNotToUse: ['The component should be detached but preserved (no detach action exists; remove is destructive).'],
    inputProps: {
      blueprintPath: P.blueprintPath, componentName: P.componentName,
      componentNames: { type: 'array', items: { type: 'string' }, description: 'Several components to remove in one call, in place of componentName; each is reported, and the call fails naming any that were not removed.' },
    },
    required: ['blueprintPath'],
    requiredOneOf: ['componentName', 'componentNames'],
    effect: 'destructive',
    behavior: { safeToRetry: false },
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'remove_scs_component', blueprintPath: '/Game/Blueprints/BP_Test', componentName: 'SCS_Mesh' },
  }),
  buildRecord({
    id: 'blueprint.reparent_scs_component',
    action: 'reparent_scs_component',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Reparent an SCS component node to a new parent node in the SCS tree.',
    whenToUse: ['An SCS component must move under a different parent node.'],
    whenNotToUse: ['The component hierarchy does not need restructuring.'],
    inputProps: { blueprintPath: P.blueprintPath, componentName: P.componentName, newParent: P.newParent },
    required: ['blueprintPath', 'componentName', 'newParent'],
    effect: 'write',
    behavior: { idempotency: 'idempotent' },
    latency: 'interactive',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'reparent_scs_component', blueprintPath: '/Game/Blueprints/BP_Test', componentName: 'SCS_Mesh', newParent: 'CameraRoot' },
  }),
  buildRecord({
    id: 'blueprint.set_scs_transform',
    action: 'set_scs_transform',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Set the transform (location, rotation, scale) of an SCS-owned component template.',
    whenToUse: ['An SCS component template transform must be updated.'],
    whenNotToUse: ['A non-SCS component transform is needed (use set_node_property or add_component).'],
    inputProps: { blueprintPath: P.blueprintPath, componentName: P.componentName, location: P.location, rotation: P.rotation, scale: P.scale },
    required: ['blueprintPath', 'componentName'],
    effect: 'write',
    behavior: { idempotency: 'idempotent', safeToRetry: true },
    latency: 'instant',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'set_scs_transform', blueprintPath: '/Game/Blueprints/BP_Test', componentName: 'SCS_Mesh' },
  }),
  buildRecord({
    id: 'blueprint.set_scs_property',
    action: 'set_scs_property',
    family: FAMILY,
    domain: DOMAIN,
    summary: 'Set a single property on an SCS-owned component template. For RelativeScale3D/RelativeLocation, note that a child inherits its parent\'s scale: Unreal multiplies parent and child scale COMPONENT-WISE in the child\'s own local axes, and rotation does NOT permute which axis each factor lands on. World size = ParentScale * ChildScale * MeshExtent per local axis, and world offset = ParentScale * RelativeLocation. Under a parent scaled non-uniformly, divide the offset you want by the parent scale on that axis.',
    whenToUse: ['One property on an SCS component template must be changed.'],
    whenNotToUse: ['Multiple properties need batch updates (use modify_scs).'],
    inputProps: { blueprintPath: P.blueprintPath, componentName: P.componentName, propertyName: P.propertyName, propertyValue: P.propertyValue },
    required: ['blueprintPath', 'componentName', 'propertyName', 'propertyValue'],
    // SCSHandlersSetProperty re-reads the property after writing and returns it
    // as verifiedValue, but only when the value exports to JSON, so it is
    // declared optional (not required).
    outputProps: { verifiedValue: P.propertyValue },
    effect: 'write',
    behavior: { idempotency: 'idempotent', safeToRetry: true },
    latency: 'instant',
    resources: 'low',
    plugins: BP_PLUGINS,
    exampleInput: { action: 'set_scs_property', blueprintPath: '/Game/Blueprints/BP_Test', componentName: 'SCS_Mesh', propertyName: 'bCastShadow', propertyValue: true },
    exampleOutput: { success: true, message: 'SCS property set', verifiedValue: true },
  }),
];
