// Both validators refuse a schema keyword outside their subset when a call reaches it (UNSUPPORTED_SCHEMA_KEYWORD), so a
// record that used one failed every call sending that parameter: exclusiveMinimum on project_uv's tileSize did, and no
// test noticed until an editor ran it. Every input and output schema is held to the subset here.

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from './support/capability-index.js';
import { SUPPORTED_SCHEMA_KEYWORDS } from './support/schema-validate.js';
import { isRecord } from '../../src/utils/validation/type-guards.js';

/** The keyword paths in schema that neither validator implements; enum and default hold data, not schemas. */
function unsupportedKeywords(schema: unknown, path: string, out: string[]): void {
  if (!isRecord(schema)) return;
  for (const [key, value] of Object.entries(schema)) {
    if (!SUPPORTED_SCHEMA_KEYWORDS.has(key)) {
      out.push(`${path}/${key}`);
    } else if (key === 'properties' && isRecord(value)) {
      for (const [name, property] of Object.entries(value)) unsupportedKeywords(property, `${path}/properties/${name}`, out);
    } else if (key === 'items' || key === 'additionalProperties') {
      unsupportedKeywords(value, `${path}/${key}`, out);
    }
  }
}

describe('record schemas use only the keywords both validators implement', () => {
  it('holds every input and output schema to the subset', () => {
    const found: string[] = [];
    for (const record of capabilityIndex().byId.values()) {
      unsupportedKeywords(record.schemas.input, `${record.id}:input`, found);
      unsupportedKeywords(record.schemas.output, `${record.id}:output`, found);
    }
    expect(found).toEqual([]);
  });

  it('finds a keyword outside the subset', () => {
    const found: string[] = [];
    unsupportedKeywords({ type: 'object', properties: { size: { type: 'number', exclusiveMinimum: 0 } } }, 'probe', found);
    expect(found).toEqual(['probe/properties/size/exclusiveMinimum']);
  });
});
