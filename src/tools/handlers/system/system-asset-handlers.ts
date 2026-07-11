import { cleanObject } from '../../../utils/serialization/safe-json.js';
import type { ITools } from '../../../types/tools/tool-interfaces.js';
import type { SystemArgs } from '../../../types/handlers/handler-types.js';
import { executeAutomationRequest } from '../foundation/dispatch/common-handlers.js';
import type { AssetValidationResult, OperationResponse } from './system-handler-types.js';

function hasMessage(value: unknown): value is { readonly message: unknown } {
  return value !== null && typeof value === 'object' && 'message' in value;
}

function errorToString(error: unknown): string | null {
  if (!error) return null;
  if (typeof error === 'string') return error;
  if (hasMessage(error)) return String(error.message);
  return String(error);
}

export async function handleGetProjectSettings(args: SystemArgs, tools: ITools): Promise<Record<string, unknown>> {
  const section = typeof args.category === 'string' && args.category.trim().length > 0
    ? args.category
    : args.section;
  const response = await executeAutomationRequest(tools, 'system_control', {
    action: 'get_project_settings',
    section
  }) as OperationResponse;

  if (response && response.success && (response.settings || response.data || response.result)) {
    return cleanObject({
      success: true,
      message: 'Project settings retrieved',
      settings: response.settings || response.data || response.result,
      ...response
    });
  }

  return cleanObject(response);
}

export async function handleValidateAssets(args: SystemArgs, tools: ITools): Promise<Record<string, unknown>> {
  const argsRecord = args as Record<string, unknown>;
  const paths: string[] = Array.isArray(argsRecord.paths)
    ? argsRecord.paths.filter((path): path is string => typeof path === 'string' && path.trim().length > 0)
    : [];
  const singlePath = typeof args.assetPath === 'string' && args.assetPath.trim().length > 0
    ? args.assetPath
    : (typeof args.path === 'string' && args.path.trim().length > 0 ? args.path : undefined);

  if (singlePath && !paths.includes(singlePath)) {
    paths.push(singlePath);
  }

  if (!paths.length) {
    return {
      success: false,
      error: 'INVALID_ARGUMENT',
      message: 'Please provide "paths", "assetPath", or "path" to validate assets.',
      action: 'validate_assets',
      results: []
    };
  }

  const response = await executeAutomationRequest(tools, 'system_control', {
    action: 'validate_assets',
    paths,
    recursive: args.recursive
  }) as Record<string, unknown>;
  const responseResult = typeof response.result === 'object' && response.result !== null && !Array.isArray(response.result)
    ? response.result as Record<string, unknown>
    : undefined;

  if (Array.isArray(response.results) || Array.isArray(responseResult?.results)) {
    return cleanObject({ ...response, ...responseResult, action: 'validate_assets' });
  }

  const results = await Promise.all(paths.map(async (assetPath): Promise<AssetValidationResult> => {
    try {
      const response = await executeAutomationRequest(tools, 'manage_asset', {
        action: 'exists',
        subAction: 'exists',
        assetPath
      }) as Record<string, unknown>;
      const result = typeof response.result === 'object' && response.result !== null
        ? response.result as Record<string, unknown>
        : undefined;
      const exists = typeof response.exists === 'boolean'
        ? response.exists
        : (typeof result?.exists === 'boolean' ? result.exists : false);
      const error = errorToString(response.error) ?? (exists ? null : 'Asset does not exist');
      return { assetPath, success: exists, error };
    } catch (error) {
      return {
        assetPath,
        success: false,
        error: error instanceof Error ? error.message : String(error)
      };
    }
  }));

  const allValid = results.every((result) => result.success !== false);
  return {
    success: allValid,
    message: allValid ? 'Asset validation completed' : 'Asset validation failed for one or more assets',
    action: 'validate_assets',
    results
  };
}

export async function handleExportAsset(args: SystemArgs, tools: ITools): Promise<Record<string, unknown>> {
  const assetPath = typeof args.assetPath === 'string' ? args.assetPath : '';
  const argsRecord = args as Record<string, unknown>;
  const exportPath = typeof argsRecord.exportPath === 'string' ? argsRecord.exportPath : '';

  if (!assetPath) {
    return {
      success: false,
      error: 'INVALID_ARGUMENT',
      message: 'assetPath is required for export_asset',
      action: 'export_asset'
    };
  }

  if (!exportPath) {
    return {
      success: false,
      error: 'INVALID_ARGUMENT',
      message: 'exportPath is required for export_asset',
      action: 'export_asset'
    };
  }

  const response = await executeAutomationRequest(
    tools,
    'system_control',
    { action: 'export_asset', assetPath, exportPath },
    'Export functionality not available - ensure editor is running'
  ) as OperationResponse;

  if (response && response.success) {
    return cleanObject({
      success: true,
      message: `Asset exported to ${exportPath}`,
      action: 'export_asset',
      assetPath,
      exportPath,
      ...response
    });
  }

  return cleanObject({
    success: false,
    error: response?.error || 'EXPORT_FAILED',
    message: response?.message || 'Export operation failed',
    action: 'export_asset',
    assetPath,
    exportPath
  });
}
