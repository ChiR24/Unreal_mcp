import { z } from 'zod';

import { hashesSchema, sourceShape } from './record-fields.js';
import { verifyFolding } from './record-folding.js';
import { verifyHashes } from './record-hashing.js';
import { isRecord } from '../../../utils/validation/type-guards.js';

export const CapabilityRecordSourceSchema = z
  .strictObject(sourceShape)
  .superRefine((record, ctx) => {
    if (!isRecord(record)) return;
    verifyFolding(record, ctx);
  });

const recordShape = { ...sourceShape, hashes: hashesSchema };

export const CapabilityRecordSchema = z
  .strictObject(recordShape)
  .superRefine((record, ctx) => {
    if (!isRecord(record)) return;
    verifyFolding(record, ctx);
    verifyHashes(record, ctx);
  });
