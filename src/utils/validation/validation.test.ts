/**
 * Unit tests for validation utility functions
 */
import { describe, it, expect } from 'vitest';
import {
    sanitizeAssetName,
    normalizeAndSanitizeAssetPath
} from './validation.js';
import { sanitizePath } from '../paths/path-security.js';

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

    it('sanitizes SQL-like patterns consistently across repeated calls', () => {
        expect(sanitizeAssetName('DROP DELETE Table')).toBe('Table');
        expect(sanitizeAssetName('DROP DELETE Table')).toBe('Table');
    });

    it('handles reserved keywords case-insensitively', () => {
        expect(sanitizeAssetName('None')).toBe('None_Asset');
        expect(sanitizeAssetName('CLASS')).toBe('CLASS_Asset');
        expect(sanitizeAssetName('native')).toBe('native_Asset');
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
            'directory traversal (..) is not allowed'
        );
    });

    it('preserves Niagara root paths', () => {
        expect(normalizeAndSanitizeAssetPath('/Niagara/Modules/EmitterState')).toBe('/Niagara/Modules/EmitterState');
    });
});
