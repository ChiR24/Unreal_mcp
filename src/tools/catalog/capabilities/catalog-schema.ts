import { z } from 'zod';

import { CapabilityRecordSchema } from './record-schema.js';

function legacyKey(legacy: { readonly tool: string; readonly action: string }): string {
  return `${legacy.tool}::${legacy.action}`;
}

export const CapabilityCatalogSchema = z
  .array(CapabilityRecordSchema)
  .superRefine((records, ctx) => {
    const canonicalByIndex = new Map<string, number>();
    const aliasSeen = new Map<string, number>();
    const legacySeen = new Map<string, number>();

    for (const [index, record] of records.entries()) {
      if (canonicalByIndex.has(record.id) && canonicalByIndex.get(record.id) !== index) {
        ctx.addIssue({
          code: 'custom',
          path: [index, 'id'],
          message: 'duplicate canonical capability id'
        });
      } else {
        canonicalByIndex.set(record.id, index);
      }

      if (aliasSeen.has(record.id)) {
        ctx.addIssue({
          code: 'custom',
          path: [index, 'id'],
          message: 'canonical id collides with an earlier capability alias'
        });
      }

      const aliasLocal = new Set<string>();
      for (const [position, alias] of record.aliases.entries()) {
        if (canonicalByIndex.has(alias)) {
          ctx.addIssue({
            code: 'custom',
            path: [index, 'aliases', position],
            message: 'alias collides with a canonical capability id'
          });
        }
        if (aliasSeen.has(alias) && aliasSeen.get(alias) !== index) {
          ctx.addIssue({
            code: 'custom',
            path: [index, 'aliases', position],
            message: 'duplicate capability alias across records'
          });
        } else if (aliasLocal.has(alias)) {
          ctx.addIssue({
            code: 'custom',
            path: [index, 'aliases', position],
            message: 'duplicate capability alias within record'
          });
        } else {
          aliasLocal.add(alias);
          aliasSeen.set(alias, index);
        }
      }

      const legacyLocal = new Set<string>();
      for (const [position, legacy] of record.legacyIds.entries()) {
        const key = legacyKey(legacy);
        if (legacySeen.has(key) && legacySeen.get(key) !== index) {
          ctx.addIssue({
            code: 'custom',
            path: [index, 'legacyIds', position],
            message: 'duplicate legacy capability id across records'
          });
        } else if (legacyLocal.has(key)) {
          ctx.addIssue({
            code: 'custom',
            path: [index, 'legacyIds', position],
            message: 'duplicate legacy capability id within record'
          });
        } else {
          legacyLocal.add(key);
          legacySeen.set(key, index);
        }
      }
    }
  });
