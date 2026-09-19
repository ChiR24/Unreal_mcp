import type { HandlerArgs } from '../../../../types/handlers/handler-types.js';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalNumber, extractOptionalBoolean } from '../../foundation/arguments/argument-helper.js';
import { nonNegativeNumberOrDefault, sendAnimationAuthoringRequest, validateOptionalPath, validateRequiredPath } from './animation-authoring-utils.js';

export async function handleAnimationBlueprintStateAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
      case 'create_anim_blueprint':
      case 'create_animation_bp':
      case 'create_animation_blueprint': {
        const params = normalizeArgs(args, [
          { key: 'name', required: true },
          { key: 'path', aliases: ['directory', 'savePath'], default: '/Game/Blueprints' },
          { key: 'skeletonPath', required: false },
          { key: 'parentClass', aliases: ['parent'], default: 'AnimInstance' },
          { key: 'save', default: true },
        ]);

        const name = extractString(params, 'name');
        const path = extractOptionalString(params, 'path') ?? '/Game/Blueprints';
        const skeletonPath = extractOptionalString(params, 'skeletonPath');
        const parentClass = extractOptionalString(params, 'parentClass') ?? 'AnimInstance';
        const save = extractOptionalBoolean(params, 'save') ?? true;

        return await sendAnimationAuthoringRequest(tools, {
          subAction: 'create_anim_blueprint',
          name,
          path,
          skeletonPath,
          parentClass,
          save,
        }, 'Failed to create anim blueprint', `Animation Blueprint '${name}' created`);
      }

  case 'add_state_machine': {
    const params = normalizeArgs(args, [
      { key: 'blueprintPath', required: true },
      { key: 'stateMachineName', required: true },
      { key: 'save', default: true },
    ]);

    const blueprintPathValidation = validateRequiredPath(params, 'blueprintPath');
    if (!blueprintPathValidation.valid) {
      return blueprintPathValidation.error;
    }
    const blueprintPath = blueprintPathValidation.sanitized;
    const stateMachineName = extractString(params, 'stateMachineName');
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'add_state_machine',
      blueprintPath,
      stateMachineName,
      save,
    }, 'Failed to add state machine', `State machine '${stateMachineName}' added`);
  }

  case 'add_state': {
    const params = normalizeArgs(args, [
      { key: 'blueprintPath', required: true },
      { key: 'stateMachineName', required: true },
      { key: 'stateName', required: true },
      { key: 'animationPath' },
      { key: 'isEntryState', default: false },
      { key: 'save', default: true },
    ]);

    const blueprintPathValidation = validateRequiredPath(params, 'blueprintPath');
    if (!blueprintPathValidation.valid) {
      return blueprintPathValidation.error;
    }
    const blueprintPath = blueprintPathValidation.sanitized;
    const stateMachineName = extractString(params, 'stateMachineName');
    const stateName = extractString(params, 'stateName');
    const animationPath = validateOptionalPath(params, 'animationPath');
    if (animationPath && !animationPath.valid) {
      return animationPath.error;
    }
    const isEntryState = extractOptionalBoolean(params, 'isEntryState') ?? false;
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'add_state',
      blueprintPath,
      stateMachineName,
      stateName,
      animationPath: animationPath?.sanitized,
      isEntryState,
      save,
    }, 'Failed to add state', `State '${stateName}' added`);
  }

  case 'add_transition': {
    const params = normalizeArgs(args, [
      { key: 'blueprintPath', required: true },
      { key: 'stateMachineName', required: true },
      { key: 'fromState', required: true },
      { key: 'toState', required: true },
      { key: 'save', default: true },
    ]);

    const blueprintPathValidation = validateRequiredPath(params, 'blueprintPath');
    if (!blueprintPathValidation.valid) {
      return blueprintPathValidation.error;
    }
    const blueprintPath = blueprintPathValidation.sanitized;
    const stateMachineName = extractString(params, 'stateMachineName');
    const fromState = extractString(params, 'fromState');
    const toState = extractString(params, 'toState');
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'add_transition',
      blueprintPath,
      stateMachineName,
      fromState,
      toState,
      save,
    }, 'Failed to add transition', `Transition from '${fromState}' to '${toState}' added`);
  }

  case 'set_transition_rules': {
    const params = normalizeArgs(args, [
      { key: 'blueprintPath', required: true },
      { key: 'stateMachineName', required: true },
      { key: 'fromState', required: true },
      { key: 'toState', required: true },
      { key: 'blendTime', default: 0.2 },
      { key: 'blendLogicType', default: 'StandardBlend' },
      { key: 'automaticTriggerRule' }, // e.g., 'TimeRemaining'
      { key: 'automaticTriggerTime' },
      { key: 'save', default: true },
    ]);

    const blueprintPathValidation = validateRequiredPath(params, 'blueprintPath');
    if (!blueprintPathValidation.valid) {
      return blueprintPathValidation.error;
    }
    const blueprintPath = blueprintPathValidation.sanitized;
    const stateMachineName = extractString(params, 'stateMachineName');
    const fromState = extractString(params, 'fromState');
    const toState = extractString(params, 'toState');
    const blendTime = nonNegativeNumberOrDefault(params['blendTime'], 0.2);
    const blendLogicType = extractOptionalString(params, 'blendLogicType') ?? 'StandardBlend';
    const automaticTriggerRule = extractOptionalString(params, 'automaticTriggerRule');
    const automaticTriggerTime = extractOptionalNumber(params, 'automaticTriggerTime');
    const save = extractOptionalBoolean(params, 'save') ?? true;

    return await sendAnimationAuthoringRequest(tools, {
      subAction: 'set_transition_rules',
      blueprintPath,
      stateMachineName,
      fromState,
      toState,
      blendTime,
      blendLogicType,
      automaticTriggerRule,
      automaticTriggerTime,
      save,
    }, 'Failed to set transition rules', 'Transition rules updated');
  }

    default:
      return undefined;
  }
}
