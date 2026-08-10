import { describe, expect, it } from 'vitest';
import {
  MAX_RESOURCE_BYTES,
  RESOURCE_ERROR_CODES,
  ResourceError,
  enforceByteBudget,
  normalizeContentPath,
  redactProjectName,
} from './resource-errors.js';

describe('resource-errors', () => {
  it('carries a typed code and the URI on failure', () => {
    // Given / When
    const error = new ResourceError(RESOURCE_ERROR_CODES.NOT_FOUND, 'ue://capability/nope', 'missing');

    // Then
    expect(error).toBeInstanceOf(Error);
    expect(error.code).toBe('RESOURCE_NOT_FOUND');
    expect(error.uri).toBe('ue://capability/nope');
    expect(error.message).toBe('missing');
  });

  it('enforces the byte budget', () => {
    // Given
    const small = 'x'.repeat(100);
    const huge = 'x'.repeat(MAX_RESOURCE_BYTES + 1);

    // When / Then
    expect(() => enforceByteBudget('ue://project', small)).not.toThrow();
    try {
      enforceByteBudget('ue://project', huge);
      throw new Error('expected budget error');
    } catch (error) {
      expect(error).toBeInstanceOf(ResourceError);
      expect((error as ResourceError).code).toBe('RESOURCE_TOO_LARGE');
    }
  });

  it('normalizes UE content paths', () => {
    // Given / When / Then
    expect(normalizeContentPath('ue://asset/x', '/Game/Foo')).toBe('/Game/Foo');
    expect(normalizeContentPath('ue://asset/x', '/Content/Foo')).toBe('/Game/Foo');
    expect(normalizeContentPath('ue://asset/x', 'Game/Foo')).toBe('/Game/Foo');
    expect(normalizeContentPath('ue://asset/x', '%2FGame%2FBar')).toBe('/Game/Bar');
    expect(normalizeContentPath('ue://asset/x', '/Game/Foo/')).toBe('/Game/Foo');
  });

  it('rejects traversal, host paths, control chars, and off-root paths', () => {
    const cases: Array<{ path: string; code: string }> = [
      { path: '/Game/../Engine/Secret', code: 'RESOURCE_TRAVERSAL_REJECTED' },
      { path: '..%2F..%2Fetc', code: 'RESOURCE_TRAVERSAL_REJECTED' },
      { path: 'C:/Windows/System32', code: 'RESOURCE_TRAVERSAL_REJECTED' },
      { path: '/home/user/secret', code: 'RESOURCE_TRAVERSAL_REJECTED' },
      { path: '\\\\server\\share', code: 'RESOURCE_TRAVERSAL_REJECTED' },
      { path: '/Unknown/Root', code: 'RESOURCE_INVALID_URI' },
      { path: '%ZZ', code: 'RESOURCE_INVALID_URI' },
      { path: '/Game/\u0001Bad', code: 'RESOURCE_INVALID_URI' },
      { path: '', code: 'RESOURCE_INVALID_URI' },
    ];
    for (const { path, code } of cases) {
      try {
        normalizeContentPath('ue://object/x', path);
        throw new Error(`expected rejection for ${path}`);
      } catch (error) {
        expect(error).toBeInstanceOf(ResourceError);
        expect((error as ResourceError).code).toBe(code);
      }
    }
  });

  it('redacts a project path down to the bare name', () => {
    // Given / When / Then
    expect(redactProjectName(undefined)).toBeUndefined();
    expect(redactProjectName('')).toBeUndefined();
    expect(redactProjectName('C:/Users/me/Proj/MyGame.uproject')).toBe('MyGame');
    expect(redactProjectName('/home/me/Proj/MyGame.uproject')).toBe('MyGame');
    expect(redactProjectName('C:\\Users\\me\\MyGame.uproject')).toBe('MyGame');
    expect(redactProjectName('MyGame')).toBe('MyGame');
    expect(redactProjectName('/home/me/Proj/')).toBeUndefined();
  });
});
