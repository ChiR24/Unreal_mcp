import type { HandlerArgs } from '../../../../types/handlers/handler-types.js';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalNumber, extractOptionalBoolean } from '../../foundation/arguments/argument-helper.js';
import { nonNegativeNumberOrDefault, optionalPositiveInteger, sendAnimationAuthoringRequest, validateRequiredPath } from './animation-authoring-utils.js';

export async function handleBlendSpaceAssetAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
  case 'create_blend_space_1d': {
    const params = normalizeArgs(args, [
      { key: 'name', required: true },
      { key: 'path', aliases: ['directory'], default: '/Game/Animations' },
      { key: 'skeletonPath', required: false },
      { key: 'axisName', default: 'Speed' },
      { key: 'axisMin', default: 0 },
      { key: 'axisMax', default: 600 },
      { key: 'save', default: true },
    ]);

    const name = extractString(params, 'name');
    const path = extractOptionalString(params, 'path') ?? '/Game/Animations';
    const skeletonPath = extractOptionalString(params, 'skeletonPath');
        const axisName = extractOptionalString(params, 'axisName') ?? 'Speed';
        const axisMin = extractOptionalNumber(params, 'axisMin') ?? 0;
        const axisMax = extractOptionalNumber(params, 'axisMax') ?? 600;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        return await sendAnimationAuthoringRequest(tools, {
          subAction: 'create_blend_space_1d',
          name,
          path,
          skeletonPath,
          axisName,
          axisMin,
          axisMax,
          save,
        }, 'Failed to create blend space 1D', `Blend Space 1D '${name}' created`);
  }

  case 'create_blend_space_2d': {
    const params = normalizeArgs(args, [
      { key: 'name', required: true },
      { key: 'path', aliases: ['directory'], default: '/Game/Animations' },
      { key: 'skeletonPath', required: false },
      { key: 'horizontalAxisName', default: 'Direction' },
      { key: 'horizontalMin', default: -180 },
      { key: 'horizontalMax', default: 180 },
      { key: 'verticalAxisName', default: 'Speed' },
      { key: 'verticalMin', default: 0 },
      { key: 'verticalMax', default: 600 },
      { key: 'save', default: true },
    ]);

    const name = extractString(params, 'name');
    const path = extractOptionalString(params, 'path') ?? '/Game/Animations';
    const skeletonPath = extractOptionalString(params, 'skeletonPath');
        const horizontalAxisName = extractOptionalString(params, 'horizontalAxisName') ?? 'Direction';
        const horizontalMin = extractOptionalNumber(params, 'horizontalMin') ?? -180;
        const horizontalMax = extractOptionalNumber(params, 'horizontalMax') ?? 180;
        const verticalAxisName = extractOptionalString(params, 'verticalAxisName') ?? 'Speed';
        const verticalMin = extractOptionalNumber(params, 'verticalMin') ?? 0;
        const verticalMax = extractOptionalNumber(params, 'verticalMax') ?? 600;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        return await sendAnimationAuthoringRequest(tools, {
          subAction: 'create_blend_space_2d',
          name,
          path,
          skeletonPath,
          horizontalAxisName,
          horizontalMin,
          horizontalMax,
          verticalAxisName,
          verticalMin,
          verticalMax,
          save,
        }, 'Failed to create blend space 2D', `Blend Space 2D '${name}' created`);
  }

  case 'add_blend_sample': {
    const params = normalizeArgs(args, [
      { key: 'assetPath', required: true },
      { key: 'animationPath', required: true },
      { key: 'sampleValue', required: true }, // For 1D: number, for 2D: {x, y}
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
    const sampleValue = params['sampleValue'];
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'add_blend_sample',
      assetPath,
      animationPath,
      sampleValue,
      save,
    }, 'Failed to add blend sample', 'Blend sample added');
  }

      case 'force_rebuild_blend_space': {
        // Wave 7+ #12: Python set_editor_property / raw sample writes don't
        // trigger PostEditChangeProperty, so the BlendSpace's cached grid
        // becomes stale (referencing ABP compiles report "sample out of bounds").
        // This action forces ValidateSampleData + PostEditChangeProperty on the
        // BS, and optionally cascade-compiles every AnimBlueprint that references
        // it (so user doesn't have to find/compile each ABP manually).
        const params = normalizeArgs(args, [
          { key: 'assetPath', aliases: ['blendSpacePath'], required: true },
          { key: 'rebuildBlendParameters', default: false },
          { key: 'compileReferencers', default: true },
          { key: 'save', default: true },
        ]);

        const assetPathValidation = validateRequiredPath(params, 'assetPath');
        if (!assetPathValidation.valid) {
          return assetPathValidation.error;
        }
        const assetPath = assetPathValidation.sanitized;
        const rebuildBlendParameters = extractOptionalBoolean(params, 'rebuildBlendParameters') ?? false;
        const compileReferencers = extractOptionalBoolean(params, 'compileReferencers') ?? true;
        const save = extractOptionalBoolean(params, 'save') ?? true;

        return await sendAnimationAuthoringRequest(tools, {
          subAction: 'force_rebuild_blend_space',
          assetPath,
          rebuildBlendParameters,
          compileReferencers,
          save,
        }, 'Failed to rebuild blend space', 'Blend space rebuilt');
      }

  case 'set_axis_settings': {
    const params = normalizeArgs(args, [
      { key: 'assetPath', required: true },
      { key: 'axis', required: true }, // 'Horizontal', 'Vertical', or 'X' for 1D
      { key: 'axisName' },
      { key: 'minValue' },
      { key: 'maxValue' },
      { key: 'gridDivisions' },
      { key: 'save', default: true },
    ]);

    const assetPathValidation = validateRequiredPath(params, 'assetPath');
    if (!assetPathValidation.valid) {
      return assetPathValidation.error;
    }
    const assetPath = assetPathValidation.sanitized;
    const axis = extractString(params, 'axis');
    const axisName = extractOptionalString(params, 'axisName');
    const minValue = extractOptionalNumber(params, 'minValue');
    const maxValue = extractOptionalNumber(params, 'maxValue');
    const gridDivisions = optionalPositiveInteger(params['gridDivisions']);
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'set_axis_settings',
      assetPath,
      axis,
      axisName,
      minValue,
      maxValue,
      gridDivisions,
      save,
    }, 'Failed to set axis settings', 'Axis settings updated');
  }

  case 'set_interpolation_settings': {
    const params = normalizeArgs(args, [
      { key: 'assetPath', required: true },
      { key: 'interpolationType', default: 'Lerp' }, // Lerp, Cubic
      { key: 'targetWeightInterpolationSpeed', default: 5.0 },
      { key: 'save', default: true },
    ]);

    const assetPathValidation = validateRequiredPath(params, 'assetPath');
    if (!assetPathValidation.valid) {
      return assetPathValidation.error;
    }
    const assetPath = assetPathValidation.sanitized;
    const interpolationType = extractOptionalString(params, 'interpolationType') ?? 'Lerp';
    const targetWeightInterpolationSpeed = nonNegativeNumberOrDefault(params['targetWeightInterpolationSpeed'], 5.0);
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'set_interpolation_settings',
      assetPath,
      interpolationType,
      targetWeightInterpolationSpeed,
      save,
    }, 'Failed to set interpolation settings', 'Interpolation settings updated');
  }

    default:
      return undefined;
  }
}
