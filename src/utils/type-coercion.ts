/**
 * Type coercion helpers for handler argument processing.
 *
 * These functions safely convert unknown values to specific types,
 * returning undefined for invalid/null/undefined inputs.
 *
 * Previously duplicated across multiple handler files.
 */

import type { Vector3, Rotator } from '../types/handler-types.js';

function finiteNumberOrDefault(value: unknown, fallback: number): number {
  if (value === undefined || value === null) return fallback;
  const numeric = Number(value);
  return Number.isFinite(numeric) ? numeric : fallback;
}

// ============================================================================
// PRIMITIVE TYPE COERCION
// ============================================================================

/**
 * Coerce unknown to number | undefined.
 * Returns undefined for null, undefined, or non-finite numbers.
 */
export function toNumber(val: unknown): number | undefined {
  if (val === undefined || val === null) return undefined;
  const n = Number(val);
  return Number.isFinite(n) ? n : undefined;
}

/**
 * Coerce unknown to boolean | undefined.
 * Returns undefined for null or undefined.
 */
export function toBoolean(val: unknown): boolean | undefined {
  if (val === undefined || val === null) return undefined;
  return Boolean(val);
}

/**
 * Coerce unknown to string | undefined.
 * Returns undefined for null or undefined.
 */
export function toString(val: unknown): string | undefined {
  if (val === undefined || val === null) return undefined;
  return String(val);
}

// ============================================================================
// VECTOR/ROTATION CONVERSION
// ============================================================================

/**
 * Convert Vector3 object to [x, y, z] tuple.
 * Returns undefined for invalid input.
 */
export function toVec3Array(v: unknown): [number, number, number] | undefined {
  if (!v || typeof v !== 'object') return undefined;
  const vec = v as Partial<Vector3>;
  const x = Number(vec.x);
  const y = Number(vec.y);
  const z = Number(vec.z);
  if (!Number.isFinite(x) || !Number.isFinite(y) || !Number.isFinite(z)) return undefined;
  return [x, y, z];
}

/**
 * Convert Rotator object to [pitch, yaw, roll] tuple.
 * Returns undefined for invalid input.
 */
export function toRotArray(r: unknown): [number, number, number] | undefined {
  if (!r || typeof r !== 'object') return undefined;
  const rot = r as Partial<Rotator>;
  const pitch = Number(rot.pitch);
  const yaw = Number(rot.yaw);
  const roll = Number(rot.roll);
  if (!Number.isFinite(pitch) || !Number.isFinite(yaw) || !Number.isFinite(roll)) return undefined;
  return [pitch, yaw, roll];
}

/**
 * Convert unknown to RGB color tuple [r, g, b].
 * Accepts array format. Returns undefined for invalid input.
 */
export function toColor3(val: unknown): [number, number, number] | undefined {
  if (!Array.isArray(val) || val.length < 3) return undefined;
  return [
    finiteNumberOrDefault(val[0], 0),
    finiteNumberOrDefault(val[1], 0),
    finiteNumberOrDefault(val[2], 0)
  ];
}

/**
 * Convert unknown to location object {x, y, z}.
 * Accepts array [x, y, z] or object {x, y, z} format.
 */
export function toLocationObj(val: unknown): { x: number; y: number; z: number } | undefined {
  if (!val) return undefined;
  if (Array.isArray(val) && val.length >= 3) {
    return {
      x: finiteNumberOrDefault(val[0], 0),
      y: finiteNumberOrDefault(val[1], 0),
      z: finiteNumberOrDefault(val[2], 0)
    };
  }
  if (typeof val === 'object') {
    const obj = val as Record<string, unknown>;
    return {
      x: finiteNumberOrDefault(obj.x, 0),
      y: finiteNumberOrDefault(obj.y, 0),
      z: finiteNumberOrDefault(obj.z, 0)
    };
  }
  return undefined;
}

/**
 * Convert unknown to rotation object {pitch, yaw, roll}.
 * Accepts array [pitch, yaw, roll] or object {pitch, yaw, roll} format.
 */
export function toRotationObj(val: unknown): { pitch: number; yaw: number; roll: number } | undefined {
  if (!val) return undefined;
  if (Array.isArray(val) && val.length >= 3) {
    return {
      pitch: finiteNumberOrDefault(val[0], 0),
      yaw: finiteNumberOrDefault(val[1], 0),
      roll: finiteNumberOrDefault(val[2], 0)
    };
  }
  if (typeof val === 'object') {
    const obj = val as Record<string, unknown>;
    return {
      pitch: finiteNumberOrDefault(obj.pitch, 0),
      yaw: finiteNumberOrDefault(obj.yaw, 0),
      roll: finiteNumberOrDefault(obj.roll, 0)
    };
  }
  return undefined;
}

// ============================================================================
// PARAMETER VALIDATION HELPERS
// ============================================================================

/**
 * Validate and clamp audio parameters to UE standard ranges.
 * Volume: 0-4, Pitch: 0.01-4
 */
export function validateAudioParams(volume?: number, pitch?: number): { volume: number; pitch: number } {
  const v = finiteNumberOrDefault(volume, 1.0);
  const p = finiteNumberOrDefault(pitch, 1.0);
  return {
    volume: Math.max(0.0, Math.min(v, 4.0)),  // Clamp volume 0-4 (standard UE range)
    pitch: Math.max(0.01, Math.min(p, 4.0))    // Clamp pitch 0.01-4
  };
}

/**
 * Normalize a name value, generating a default if empty/invalid.
 * Used for auto-generating light, actor, etc. names.
 */
export function normalizeName(value: unknown, defaultName?: string, prefix: string = 'Object'): string {
  if (typeof value === 'string') {
    const trimmed = value.trim();
    if (trimmed.length > 0) return trimmed;
  }
  if (typeof defaultName === 'string') {
    const trimmedDefault = defaultName.trim();
    if (trimmedDefault.length > 0) return trimmedDefault;
  }
  return `${prefix}_${Date.now()}_${Math.floor(Math.random() * 1000)}`;
}
