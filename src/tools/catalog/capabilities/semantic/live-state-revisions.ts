import { z } from 'zod';

const RevisionCounterSchema = z.number().int().min(1);

export const LiveStateRevisionsSchema = z
  .strictObject({
    selection: RevisionCounterSchema,
    level: RevisionCounterSchema,
    assetRegistry: RevisionCounterSchema,
    package: RevisionCounterSchema
  })
  .readonly();

export type LiveStateRevisions = z.infer<typeof LiveStateRevisionsSchema>;

export function liveStateRevisionsFromEnvelope(value: unknown): LiveStateRevisions | undefined {
  if (typeof value !== 'object' || value === null || Array.isArray(value)) return undefined;
  const parsed = LiveStateRevisionsSchema.safeParse(Reflect.get(value, 'liveRevisions'));
  return parsed.success ? parsed.data : undefined;
}
