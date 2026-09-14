import { z } from 'zod';

import { computeCapabilityHashes, readField } from './hashing.js';
import { isRecord } from '../../../utils/validation/type-guards.js';

export function verifyHashes(record: Record<string, unknown>, ctx: z.RefinementCtx): void {
  const { hashes, ...source } = record;
  const computed = computeCapabilityHashes(source);
  if (!isRecord(hashes)) {
    ctx.addIssue({
      code: 'custom',
      path: ['hashes'],
      message: 'hashes must be a JSON object'
    });
    return;
  }
  const rawSchemaHash = readField(hashes, 'schema');
  const schemaHash = typeof rawSchemaHash === 'string' ? rawSchemaHash : undefined;
  const rawContentHash = readField(hashes, 'content');
  const contentHash = typeof rawContentHash === 'string' ? rawContentHash : undefined;
  if (computed.schema !== schemaHash) {
    ctx.addIssue({
      code: 'custom',
      path: ['hashes', 'schema'],
      message: 'schema hash mismatch'
    });
  }
  if (computed.content !== contentHash) {
    ctx.addIssue({
      code: 'custom',
      path: ['hashes', 'content'],
      message: 'content hash mismatch'
    });
  }
}
