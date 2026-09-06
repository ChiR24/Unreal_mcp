import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import type { AssetHandlerContext } from './asset-handler-types.js';
import { assetSuccessResponse } from './asset-responses.js';
import { validatePathSecurity, validatePathsSecurity } from './asset-validation.js';

export async function handleBulkAssetAction(action: string, context: AssetHandlerContext): Promise<Record<string, unknown> | undefined> {
  switch (action) {
    case 'bulk_rename':
      return handleBulkRename(context);
    case 'bulk_delete':
      return handleBulkDelete(context);
    default:
      return undefined;
  }
}

async function handleBulkRename(context: AssetHandlerContext): Promise<Record<string, unknown>> {
  const folderPath = context.assetArgs.folderPath ?? context.assetArgs.path;
  const assetPaths = context.assetArgs.assetPaths ?? context.assetArgs.paths;
  const folderPathSecurity = validatePathSecurity(
    typeof folderPath === 'string' ? folderPath : undefined,
    'folderPath'
  );
  if (folderPathSecurity) return folderPathSecurity;

  const pathParamSecurity = validatePathSecurity(
    typeof context.assetArgs.path === 'string' ? context.assetArgs.path : undefined,
    'path'
  );
  if (pathParamSecurity) return pathParamSecurity;

  const assetPathsSecurity = validatePathsSecurity(assetPaths, 'assetPaths');
  if (assetPathsSecurity) return assetPathsSecurity;

  if (!folderPath && (!assetPaths || (Array.isArray(assetPaths) && assetPaths.length === 0))) {
    return ResponseFactory.errorWithCode('INVALID_ARGUMENT', 'Either folderPath or assetPaths is required for bulk_rename');
  }

  const searchText = typeof context.assetArgs.searchText === 'string'
    ? context.assetArgs.searchText
    : (typeof context.assetArgs.pattern === 'string' ? context.assetArgs.pattern : undefined);
  const replaceText = typeof context.assetArgs.replaceText === 'string'
    ? context.assetArgs.replaceText
    : (typeof context.assetArgs.replacement === 'string' ? context.assetArgs.replacement : undefined);

  const res = await executeAutomationRequest(context.tools, 'bulk_rename', {
    folderPath,
    assetPaths,
    searchText,
    replaceText,
    prefix: context.assetArgs.prefix,
    suffix: context.assetArgs.suffix,
    checkoutFiles: context.assetArgs.checkoutFiles
  });
  return assetSuccessResponse(res, 'Bulk rename completed', 'BULK_RENAME_FAILED', 'Bulk rename failed', {
    folderPath,
    assetPaths
  });
}

async function handleBulkDelete(context: AssetHandlerContext): Promise<Record<string, unknown>> {
  const folderPath = context.assetArgs.folderPath ?? context.assetArgs.path;
  const assetPaths = context.assetArgs.assetPaths ?? context.assetArgs.paths;
  const folderPathSecurity = validatePathSecurity(
    typeof folderPath === 'string' ? folderPath : undefined,
    'folderPath'
  );
  if (folderPathSecurity) return folderPathSecurity;

  const assetPathsSecurity = validatePathsSecurity(assetPaths, 'assetPaths');
  if (assetPathsSecurity) return assetPathsSecurity;

  if (!folderPath && (!assetPaths || (Array.isArray(assetPaths) && assetPaths.length === 0))) {
    return ResponseFactory.errorWithCode('INVALID_ARGUMENT', 'Either folderPath or assetPaths is required for bulk_delete');
  }

  const res = await executeAutomationRequest(context.tools, 'bulk_delete', {
    ...context.args,
    folderPath,
    assetPaths
  });
  return assetSuccessResponse(res, 'Bulk delete completed', 'BULK_DELETE_FAILED', 'Bulk delete failed', {
    folderPath,
    assetPaths
  });
}
