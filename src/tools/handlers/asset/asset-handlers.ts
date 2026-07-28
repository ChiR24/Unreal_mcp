import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { HandlerArgs } from '../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import {
  handleCreateFolder,
  handleDeleteAssets,
  handleDuplicateAsset,
  handleImportAsset,
  handleListAssets,
  handleMoveAsset,
  handleRenameAsset
} from './asset-basic-actions.js';
import { handleBulkAssetAction } from './asset-bulk-actions.js';
import { invalidateAssetCacheForMutation } from './asset-cache-invalidation.js';
import { createAssetContext, type AssetOperationResponse } from './asset-handler-types.js';
import { handleMaterialAssetAction } from './asset-material-actions.js';
import { handleAssetMetadataAction } from './asset-query-actions.js';
import { isValidAssetAction, validAssetActionMessage } from './asset-validation.js';

export async function handleAssetTools(action: string, args: HandlerArgs, tools: ITools): Promise<Record<string, unknown>> {
  const context = createAssetContext(args, tools);
  try {
    const result = await routeAssetAction(action, context);
    invalidateAssetCacheForMutation(tools, action, args, result);
    return result;
  } catch (error) {
    if (error instanceof Error) {
      return ResponseFactory.error(error);
    }
    return ResponseFactory.error(error);
  }
}

async function routeAssetAction(
  action: string,
  context: ReturnType<typeof createAssetContext>
): Promise<Record<string, unknown>> {
  switch (action) {
    case 'list':
      return await handleListAssets(context);
    case 'create_folder':
      return await handleCreateFolder(context);
    case 'import':
      return await handleImportAsset(context);
    case 'duplicate_asset':
    case 'duplicate':
      return await handleDuplicateAsset(context);
    case 'rename_asset':
    case 'rename':
      return await handleRenameAsset(context);
    case 'move_asset':
    case 'move':
      return await handleMoveAsset(context);
    case 'delete_assets':
    case 'delete_asset':
    case 'delete':
      return await handleDeleteAssets(context);
    default: {
      const metadataResult = await handleAssetMetadataAction(action, context);
      if (metadataResult) return metadataResult;

      const materialResult = await handleMaterialAssetAction(action, context);
      if (materialResult) return materialResult;

      const bulkResult = await handleBulkAssetAction(action, context);
      if (bulkResult) return bulkResult;

      return await handleDefaultAssetAction(action, context);
    }
  }
}

async function handleDefaultAssetAction(
  action: string,
  context: ReturnType<typeof createAssetContext>
): Promise<Record<string, unknown>> {
  if (!isValidAssetAction(action)) {
    return cleanObject({
      success: false,
      error: 'UNKNOWN_ACTION',
      message: `Unknown asset action: ${action}. Valid actions are: ${validAssetActionMessage()}`,
      action: action || 'manage_asset',
      assetPath: context.assetArgs.assetPath ?? context.assetArgs.path
    });
  }

  const res = await executeAutomationRequest(
    context.tools,
    action || 'manage_asset',
    { ...context.args, subAction: action }
  ) as AssetOperationResponse;
  const errorCode = typeof res.error === 'string' ? res.error.toUpperCase() : '';
  const message = typeof res.message === 'string' ? res.message : '';

  if (errorCode === 'UNKNOWN_ACTION' || errorCode === 'INVALID_SUBACTION' ||
      message.toLowerCase().includes('unknown action') || message.toLowerCase().includes('unknown subaction')) {
    return cleanObject({
      success: false,
      error: 'UNKNOWN_ACTION',
      message: `Unknown asset action: ${action}`,
      action: action || 'manage_asset',
      assetPath: context.assetArgs.assetPath ?? context.assetArgs.path
    });
  }

  if (res.success === false) {
    return cleanObject({
      success: false,
      error: errorCode || 'OPERATION_FAILED',
      message: message || 'Asset operation failed',
      action: action || 'manage_asset',
      assetPath: context.assetArgs.assetPath ?? context.assetArgs.path,
      data: res
    });
  }

  return ResponseFactory.success(res, 'Asset action executed successfully');
}
