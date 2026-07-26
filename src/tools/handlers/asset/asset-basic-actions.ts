import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { normalizeAndSanitizeAssetPath } from '../../../utils/validation/validation.js';
import { executeAutomationRequest, getTimeoutMs } from '../foundation/dispatch/common-handlers.js';
import {
  extractOptionalBoolean,
  extractOptionalNumber,
  extractOptionalObject,
  extractOptionalString,
  extractString,
  normalizeArgs
} from '../foundation/arguments/argument-helper.js';
import type { AssetHandlerContext, AssetListItem, AssetListResponse, AssetOperationResponse } from './asset-handler-types.js';
import { assetSuccessResponse, failedOperationResponse } from './asset-responses.js';
import { validatePathsSecurity } from './asset-validation.js';

function normalizeDeletePath(path: string): string {
  let normalized = path.replace(/\\/g, '/').trim();
  const lastSlash = normalized.lastIndexOf('/');
  if (lastSlash >= 0) {
    const afterSlash = normalized.substring(lastSlash + 1);
    const dotIndex = afterSlash.indexOf('.');
    if (dotIndex > 0) {
      normalized = normalized.substring(0, lastSlash + 1 + dotIndex);
    }
  }
  return normalized;
}

const ASSET_LIST_DEFAULT_LIMIT = 50;
const ASSET_LIST_MAX_LIMIT = 500;
const ASSET_LIST_MIN_LIMIT = 1;

function resolveNumeric(raw: unknown): number | undefined {
  if (typeof raw === 'number') return Number.isFinite(raw) ? raw : undefined;
  if (typeof raw === 'string' && raw.trim() !== '' && Number.isFinite(Number(raw))) return Number(raw);
  return undefined;
}

function normalizeListLimit(raw: unknown): number {
  const value = resolveNumeric(raw);
  if (value === undefined) return ASSET_LIST_DEFAULT_LIMIT;
  return Math.min(ASSET_LIST_MAX_LIMIT, Math.max(ASSET_LIST_MIN_LIMIT, Math.floor(value)));
}

function normalizeListOffset(raw: unknown): number {
  const value = resolveNumeric(raw);
  if (value === undefined || value < 0) return 0;
  return Math.floor(value);
}

export async function handleListAssets(context: AssetHandlerContext): Promise<Record<string, unknown>> {
  const params = normalizeArgs(context.args, [
    { key: 'path', aliases: ['directory', 'directoryPath', 'assetPath'], default: '/Game' },
    { key: 'limit' },
    { key: 'offset' },
    { key: 'cursor' },
    { key: 'recursive', aliases: ['recursivePaths'], default: false },
    { key: 'depth' },
    { key: 'includeTags', default: false },
    { key: 'includeMetadata', aliases: ['withMetadata'], default: undefined }
  ]);

  const pagination = extractOptionalObject(params, 'pagination');

  const limit = normalizeListLimit(params['limit'] ?? (pagination ? pagination['limit'] : undefined));
  const offset = normalizeListOffset(params['offset'] ?? (pagination ? pagination['offset'] : undefined));
  const cursor = extractOptionalString(params, 'cursor');
  const recursive = extractOptionalBoolean(params, 'recursive') ?? false;
  const depth = extractOptionalNumber(params, 'depth');
  const includeTags = extractOptionalBoolean(params, 'includeTags') ?? false;
  const includeMetadata = extractOptionalBoolean(params, 'includeMetadata');

  // Canonical path normalization; also throws on traversal attempts, which the
  // domain dispatcher turns into a structured SECURITY_VIOLATION error.
  const path = normalizeAndSanitizeAssetPath(extractOptionalString(params, 'path') ?? '/Game');

  const res = await executeAutomationRequest(context.tools, 'list', {
    path,
    recursive: recursive === true || (depth !== undefined && depth > 0),
    depth,
    limit,
    offset,
    cursor: cursor ?? undefined,
    includeTags,
    includeMetadata
  }) as AssetListResponse;

  if (!res || res.success === false) {
    return ResponseFactory.errorWithCode(
      typeof res.error === 'string' ? res.error : 'LIST_FAILED',
      typeof res.message === 'string' ? res.message : 'Asset listing failed',
      { path, cursor: cursor ?? null }
    );
  }

  const assets: AssetListItem[] = Array.isArray(res.assets)
    ? res.assets
    : (Array.isArray(res.result) ? res.result : (res.result?.assets || []));
  const folders: string[] = Array.isArray(res.folders) ? res.folders : (Array.isArray(res.result) ? [] : (res.result?.folders || []));
  const totalCount = typeof res.totalCount === 'number' ? res.totalCount : assets.length;
  const count = typeof res.count === 'number' ? res.count : assets.length;
  const returnedLimit = typeof res.limit === 'number' ? res.limit : limit;
  const returnedOffset = typeof res.offset === 'number' ? res.offset : offset;
  const hasMore = res.hasMore === true;
  const nextOffset = typeof res.nextOffset === 'number' ? res.nextOffset : (hasMore ? returnedOffset + count : totalCount);
  const returnedCursor = typeof res.cursor === 'string' && res.cursor.length > 0 ? res.cursor : (cursor ?? null);
  const nextCursor = typeof res.nextCursor === 'string' && res.nextCursor.length > 0 ? res.nextCursor : null;

  let message = `Listed ${count} asset(s) (offset ${returnedOffset}, limit ${returnedLimit}, total ${totalCount})`;
  message += ` in ${path}`;
  if (folders.length > 0) message += `; ${folders.length} subfolder(s)`;
  if (hasMore) message += '; more available';

  const rawTruncated = res.metadataTruncated ??
    (Array.isArray(res.result) ? undefined : (res.result as Record<string, unknown> | undefined)?.metadataTruncated);
  return ResponseFactory.success({
    assets,
    folders,
    totalCount,
    count,
    limit: returnedLimit,
    offset: returnedOffset,
    hasMore,
    nextOffset,
    cursor: returnedCursor,
    nextCursor,
    ...(typeof rawTruncated === 'boolean' ? { metadataTruncated: rawTruncated } : {})
  }, message);
}

export async function handleCreateFolder(context: AssetHandlerContext): Promise<Record<string, unknown>> {
  const folderPath = extractString(normalizeArgs(context.args, [
    { key: 'path', aliases: ['directoryPath'], required: true }
  ]), 'path').trim();
  if (!folderPath.startsWith('/')) {
    return ResponseFactory.error('VALIDATION_ERROR', `Invalid folder path: '${folderPath}'. Path must start with '/'`);
  }
  const res = await executeAutomationRequest(context.tools, 'manage_asset', {
    path: folderPath,
    subAction: 'create_folder'
  }) as AssetOperationResponse;
  return assetSuccessResponse(res, 'Folder created successfully', 'CREATE_FOLDER_FAILED', 'Folder creation failed', { path: folderPath });
}

export async function handleImportAsset(context: AssetHandlerContext): Promise<Record<string, unknown>> {
  const params = normalizeArgs(context.args, [
    { key: 'sourcePath', required: true },
    { key: 'destinationPath', required: true },
    { key: 'overwrite', default: false },
    { key: 'save', default: true }
  ]);
  const sourcePath = extractString(params, 'sourcePath');
  const destinationPath = extractString(params, 'destinationPath');
  const res = await executeAutomationRequest(context.tools, 'manage_asset', {
    sourcePath,
    destinationPath,
    overwrite: extractOptionalBoolean(params, 'overwrite') ?? false,
    save: extractOptionalBoolean(params, 'save') ?? true,
    subAction: 'import'
  }) as AssetOperationResponse;
  if (res.success === false) {
    return failedOperationResponse(res, 'IMPORT_FAILED', 'Asset import failed', { sourcePath, destinationPath });
  }
  return ResponseFactory.success(res, 'Asset imported successfully');
}

export async function handleDuplicateAsset(context: AssetHandlerContext): Promise<Record<string, unknown>> {
  const params = normalizeArgs(context.args, [
    { key: 'sourcePath', aliases: ['assetPath'], required: true },
    { key: 'destinationPath' },
    { key: 'newName' }
  ]);
  const sourcePath = extractString(params, 'sourcePath');
  let destinationPath = extractOptionalString(params, 'destinationPath');
  const newName = extractOptionalString(params, 'newName');
  if (newName) {
    if (!destinationPath) {
      const lastSlash = sourcePath.lastIndexOf('/');
      destinationPath = `${lastSlash > 0 ? sourcePath.substring(0, lastSlash) : '/Game'}/${newName}`;
    } else if (!destinationPath.endsWith(newName) && destinationPath.endsWith('/')) {
      destinationPath = `${destinationPath}${newName}`;
    }
  }
  if (!destinationPath) throw new Error('destinationPath or newName is required for duplicate action');

  const res = await executeAutomationRequest(context.tools, 'manage_asset', {
    sourcePath,
    destinationPath,
    subAction: 'duplicate'
  }) as AssetOperationResponse;
  return assetSuccessResponse(res, 'Asset duplicated successfully', 'DUPLICATE_FAILED', 'Asset duplication failed', { sourcePath, destinationPath });
}

export async function handleRenameAsset(context: AssetHandlerContext): Promise<Record<string, unknown>> {
  const params = normalizeArgs(context.args, [
    { key: 'sourcePath', aliases: ['assetPath'], required: true },
    { key: 'destinationPath' },
    { key: 'newName' }
  ]);
  const sourcePath = extractString(params, 'sourcePath');
  const newName = extractOptionalString(params, 'newName');
  let destinationPath = extractOptionalString(params, 'destinationPath');
  if (!destinationPath && newName) {
    const lastSlash = sourcePath.lastIndexOf('/');
    destinationPath = `${lastSlash > 0 ? sourcePath.substring(0, lastSlash) : '/Game'}/${newName}`;
  }
  if (!destinationPath) throw new Error('Missing destinationPath or newName');

  const payload: Record<string, unknown> = { sourcePath, destinationPath, subAction: 'rename' };
  if (newName) payload.newName = newName;
  const res = await executeAutomationRequest(context.tools, 'manage_asset', payload) as AssetOperationResponse;
  if (res.success === false && String(res.message || '').toLowerCase().includes('exists')) {
    return cleanObject({
      success: false,
      error: 'ASSET_ALREADY_EXISTS',
      message: res.message || 'Asset already exists at destination',
      sourcePath,
      destinationPath
    });
  }
  return cleanObject(res);
}

export async function handleMoveAsset(context: AssetHandlerContext): Promise<Record<string, unknown>> {
  const params = normalizeArgs(context.args, [
    { key: 'sourcePath', aliases: ['assetPath'], required: true },
    { key: 'destinationPath' }
  ]);
  const sourcePath = extractString(params, 'sourcePath');
  let destinationPath = extractOptionalString(params, 'destinationPath');
  const assetName = sourcePath.split('/').pop();
  if (assetName && destinationPath && !destinationPath.endsWith(assetName)) {
    destinationPath = `${destinationPath.replace(/\/$/, '')}/${assetName}`;
  }
  const res = await executeAutomationRequest(context.tools, 'manage_asset', {
    sourcePath,
    destinationPath: destinationPath ?? '',
    subAction: 'move'
  }) as AssetOperationResponse;
  return assetSuccessResponse(res, 'Asset moved successfully', 'MOVE_FAILED', 'Asset move failed', { sourcePath, destinationPath: destinationPath ?? '' });
}

export async function handleDeleteAssets(context: AssetHandlerContext): Promise<Record<string, unknown>> {
  let paths: string[] = [];
  if (Array.isArray(context.assetArgs.paths)) {
    paths = context.assetArgs.paths.filter((path): path is string => typeof path === 'string' && path.trim().length > 0);
  } else if (Array.isArray(context.assetArgs.assetPaths) || Array.isArray(context.assetArgs.asset_paths)) {
    paths = (context.assetArgs.assetPaths || context.assetArgs.asset_paths) as string[];
  } else {
    const single = context.assetArgs.assetPath || context.assetArgs.asset_path || context.assetArgs.path;
    if (typeof single === 'string' && single.trim()) paths = [single.trim()];
  }
  if (paths.length === 0) {
    return ResponseFactory.error('INVALID_ARGUMENT', 'No paths provided for delete action. Provide assetPath (string) or assetPaths (array).');
  }

  const securityError = validatePathsSecurity(paths, 'paths');
  if (securityError) return securityError;

  const normalizedPaths = paths.map(normalizeDeletePath);
  const timeoutMs = typeof context.args.timeoutMs === 'number'
    ? context.args.timeoutMs
    : getTimeoutMs();
  const res = await executeAutomationRequest(context.tools, 'manage_asset', {
    paths: normalizedPaths,
    subAction: 'delete'
  }, 'Automation bridge not available for asset deletion', { timeoutMs }) as AssetOperationResponse;
  if (res.success === false) {
    return failedOperationResponse(res, 'OPERATION_FAILED', 'Asset deletion failed', { paths: normalizedPaths });
  }
  return ResponseFactory.success(res, 'Assets deleted successfully');
}
