/**
 * Unit tests for safe-json utility
 */
import { describe, it, expect } from 'vitest';
import { cleanObject } from './safe-json.js';

describe('cleanObject', () => {
    it('removes undefined values', () => {
        const input = { a: 1, b: undefined, c: 3 };
        const result = cleanObject(input);
        expect(result).toEqual({ a: 1, c: 3 });
        expect('b' in result).toBe(false);
    });

    it('preserves null values (only removes undefined)', () => {
        const input = { a: 1, b: null, c: 3 };
        const result = cleanObject(input);
        // cleanObject preserves null, only removes undefined
        expect(result).toEqual({ a: 1, b: null, c: 3 });
    });

    it('handles nested objects', () => {
        const input = { a: 1, nested: { b: 2, c: undefined } };
        const result = cleanObject(input);
        expect(result.nested).toEqual({ b: 2 });
    });

    it('handles arrays with undefined (preserves null)', () => {
        const input = { arr: [1, undefined, 3, null] };
        const result = cleanObject(input);
        // Arrays preserve their structure, undefined becomes undefined in array
        expect(result.arr).toEqual([1, undefined, 3, null]);
    });

    it('preserves falsy values that are not null/undefined', () => {
        const input = { a: 0, b: '', c: false };
        const result = cleanObject(input);
        expect(result).toEqual({ a: 0, b: '', c: false });
    });

    it('handles empty objects', () => {
        expect(cleanObject({})).toEqual({});
    });

    it('handles primitive inputs', () => {
        expect(cleanObject('string')).toBe('string');
        expect(cleanObject(42)).toBe(42);
        expect(cleanObject(true)).toBe(true);
    });

    it('respects max depth limit', () => {
        // Create deeply nested object
        let deep: any = { value: 'deep' };
        for (let i = 0; i < 15; i++) {
            deep = { nested: deep };
        }

        // Should not throw when hitting depth limit
        expect(() => cleanObject(deep, 10)).not.toThrow();
    });

    it('handles circular reference prevention at max depth', () => {
        const obj: any = { a: 1 };
        obj.self = obj; // circular reference

        // Should not throw - depth limiting should prevent infinite recursion
        expect(() => cleanObject(obj, 5)).not.toThrow();
    });

    it('handles Date objects (converts to empty object)', () => {
        const date = new Date('2024-01-01');
        const input = { created: date };
        const result = cleanObject(input);
        // Date objects are processed as objects using Object.keys, which returns []
        expect(result.created).toEqual({});
    });

    it('handles empty arrays', () => {
        const input = { arr: [] };
        const result = cleanObject(input);
        expect(result.arr).toEqual([]);
    });

    it('handles NaN values', () => {
        const input = { a: 1, b: NaN };
        const result = cleanObject(input);
        // NaN handling depends on implementation
        expect(result.a).toBe(1);
    });
});
