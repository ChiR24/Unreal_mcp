import { z } from 'zod';

const RevisionCounterSchema = z.number().int().min(1);

export const LiveStateRevisionsSchema = z
  .strictObject({
    selection: RevisionCounterSchema,
    level: RevisionCounterSchema,
    assetRegistry: RevisionCounterSchema,
    package: RevisionCounterSchema,
    // Set by the plugin at startup; changes only when the editor process restarts (dogfood #44).
    serverInstanceId: z.string().min(1).optional()
  })
  .readonly();

export type LiveStateRevisions = z.infer<typeof LiveStateRevisionsSchema>;

export function liveStateRevisionsFromEnvelope(value: unknown): LiveStateRevisions | undefined {
  if (typeof value !== 'object' || value === null || Array.isArray(value)) return undefined;
  const parsed = LiveStateRevisionsSchema.safeParse(Reflect.get(value, 'liveRevisions'));
  return parsed.success ? parsed.data : undefined;
}
