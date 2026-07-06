import type { LevelArgs } from '../../../../types/handlers/handler-types.js';
import { normalizePathFields } from '../../foundation/dispatch/common-handlers.js';

export const LEVEL_STRUCTURE_ONLY_ACTIONS = new Set([
  'load_cells',
  'set_datalayer',
  'cleanup_invalid_datalayers',
  'create_datalayer'
]);

export const LIGHT_TYPE_CLASS_PATHS = new Map<string, string>([
  ['point', '/Script/Engine.PointLight'],
  ['pointlight', '/Script/Engine.PointLight'],
  ['directional', '/Script/Engine.DirectionalLight'],
  ['directionallight', '/Script/Engine.DirectionalLight'],
  ['spot', '/Script/Engine.SpotLight'],
  ['spotlight', '/Script/Engine.SpotLight'],
  ['sky', '/Script/Engine.SkyLight'],
  ['skylight', '/Script/Engine.SkyLight'],
  ['rect', '/Script/Engine.RectLight'],
  ['rectlight', '/Script/Engine.RectLight']
]);

export const CREATE_LIGHT_TYPES = new Set<string>([
  'point',
  'directional',
  'spot',
  'sky',
  'rect',
  'pointlight',
  'directionallight',
  'spotlight',
  'skylight',
  'rectlight'
]);

export interface ResultPayload {
  exists?: boolean;
  path?: string;
  class?: string;
  error?: string;
  message?: string;
  [key: string]: unknown;
}

export function normalizeLightType(lightType: unknown): string | undefined {
  return typeof lightType === 'string' ? lightType.trim().toLowerCase() : undefined;
}

function normalizeLevelPathValue(path: string): string {
  const normalized = normalizePathFields({ path }, ['path']);
  return normalized.path as string;
}

function normalizeLevelPathArray(paths: unknown): string[] | undefined {
  if (!Array.isArray(paths)) return undefined;
  return paths
    .filter((path): path is string => typeof path === 'string')
    .map(path => normalizeLevelPathValue(path));
}

export function normalizeLevelArgs(args: LevelArgs): LevelArgs {
  const raw = args as Record<string, unknown>;
  const assetPath = typeof raw.assetPath === 'string' ? raw.assetPath : undefined;
  const targetPath = typeof raw.targetPath === 'string' ? raw.targetPath : undefined;
  const pathAlias = typeof raw.path === 'string' ? raw.path : undefined;
  const mapped = {
    ...args,
    levelPath: (raw.level_path as string | undefined) ?? args.levelPath ?? assetPath ?? pathAlias,
    levelName: (raw.level_name as string | undefined) ?? args.levelName,
    savePath: (raw.save_path as string | undefined) ?? args.savePath,
    destinationPath: (raw.destination_path as string | undefined) ?? args.destinationPath ?? targetPath,
    subLevelPath: (raw.sublevelPath as string | undefined) ?? (raw.sub_level_path as string | undefined) ?? args.subLevelPath,
    parentLevel: (raw.parent_level as string | undefined) ?? args.parentLevel,
    parentPath: (raw.parent_path as string | undefined) ?? args.parentPath,
    streamingMethod: (raw.streaming_method as 'Blueprint' | 'AlwaysLoaded' | undefined) ?? args.streamingMethod,
    sourcePath: (raw.source_path as string | undefined) ?? args.sourcePath,
    newName: (raw.new_name as string | undefined) ?? (args as Record<string, unknown>).newName as string | undefined,
    levelPaths: normalizeLevelPathArray(raw.level_paths ?? args.levelPaths),
    packagePath: (raw.package_path as string | undefined) ?? args.packagePath,
    exportPath: (raw.export_path as string | undefined) ?? args.exportPath,
  };
  return normalizePathFields(mapped as Record<string, unknown>, [
    'levelPath',
    'savePath',
    'destinationPath',
    'subLevelPath',
    'parentLevel',
    'parentPath',
    'sourcePath',
    'levelPaths',
    'level_paths',
    'packagePath',
    'exportPath',
    'path'
  ]) as LevelArgs;
}
