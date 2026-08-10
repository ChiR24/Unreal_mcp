import { describe, expect, it } from 'vitest';

import { CapabilityIdSchema } from '../identifiers.js';

import { ReceiptSchema } from './envelope.js';
import {
  SemanticBoundaryError
} from './errors.js';
import {
  expectHandleKind,
  parseActorRef,
  parseComponentRef,
  parseNodeRef,
  type TypedHandle,
  TypedHandleSchema,
} from './handles.js';
import {
  parseAssetPath,
  parseClassPath,
  parseObjectPath
} from './paths.js';

const CAP = CapabilityIdSchema.parse('asset.import');

describe('typed handle discrimination', () => {
  it('rejects a mismatched handle kind before dispatch', () => {
    const actor: TypedHandle = { kind: 'actor', ref: parseActorRef('Foo') };
    expect(() => expectHandleKind(actor, 'component')).toThrow();
  });

  it('accepts a matching handle kind', () => {
    const actor: TypedHandle = { kind: 'actor', ref: parseActorRef('Foo') };
    expect(() => expectHandleKind(actor, 'actor')).not.toThrow();
  });
});

describe('TypedHandleSchema exact contract (RED: handles are discriminable)', () => {
  it('rejects a handle missing its ref/path', () => {
    expect(TypedHandleSchema.safeParse({ kind: 'actor' }).success).toBe(false);
    expect(TypedHandleSchema.safeParse({ kind: 'asset' }).success).toBe(false);
  });

  it('rejects an unknown handle kind', () => {
    expect(TypedHandleSchema.safeParse({ kind: 'ghost', ref: 'X' }).success).toBe(false);
  });

  it('accepts a valid asset handle', () => {
    expect(TypedHandleSchema.safeParse({ kind: 'asset', path: '/Game/foo' }).success).toBe(true);
  });
});

describe('expectHandleKind static narrowing (RED: return must be the narrowed variant per literal kind)', () => {
  it('narrows to the actor variant for literal "actor"', () => {
    const handle = { kind: 'actor', ref: parseActorRef('Foo') } as const;
    const narrowed = expectHandleKind(handle, 'actor');
    const ref: string = narrowed.ref;
    expect(ref).toBe('Foo');
  });

  it('narrows to the component variant for literal "component"', () => {
    const handle = { kind: 'component', ref: parseComponentRef('Comp') } as const;
    const narrowed = expectHandleKind(handle, 'component');
    const ref: string = narrowed.ref;
    expect(ref).toBe('Comp');
  });

  it('narrows to the node variant for literal "node"', () => {
    const handle = { kind: 'node', ref: parseNodeRef('Node1') } as const;
    const narrowed = expectHandleKind(handle, 'node');
    const ref: string = narrowed.ref;
    expect(ref).toBe('Node1');
  });

  it('narrows to the object variant for literal "object"', () => {
    const handle = { kind: 'object', path: parseObjectPath('/Game/Maps/Level') } as const;
    const narrowed = expectHandleKind(handle, 'object');
    const path: string = narrowed.path;
    expect(path).toBe('/Game/Maps/Level');
  });

  it('narrows to the asset variant for literal "asset"', () => {
    const handle = { kind: 'asset', path: parseAssetPath('/Game/Foo') } as const;
    const narrowed = expectHandleKind(handle, 'asset');
    const path: string = narrowed.path;
    expect(path).toBe('/Game/Foo');
  });

  it('narrows to the class variant for literal "class"', () => {
    const handle = { kind: 'class', path: parseClassPath('/Script/CoreUObject.Object') } as const;
    const narrowed = expectHandleKind(handle, 'class');
    const path: string = narrowed.path;
    expect(path).toBe('/Script/CoreUObject.Object');
  });

  it('throws HANDLE_KIND_MISMATCH on a wrong kind (runtime guard preserved)', () => {
    const handle = { kind: 'actor', ref: parseActorRef('Foo') } as const;
    try {
      expectHandleKind(handle, 'component');
      throw new Error('expected throw');
    } catch (err) {
      expect(err).toBeInstanceOf(SemanticBoundaryError);
      expect(
        err instanceof SemanticBoundaryError && err.semanticError.code === 'HANDLE_KIND_MISMATCH'
      ).toBe(true);
    }
  });
});

describe('TypedHandleSchema / ReceiptSchema outputs are frozen at the boundary (RED: readonly is runtime-deep, not just a type alias)', () => {
  it('TypedHandleSchema.parse freezes the parsed handle object (Object.isFrozen === true)', () => {
    const parsed = TypedHandleSchema.parse({ kind: 'actor', ref: 'Foo' });
    expect(Object.isFrozen(parsed)).toBe(true);
  });

  it('TypedHandleSchema.parse freezes every variant (object/asset/class carry a path)', () => {
    const objectHandle = TypedHandleSchema.parse({ kind: 'object', path: '/Game/Maps/Level' });
    const assetHandle = TypedHandleSchema.parse({ kind: 'asset', path: '/Game/Foo' });
    const classHandle = TypedHandleSchema.parse({ kind: 'class', path: '/Script/CoreUObject.Object' });
    expect(Object.isFrozen(objectHandle)).toBe(true);
    expect(Object.isFrozen(assetHandle)).toBe(true);
    expect(Object.isFrozen(classHandle)).toBe(true);
  });

  it('ReceiptSchema.parse freezes a success receipt and its nested handle items', () => {
    const receipt = ReceiptSchema.parse({
      status: 'success',
      capabilityId: CAP,
      handles: [{ kind: 'actor', ref: 'Foo' }],
      changes: ['created'],
      warnings: [],
      nextCalls: [],
      data: { x: 1 }
    });
    if (receipt.status !== 'success') throw new Error('expected success receipt');
    expect(Object.isFrozen(receipt)).toBe(true);
    expect(Object.isFrozen(receipt.handles)).toBe(true);
    expect(Object.isFrozen(receipt.handles[0])).toBe(true);
    expect(Object.isFrozen(receipt.changes)).toBe(true);
  });

  it('ReceiptSchema.parse freezes an error receipt and its nested error/nextCalls', () => {
    const receipt = ReceiptSchema.parse({
      status: 'error',
      capabilityId: CAP,
      error: { kind: 'path', code: 'PATH_TRAVERSAL', message: 't', input: '/x' },
      nextCalls: []
    });
    if (receipt.status !== 'error') throw new Error('expected error receipt');
    expect(Object.isFrozen(receipt)).toBe(true);
    expect(Object.isFrozen(receipt.error)).toBe(true);
    expect(Object.isFrozen(receipt.nextCalls)).toBe(true);
  });
});

describe('schema strictness: unknown fields rejected (audit)', () => {
  it('rejects an unknown field on a typed handle', () => {
    expect(
      TypedHandleSchema.safeParse({ kind: 'asset', path: '/Game/foo', leaked: true }).success
    ).toBe(false);
  });
});
