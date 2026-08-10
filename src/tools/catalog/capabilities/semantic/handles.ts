import { z } from 'zod';

import { SemanticBoundaryError, type SemanticError } from './errors.js';

import {
  AssetPathSchema,
  ClassPathSchema,
  ObjectPathSchema
} from './paths.js';

export const ActorRefSchema = z.string().min(1).max(512).brand<'ActorRef'>();
export type ActorRef = z.infer<typeof ActorRefSchema>;

export const ComponentRefSchema = z.string().min(1).max(512).brand<'ComponentRef'>();
export type ComponentRef = z.infer<typeof ComponentRefSchema>;

export const NodeRefSchema = z.string().min(1).max(512).brand<'NodeRef'>();
export type NodeRef = z.infer<typeof NodeRefSchema>;

// Exact, strict, discriminable schema for the typed-handle union. A component
// ref can never satisfy an actor-targeting contract: the `kind` discriminant is
// branded/checked here so a malformed handle is rejected at the wire boundary.
// Each branch uses `.readonly()` so Zod v4 deep-freezes the parsed output
// (Object.isFrozen === true) - the readonly guarantee is runtime-real, not just
// a disconnected type alias.
export const TypedHandleSchema = z.discriminatedUnion('kind', [
  z.strictObject({ kind: z.literal('actor'), ref: ActorRefSchema }).readonly(),
  z.strictObject({ kind: z.literal('component'), ref: ComponentRefSchema }).readonly(),
  z.strictObject({ kind: z.literal('node'), ref: NodeRefSchema }).readonly(),
  z.strictObject({ kind: z.literal('object'), path: ObjectPathSchema }).readonly(),
  z.strictObject({ kind: z.literal('asset'), path: AssetPathSchema }).readonly(),
  z.strictObject({ kind: z.literal('class'), path: ClassPathSchema }).readonly()
]);

// `TypedHandle` and `HandleKind` are derived from `TypedHandleSchema` so the typed
// handle union can never drift from its Zod contract (single source of truth;
// no hand-written duplicate). Readonly composition ensures immutable fields.
export type TypedHandle = Readonly<z.infer<typeof TypedHandleSchema>>;
export type HandleKind = z.infer<typeof TypedHandleSchema>['kind'];

export function parseActorRef(input: unknown): ActorRef {
  return ActorRefSchema.parse(input);
}

export function parseComponentRef(input: unknown): ComponentRef {
  return ComponentRefSchema.parse(input);
}

export function parseNodeRef(input: unknown): NodeRef {
  return NodeRefSchema.parse(input);
}

// Reject a handle presented for the wrong kind before dispatch; a component ref
// must never satisfy an actor-targeting contract (or vice versa). Overloads give
// callers with a literal kind the statically narrowed variant (Extract<TypedHandle,
// { kind: K }>), so variant-specific fields (e.g. `.ref`, `.path`) are accessible
// without re-discriminating. The implementation signature returns the wide
// TypedHandle union (no cast); the guard ensures runtime safety.
export function expectHandleKind<K extends HandleKind>(
  handle: TypedHandle,
  expected: K
): Extract<TypedHandle, { kind: K }>;
export function expectHandleKind(handle: TypedHandle, expected: HandleKind): TypedHandle;
export function expectHandleKind(handle: TypedHandle, expected: HandleKind): TypedHandle {
  if (handle.kind !== expected) {
    const semanticError: SemanticError = {
      kind: 'handle',
      code: 'HANDLE_KIND_MISMATCH',
      expected,
      received: handle.kind,
      message: `Expected handle kind '${expected}' but received '${handle.kind}'`
    };
    throw new SemanticBoundaryError(semanticError);
  }
  return handle;
}
