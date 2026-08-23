import type { EffectArgs } from '../../../types/handlers/handler-types.js';
import { sanitizePath } from '../../../utils/paths/path-security.js';

export function ensureActionAndSubAction(action: string, args: Record<string, unknown>): void {
  if (!args || typeof args !== 'object') return;
  if (!args.action) {
    args.action = action;
  }
  if (!args.subAction) {
    args.subAction = args.action;
  }
}

export function sanitizeEffectPaths(args: Record<string, unknown>): void {
  const pathFields = [
    'path',
    'savePath',
    'assetPath',
    'systemPath',
    'emitterPath',
    'material',
    'materialPath',
    'mesh',
    'meshPath',
    'skeletalMeshPath',
    'staticMeshPath',
    'preset',
    'system'
  ];

  for (const field of pathFields) {
    const value = args[field];
    if (typeof value === 'string' && value.trim().startsWith('/')) {
      args[field] = sanitizePath(value);
    }
  }
}

export function applyEffectArgumentAliases(mutableArgs: Record<string, unknown>, argsTyped: EffectArgs): void {
  const rawSystem = mutableArgs.system as string | undefined;
  if (rawSystem && !(mutableArgs.systemPath as string | undefined)) {
    mutableArgs.systemPath = rawSystem;
  }

  // Scoped, not global: validate_niagara_system's native handler reads ONLY
  // SystemPath, so a request carrying the declared `assetPath` spelling reached
  // Unreal with nothing to validate. Sibling actions (get_niagara_info and the
  // module/script readers) canonically read assetPath, so they must keep
  // dispatching it untouched.
  if (mutableArgs.action === 'validate_niagara_system') {
    const rawAssetPath = mutableArgs.assetPath as string | undefined;
    if (rawAssetPath && !(mutableArgs.systemPath as string | undefined)) {
      mutableArgs.systemPath = rawAssetPath;
    }
  }

  const rawTemplate = mutableArgs.template as string | undefined;
  if (rawTemplate && !(mutableArgs.preset as string | undefined)) {
    mutableArgs.preset = rawTemplate;
  }

  const rawPath = mutableArgs.path as string | undefined;
  if (rawPath && !(mutableArgs.savePath as string | undefined)) {
    mutableArgs.savePath = rawPath;
  }

  const rawEmitter = mutableArgs.emitter as string | undefined;
  if (rawEmitter && !(mutableArgs.emitterName as string | undefined)) {
    mutableArgs.emitterName = rawEmitter;
  }

  const rawParamName = mutableArgs.paramName as string | undefined;
  if (rawParamName && !(mutableArgs.parameterName as string | undefined)) {
    mutableArgs.parameterName = rawParamName;
  }

  const rawPropertyName = mutableArgs.propertyName as string | undefined;
  if (rawPropertyName && !(mutableArgs.parameterName as string | undefined)) {
    mutableArgs.parameterName = rawPropertyName;
  }

  if (mutableArgs.propertyValue !== undefined && mutableArgs.value === undefined) {
    mutableArgs.value = mutableArgs.propertyValue;
  }

  if (mutableArgs.propertyValue !== undefined && mutableArgs.parameterValue === undefined) {
    mutableArgs.parameterValue = mutableArgs.propertyValue;
  }

  const rawParamType = mutableArgs.paramType as string | undefined;
  if (rawParamType && !(mutableArgs.parameterType as string | undefined)) {
    mutableArgs.parameterType = rawParamType;
  }

  const rawMaterial = mutableArgs.material as string | undefined;
  if (rawMaterial && !(mutableArgs.materialPath as string | undefined)) {
    mutableArgs.materialPath = rawMaterial;
  }

  const rawMesh = mutableArgs.mesh as string | undefined;
  if (rawMesh && !(mutableArgs.meshPath as string | undefined)) {
    mutableArgs.meshPath = rawMesh;
  }

  if (typeof mutableArgs.count === 'number' && mutableArgs.burstCount === undefined) {
    mutableArgs.burstCount = mutableArgs.count;
  }

  if (typeof mutableArgs.unitsPerSpawn === 'number' && mutableArgs.spawnPerUnit === undefined) {
    mutableArgs.spawnPerUnit = mutableArgs.unitsPerSpawn;
  }

  if (typeof mutableArgs.strength === 'number' && mutableArgs.forceStrength === undefined) {
    mutableArgs.forceStrength = mutableArgs.strength;
  }

  if (typeof mutableArgs.offsetAmount === 'number' && mutableArgs.cameraOffset === undefined) {
    mutableArgs.cameraOffset = mutableArgs.offsetAmount;
  }

  const rawType = mutableArgs.type as string | undefined;
  if (rawType && !(mutableArgs.parameterType as string | undefined)) {
    mutableArgs.parameterType = rawType.charAt(0).toUpperCase() + rawType.slice(1);
  }

  if (argsTyped.shape && !mutableArgs.shapeType && mutableArgs.action === 'debug_shape') {
    mutableArgs.shapeType = argsTyped.shape;
  }
}
