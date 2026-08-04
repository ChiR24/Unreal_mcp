import type { ITools } from '../../../types/tools/tool-interfaces.js';
import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { executeAutomationRequest, requireNonEmptyString } from '../foundation/dispatch/common-handlers.js';
import { validateRequiredFields } from '../foundation/arguments/batch-validation.js';
import {
  getErrorString,
  getMessageString,
  type SequenceActionResponse
} from './sequence-handler-state.js';

export async function handleSequenceAssetAction(
  action: string,
  args: Record<string, unknown>,
  tools: ITools
): Promise<unknown | undefined> {
  switch (action) {
    case 'duplicate': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const destDir = requireNonEmptyString(args.destinationPath, 'destinationPath', 'Missing required parameter: destinationPath');
      const defaultNewName = path.split('/').pop() || '';
      const newName = requireNonEmptyString(args.newName || defaultNewName, 'newName', 'Missing required parameter: newName');
      const baseDir = destDir.replace(/\/$/, '');
      const destPath = `${baseDir}/${newName}`;
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        subAction: 'duplicate',
        path,
        destinationPath: destPath,
        newName
      });
      return cleanObject(res);
    }
    case 'rename': {
      const { path, newName } = validateRequiredFields(args, ['path', 'newName']);
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        newName,
        subAction: 'rename'
      }) as SequenceActionResponse;
      const errorCode = getErrorString(res).toUpperCase();
      const msgLower = getMessageString(res).toLowerCase();
      if (res && res.success === false && (errorCode === 'OPERATION_FAILED' || msgLower.includes('failed to rename sequence'))) {
        return cleanObject({
          success: false,
          error: 'OPERATION_FAILED',
          message: res.message || 'Failed to rename sequence',
          action: 'rename',
          path,
          newName
        });
      }
      return cleanObject(res);
    }
    case 'delete': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'delete'
      }) as SequenceActionResponse;

      return cleanObject(res);
    }
    case 'get_metadata': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const res = await executeAutomationRequest(tools, 'manage_sequence', {
        ...args,
        path,
        subAction: 'get_metadata'
      });
      return cleanObject(res);
    }
    case 'set_metadata': {
      const path = requireNonEmptyString(args.path, 'path', 'Missing required parameter: path');
      const metadata = (args.metadata && typeof args.metadata === 'object') ? args.metadata as Record<string, unknown> : {};
      const res = await executeAutomationRequest(tools, 'set_metadata', { assetPath: path, metadata });
      return cleanObject(res);
    }
    default:
      return undefined;
  }
}
