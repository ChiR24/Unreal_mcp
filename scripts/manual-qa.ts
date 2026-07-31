// Task-3 manual QA: literal Node import of the rebuilt semantic boundary.
// Prints one binary PASS/FAIL line per probed invariant. Exit 0 iff all pass.
// Run: node --loader ts-node/esm scripts/manual-qa.ts

import { CapabilityIdSchema } from '../src/tools/catalog/capabilities/identifiers.js';
import {
  buildSuccessReceipt,
  ReceiptSchema,
  serializeReceipt
} from '../src/tools/catalog/capabilities/semantic/envelope.js';
import { SemanticBoundaryError, SemanticErrorSchema } from '../src/tools/catalog/capabilities/semantic/errors.js';
import { parseExecutionOptions } from '../src/tools/catalog/capabilities/semantic/execution-options.js';
import { LinearColorSchema } from '../src/tools/catalog/capabilities/semantic/geometry.js';
import {
  expectHandleKind,
  parseActorRef,
  TypedHandleSchema
} from '../src/tools/catalog/capabilities/semantic/handles.js';
import {
  AssetPathSchema,
  ClassPathSchema,
  ObjectPathSchema,
  parseAssetPath,
  parseClassPath,
  parseObjectPath
} from '../src/tools/catalog/capabilities/semantic/paths.js';

let failed = 0;
function check(name: string, cond: boolean): void {
  if (cond) {
        console.log(`PASS ${name}`);
  } else {
    failed++;
        console.log(`FAIL ${name}`);
  }
}

// 1. /Content/Foo normalizes exactly once to /Game/Foo.
check(
  'content-normalization-once',
  parseAssetPath('/Content/Foo/Content/Bar') === '/Game/Foo/Content/Bar' &&
    parseAssetPath('/Content/Foo') === '/Game/Foo' &&
    parseAssetPath('/Game/Foo') === '/Game/Foo'
);

// 2. Stable receipt serialization with typed handle + JSON data, key-order independent.
const CAP = CapabilityIdSchema.parse('asset.import');
const r1 = serializeReceipt(
  buildSuccessReceipt({
    capabilityId: CAP,
    handles: [{ kind: 'actor', ref: parseActorRef('Foo') }],
    data: { spawnId: 'ABC', location: { x: 0, y: 0, z: 0 } }
  })
);
const r2 = serializeReceipt(
  buildSuccessReceipt({
    capabilityId: CAP,
    data: { location: { x: 0, y: 0, z: 0 }, spawnId: 'ABC' },
    handles: [{ kind: 'actor', ref: parseActorRef('Foo') }]
  })
);
check('stable-receipt-typed-handle', r1 === r2 && r1.includes('"spawnId":"ABC"'));

// 3. Unsupported option rejected before dispatch as a typed SemanticBoundaryError.
let unsupportedTyped = false;
try {
  parseExecutionOptions({ durationSeconds: 5 }, ['timeoutMs']);
} catch (err) {
  unsupportedTyped =
    err instanceof SemanticBoundaryError && err.semanticError.code === 'UNSUPPORTED_OPTION';
}
check('unsupported-option', unsupportedTyped);

// 4. Mismatched handle kind rejected before dispatch.
let mismatchTyped = false;
try {
  expectHandleKind({ kind: 'actor', ref: parseActorRef('Foo') }, 'component');
} catch (err) {
  mismatchTyped =
    err instanceof SemanticBoundaryError && err.semanticError.code === 'HANDLE_KIND_MISMATCH';
}
check('mismatched-handle', mismatchTyped);

// 5. Path traversal rejected as a typed SemanticBoundaryError.
let traversalTyped = false;
try {
  parseAssetPath('/Game/../Foo');
} catch (err) {
  traversalTyped =
    err instanceof SemanticBoundaryError && err.semanticError.code === 'PATH_TRAVERSAL';
}
check('traversal', traversalTyped);

// 6. Wrong-unit color (0-255) rejected, not silently clamped.
check('wrong-unit-color', !LinearColorSchema.safeParse({ r: 5, g: 0, b: 0 }).success);

// 7. Exact schema rejection: a malformed typed handle is rejected, not z.unknown-accept.
const malformed = ReceiptSchema.safeParse({
  status: 'success',
  capabilityId: 'asset.import',
  handles: [{ kind: 'actor' }],
  changes: [],
  warnings: [],
  nextCalls: [],
  data: {}
});
check('exact-schema-rejection', !malformed.success);

// 8. JSON-only data: a function value is rejected at the boundary.
const fnData = ReceiptSchema.safeParse({
  status: 'success',
  capabilityId: 'asset.import',
  handles: [],
  changes: [],
  warnings: [],
  nextCalls: [],
  data: () => 1
});
check('json-only-data', !fnData.success);

// 9. Invalid path root rejected as a typed SemanticBoundaryError.
let invalidRootTyped = false;
try {
  parseClassPath('/Foo/Bar');
} catch (err) {
  invalidRootTyped =
    err instanceof SemanticBoundaryError && err.semanticError.code === 'INVALID_PATH_ROOT';
}
check('invalid-root', invalidRootTyped);

// 10. Strict rejection of unknown fields on the typed error contract.
check(
  'strict-unknown-field',
  !SemanticErrorSchema.safeParse({
    kind: 'unknown',
    code: 'UNKNOWN_ERROR',
    message: 'x',
    leaked: 1
  }).success
);

// 11. Direct exported schema rejects traversal (cannot mint unsafe branded string).
let directSchemaTraversalRejected = false;
try {
  AssetPathSchema.parse('/Game/../Foo');
} catch {
  directSchemaTraversalRejected = true;
}
check('direct-schema-traversal-rejection', directSchemaTraversalRejected);

// 12. Direct exported schema normalizes /Content to /Game.
check('direct-schema-content-normalization', AssetPathSchema.parse('/Content/Foo') === '/Game/Foo');

// 13. Direct exported schema rejects invalid root.
let directSchemaInvalidRootRejected = false;
try {
  ObjectPathSchema.parse('/Foo/Bar');
} catch {
  directSchemaInvalidRootRejected = true;
}
check('direct-schema-invalid-root-rejection', directSchemaInvalidRootRejected);

// 14. Valid single-colon :Property suffix preserved (no preceding dot).
check(
  'single-colon-property-suffix',
  parseObjectPath('/Game/Maps/Level:PersistentLevel') === '/Game/Maps/Level:PersistentLevel'
);

// 15. Valid :: double-colon suffix preserved.
check(
  'double-colon-suffix',
  parseClassPath('/Script/CoreUObject.Class::StaticClass') ===
    '/Script/CoreUObject.Class::StaticClass'
);

// 16. AssetPathSchema.safeParse on invalid input returns { success: false } (never throws).
let assetSafeParseNoThrow = false;
try {
  const r = AssetPathSchema.safeParse('/Game/../Evil');
  assetSafeParseNoThrow = !r.success;
} catch {
  assetSafeParseNoThrow = false;
}
check('asset-schema-safeparse-never-throws', assetSafeParseNoThrow);

// 17. ObjectPathSchema.safeParse on invalid input returns { success: false } (never throws).
let objectSafeParseNoThrow = false;
try {
  const r = ObjectPathSchema.safeParse('/Foo/Bar');
  objectSafeParseNoThrow = !r.success;
} catch {
  objectSafeParseNoThrow = false;
}
check('object-schema-safeparse-never-throws', objectSafeParseNoThrow);

// 18. ClassPathSchema.safeParse on invalid input returns { success: false } (never throws).
let classSafeParseNoThrow = false;
try {
  const r = ClassPathSchema.safeParse('/Game//Foo');
  classSafeParseNoThrow = !r.success;
} catch {
  classSafeParseNoThrow = false;
}
check('class-schema-safeparse-never-throws', classSafeParseNoThrow);

// 19. parseAssetPath still throws typed SemanticBoundaryError (not ZodError) on invalid input.
let parseAssetTypedThrow = false;
try {
  parseAssetPath('/Game/../Evil');
} catch (err) {
  parseAssetTypedThrow =
    err instanceof SemanticBoundaryError && err.semanticError.code === 'PATH_TRAVERSAL';
}
check('parse-asset-path-typed-throw', parseAssetTypedThrow);

// 20. expectHandleKind narrows: a matching actor handle exposes .ref (typed narrowing at the boundary).
const narrowedHandle = expectHandleKind(
  { kind: 'actor', ref: parseActorRef('Foo') },
  'actor'
);
check('expect-handle-kind-narrows-actor', narrowedHandle.ref === 'Foo');

// 21. TypedHandleSchema.parse output is frozen (readonly is runtime-deep, not just a type alias).
const frozenHandle = TypedHandleSchema.parse({ kind: 'actor', ref: 'Bar' });
check('typed-handle-schema-frozen', Object.isFrozen(frozenHandle));

// 22. ReceiptSchema.parse output is frozen with nested handle items frozen.
const frozenReceipt = ReceiptSchema.parse({
  status: 'success',
  capabilityId: CAP,
  handles: [{ kind: 'actor', ref: 'Foo' }],
  changes: [],
  warnings: [],
  nextCalls: [],
  data: {}
});
check(
  'receipt-schema-frozen-nested',
  frozenReceipt.status === 'success' &&
    Object.isFrozen(frozenReceipt) &&
    Object.isFrozen(frozenReceipt.handles) &&
    Object.isFrozen(frozenReceipt.handles[0])
);

console.log(failed === 0 ? 'MANUAL_QA_SUMMARY: ALL PASS (0 failures)' : `MANUAL_QA_SUMMARY: ${failed} FAILURE(S)`);
process.exit(failed === 0 ? 0 : 1);
