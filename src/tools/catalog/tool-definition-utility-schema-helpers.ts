// The one schema fragment this module needs. It used to live in a
// `commonSchemas` dictionary of ~400 reusable fragments, but the parent tool
// definitions are generated from capability records now and reference their own
// per-action properties, so nothing else read a single entry of it.
const ACTION_PARAMS_SCHEMA = {
  type: 'object',
  description: 'Optional action-specific parameters. These are merged with top-level arguments before routing for clients that cannot send arbitrary top-level fields.',
  additionalProperties: true
} as const;

type ToolInputSchemaDefinition = {
  readonly inputSchema: Record<string, unknown>;
};

function isSchemaObject(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

export function addActionParamsSchema(definitions: ToolInputSchemaDefinition[]): void {
  for (const definition of definitions) {
    const schema = definition.inputSchema;
    const rawProperties = schema.properties;
    if (!isSchemaObject(rawProperties)) continue;

    if (rawProperties.action === undefined || rawProperties.params !== undefined) continue;

    rawProperties.params = ACTION_PARAMS_SCHEMA;
    if (schema.additionalProperties === undefined) {
      schema.additionalProperties = true;
    }
  }
}
