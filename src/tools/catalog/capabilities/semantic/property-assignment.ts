import { z } from 'zod';

// Structured JSON value boundaries for per-action property assignment and metadata.
// `z.json()` enforces JSON-safe values (rejecting functions, bigint, symbols, etc.)
// at the boundary, so downstream code receives a typed JsonValue without re-checking.

export const JsonValueSchema = z.json();

export const PropertyAssignmentSchema = z
  .strictObject({
    name: z.string().min(1).max(256),
    value: JsonValueSchema
  })
  .readonly();
export type PropertyAssignment = z.infer<typeof PropertyAssignmentSchema>;

export const MetadataSchema = z.record(z.string().min(1).max(256), JsonValueSchema).readonly();
export type Metadata = z.infer<typeof MetadataSchema>;

export function parsePropertyAssignment(input: unknown): PropertyAssignment {
  return PropertyAssignmentSchema.parse(input);
}

export function parseMetadata(input: unknown): Metadata {
  return MetadataSchema.parse(input);
}
