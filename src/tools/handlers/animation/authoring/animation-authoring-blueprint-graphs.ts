import type { HandlerArgs } from '../../../../types/handlers/handler-types.js';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalNumber, extractOptionalBoolean, extractOptionalArray } from '../../foundation/arguments/argument-helper.js';
import { sendAnimationAuthoringRequest, validateRequiredPath } from './animation-authoring-utils.js';

export async function handleAnimationBlueprintGraphAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {

  case 'add_blend_node': {
    const params = normalizeArgs(args, [
      { key: 'blueprintPath', required: true },
      { key: 'blendType', required: true }, // TwoWayBlend, BlendByBool, BlendPosesByBool, etc.
      { key: 'nodeName' },
      { key: 'x', default: 0 },
      { key: 'y', default: 0 },
      { key: 'save', default: true },
    ]);

    const blueprintPathValidation = validateRequiredPath(params, 'blueprintPath');
    if (!blueprintPathValidation.valid) {
      return blueprintPathValidation.error;
    }
    const blueprintPath = blueprintPathValidation.sanitized;
    const blendType = extractString(params, 'blendType');
    const nodeName = extractOptionalString(params, 'nodeName');
    const x = extractOptionalNumber(params, 'x') ?? 0;
    const y = extractOptionalNumber(params, 'y') ?? 0;
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'add_blend_node',
      blueprintPath,
      blendType,
      nodeName,
      x,
      y,
      save,
    }, 'Failed to add blend node', 'Blend node added');
  }

  case 'add_cached_pose': {
    const params = normalizeArgs(args, [
      { key: 'blueprintPath', required: true },
      { key: 'cacheName', required: true },
      { key: 'save', default: true },
    ]);

    const blueprintPathValidation = validateRequiredPath(params, 'blueprintPath');
    if (!blueprintPathValidation.valid) {
      return blueprintPathValidation.error;
    }
    const blueprintPath = blueprintPathValidation.sanitized;
    const cacheName = extractString(params, 'cacheName');
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'add_cached_pose',
      blueprintPath,
      cacheName,
      save,
    }, 'Failed to add cached pose', `Cached pose '${cacheName}' added`);
  }

  case 'add_slot_node': {
    const params = normalizeArgs(args, [
      { key: 'blueprintPath', required: true },
      { key: 'slotName', required: true },
      { key: 'save', default: true },
    ]);

    const blueprintPathValidation = validateRequiredPath(params, 'blueprintPath');
    if (!blueprintPathValidation.valid) {
      return blueprintPathValidation.error;
    }
    const blueprintPath = blueprintPathValidation.sanitized;
    const slotName = extractString(params, 'slotName');
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'add_slot_node',
      blueprintPath,
      slotName,
      save,
    }, 'Failed to add slot node', `Slot node '${slotName}' added`);
  }

  case 'add_layered_blend_per_bone': {
    const params = normalizeArgs(args, [
      { key: 'blueprintPath', required: true },
      { key: 'layerSetup' }, // Array of {branchFilters: [{boneName, blendDepth}]}
      { key: 'save', default: true },
    ]);

    const blueprintPathValidation = validateRequiredPath(params, 'blueprintPath');
    if (!blueprintPathValidation.valid) {
      return blueprintPathValidation.error;
    }
    const blueprintPath = blueprintPathValidation.sanitized;
    const layerSetup = extractOptionalArray(params, 'layerSetup');
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'add_layered_blend_per_bone',
      blueprintPath,
      layerSetup,
      save,
    }, 'Failed to add layered blend per bone', 'Layered blend per bone added');
  }

  case 'set_anim_graph_node_value': {
    const params = normalizeArgs(args, [
      { key: 'blueprintPath', required: true },
      { key: 'nodeName', required: true },
      { key: 'propertyName', required: true },
      { key: 'value', required: true },
      { key: 'save', default: true },
    ]);

    const blueprintPathValidation = validateRequiredPath(params, 'blueprintPath');
    if (!blueprintPathValidation.valid) {
      return blueprintPathValidation.error;
    }
    const blueprintPath = blueprintPathValidation.sanitized;
    const nodeName = extractString(params, 'nodeName');
    const propertyName = extractString(params, 'propertyName');
    const value = params['value'];
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'set_anim_graph_node_value',
      blueprintPath,
      nodeName,
      propertyName,
      value,
      save,
    }, 'Failed to set anim graph node value', 'Anim graph node value set');
  }

    default:
      return undefined;
  }
}
