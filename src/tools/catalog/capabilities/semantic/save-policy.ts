import { z } from 'zod';

// Save policy controls when a mutation is persisted: immediate flush, deferred
// batch, explicit none, or an interactive user prompt. Closed enum at the boundary.

export const SavePolicySchema = z.enum(['immediate', 'deferred', 'none', 'user_prompt']);
export type SavePolicy = z.infer<typeof SavePolicySchema>;

export function parseSavePolicy(input: unknown): SavePolicy {
  return SavePolicySchema.parse(input);
}
