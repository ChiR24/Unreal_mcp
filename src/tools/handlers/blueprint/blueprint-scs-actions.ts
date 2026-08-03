import {
  blueprintTarget,
  commonTimingPayload,
  executeBlueprintRequest,
  optionalBoolean,
  optionalNumber,
  optionalString
} from './blueprint-action-context.js';
import type { BlueprintActionContext, BlueprintActionHandler } from './blueprint-action-context.js';
import { isRecord } from '../../../utils/validation/type-guards.js';

export const blueprintScsHandlers: Readonly<Record<string, BlueprintActionHandler>> = {
  add_component: async (context) => await executeBlueprintRequest(context, 'blueprint_modify_scs', {
    blueprintPath: blueprintTarget(context),
    operations: [{
      type: 'add_component',
      componentName: context.argsTyped.componentName ?? '',
      componentClass: context.argsTyped.componentType || optionalString(context.argsRecord.componentClass) || 'SceneComponent',
      attachTo: context.argsTyped.attachTo,
      transform: context.argsRecord.transform,
      properties: context.argsTyped.properties
    }],
    compile: optionalBoolean(context.argsRecord.applyAndSave),
    save: optionalBoolean(context.argsRecord.applyAndSave),
    ...commonTimingPayload(context)
  }),
  modify_scs: async (context) => await executeBlueprintRequest(context, 'blueprint_modify_scs', {
    blueprintPath: blueprintTarget(context),
    operations: Array.isArray(context.argsRecord.operations) ? context.argsRecord.operations : [],
    compile: optionalBoolean(context.argsRecord.applyAndSave),
    save: optionalBoolean(context.argsRecord.applyAndSave),
    ...commonTimingPayload(context)
  }),
  set_scs_transform: async (context) => await handleSetScsTransform(context),
  add_scs_component: async (context) => await executeBlueprintRequest(context, 'add_scs_component', {
    blueprint_path: blueprintTarget(context),
    component_class: optionalString(context.argsRecord.componentClass) || context.argsTyped.componentType || 'SceneComponent',
    component_name: context.argsTyped.componentName ?? '',
    parent_component: context.argsTyped.parentComponent ?? optionalString(context.argsRecord.attachTo) ?? '',
    mesh_path: optionalString(context.argsRecord.meshPath),
    material_path: optionalString(context.argsRecord.materialPath),
    timeoutMs: optionalNumber(context.argsRecord.timeoutMs)
  }),
  reparent_scs_component: async (context) => await executeBlueprintRequest(context, 'reparent_scs_component', {
    blueprint_path: blueprintTarget(context),
    component_name: context.argsTyped.componentName ?? '',
    new_parent: optionalString(context.argsRecord.newParent) ?? '',
    timeoutMs: optionalNumber(context.argsRecord.timeoutMs)
  }),
  set_scs_property: async (context) => await executeBlueprintRequest(context, 'set_scs_component_property', {
    blueprint_path: blueprintTarget(context),
    component_name: context.argsTyped.componentName ?? '',
    property_name: context.argsTyped.propertyName ?? '',
    property_value: context.argsTyped.value !== undefined ? context.argsTyped.value : context.argsRecord.propertyValue,
    timeoutMs: optionalNumber(context.argsRecord.timeoutMs)
  }),
  remove_scs_component: async (context) => await executeBlueprintRequest(context, 'remove_scs_component', {
    blueprint_path: blueprintTarget(context),
    component_name: context.argsTyped.componentName ?? '',
    timeoutMs: optionalNumber(context.argsRecord.timeoutMs)
  }),
  get_scs: async (context) => await executeBlueprintRequest(context, 'get_blueprint_scs', {
    blueprint_path: blueprintTarget(context),
    timeoutMs: optionalNumber(context.argsRecord.timeoutMs)
  })
};

async function handleSetScsTransform(context: BlueprintActionContext): Promise<Record<string, unknown>> {
  const location = vector3Payload(context.argsRecord.location, 0);
  const rotation = rotationPayload(context.argsRecord.rotation);
  const scale = vector3Payload(context.argsRecord.scale, 1);
  return await executeBlueprintRequest(context, 'set_scs_component_transform', {
    blueprint_path: blueprintTarget(context),
    component_name: context.argsTyped.componentName ?? '',
    location,
    rotation,
    scale,
    timeoutMs: optionalNumber(context.argsRecord.timeoutMs)
  });
}

function vector3Payload(value: unknown, fallback: number): readonly [number, number, number] | undefined {
  // The schema advertises these as number arrays ([x, y, z]); accept that form
  // first — dropping it here silently forwarded nothing, so the native handler
  // wrote defaults and reported success (a no-op that looked like a pass).
  if (Array.isArray(value) && value.length >= 3) {
    return [numberOrFallback(value[0], fallback), numberOrFallback(value[1], fallback), numberOrFallback(value[2], fallback)];
  }
  if (!isRecord(value)) {
    return undefined;
  }
  return [numberOrFallback(value.x, fallback), numberOrFallback(value.y, fallback), numberOrFallback(value.z, fallback)];
}

function rotationPayload(value: unknown): readonly [number, number, number] | undefined {
  // Schema form: [pitch, yaw, roll] number array (the {pitch,yaw,roll} object stays for compat).
  if (Array.isArray(value) && value.length >= 3) {
    return [numberOrFallback(value[0], 0), numberOrFallback(value[1], 0), numberOrFallback(value[2], 0)];
  }
  if (!isRecord(value)) {
    return undefined;
  }
  return [numberOrFallback(value.pitch, 0), numberOrFallback(value.yaw, 0), numberOrFallback(value.roll, 0)];
}

function numberOrFallback(value: unknown, fallback: number): number {
  return typeof value === 'number' ? value : fallback;
}
