import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { validateRequiredFields } from '../foundation/arguments/batch-validation.js';
import {
  getErrorString,
  getMessageString,
  type SequenceActionResponse
} from './sequence-handler-state.js';
import { handleSequenceBindingAction } from './sequence-binding-actions.js';

// Rotation accepts either spelling because both reach Unreal's FRotator; a
// location or scale has only one.
const TRANSFORM_COMPONENT_KEYS: Readonly<Record<string, readonly (readonly string[])[]>> = {
  location: [['x', 'y', 'z']],
  scale: [['x', 'y', 'z']],
  rotation: [['pitch', 'yaw', 'roll'], ['x', 'y', 'z']]
};

function requireOptionalString(value: unknown): string | undefined {
  return typeof value === 'string' && value.trim().length > 0 ? value : undefined;
}

// A composed Transform keyframe carries location/rotation/scale sub-objects. A
// half-filled component (a location missing x) reached Unreal and came back as
// a generic failure, so the offending axis is named here instead, pre-dispatch.
// Components that are absent stay absent: a partial Transform is legal.
function describeTransformValueViolation(property: unknown, value: unknown): string | undefined {
  if (property !== 'Transform') return undefined;
  if (typeof value !== 'object' || value === null || Array.isArray(value)) return undefined;
  const composed = value as Record<string, unknown>;
  for (const [component, acceptedKeySets] of Object.entries(TRANSFORM_COMPONENT_KEYS)) {
    const part = composed[component];
    if (part === undefined) continue;
    if (typeof part !== 'object' || part === null || Array.isArray(part)) {
      return `add_keyframe value.${component} must be an object of finite numbers.`;
    }
    const fields = part as Record<string, unknown>;
    const satisfied = acceptedKeySets.some((keys) =>
      keys.every((key) => typeof fields[key] === 'number' && Number.isFinite(fields[key]))
    );
    if (!satisfied) {
      const expected = acceptedKeySets.map((keys) => keys.join('/')).join(' or ');
      return `add_keyframe value.${component} requires finite ${expected}.`;
    }
  }
  return undefined;
}

export async function handleSequenceCoreAction(
  action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<unknown | undefined> {
  switch (action) {
    case 'create': {
      const { name } = validateRequiredFields(args, ['name']);
      const basePath = typeof args.path === 'string' ? args.path.trim().replace(/\/$/, '') : '/Game/Sequences';

      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        name,
        path: basePath,
        subAction: 'create'
      }) as SequenceActionResponse;

      let sequencePath: string | undefined;
      if (res && res.result && typeof res.result.sequencePath === 'string') {
        sequencePath = res.result.sequencePath;
      } else if (typeof args.path === 'string' && args.path.trim().length > 0) {
        const p = args.path.trim().replace(/\/$/, '');
        sequencePath = `${p}/${name}`;
      }
      const errorCode = getErrorString(res).toUpperCase();
      const msgLower = getMessageString(res).toLowerCase();
      if (res && res.success === false && (errorCode === 'FACTORY_NOT_AVAILABLE' || msgLower.includes('ulevelsequencefactorynew not available'))) {
        const path = sequencePath || (typeof args.path === 'string' ? args.path : undefined);
        return cleanObject({
          success: false,
          error: 'FACTORY_NOT_AVAILABLE',
          message: res.message || 'Sequence creation failed: factory not available',
          action: 'create',
          name,
          path,
          sequencePath,
          handled: true
        });
      }

      return cleanObject(res);
    }
    case 'open': {
      const { path } = validateRequiredFields(args, ['path']);
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'open'
      }) as SequenceActionResponse;
      return cleanObject(res);
    }
    case 'add_camera': {
      const { path } = validateRequiredFields(args, ['path']);
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        spawnable: args.spawnable !== false,
        subAction: 'add_camera'
      }) as SequenceActionResponse;
      return cleanObject(res);
    }
    case 'add_actor': {
      return await handleSequenceBindingAction(action, args, tools);
    }
    case 'add_keyframe': {
      const { path } = validateRequiredFields(args, ['path']);
      // The record declares requiredOneOf ['bindingId','actorName'], so a
      // bindingId-only call is contract-valid; demanding actorName here refused
      // the very request the published contract advertises.
      const actorName = requireOptionalString(args.actorName);
      const bindingId = requireOptionalString(args.bindingId);
      if (actorName === undefined && bindingId === undefined) {
        throw new Error('Missing required parameter: one of actorName or bindingId');
      }
      const property = typeof args.property === 'string' ? args.property : 'Transform';
      const frame = typeof args.frame === 'number' ? args.frame : Number(args.frame);
      if (!Number.isFinite(frame)) {
        throw new Error('Missing or invalid required parameter: frame (must be a number)');
      }

      const transformViolation = describeTransformValueViolation(property, args.value);
      if (transformViolation !== undefined) {
        return cleanObject({
          success: false,
          error: 'INVALID_ARGUMENT',
          message: transformViolation,
          action: 'add_keyframe',
          path,
          property,
          frame
        });
      }

      const payload: Record<string, unknown> = {
        ...args,
        path,
        ...(actorName === undefined ? {} : { actorName }),
        ...(bindingId === undefined ? {} : { bindingId }),
        property,
        frame,
        subAction: 'add_keyframe'
      };

      if (property === 'Location') {
        payload.property = 'Transform';
        payload.value = { location: args.value };
      } else if (property === 'Rotation') {
        payload.property = 'Transform';
        payload.value = { rotation: args.value };
      } else if (property === 'Scale') {
        payload.property = 'Transform';
        payload.value = { scale: args.value };
      }

      const res = await executeAutomationRequest(tools, 'manage_sequence', payload) as SequenceActionResponse;
      const errorCode = getErrorString(res).toUpperCase();
      const msgLower = getMessageString(res).toLowerCase();

      if (errorCode === 'INVALID_ARGUMENT' || msgLower.includes('frame number is required')) {
        return cleanObject(res);
      }

      if (res && res.success === false) {
        const isBindingIssue = errorCode === 'BINDING_NOT_FOUND' || msgLower.includes('binding not found');
        const isUnsupported = errorCode === 'UNSUPPORTED_PROPERTY' || msgLower.includes('unsupported property') || msgLower.includes('invalid_sequence_type');
        const isInvalidSeq = errorCode === 'INVALID_SEQUENCE' || msgLower.includes('sequence not found') || msgLower.includes('requires a sequence path');

        if (path && isInvalidSeq) {
          return cleanObject({
            success: false,
            error: 'NOT_FOUND',
            message: res.message || 'Sequence not found',
            action: 'add_keyframe',
            path,
            actorName,
            property,
            frame
          });
        }

        if (path && (isBindingIssue || isUnsupported)) {
          return cleanObject(res);
        }
      }

      return cleanObject(res);
    }
    case 'add_spawnable_from_class': {
      const { className, path } = validateRequiredFields(args, ['className', 'path']);
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        className,
        path,
        subAction: 'add_spawnable_from_class'
      });
      return cleanObject(res);
    }
    default:
      return undefined;
  }
}
