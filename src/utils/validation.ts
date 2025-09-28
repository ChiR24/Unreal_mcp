/**
 * Validation and sanitization utilities for Unreal Engine assets
 */

import { toRotTuple, toVec3Tuple } from './normalize.js';

/**
 * Maximum path length allowed in Unreal Engine
 */
const MAX_PATH_LENGTH = 260;

/**
 * Invalid characters for Unreal Engine asset names
 * Note: Dashes are allowed in Unreal asset names
 */
// eslint-disable-next-line no-useless-escape
const INVALID_CHARS = /[@#%$&*()+=\[\]{}<>?|\\;:'"`,~!\s]/g;

/**
 * Reserved keywords that shouldn't be used as names
 */
const RESERVED_KEYWORDS = [
  'None', 'null', 'undefined', 'true', 'false',
  'class', 'struct', 'enum', 'interface',
  'default', 'transient', 'native'
];

/**
 * Sanitize an asset name for Unreal Engine
 * @param name The name to sanitize
 * @returns Sanitized name
 */
export function sanitizeAssetName(name: string): string {
  if (!name || typeof name !== 'string') {
    return 'Asset';
  }
  
  // Remove leading/trailing whitespace
  let sanitized = name.trim();
  
  // Replace invalid characters with underscores
  sanitized = sanitized.replace(INVALID_CHARS, '_');
  
  // Remove consecutive underscores
  sanitized = sanitized.replace(/_+/g, '_');
  
  // Remove leading/trailing underscores
  sanitized = sanitized.replace(/^_+|_+$/g, '');
  
  // If name is empty after sanitization, use default
  if (!sanitized) {
    return 'Asset';
  }
  
  // If name is a reserved keyword, append underscore
  if (RESERVED_KEYWORDS.includes(sanitized)) {
    sanitized = `${sanitized}_Asset`;
  }
  
  // Ensure name starts with a letter
  if (!/^[A-Za-z]/.test(sanitized)) {
    sanitized = `Asset_${sanitized}`;
  }
  
  // Truncate overly long names to reduce risk of hitting path length limits
  if (sanitized.length > 64) {
    sanitized = sanitized.slice(0, 64);
  }
  
  return sanitized;
}

/**
 * Sanitize a path for Unreal Engine
 * @param path The path to sanitize
 * @returns Sanitized path
 */
export function sanitizePath(path: string): string {
  if (!path || typeof path !== 'string') {
    return '/Game';
  }
  
  // Normalize slashes
  path = path.replace(/\\/g, '/');

  // Ensure path starts with /
  if (!path.startsWith('/')) {
    path = `/${path}`;
  }
  
  // Split path into segments and sanitize each
  let segments = path.split('/').filter(s => s.length > 0);

  if (segments.length === 0) {
    return '/Game';
  }

  // Ensure the first segment is a valid root (Game, Engine, Script, Temp)
  const ROOTS = new Set(['Game', 'Engine', 'Script', 'Temp']);
  if (!ROOTS.has(segments[0])) {
    segments = ['Game', ...segments];
  }

  const sanitizedSegments = segments.map(segment => {
    // Don't sanitize Game, Engine, or other root folders
    if (['Game', 'Engine', 'Script', 'Temp'].includes(segment)) {
      return segment;
    }
    return sanitizeAssetName(segment);
  });
  
  // Reconstruct path
  const sanitizedPath = '/' + sanitizedSegments.join('/');
  
  return sanitizedPath;
}

/**
 * Validate path length
 * @param path The full path to validate
 * @returns Object with validation result
 */
export function validatePathLength(path: string): { valid: boolean; error?: string } {
  if (path.length > MAX_PATH_LENGTH) {
    return {
      valid: false,
      error: `Path too long (${path.length} characters). Maximum allowed is ${MAX_PATH_LENGTH} characters.`
    };
  }
  return { valid: true };
}

/**
 * Validate and sanitize asset parameters
 * @param params Object containing name and optionally savePath
 * @returns Sanitized parameters with validation result
 */
export function validateAssetParams(params: {
  name: string;
  savePath?: string;
  [key: string]: any;
}): {
  valid: boolean;
  sanitized: typeof params;
  error?: string;
} {
  // Sanitize name
  const sanitizedName = sanitizeAssetName(params.name);
  
  // Sanitize path if provided
  const sanitizedPath = params.savePath 
    ? sanitizePath(params.savePath)
    : params.savePath;
  
  // Construct full path for validation
  const fullPath = sanitizedPath
    ? `${sanitizedPath}/${sanitizedName}`
    : `/Game/${sanitizedName}`;
  
  // Validate path length
  const pathValidation = validatePathLength(fullPath);
  
  if (!pathValidation.valid) {
    return {
      valid: false,
      sanitized: params,
      error: pathValidation.error
    };
  }
  
  return {
    valid: true,
    sanitized: {
      ...params,
      name: sanitizedName,
      ...(sanitizedPath && { savePath: sanitizedPath })
    }
  };
}

/**
 * Extract valid skeletal mesh path from various inputs
 * @param input The input path which might be a skeleton or mesh
 * @returns Corrected skeletal mesh path or null
 */
export function resolveSkeletalMeshPath(input: string): string | null {
  if (!input || typeof input !== 'string') {
    return null;
  }
  
  // Common skeleton to mesh mappings
  const skeletonToMeshMap: { [key: string]: string } = {
    '/Game/Mannequin/Character/Mesh/UE4_Mannequin_Skeleton': '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple',
    '/Game/Characters/Mannequins/Meshes/SK_Mannequin': '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple',
    '/Game/Mannequin/Character/Mesh/SK_Mannequin': '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple',
    '/Game/Characters/Mannequin_UE4/Meshes/UE4_Mannequin_Skeleton': '/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple',
    '/Game/Characters/Mannequins/Skeletons/UE5_Mannequin_Skeleton': '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple',
    '/Game/Characters/Mannequins/Skeletons/UE5_Female_Mannequin_Skeleton': '/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple',
    '/Game/Characters/Mannequins/Skeletons/UE5_Manny_Skeleton': '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple',
    '/Game/Characters/Mannequins/Skeletons/UE5_Quinn_Skeleton': '/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple'
  };
  
  // Check if this is a known skeleton path
  if (skeletonToMeshMap[input]) {
    return skeletonToMeshMap[input];
  }
  
  // If it contains _Skeleton, try to convert to mesh name
  if (input.includes('_Skeleton')) {
    // Try common replacements
    let meshPath = input.replace('_Skeleton', '');
    // Mapping for replacements
    const replacements: { [key: string]: string } = {
      '/SK_': '/SKM_',
      'UE4_Mannequin': 'SKM_Manny',
      'UE5_Mannequin': 'SKM_Manny',
      'UE5_Manny': 'SKM_Manny',
      'UE5_Quinn': 'SKM_Quinn'
    };
    // Apply all replacements using regex
    meshPath = meshPath.replace(
      new RegExp(Object.keys(replacements).join('|'), 'g'),
      match => replacements[match]
    );
    return meshPath;
  }
  
  // If it starts with SK_ (skeleton prefix), try SKM_ (skeletal mesh prefix)
  if (input.includes('/SK_')) {
    return input.replace('/SK_', '/SKM_');
  }
  
  // Return as-is if no conversion needed
  return input;
}

/**
 * Concurrency delay to prevent race conditions
 * @param ms Milliseconds to delay
 */
export async function concurrencyDelay(ms: number = 100): Promise<void> {
  return new Promise(resolve => setTimeout(resolve, ms));
}

/**
 * Ensure the provided value is a finite number within optional bounds.
 * @throws if the value is not a finite number or violates bounds
 */
export function validateNumber(
  value: unknown,
  label: string,
  {
    min,
    max,
    allowZero = true
  }: { min?: number; max?: number; allowZero?: boolean } = {}
): number {
  if (typeof value !== 'number' || !Number.isFinite(value)) {
    throw new Error(`Invalid ${label}: expected a finite number`);
  }

  if (!allowZero && value === 0) {
    throw new Error(`Invalid ${label}: zero is not allowed`);
  }

  if (typeof min === 'number' && value < min) {
    throw new Error(`Invalid ${label}: must be >= ${min}`);
  }

  if (typeof max === 'number' && value > max) {
    throw new Error(`Invalid ${label}: must be <= ${max}`);
  }

  return value;
}

/**
 * Validate an array (tuple) of finite numbers, preserving the original shape.
 * @throws if the tuple has the wrong length or contains invalid values
 */
export function ensureVector3(value: unknown, label: string): [number, number, number] {
  const tuple = toVec3Tuple(value);
  if (!tuple) {
    throw new Error(`Invalid ${label}: expected an object with x,y,z or an array of 3 numbers`);
  }
  return tuple;
}

export function ensureColorRGB(value: unknown, label: string): [number, number, number] {
  return ensureVector3(value, label);
}

export function ensureRotation(value: unknown, label: string): [number, number, number] {
  const tuple = toRotTuple(value);
  if (!tuple) {
    throw new Error(`Invalid ${label}: expected an object with pitch,yaw,roll or an array of 3 numbers`);
  }
  return tuple;
}
