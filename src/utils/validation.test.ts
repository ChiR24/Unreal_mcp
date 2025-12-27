/**
 * Unit tests for validation utility functions
 */
import { describe, it, expect } from 'vitest';
import {
    sanitizeAssetName,
    sanitizePath,
    validatePathLength,
    validateAssetParams,
    validateNumber,
    ensureVector3,
    ensureRotation
} from './validation.js';

describe('sanitizeAssetName', () => {
    it('removes invalid characters', () => {
        // ! is replaced with _, then trailing _ is stripped
        expect(sanitizeAssetName('My Asset!')).toBe('My_Asset');
    });

    it('removes leading/trailing whitespace', () => {
        expect(sanitizeAssetName('  MyAsset  ')).toBe('MyAsset');
    });

    it('preserves valid names', () => {
        expect(sanitizeAssetName('ValidName_123')).toBe('ValidName_123');
    });

    it('replaces spaces with underscores', () => {
        expect(sanitizeAssetName('My Cool Asset')).toBe('My_Cool_Asset');
    });

    it('handles empty strings', () => {
        const result = sanitizeAssetName('');
        expect(result.length).toBeGreaterThan(0);
    });

    it('removes consecutive underscores', () => {
        const result = sanitizeAssetName('My__Asset');
        expect(result).not.toContain('__');
    });
});

describe('sanitizePath', () => {
    it('normalizes forward slashes', () => {
        const result = sanitizePath('/Game/MyAsset');
        expect(result).toContain('/');
        expect(result).not.toContain('\\\\');
    });

    it('removes double slashes', () => {
        const result = sanitizePath('/Game//MyAsset');
        expect(result).not.toContain('//');
    });

    it('handles backslashes', () => {
        const result = sanitizePath('\\Game\\MyAsset');
        expect(result).toContain('/');
    });

    it('sanitizes path segments with dots', () => {
        expect(() => sanitizePath('/Game/../MyAsset')).toThrow(
            'Path traversal (..) is not allowed'
        );
    });
});

describe('validatePathLength', () => {
    it('accepts valid short paths', () => {
        const result = validatePathLength('/Game/MyAsset');
        expect(result.valid).toBe(true);
    });

    it('accepts empty paths (length 0 < 260)', () => {
        const result = validatePathLength('');
        // Empty string has length 0, which is < 260, so it's valid
        expect(result.valid).toBe(true);
    });

    it('rejects excessively long paths', () => {
        const longPath = '/Game/' + 'a'.repeat(300);
        const result = validatePathLength(longPath);
        expect(result.valid).toBe(false);
        expect(result.error).toBeDefined();
    });
});

describe('validateAssetParams', () => {
    it('validates correct create params', () => {
        const result = validateAssetParams({
            action: 'create',
            name: 'MyAsset',
            path: '/Game/Assets'
        });
        expect(result.valid).toBe(true);
    });

    it('handles names that need sanitization', () => {
        const result = validateAssetParams({
            name: '',
            savePath: '/Game'
        });
        // Empty name gets sanitized to 'Asset', so this should be valid
        expect(result.valid).toBe(true);
    });

    it('validates path params', () => {
        const result = validateAssetParams({
            name: 'Target',
            savePath: '/Game/Source'
        });
        expect(result.valid).toBe(true);
    });
});

describe('validateNumber', () => {
    it('accepts valid finite numbers', () => {
        expect(() => validateNumber(42, 'test')).not.toThrow();
    });

    it('rejects NaN', () => {
        expect(() => validateNumber(NaN, 'test')).toThrow('expected a finite number');
    });

    it('rejects Infinity', () => {
        expect(() => validateNumber(Infinity, 'test')).toThrow();
    });

    it('respects minimum constraint', () => {
        expect(() => validateNumber(5, 'test', { min: 10 })).toThrow('must be >=');
    });

    it('respects maximum constraint', () => {
        expect(() => validateNumber(15, 'test', { max: 10 })).toThrow('must be <=');
    });

    it('allows zero by default', () => {
        expect(() => validateNumber(0, 'test')).not.toThrow();
    });

    it('rejects zero when allowZero is false', () => {
        expect(() => validateNumber(0, 'test', { allowZero: false })).toThrow('zero is not allowed');
    });
});

describe('ensureVector3', () => {
    it('accepts object format with x, y, z', () => {
        const result = ensureVector3({ x: 1, y: 2, z: 3 }, 'location');
        expect(result).toEqual([1, 2, 3]);
    });

    it('accepts array format', () => {
        const result = ensureVector3([1, 2, 3], 'location');
        expect(result).toEqual([1, 2, 3]);
    });

    it('throws on invalid object', () => {
        expect(() => ensureVector3({ x: 1 }, 'location')).toThrow();
    });

    it('throws on wrong array length', () => {
        expect(() => ensureVector3([1, 2], 'location')).toThrow();
    });

    it('throws on non-number values', () => {
        expect(() => ensureVector3({ x: 'a', y: 2, z: 3 }, 'location')).toThrow();
    });
});

describe('ensureRotation', () => {
    it('accepts object format with pitch, yaw, roll', () => {
        const result = ensureRotation({ pitch: 0, yaw: 90, roll: 0 }, 'rotation');
        expect(result).toEqual([0, 90, 0]);
    });

    it('accepts array format', () => {
        const result = ensureRotation([0, 90, 0], 'rotation');
        expect(result).toEqual([0, 90, 0]);
    });

    it('throws on invalid input', () => {
        expect(() => ensureRotation('invalid', 'rotation')).toThrow();
    });
});
