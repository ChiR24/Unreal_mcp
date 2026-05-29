export interface Vec3Obj { x: number; y: number; z: number; }
export interface Rot3Obj { pitch: number; yaw: number; roll: number; }

export type Vec3Tuple = [number, number, number];
export type Rot3Tuple = [number, number, number];

/** Input that may represent a 3D vector */
type VectorInput = Vec3Obj | Vec3Tuple | Record<string, unknown> | unknown[];

/** Input that may represent a 3D rotation */
type RotationInput = Rot3Obj | Rot3Tuple | Record<string, unknown> | unknown[];

function coerceFiniteNumber(value: unknown): number | undefined {
  return toFiniteNumber(value);
}

/**
 * Convert various input formats to a Vec3 object
 * @param input - Array, object with x/y/z or X/Y/Z properties
 */
export function toVec3Object(input: VectorInput | unknown): Vec3Obj | null {
  if (Array.isArray(input) && input.length === 3) {
    const x = coerceFiniteNumber(input[0]);
    const y = coerceFiniteNumber(input[1]);
    const z = coerceFiniteNumber(input[2]);
    if (x !== undefined && y !== undefined && z !== undefined) {
      return { x, y, z };
    }
  }
  if (input && typeof input === 'object' && !Array.isArray(input)) {
    const obj = input as Record<string, unknown>;
    const x = coerceFiniteNumber(obj.x ?? obj.X);
    const y = coerceFiniteNumber(obj.y ?? obj.Y);
    const z = coerceFiniteNumber(obj.z ?? obj.Z);
    if (x !== undefined && y !== undefined && z !== undefined) {
      return { x, y, z };
    }
  }
  return null;
}

/**
 * Convert various input formats to a Rotation object
 * @param input - Array, object with pitch/yaw/roll or Pitch/Yaw/Roll properties
 */
export function toRotObject(input: RotationInput | unknown): Rot3Obj | null {
  if (Array.isArray(input) && input.length === 3) {
    const pitch = coerceFiniteNumber(input[0]);
    const yaw = coerceFiniteNumber(input[1]);
    const roll = coerceFiniteNumber(input[2]);
    if (pitch !== undefined && yaw !== undefined && roll !== undefined) {
      return { pitch, yaw, roll };
    }
  }
  if (input && typeof input === 'object' && !Array.isArray(input)) {
    const obj = input as Record<string, unknown>;
    const pitch = coerceFiniteNumber(obj.pitch ?? obj.Pitch);
    const yaw = coerceFiniteNumber(obj.yaw ?? obj.Yaw);
    const roll = coerceFiniteNumber(obj.roll ?? obj.Roll);
    if (pitch !== undefined && yaw !== undefined && roll !== undefined) {
      return { pitch, yaw, roll };
    }
  }
  return null;
}

/**
 * Convert vector input to a tuple format [x, y, z]
 */
export function toVec3Tuple(input: VectorInput | unknown): Vec3Tuple | null {
  const vec = toVec3Object(input);
  if (!vec) {
    return null;
  }
  const { x, y, z } = vec;
  return [x, y, z];
}

/**
 * Convert rotation input to a tuple format [pitch, yaw, roll]
 */
export function toRotTuple(input: RotationInput | unknown): Rot3Tuple | null {
  const rot = toRotObject(input);
  if (!rot) {
    return null;
  }
  const { pitch, yaw, roll } = rot;
  return [pitch, yaw, roll];
}

/**
 * Parse a raw value into a finite number when possible.
 * Accepts strings like "1.0" and returns number or undefined when invalid.
 */
export function toFiniteNumber(raw: unknown): number | undefined {
  if (typeof raw === 'number' && Number.isFinite(raw)) return raw;
  if (typeof raw === 'string') {
    const trimmed = raw.trim();
    if (trimmed.length === 0) return undefined;
    const parsed = Number(trimmed);
    if (Number.isFinite(parsed)) return parsed;
  }
  return undefined;
}

/**
 * Normalize a partial vector input. Unlike toVec3Object, this accepts
 * partial specifications and returns an object containing only present
 * components (x/y/z) when any are provided; otherwise returns undefined.
 */
export function normalizePartialVector(value: unknown, alternateKeys: string[] = ['x', 'y', 'z']): Record<string, number> | undefined {
  if (value === undefined || value === null) return undefined;
  const result: Record<string, number> = {};
  const assignIfPresent = (component: 'x' | 'y' | 'z', raw: unknown) => {
    const num = toFiniteNumber(raw);
    if (num !== undefined) result[component] = num;
  };

  if (Array.isArray(value)) {
    if (value.length > 0) assignIfPresent('x', value[0]);
    if (value.length > 1) assignIfPresent('y', value[1]);
    if (value.length > 2) assignIfPresent('z', value[2]);
  } else if (typeof value === 'object') {
    const obj = value as Record<string, unknown>;
    assignIfPresent('x', obj.x ?? obj[alternateKeys[0]]);
    assignIfPresent('y', obj.y ?? obj[alternateKeys[1]]);
    assignIfPresent('z', obj.z ?? obj[alternateKeys[2]]);
  } else {
    assignIfPresent('x', value);
  }

  return Object.keys(result).length > 0 ? result : undefined;
}

/**
 * Normalize a transform-like input into a minimal object containing
 * location/rotation/scale partial descriptors when present.
 */
export function normalizeTransformInput(transform: unknown): Record<string, unknown> | undefined {
  if (!transform || typeof transform !== 'object') return undefined;
  const transformObj = transform as Record<string, unknown>;
  const result: Record<string, unknown> = {};
  const location = normalizePartialVector(transformObj.location);
  if (location) result.location = location;
  const rotation = normalizePartialVector(transformObj.rotation, ['pitch', 'yaw', 'roll']);
  if (rotation) result.rotation = rotation;
  const scale = normalizePartialVector(transformObj.scale);
  if (scale) result.scale = scale;
  return Object.keys(result).length > 0 ? result : undefined;
}
