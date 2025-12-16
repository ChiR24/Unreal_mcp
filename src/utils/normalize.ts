export interface Vec3Obj { x: number; y: number; z: number; }
export interface Rot3Obj { pitch: number; yaw: number; roll: number; }

export type Vec3Tuple = [number, number, number];
export type Rot3Tuple = [number, number, number];

export function toVec3Object(input: any): Vec3Obj | null {
  try {
    if (Array.isArray(input) && input.length === 3) {
      const [x, y, z] = input;
      if ([x, y, z].every(v => typeof v === 'number' && isFinite(v))) {
        return { x, y, z };
      }
    }
    if (input && typeof input === 'object') {
      const x = Number((input as any).x ?? (input as any).X);
      const y = Number((input as any).y ?? (input as any).Y);
      const z = Number((input as any).z ?? (input as any).Z);
      if ([x, y, z].every(v => typeof v === 'number' && !isNaN(v) && isFinite(v))) {
        return { x, y, z };
      }
    }
  } catch {}
  return null;
}

export function toRotObject(input: any): Rot3Obj | null {
  try {
    if (Array.isArray(input) && input.length === 3) {
      const [pitch, yaw, roll] = input;
      if ([pitch, yaw, roll].every(v => typeof v === 'number' && isFinite(v))) {
        return { pitch, yaw, roll };
      }
    }
    if (input && typeof input === 'object') {
      const pitch = Number((input as any).pitch ?? (input as any).Pitch);
      const yaw = Number((input as any).yaw ?? (input as any).Yaw);
      const roll = Number((input as any).roll ?? (input as any).Roll);
      if ([pitch, yaw, roll].every(v => typeof v === 'number' && !isNaN(v) && isFinite(v))) {
        return { pitch, yaw, roll };
      }
    }
  } catch {}
  return null;
}

export function toVec3Tuple(input: any): Vec3Tuple | null {
  const vec = toVec3Object(input);
  if (!vec) {
    return null;
  }
  const { x, y, z } = vec;
  return [x, y, z];
}

export function toRotTuple(input: any): Rot3Tuple | null {
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
export function normalizePartialVector(value: any, alternateKeys: string[] = ['x', 'y', 'z']): Record<string, number> | undefined {
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
export function normalizeTransformInput(transform: any): Record<string, unknown> | undefined {
  if (!transform || typeof transform !== 'object') return undefined;
  const result: Record<string, unknown> = {};
  const location = normalizePartialVector(transform.location);
  if (location) result.location = location;
  const rotation = normalizePartialVector(transform.rotation, ['pitch', 'yaw', 'roll']);
  if (rotation) result.rotation = rotation;
  const scale = normalizePartialVector(transform.scale);
  if (scale) result.scale = scale;
  return Object.keys(result).length > 0 ? result : undefined;
}

