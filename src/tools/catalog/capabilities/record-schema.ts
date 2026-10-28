import { z } from 'zod';

import { readField } from './hashing.js';
import { hashesSchema, sourceShape } from './record-fields.js';
import { verifyFolding } from './record-folding.js';
import { verifyHashes } from './record-hashing.js';
import { isRecord } from '../../../utils/validation/type-guards.js';

const SCOPE_RANK: Readonly<Record<string, number>> = { read: 0, write: 1, destructive: 2, admin: 3 };

// policy.raisedBy can only tighten a call: each key is a declared input parameter, and each scope is stricter than
// the record's own requiredScope.
function verifyRaisedBy(record: Record<string, unknown>, ctx: z.RefinementCtx): void {
  const policy = readField(record, 'policy');
  const raisedBy = isRecord(policy) ? readField(policy, 'raisedBy') : undefined;
  if (!isRecord(policy) || !isRecord(raisedBy)) return;
  const schemas = readField(record, 'schemas');
  const input = isRecord(schemas) ? readField(schemas, 'input') : undefined;
  const properties = isRecord(input) && isRecord(input.properties) ? input.properties : {};
  const base = SCOPE_RANK[String(policy.requiredScope)] ?? 0;
  for (const [name, scope] of Object.entries(raisedBy)) {
    if (!Object.hasOwn(properties, name)) {
      ctx.addIssue({ code: 'custom', path: ['policy', 'raisedBy', name], message: 'a scope-raising parameter must be a declared input property' });
    } else if ((SCOPE_RANK[String(scope)] ?? 0) <= base) {
      ctx.addIssue({ code: 'custom', path: ['policy', 'raisedBy', name], message: 'a raised scope must be stricter than requiredScope' });
    }
  }
}

export const CapabilityRecordSourceSchema = z
  .strictObject(sourceShape)
  .superRefine((record, ctx) => {
    if (!isRecord(record)) return;
    verifyFolding(record, ctx);
    verifyRaisedBy(record, ctx);
  });

const recordShape = { ...sourceShape, hashes: hashesSchema };

export const CapabilityRecordSchema = z
  .strictObject(recordShape)
  .superRefine((record, ctx) => {
    if (!isRecord(record)) return;
    verifyFolding(record, ctx);
    verifyRaisedBy(record, ctx);
    verifyHashes(record, ctx);
  });
