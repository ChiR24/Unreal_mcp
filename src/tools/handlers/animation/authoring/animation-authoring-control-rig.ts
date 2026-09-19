import type { HandlerArgs } from '../../../../types/handlers/handler-types.js';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import { normalizeArgs, extractString, extractOptionalString, extractOptionalBoolean } from '../../foundation/arguments/argument-helper.js';
import { sendAnimationAuthoringRequest } from './animation-authoring-utils.js';

export async function handleControlRigAction(
  action: string,
  args: HandlerArgs,
  tools: ITools
): Promise<Record<string, unknown> | undefined> {
  switch (action) {
    case 'create_control_rig': {
      const params = normalizeArgs(args, [
        { key: 'name', required: true },
        { key: 'path', aliases: ['directory'], default: '/Game/ControlRigs' },
        { key: 'skeletalMeshPath', required: false },
        { key: 'skeletonPath', required: false },
        { key: 'save', default: true },
      ]);

      const name = extractString(params, 'name');
      const path = extractOptionalString(params, 'path') ?? '/Game/ControlRigs';
      const skeletalMeshPath = extractOptionalString(params, 'skeletalMeshPath');
      const skeletonPath = extractOptionalString(params, 'skeletonPath');
      const save = extractOptionalBoolean(params, 'save') ?? true;

      return await sendAnimationAuthoringRequest(tools, {
        subAction: 'create_control_rig',
        name,
        path,
        skeletalMeshPath,
        skeletonPath,
        save,
      }, 'Failed to create control rig', `Control Rig '${name}' created`);
    }

    default:
      return undefined;
  }
}
