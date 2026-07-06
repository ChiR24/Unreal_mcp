import { cleanObject } from '../../../../utils/serialization/safe-json.js';
import type { ITools } from '../../../../types/tools/tool-interfaces.js';
import type { HandlerArgs, LevelArgs } from '../../../../types/handlers/handler-types.js';
import { executeAutomationRequest, requireNonEmptyString, validateSecurityPatterns } from '../../foundation/dispatch/common-handlers.js';
import {
  LEVEL_STRUCTURE_ONLY_ACTIONS,
  normalizeLevelArgs
} from './level-handler-utils.js';
import {
  handleCreateLevel,
  handleDeleteLevel,
  handleDuplicateLevel,
  handleExportLevel,
  handleImportLevel,
  handleRenameLevel,
  handleValidateLevel
} from './level-asset-handlers.js';
import {
  handleBuildLighting,
  handleCreateLight,
  handleSpawnLight
} from './level-light-handlers.js';

export async function handleLevelTools(action: string, args: HandlerArgs, tools: ITools): Promise<Record<string, unknown>> {
  const argsTyped = normalizeLevelArgs(args as LevelArgs);

  if (LEVEL_STRUCTURE_ONLY_ACTIONS.has(action)) {
    return cleanObject({
      success: false,
      error: 'INVALID_ACTION',
      message: `${action} belongs to manage_level_structure/world partition handlers, not manage_level`,
      action,
      tool: 'manage_level_structure'
    });
  }

  const securityError = validateSecurityPatterns(argsTyped as Record<string, unknown>);
  if (securityError) {
    return cleanObject({
      success: false,
      error: 'SECURITY_VIOLATION',
      message: securityError,
      action
    });
  }

  switch (action) {
    case 'load':
    case 'load_level':
      return await handleLoadLevel(argsTyped, tools);

    case 'save':
    case 'save_level':
      return await handleSaveLevel(argsTyped, tools);

    case 'save_as':
    case 'save_level_as':
      return await handleSaveLevelAs(action, argsTyped, tools);

    case 'create_level':
      return await handleCreateLevel(argsTyped, tools);

    case 'add_sublevel':
      return await handleAddSublevel(argsTyped, tools);

    case 'stream':
      return await handleStreamLevel(action, argsTyped, tools);

    case 'unload':
      return await handleUnloadLevel(action, argsTyped, tools);

    case 'create_light':
      return await handleCreateLight(action, argsTyped, tools);

    case 'spawn_light':
      return await handleSpawnLight(args, argsTyped, tools);

    case 'build_lighting':
      return await handleBuildLighting(args, argsTyped, tools);

    case 'export_level':
      return await handleExportLevel(argsTyped, tools);

    case 'import_level':
      return await handleImportLevel(argsTyped, tools);

    case 'list_levels':
      return cleanObject(await executeAutomationRequest(tools, 'list_levels', {})) as Record<string, unknown>;

    case 'get_summary':
      return cleanObject(await executeAutomationRequest(tools, 'manage_level', {
        action: 'get_summary',
        levelPath: argsTyped.levelPath
      })) as Record<string, unknown>;

    case 'delete':
    case 'delete_level':
      return await handleDeleteLevel(action, argsTyped, tools);

    case 'rename_level':
      return await handleRenameLevel(action, argsTyped, tools);

    case 'duplicate_level':
      return await handleDuplicateLevel(action, argsTyped, tools);

    case 'get_current_level':
      return cleanObject(await executeAutomationRequest(tools, 'manage_level', { action })) as Record<string, unknown>;

    case 'set_metadata':
      return await handleSetMetadata(argsTyped, tools);

    case 'validate_level':
      return await handleValidateLevel(argsTyped, tools);

    default:
      return await executeAutomationRequest(tools, 'manage_level', args) as Record<string, unknown>;
  }
}

async function handleLoadLevel(argsTyped: LevelArgs, tools: ITools): Promise<Record<string, unknown>> {
  const levelPath = requireNonEmptyString(argsTyped.levelPath, 'levelPath', 'Missing required parameter: levelPath');
  const res = await executeAutomationRequest(tools, 'manage_level', {
    action: 'load',
    levelPath,
    streaming: !!argsTyped.streaming,
    saveDirtyPackages: !!argsTyped.saveDirtyPackages
  }) as Record<string, unknown>;
  return cleanObject(res);
}

async function handleSaveLevel(argsTyped: LevelArgs, tools: ITools): Promise<Record<string, unknown>> {
  const targetPath = argsTyped.levelPath || argsTyped.savePath;
  if (targetPath) {
    const res = await executeAutomationRequest(tools, 'manage_level', {
      action: 'save_level_as',
      savePath: targetPath
    }) as Record<string, unknown>;
    return cleanObject(res);
  }
  const res = await executeAutomationRequest(tools, 'manage_level', {
    action: 'save',
    levelName: argsTyped.levelName
  }) as Record<string, unknown>;
  return cleanObject(res);
}

async function handleSaveLevelAs(action: string, argsTyped: LevelArgs, tools: ITools): Promise<Record<string, unknown>> {
  const targetPath = argsTyped.savePath || argsTyped.destinationPath || argsTyped.levelPath;
  if (!targetPath) {
    return {
      success: false,
      error: 'INVALID_ARGUMENT',
      message: 'savePath is required for save_as action',
      action
    };
  }
  const res = await executeAutomationRequest(tools, 'manage_level', {
    action: 'save_level_as',
    savePath: targetPath
  }) as Record<string, unknown>;
  return cleanObject(res);
}

async function handleAddSublevel(argsTyped: LevelArgs, tools: ITools): Promise<Record<string, unknown>> {
  const subLevelPath = requireNonEmptyString(argsTyped.subLevelPath || argsTyped.levelPath, 'subLevelPath', 'Missing required parameter: subLevelPath');
  const res = await executeAutomationRequest(tools, 'manage_level', {
    action: 'add_sublevel',
    subLevelPath,
    parentLevel: argsTyped.parentLevel || argsTyped.parentPath,
    streamingMethod: argsTyped.streamingMethod
  }) as Record<string, unknown>;
  return cleanObject(res);
}

async function handleStreamLevel(action: string, argsTyped: LevelArgs, tools: ITools): Promise<Record<string, unknown>> {
  const levelPath = typeof argsTyped.levelPath === 'string' ? argsTyped.levelPath : undefined;
  const levelName = typeof argsTyped.levelName === 'string' ? argsTyped.levelName : undefined;
  if (!levelPath && !levelName) {
    return cleanObject({
      success: false,
      error: 'INVALID_ARGUMENT',
      message: 'Missing required parameter: levelPath (or levelName)',
      action
    });
  }
  if (typeof argsTyped.shouldBeLoaded !== 'boolean') {
    return cleanObject({
      success: false,
      error: 'INVALID_ARGUMENT',
      message: 'Missing required parameter: shouldBeLoaded (boolean)',
      action,
      levelPath,
      levelName
    });
  }

  const res = await executeAutomationRequest(tools, 'stream_level', {
    levelPath,
    levelName,
    shouldBeLoaded: argsTyped.shouldBeLoaded,
    shouldBeVisible: argsTyped.shouldBeVisible
  }) as Record<string, unknown>;
  return cleanObject(res);
}

async function handleUnloadLevel(action: string, argsTyped: LevelArgs, tools: ITools): Promise<Record<string, unknown>> {
  const levelPath = typeof argsTyped.levelPath === 'string' ? argsTyped.levelPath : undefined;
  const levelName = typeof argsTyped.levelName === 'string' ? argsTyped.levelName : undefined;
  if (!levelPath && !levelName) {
    return cleanObject({
      success: false,
      error: 'INVALID_ARGUMENT',
      message: 'Missing required parameter: levelPath (or levelName)',
      action
    });
  }
  const res = await executeAutomationRequest(tools, 'stream_level', {
    levelPath,
    levelName,
    shouldBeLoaded: false,
    shouldBeVisible: false
  }) as Record<string, unknown>;
  return cleanObject(res);
}

async function handleSetMetadata(argsTyped: LevelArgs, tools: ITools): Promise<Record<string, unknown>> {
  const levelPath = requireNonEmptyString(argsTyped.levelPath, 'levelPath', 'Missing required parameter: levelPath');
  const metadata = (argsTyped.metadata && typeof argsTyped.metadata === 'object') ? argsTyped.metadata : {};
  const res = await executeAutomationRequest(tools, 'set_metadata', { assetPath: levelPath, metadata });
  return cleanObject(res) as Record<string, unknown>;
}
