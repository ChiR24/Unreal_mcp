import type { HandlerArgs } from '../../../../types/handlers/handler-types.js';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import { ResponseFactory } from '../../../../utils/responses/response-factory.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalBoolean } from '../../foundation/arguments/argument-helper.js';
import { nonNegativeIntegerOrDefault, nonNegativeNumberOrDefault, optionalNonNegativeNumber, optionalPositiveNumber, sendAnimationAuthoringRequest, validateRequiredPath } from './animation-authoring-utils.js';

export async function handleMontageAssetAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
  case 'create_montage': {
    const params = normalizeArgs(args, [
      { key: 'name', required: true },
      { key: 'path', aliases: ['directory'], default: '/Game/Animations' },
      { key: 'skeletonPath', required: false },
      { key: 'slotName', default: 'DefaultSlot' },
      { key: 'save', default: true },
    ]);

    const name = extractString(params, 'name');
    const path = extractOptionalString(params, 'path') ?? '/Game/Animations';
    const skeletonPath = extractOptionalString(params, 'skeletonPath');
        const slotName = extractOptionalString(params, 'slotName') ?? 'DefaultSlot';
        const save = extractOptionalBoolean(params, 'save') ?? true;

        return await sendAnimationAuthoringRequest(tools, {
          subAction: 'create_montage',
          name,
          path,
          skeletonPath,
          slotName,
          save,
        }, 'Failed to create montage', `Montage '${name}' created`);
  }

  case 'add_montage_section': {
    const params = normalizeArgs(args, [
      { key: 'assetPath', aliases: ['montagePath'], required: true },
      { key: 'sectionName', required: true },
      { key: 'startTime', required: true },
      { key: 'save', default: true },
    ]);

    const assetPathValidation = validateRequiredPath(params, 'assetPath');
    if (!assetPathValidation.valid) {
      return assetPathValidation.error;
    }
    const assetPath = assetPathValidation.sanitized;
    const sectionName = extractString(params, 'sectionName');
    const startTime = nonNegativeNumberOrDefault(params['startTime'], 0);
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'add_montage_section',
      assetPath,
      sectionName,
      startTime,
      save,
    }, 'Failed to add montage section', `Section '${sectionName}' added`);
  }

  case 'add_montage_slot': {
    const params = normalizeArgs(args, [
      { key: 'assetPath', aliases: ['montagePath'], required: true },
      { key: 'animationPath', required: true },
      { key: 'slotName', default: 'DefaultSlot' },
      { key: 'startTime', default: 0 },
      { key: 'save', default: true },
    ]);

    const assetPathValidation = validateRequiredPath(params, 'assetPath');
    if (!assetPathValidation.valid) {
      return assetPathValidation.error;
    }
    const assetPath = assetPathValidation.sanitized;
    const animationPathValidation = validateRequiredPath(params, 'animationPath');
    if (!animationPathValidation.valid) {
      return animationPathValidation.error;
    }
    const animationPath = animationPathValidation.sanitized;
    const slotName = extractOptionalString(params, 'slotName') ?? 'DefaultSlot';
    const startTime = nonNegativeNumberOrDefault(params['startTime'], 0);
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'add_montage_slot',
      assetPath,
      animationPath,
      slotName,
      startTime,
      save,
    }, 'Failed to add montage slot', 'Animation added to montage slot');
  }

  case 'set_section_timing': {
    const params = normalizeArgs(args, [
      { key: 'assetPath', aliases: ['montagePath'], required: true },
      { key: 'sectionName', required: true },
      { key: 'startTime' },
      { key: 'length' },
      { key: 'save', default: true },
    ]);

    const assetPathValidation = validateRequiredPath(params, 'assetPath');
    if (!assetPathValidation.valid) {
      return assetPathValidation.error;
    }
    const assetPath = assetPathValidation.sanitized;
    const sectionName = extractString(params, 'sectionName');
    const startTime = optionalNonNegativeNumber(params['startTime']);
    const length = optionalPositiveNumber(params['length']);
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'set_section_timing',
      assetPath,
      sectionName,
      startTime,
      length,
      save,
    }, 'Failed to set section timing', 'Section timing updated');
  }

  case 'add_montage_notify': {
    const params = normalizeArgs(args, [
      { key: 'assetPath', aliases: ['montagePath'], required: true },
      { key: 'notifyClass', required: false },
      { key: 'time', required: true },
      { key: 'trackIndex', default: 0 },
      { key: 'notifyName' },
      { key: 'save', default: true },
    ]);

    const assetPathValidation = validateRequiredPath(params, 'assetPath');
    if (!assetPathValidation.valid) {
      return assetPathValidation.error;
    }
    const assetPath = assetPathValidation.sanitized;
    const notifyClass = extractOptionalString(params, 'notifyClass');
    const time = nonNegativeNumberOrDefault(params['time'], 0);
    const trackIndex = nonNegativeIntegerOrDefault(params['trackIndex'], 0);
    const notifyName = extractOptionalString(params, 'notifyName');
    const save = extractOptionalBoolean(params, 'save') ?? true;
    if (!notifyClass && !notifyName) {
      // The authoring handler rejects the empty/empty pair with MISSING_NOTIFY_PARAMS; fail here so
      // the doomed call does not burn a bridge round-trip. Either key alone is a valid call shape.
      return ResponseFactory.errorWithCode('MISSING_NOTIFY_PARAMS', "add_montage_notify requires at least one of 'notifyName' or 'notifyClass'");
    }

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'add_montage_notify',
      assetPath,
      notifyClass,
      time,
      trackIndex,
      notifyName,
      save,
    }, 'Failed to add montage notify', 'Montage notify added');
  }

    default:
      return undefined;
  }
}
