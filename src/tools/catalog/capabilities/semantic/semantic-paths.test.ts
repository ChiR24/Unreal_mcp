import { describe, expect, it } from 'vitest';

import { SemanticBoundaryError } from './errors.js';
import {
  AssetPathSchema,
  ClassPathSchema,
  ObjectPathSchema,
  parseAssetPath,
  parseClassPath,
  parseObjectPath
} from './paths.js';

describe('AssetPath boundary parsing', () => {
  it('normalizes /Content/Foo to /Game/Foo exactly once', () => {
    expect(parseAssetPath('/Content/Foo')).toBe('/Game/Foo');
    expect(parseAssetPath('/Game/Foo')).toBe('/Game/Foo');
  });

  it('normalizes a deeper /Content tree once', () => {
    expect(parseAssetPath('/Content/Foo/Bar')).toBe('/Game/Foo/Bar');
  });

  it('rejects directory traversal', () => {
    expect(() => parseAssetPath('/Game/../Foo')).toThrow(/traversal/i);
  });

  it('rejects an invalid root', () => {
    expect(() => parseAssetPath('/Foo/Bar')).toThrow(/start with/i);
  });
});

describe('ObjectPath / ClassPath boundary parsing', () => {
  it('preserves object-path suffix and normalizes /Content', () => {
    expect(parseObjectPath('/Content/Maps/Level.Level:PersistentLevel')).toBe(
      '/Game/Maps/Level.Level:PersistentLevel'
    );
  });

  it('rejects traversal inside an object path', () => {
    expect(() => parseObjectPath('/Game/../Foo.Bar')).toThrow(/traversal/i);
  });

  it('rejects an invalid root for class paths', () => {
    expect(() => parseClassPath('/Foo/Bar')).toThrow(/start with/i);
  });
});

describe('ObjectPath / ClassPath strict sanitization (RED: shared sanitizePath + suffix hardening)', () => {
  it('rejects a double slash strictly (not silently normalized)', () => {
    expect(() => parseObjectPath('/Game//Foo')).toThrow(SemanticBoundaryError);
    expect(() => parseClassPath('/Game//Foo')).toThrow(SemanticBoundaryError);
  });

  it('rejects a quote character in the path', () => {
    expect(() => parseObjectPath('/Game/Foo"Bar')).toThrow(SemanticBoundaryError);
    expect(() => parseClassPath('/Game/Foo"Bar')).toThrow(SemanticBoundaryError);
  });

  it('rejects a control character in the path', () => {
    expect(() => parseObjectPath('/Game/Foo\x01Bar')).toThrow(SemanticBoundaryError);
    expect(() => parseClassPath('/Game/Foo\x01Bar')).toThrow(SemanticBoundaryError);
  });

  it('rejects a bad suffix carrying an illegal character', () => {
    expect(() => parseObjectPath('/Game/Foo|Bar')).toThrow(SemanticBoundaryError);
    expect(() => parseClassPath('/Game/Foo|Bar')).toThrow(SemanticBoundaryError);
  });

  it('rejects traversal inside a class path', () => {
    expect(() => parseClassPath('/Game/../Foo')).toThrow(/traversal/i);
  });

  it('normalizes a lowercase root to the canonical casing', () => {
    expect(parseObjectPath('/game/Foo.Bar')).toBe('/Game/Foo.Bar');
    expect(parseClassPath('/game/Module.Class')).toBe('/Game/Module.Class');
  });

  it('keeps the suffix intact through shared sanitization', () => {
    expect(parseObjectPath('/Game/Maps/Level.Sub:PersistentLevel')).toBe(
      '/Game/Maps/Level.Sub:PersistentLevel'
    );
  });
});

describe('path boundary typed errors (RED: must be SemanticBoundaryError, not bare Error)', () => {
  it('throws a typed PATH_TRAVERSAL SemanticBoundaryError for asset traversal', () => {
    expect(() => parseAssetPath('/Game/../Foo')).toThrow(SemanticBoundaryError);
    try {
      parseAssetPath('/Game/../Foo');
      throw new Error('expected throw');
    } catch (err) {
      expect(err).toBeInstanceOf(SemanticBoundaryError);
      expect(err instanceof SemanticBoundaryError && err.semanticError.code === 'PATH_TRAVERSAL').toBe(true);
    }
  });

  it('throws a typed INVALID_PATH_ROOT SemanticBoundaryError for object paths', () => {
    expect(() => parseObjectPath('/Foo/Bar')).toThrow(SemanticBoundaryError);
    try {
      parseObjectPath('/Foo/Bar');
      throw new Error('expected throw');
    } catch (err) {
      expect(err).toBeInstanceOf(SemanticBoundaryError);
      expect(err instanceof SemanticBoundaryError && err.semanticError.code === 'INVALID_PATH_ROOT').toBe(true);
    }
  });

  it('throws a typed INVALID_PATH_ROOT SemanticBoundaryError for class paths', () => {
    expect(() => parseClassPath('/Foo/Bar')).toThrow(SemanticBoundaryError);
  });

  it('normalizes a /Content prefix exactly once, leaving a later /Content intact', () => {
    expect(parseAssetPath('/Content/Foo/Content/Bar')).toBe('/Game/Foo/Content/Bar');
  });
});

describe('exported schemas enforce sanitization directly (RED: no bypass of canonical path)', () => {
  it('AssetPathSchema rejects directory traversal when called directly', () => {
    expect(() => AssetPathSchema.parse('/Game/../Foo')).toThrow();
  });

  it('ObjectPathSchema rejects directory traversal when called directly', () => {
    expect(() => ObjectPathSchema.parse('/Game/../Foo')).toThrow();
  });

  it('ClassPathSchema rejects an invalid root when called directly', () => {
    expect(() => ClassPathSchema.parse('/Foo/Bar')).toThrow();
  });

  it('AssetPathSchema rejects an invalid root when called directly', () => {
    expect(() => AssetPathSchema.parse('/Foo/Bar')).toThrow();
  });

  it('AssetPathSchema normalizes /Content to /Game when called directly', () => {
    expect(AssetPathSchema.parse('/Content/Foo')).toBe('/Game/Foo');
  });

  it('ObjectPathSchema normalizes /Content to /Game when called directly', () => {
    expect(ObjectPathSchema.parse('/Content/Maps/Level.Level:PersistentLevel')).toBe(
      '/Game/Maps/Level.Level:PersistentLevel'
    );
  });

  it('AssetPathSchema is idempotent: a sanitized /Game path is stable', () => {
    expect(AssetPathSchema.parse('/Game/Foo')).toBe('/Game/Foo');
    expect(AssetPathSchema.parse(AssetPathSchema.parse('/Content/Foo'))).toBe('/Game/Foo');
  });

  it('direct schema rejection throws (no unsafe branded string is minted)', () => {
    // A successful parse of traversal would mint an unsafe branded string; this must reject.
    let minted = false;
    try {
      AssetPathSchema.parse('/Game/../Foo');
      minted = true;
    } catch {
      // expected rejection
    }
    expect(minted).toBe(false);
  });
});

describe('exported schema .safeParse() never throws on invalid input (RED: safeParse contract)', () => {
  // Given an invalid path input, When schema.safeParse() is called, Then it must
  // return { success: false } rather than throwing. This is the Zod safeParse
  // contract: a consumer calling safeParse to avoid exceptions must never crash.
  // Cases: bad root, traversal, illegal suffix char, control char, double slash.

  it('AssetPathSchema.safeParse returns failure (not throw) for an invalid root', () => {
    const result = AssetPathSchema.safeParse('/Foo/Bar');
    expect(result.success).toBe(false);
  });

  it('AssetPathSchema.safeParse returns failure (not throw) for directory traversal', () => {
    const result = AssetPathSchema.safeParse('/Game/../Evil');
    expect(result.success).toBe(false);
  });

  it('AssetPathSchema.safeParse returns failure (not throw) for a control character', () => {
    const result = AssetPathSchema.safeParse('/Game/A\x01B');
    expect(result.success).toBe(false);
  });

  it('AssetPathSchema.safeParse returns failure (not throw) for an illegal suffix character', () => {
    const result = AssetPathSchema.safeParse('/Game/A<Bad');
    expect(result.success).toBe(false);
  });

  it('ObjectPathSchema.safeParse returns failure (not throw) for an invalid root', () => {
    const result = ObjectPathSchema.safeParse('/Foo/Bar');
    expect(result.success).toBe(false);
  });

  it('ObjectPathSchema.safeParse returns failure (not throw) for directory traversal', () => {
    const result = ObjectPathSchema.safeParse('/Game/../Evil');
    expect(result.success).toBe(false);
  });

  it('ObjectPathSchema.safeParse returns failure (not throw) for a control character', () => {
    const result = ObjectPathSchema.safeParse('/Game/A\x01B');
    expect(result.success).toBe(false);
  });

  it('ObjectPathSchema.safeParse returns failure (not throw) for an illegal suffix character', () => {
    const result = ObjectPathSchema.safeParse('/Game/A<Bad');
    expect(result.success).toBe(false);
  });

  it('ObjectPathSchema.safeParse returns failure (not throw) for a double slash', () => {
    const result = ObjectPathSchema.safeParse('/Game//Foo');
    expect(result.success).toBe(false);
  });

  it('ClassPathSchema.safeParse returns failure (not throw) for an invalid root', () => {
    const result = ClassPathSchema.safeParse('/Foo/Bar');
    expect(result.success).toBe(false);
  });

  it('ClassPathSchema.safeParse returns failure (not throw) for directory traversal', () => {
    const result = ClassPathSchema.safeParse('/Game/../Evil');
    expect(result.success).toBe(false);
  });

  it('ClassPathSchema.safeParse returns failure (not throw) for a control character', () => {
    const result = ClassPathSchema.safeParse('/Game/A\x01B');
    expect(result.success).toBe(false);
  });

  it('ClassPathSchema.safeParse returns failure (not throw) for an illegal suffix character', () => {
    const result = ClassPathSchema.safeParse('/Game/A<Bad');
    expect(result.success).toBe(false);
  });

  it('ClassPathSchema.safeParse returns failure (not throw) for a double slash', () => {
    const result = ClassPathSchema.safeParse('/Game//Foo');
    expect(result.success).toBe(false);
  });

  it('AssetPathSchema.safeParse returns success for a valid /Game path (sanity)', () => {
    const result = AssetPathSchema.safeParse('/Game/Foo');
    expect(result.success).toBe(true);
  });

  it('ObjectPathSchema.safeParse returns success for a valid object path with suffix (sanity)', () => {
    const result = ObjectPathSchema.safeParse('/Game/Maps/Level.Sub:PersistentLevel');
    expect(result.success).toBe(true);
  });

  it('ClassPathSchema.safeParse returns success for a valid class path with :: suffix (sanity)', () => {
    const result = ClassPathSchema.safeParse('/Script/CoreUObject.Class::StaticClass');
    expect(result.success).toBe(true);
  });
});

describe('valid single-colon :Property suffix is preserved (RED: suffix splitting supports :Property)', () => {
  it('preserves a :Property suffix on an object path without a preceding dot', () => {
    expect(parseObjectPath('/Game/Maps/Level:PersistentLevel')).toBe(
      '/Game/Maps/Level:PersistentLevel'
    );
  });

  it('preserves a :Property suffix on a class path', () => {
    expect(parseClassPath('/Script/CoreUObject.Object:Name')).toBe(
      '/Script/CoreUObject.Object:Name'
    );
  });

  it('preserves a :: suffix (double-colon member form)', () => {
    expect(parseClassPath('/Script/CoreUObject.Class::StaticClass')).toBe(
      '/Script/CoreUObject.Class::StaticClass'
    );
  });

  it('preserves a .Subobject suffix (dot subobject form)', () => {
    expect(parseObjectPath('/Game/Maps/Level.SubLevel')).toBe('/Game/Maps/Level.SubLevel');
  });

  it('preserves a combined .Subobject:Property suffix', () => {
    expect(parseObjectPath('/Game/Maps/Level.Sub:Property')).toBe(
      '/Game/Maps/Level.Sub:Property'
    );
  });

  it('direct schema call preserves a :Property suffix', () => {
    expect(ObjectPathSchema.parse('/Game/Maps/Level:PersistentLevel')).toBe(
      '/Game/Maps/Level:PersistentLevel'
    );
  });

  it('rejects an illegal control character in a :Property suffix', () => {
    expect(() => parseObjectPath('/Game/Maps/Level:Prop\x01erty')).toThrow(SemanticBoundaryError);
  });
});
