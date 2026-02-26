/**
 * Tests for type-coercion.ts utilities
 */
import { describe, it, expect } from 'vitest';
import {
  toNumber,
  toBoolean,
  toString,
  toVec3Array,
  toRotArray,
  toColor3,
  toLocationObj,
  toRotationObj,
  validateAudioParams,
  normalizeName
} from './type-coercion.js';

describe('toNumber', () => {
  it('should return undefined for null/undefined', () => {
    expect(toNumber(null)).toBeUndefined();
    expect(toNumber(undefined)).toBeUndefined();
  });

  it('should convert valid numbers', () => {
    expect(toNumber(42)).toBe(42);
    expect(toNumber('42')).toBe(42);
    expect(toNumber(3.14)).toBe(3.14);
  });

  it('should return undefined for non-finite numbers', () => {
    expect(toNumber(Infinity)).toBeUndefined();
    expect(toNumber(-Infinity)).toBeUndefined();
    expect(toNumber(NaN)).toBeUndefined();
  });
});

describe('toBoolean', () => {
  it('should return undefined for null/undefined', () => {
    expect(toBoolean(null)).toBeUndefined();
    expect(toBoolean(undefined)).toBeUndefined();
  });

  it('should convert to boolean', () => {
    expect(toBoolean(true)).toBe(true);
    expect(toBoolean(false)).toBe(false);
    expect(toBoolean(1)).toBe(true);
    expect(toBoolean(0)).toBe(false);
    expect(toBoolean('true')).toBe(true);
    expect(toBoolean('')).toBe(false);
  });
});

describe('toString', () => {
  it('should return undefined for null/undefined', () => {
    expect(toString(null)).toBeUndefined();
    expect(toString(undefined)).toBeUndefined();
  });

  it('should convert to string', () => {
    expect(toString('hello')).toBe('hello');
    expect(toString(42)).toBe('42');
    expect(toString(true)).toBe('true');
  });
});

describe('toVec3Array', () => {
  it('should return undefined for invalid input', () => {
    expect(toVec3Array(undefined)).toBeUndefined();
    expect(toVec3Array(null)).toBeUndefined();
    expect(toVec3Array({})).toBeUndefined();
  });

  it('should convert Vector3 to array', () => {
    expect(toVec3Array({ x: 1, y: 2, z: 3 })).toEqual([1, 2, 3]);
    expect(toVec3Array({ x: 0, y: 0, z: 0 })).toEqual([0, 0, 0]);
  });
});

describe('toRotArray', () => {
  it('should return undefined for invalid input', () => {
    expect(toRotArray(undefined)).toBeUndefined();
    expect(toRotArray(null)).toBeUndefined();
  });

  it('should convert Rotator to array', () => {
    expect(toRotArray({ pitch: 10, yaw: 20, roll: 30 })).toEqual([10, 20, 30]);
  });
});

describe('toColor3', () => {
  it('should return undefined for invalid input', () => {
    expect(toColor3(undefined)).toBeUndefined();
    expect(toColor3([1, 2])).toBeUndefined(); // Too few elements
  });

  it('should convert array to color tuple', () => {
    expect(toColor3([255, 128, 0])).toEqual([255, 128, 0]);
    expect(toColor3([1.0, 0.5, 0.0])).toEqual([1.0, 0.5, 0.0]);
  });
});

describe('toLocationObj', () => {
  it('should return undefined for invalid input', () => {
    expect(toLocationObj(undefined)).toBeUndefined();
    expect(toLocationObj(null)).toBeUndefined();
  });

  it('should convert array to location object', () => {
    expect(toLocationObj([100, 200, 300])).toEqual({ x: 100, y: 200, z: 300 });
  });

  it('should convert object format', () => {
    expect(toLocationObj({ x: 10, y: 20, z: 30 })).toEqual({ x: 10, y: 20, z: 30 });
  });
});

describe('toRotationObj', () => {
  it('should return undefined for invalid input', () => {
    expect(toRotationObj(undefined)).toBeUndefined();
    expect(toRotationObj(null)).toBeUndefined();
  });

  it('should convert array to rotation object', () => {
    expect(toRotationObj([45, 90, 0])).toEqual({ pitch: 45, yaw: 90, roll: 0 });
  });

  it('should convert object format', () => {
    expect(toRotationObj({ pitch: 10, yaw: 20, roll: 30 })).toEqual({ pitch: 10, yaw: 20, roll: 30 });
  });
});

describe('validateAudioParams', () => {
  it('should return defaults for undefined params', () => {
    const result = validateAudioParams();
    expect(result.volume).toBe(1.0);
    expect(result.pitch).toBe(1.0);
  });

  it('should clamp volume to 0-4', () => {
    expect(validateAudioParams(5.0).volume).toBe(4.0);
    expect(validateAudioParams(-1.0).volume).toBe(0.0);
    expect(validateAudioParams(2.0).volume).toBe(2.0);
  });

  it('should clamp pitch to 0.01-4', () => {
    expect(validateAudioParams(1.0, 5.0).pitch).toBe(4.0);
    expect(validateAudioParams(1.0, 0.001).pitch).toBe(0.01);
    expect(validateAudioParams(1.0, 2.0).pitch).toBe(2.0);
  });
});

describe('normalizeName', () => {
  it('should return valid string unchanged', () => {
    expect(normalizeName('MyLight')).toBe('MyLight');
  });

  it('should return default name for empty/invalid', () => {
    expect(normalizeName('', 'DefaultLight')).toBe('DefaultLight');
    expect(normalizeName(null, 'DefaultLight')).toBe('DefaultLight');
    expect(normalizeName(undefined, 'DefaultLight')).toBe('DefaultLight');
  });

  it('should generate name when no default provided', () => {
    const result = normalizeName(null);
    expect(result).toMatch(/^Object_\d+_\d+$/);
  });

  it('should use custom prefix', () => {
    const result = normalizeName(null, undefined, 'Light');
    expect(result).toMatch(/^Light_\d+_\d+$/);
  });
});
