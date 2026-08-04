import { requireNonEmptyString } from './handler-argument-validation.js';

export function validateRequiredFields(
  args: Record<string, unknown>,
  fields: readonly string[]
): Record<string, string> {
  const result: Record<string, string> = {};
  for (const field of fields) {
    requireNonEmptyString(args[field], field, `Missing required parameter: ${field}`);
    result[field] = args[field] as string;
  }
  return result;
}