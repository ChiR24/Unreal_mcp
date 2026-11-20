import { z } from 'zod';

import { readField } from './hashing.js';
import { hashesSchema, sourceShape } from './record-fields.js';
import { verifyFolding } from './record-folding.js';
import { verifyHashes } from './record-hashing.js';
import { isRecord } from '../../../utils/validation/type-guards.js';

const SCOPE_RANK: Readonly<Record<string, number>> = { read: 0, write: 1, destructive: 2, admin: 3 };
const CONSENT_RANK: Readonly<Record<string, number>> = { none: 0, explicit: 1, elevated: 2 };

// policy.raisedBy and policy.consentRaisedBy can only tighten a call: each key is a declared input parameter, and each
// value is stricter than the record's own requiredScope or consent.
function verifyRaisedBy(record: Record<string, unknown>, ctx: z.RefinementCtx): void {
  const policy = readField(record, 'policy');
  if (!isRecord(policy)) return;
  const schemas = readField(record, 'schemas');
  const input = isRecord(schemas) ? readField(schemas, 'input') : undefined;
  const properties = isRecord(input) && isRecord(input.properties) ? input.properties : {};
  const checks = [
    { field: 'raisedBy', own: 'requiredScope', ranks: SCOPE_RANK, what: 'scope' },
    { field: 'consentRaisedBy', own: 'consent', ranks: CONSENT_RANK, what: 'consent' }
  ] as const;
  for (const { field, own, ranks, what } of checks) {
    const raised = readField(policy, field);
    if (!isRecord(raised)) continue;
    const base = ranks[String(policy[own])] ?? 0;
    for (const [name, value] of Object.entries(raised)) {
      if (!Object.hasOwn(properties, name)) {
        ctx.addIssue({ code: 'custom', path: ['policy', field, name], message: `a ${what}-raising parameter must be a declared input property` });
      } else if ((ranks[String(value)] ?? 0) <= base) {
        ctx.addIssue({ code: 'custom', path: ['policy', field, name], message: `a raised ${what} must be stricter than ${own}` });
      }
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
