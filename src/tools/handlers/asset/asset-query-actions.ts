import { cleanObject } from '../../../utils/serialization/safe-json.js';
import { ResponseFactory } from '../../../utils/responses/response-factory.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import {
  extractOptionalArray,
  extractOptionalBoolean,
  extractOptionalNumber,
  extractOptionalString,
  extractString,
  normalizeArgs
} from '../foundation/arguments/argument-helper.js';
import type { AssetHandlerContext, AssetOperationResponse } from './asset-handler-types.js';
import { assetSuccessResponse, failedOperationResponse } from './asset-responses.js';
import { validatePathSecurity, validatePathsSecurity } from './asset-validation.js';

export async function handleAssetMetadataAction(action: string, context: AssetHandlerContext): Promise<Record<string, unknown> | undefined> {
  switch (action) {
    case 'generate_lods': {
      const params = normalizeArgs(context.args, [{ key: 'assetPath', required: true }, { key: 'lodCount', required: true }]);
      const res = await executeAutomationRequest(context.tools, 'manage_asset', {
        assetPath: extractString(params, 'assetPath'),
        lodCount: typeof params.lodCount === 'number' ? params.lodCount : Number(params.lodCount),
        subAction: 'generate_lods'
      }) as AssetOperationResponse;
      return assetSuccessResponse(res, 'LODs generated successfully', 'GENERATE_LODS_FAILED', 'LODs generation failed');
    }
    case 'create_thumbnail': {
      const params = normalizeArgs(context.args, [{ key: 'assetPath', required: true }, { key: 'width' }, { key: 'height' }]);
      const res = await executeAutomationRequest(context.tools, 'manage_asset', {
        assetPath: extractString(params, 'assetPath'),
        width: extractOptionalNumber(params, 'width'),
        height: extractOptionalNumber(params, 'height'),
        subAction: 'generate_thumbnail'
      }) as AssetOperationResponse;
      return assetSuccessResponse(res, 'Thumbnail created successfully', 'THUMBNAIL_FAILED', 'Thumbnail creation failed');
    }
    case 'set_tags': {
      const params = normalizeArgs(context.args, [{ key: 'assetPath', required: true }, { key: 'tags', required: true }]);
      const res = await executeAutomationRequest(context.tools, 'set_tags', {
        assetPath: extractString(params, 'assetPath'),
        tags: extractOptionalArray<string>(params, 'tags') ?? []
      });
      return assetSuccessResponse(res, 'Tags set successfully', 'SET_TAGS_FAILED', 'Tag update failed');
    }
    case 'set_metadata': {
      const params = normalizeArgs(context.args, [{ key: 'assetPath', required: true }, { key: 'metadata', required: true }]);
      const assetPath = extractString(params, 'assetPath');
      const metadata = params.metadata as Record<string, unknown>;
      const res = await executeAutomationRequest(context.tools, 'set_metadata', { ...context.args, assetPath, metadata });
      return assetSuccessResponse(res, 'Metadata set successfully', 'SET_METADATA_FAILED', 'Metadata update failed');
    }
    case 'get_metadata': {
      return handleGetMetadata(context);
    }
    case 'validate':
    case 'validate_asset': {
      const assetPath = extractString(normalizeArgs(context.args, [{ key: 'assetPath', required: true }]), 'assetPath');
      const res = await executeAutomationRequest(context.tools, 'manage_asset', { assetPath, subAction: 'validate' }) as AssetOperationResponse;
      return assetSuccessResponse(res, 'Asset validation complete', 'VALIDATE_FAILED', 'Asset validation failed');
    }
    case 'generate_report': {
      const params = normalizeArgs(context.args, [{ key: 'directory' }, { key: 'reportType' }, { key: 'outputPath' }]);
      const res = await executeAutomationRequest(context.tools, 'manage_asset', {
        directory: extractOptionalString(params, 'directory') ?? '',
        reportType: extractOptionalString(params, 'reportType'),
        outputPath: extractOptionalString(params, 'outputPath'),
        subAction: 'generate_report'
      }) as AssetOperationResponse;
      return assetSuccessResponse(res, 'Report generated successfully', 'REPORT_FAILED', 'Report generation failed');
    }
    case 'search_assets': {
      return handleSearchAssets(context);
    }
    case 'find_by_tag': {
      return handleFindByTag(context);
    }
    case 'get_dependencies':
    case 'get_source_control_state':
    case 'source_control_enable':
    case 'analyze_graph':
    case 'fixup_redirectors':
    case 'exists':
      return handleSimpleQueryAction(action, context);
    default:
      return undefined;
  }
}

async function handleGetMetadata(context: AssetHandlerContext): Promise<Record<string, unknown>> {
  const params = normalizeArgs(context.args, [{ key: 'assetPath', required: true }]);
  const assetPath = extractString(params, 'assetPath');
  const res = await executeAutomationRequest(context.tools, 'manage_asset', {
    assetPath,
    subAction: 'get_metadata'
  }) as AssetOperationResponse;
  if (res.success === false) {
    return failedOperationResponse(res, 'GET_METADATA_FAILED', 'Metadata read failed', { assetPath });
  }
  const tags = res.tags || {};
  const metadata = res.metadata || {};
  const merged = { ...tags, ...metadata };
  const cleanRes = cleanObject(res);
  cleanRes.message = `Metadata retrieved (${Object.keys(merged).length} items)`;
  cleanRes.tags = tags;
  if (Object.keys(metadata).length > 0) {
    cleanRes.metadata = metadata;
  }
  return ResponseFactory.success(cleanRes, cleanRes.message as string);
}

async function handleSearchAssets(context: AssetHandlerContext): Promise<Record<string, unknown>> {
  const params = normalizeArgs(context.args, [
    { key: 'searchText' }, { key: 'classNames' }, { key: 'packagePaths' }, { key: 'recursivePaths' },
    { key: 'recursiveClasses' }, { key: 'limit' }, { key: 'offset' }
  ]);
  const searchText = extractOptionalString(params, 'searchText');
  const packagePaths = extractOptionalArray<string>(params, 'packagePaths');
  const securityError = validatePathsSecurity(packagePaths, 'packagePaths');
  if (securityError) return securityError;

  const res = await executeAutomationRequest(context.tools, 'asset_query', {
    searchText,
    classNames: extractOptionalArray<string>(params, 'classNames'),
    packagePaths,
    recursivePaths: extractOptionalBoolean(params, 'recursivePaths') ?? (searchText ? true : undefined),
    recursiveClasses: extractOptionalBoolean(params, 'recursiveClasses'),
    limit: extractOptionalNumber(params, 'limit'),
    offset: extractOptionalNumber(params, 'offset'),
    subAction: 'search_assets'
  }) as AssetOperationResponse;
  return assetSuccessResponse(res, 'Assets found', 'SEARCH_FAILED', 'Asset search failed');
}

async function handleFindByTag(context: AssetHandlerContext): Promise<Record<string, unknown>> {
  const params = normalizeArgs(context.args, [{ key: 'tag', required: true }, { key: 'value' }]);
  const assetPathCheck = validatePathSecurity(context.assetArgs.assetPath, 'assetPath') ?? validatePathSecurity(context.assetArgs.path, 'path');
  if (assetPathCheck) return assetPathCheck;

  const res = await executeAutomationRequest(context.tools, 'asset_query', {
    tag: extractString(params, 'tag'),
    value: extractOptionalString(params, 'value'),
    subAction: 'find_by_tag'
  }) as AssetOperationResponse;
  return assetSuccessResponse(res, 'Assets found by tag', 'FIND_BY_TAG_FAILED', 'Tag search failed');
}

export async function handleSimpleQueryAction(action: string, context: AssetHandlerContext): Promise<Record<string, unknown>> {
  if (action === 'source_control_enable') {
    const provider = extractOptionalString(normalizeArgs(context.args, [{ key: 'provider', default: 'None' }]), 'provider') ?? 'None';
    return assetSuccessResponse(await executeAutomationRequest(context.tools, 'source_control_enable', { provider }), 'Source control enabled', 'SOURCE_CONTROL_FAILED', 'Source control enable failed');
  }
  if (action === 'fixup_redirectors') {
    const directoryRaw = typeof context.assetArgs.directory === 'string' && context.assetArgs.directory.trim().length > 0
      ? context.assetArgs.directory.trim()
      : (typeof context.assetArgs.directoryPath === 'string' && context.assetArgs.directoryPath.trim().length > 0 ? context.assetArgs.directoryPath.trim() : '');
    const payload: Record<string, unknown> = { ...context.args };
    if (directoryRaw) payload.directoryPath = directoryRaw;
    return assetSuccessResponse(await executeAutomationRequest(context.tools, 'fixup_redirectors', payload), 'Redirectors fixed up successfully', 'FIXUP_REDIRECTORS_FAILED', 'Redirector fixup failed');
  }

  // The manage_asset schema advertises an `assetPaths` array for get_source_control_state,
  // and the handler this routes to reads it (AssetWorkflow/...SourceControlState.cpp takes
  // assetPath OR assetPaths). This wrapper still required the singular assetPath
  // unconditionally and never forwarded the array, so an array-only call failed with
  // "Missing required argument: assetPath" before it could reach that handler.
  const assetPathsRaw = action === 'get_source_control_state' ? context.args.assetPaths : undefined;
  const assetPaths = Array.isArray(assetPathsRaw)
    ? assetPathsRaw.filter((p): p is string => typeof p === 'string' && p.trim().length > 0).map(p => p.trim())
    : [];
  const params = normalizeArgs(context.args, [
    { key: 'assetPath', required: action !== 'analyze_graph' && assetPaths.length === 0 },
    { key: 'recursive' },
    { key: 'maxDepth' }
  ]);
  const assetPath = assetPaths.length > 0
    ? (extractOptionalString(params, 'assetPath') ?? '')
    : extractString(params, 'assetPath');
  const payload = action === 'analyze_graph'
    ? { assetPath, maxDepth: extractOptionalNumber(params, 'maxDepth') }
    : {
      assetPath,
      recursive: extractOptionalBoolean(params, 'recursive'),
      subAction: action,
      ...(assetPaths.length > 0 ? { assetPaths } : {})
    };
  // get_source_control_state routes through canonical manage_asset; the C++
  // HandleAssetAction resolves subAction to the concrete leaf handler, so the
  // #608 raw-action hang no longer applies. analyze_graph keeps get_asset_graph.
  const requestAction = action === 'analyze_graph'
    ? 'get_asset_graph'
    : action === 'get_source_control_state'
      ? 'manage_asset'
      : action === 'exists'
        ? 'exists'
        : 'manage_asset';
  const messages: Record<string, string> = {
    get_dependencies: 'Dependencies retrieved',
    get_source_control_state: 'Source control state retrieved',
    analyze_graph: 'Graph analysis complete',
    exists: 'Asset existence check complete'
  };
  return assetSuccessResponse(await executeAutomationRequest(context.tools, requestAction, payload), messages[action] ?? 'Asset action complete');
}
