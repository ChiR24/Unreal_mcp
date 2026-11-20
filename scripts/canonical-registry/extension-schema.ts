// scripts/canonical-registry/extension-schema.ts
//
// Emits the JSON Schema an extension provider's RecordsJson() is checked against (IEngineWireCapabilityProvider.h):
// an array of {record} entries, each record in the shape the generated native shards carry. It is built from the TS
// record schema, so the two cannot drift, and it is a generator target, so `registry:check` fails when it does. The
// TS schema's cross-field rules (folding, hashes) cannot be stated in JSON Schema and are left out; EngineWire does
// not check an extension record's hashes, so any object stands there.

import { z } from 'zod';
import { CapabilityRecordSchema } from '../../src/tools/catalog/capabilities/record-schema.js';

export function buildExtensionRecordSchema(): string {
  // Its $refs point at #/$defs, so the definitions move up to this document's root with it.
  const { $schema: _dialect, $defs, ...record } = z.toJSONSchema(CapabilityRecordSchema, { io: 'input', unrepresentable: 'any' });
  const properties = record.properties ?? {};
  const schema = {
    $schema: 'https://json-schema.org/draft/2020-12/schema',
    ...($defs === undefined ? {} : { $defs }),
    title: 'EngineWire extension records',
    description: 'What IEngineWireCapabilityProvider::RecordsJson() returns: one {record} per capability, each on the '
      + "provider's own ext_<plugin> parent tool. Generated from the capability record schema; do not edit.",
    type: 'array',
    items: {
      type: 'object',
      additionalProperties: false,
      required: ['record'],
      properties: {
        record: {
          ...record,
          properties: {
            ...properties,
            hashes: { type: 'object', description: 'Required, any values: EngineWire does not check an extension record\'s hashes.' },
          },
        },
      },
    },
  };
  return `${JSON.stringify(schema, null, 2)}\n`;
}
