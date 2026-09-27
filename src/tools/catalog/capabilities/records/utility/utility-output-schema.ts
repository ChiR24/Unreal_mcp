// Draft-2020-12 schema assembly for the utility capability records.
//
// Both sides of a utility record are closed objects built from a field-name
// list: the INPUT side infers a type per name (`utility-schema-pins.ts`), the
// OUTPUT side takes an explicit property map (`output-glossary.ts`). They were
// separate functions in `helpers.ts`; they live together here because they are
// the only two call sites of the shared schema envelope below.

import type { Draft202012ObjectSchema, JsonObject } from '../../model.js';
import { property } from './utility-schema-pins.js';
import { outputProperty } from './output-glossary.js';
import { schema } from '../shared/record-presets.js';

/** Builds the closed INPUT schema: `action` plus the declared params, typed by name. */
export function inputSchema(
  fields: readonly string[],
  required: readonly string[],
  requiredOneOf?: readonly string[],
): Draft202012ObjectSchema {
  const properties: Record<string, JsonObject> = {};
  for (const field of fields) properties[field] = property(field);
  return schema(properties, required, requiredOneOf);
}

/** Builds the closed OUTPUT schema from the semantic output glossary. */
export function outputSchema(fields: readonly string[], required: readonly string[]): Draft202012ObjectSchema {
  const properties: Record<string, JsonObject> = {};
  for (const field of fields) properties[field] = outputProperty(field);
  return schema(properties, required);
}
