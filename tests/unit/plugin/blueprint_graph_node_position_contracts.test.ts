import { readFileSync, readdirSync } from 'node:fs';
import { resolve } from 'node:path';

import Ajv from 'ajv/dist/2020.js';
import { describe, expect, it } from 'vitest';

const graphDomainRoot = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/BlueprintGraph',
);

interface CanonicalRecord {
  readonly id: string;
  readonly schemas: { readonly input: Record<string, unknown> };
}

const canonicalRecords = (
  JSON.parse(
    readFileSync(
      resolve(
        process.cwd(),
        'src/tools/catalog/capabilities/generated/canonical-registry.generated.json',
      ),
      'utf8',
    ),
  ) as { readonly records: readonly CanonicalRecord[] }
).records;

// manage_blueprint declares posX/posY for node placement; bare x/y mean
// "Canvas position for a HUD element" on the same tool. The native transport
// forwards the payload unnormalized, so any handler reading a payload-root x
// must also accept posX or every node lands at (0,0).
const PAYLOAD_ROOT_X = 'Context.Payload->TryGetNumberField(TEXT("x")';
const PAYLOAD_ROOT_Y = 'Context.Payload->TryGetNumberField(TEXT("y")';
const PAYLOAD_ROOT_POS_X = 'Context.Payload->TryGetNumberField(TEXT("posX")';
const PAYLOAD_ROOT_POS_Y = 'Context.Payload->TryGetNumberField(TEXT("posY")';

const NODE_PLACEMENT_CAPABILITIES = [
  'blueprint.create_node',
  'blueprint.create_reroute_node',
] as const;

const graphHandlerSources = readdirSync(graphDomainRoot)
  .filter((name) => name.endsWith('.cpp'))
  .map((name) => ({ name, source: readFileSync(resolve(graphDomainRoot, name), 'utf8') }));

const findRecord = (id: string): CanonicalRecord => {
  const record = canonicalRecords.find((candidate) => candidate.id === id);
  if (!record) {
    throw new Error(`capability record ${id} is missing from the canonical registry`);
  }
  return record;
};

describe('BlueprintGraph node position alias contracts', () => {
  it('accepts posX alongside every payload-root x read', () => {
    const offenders = graphHandlerSources
      .filter(({ source }) => source.includes(PAYLOAD_ROOT_X))
      .filter(({ source }) => !source.includes(PAYLOAD_ROOT_POS_X))
      .map(({ name }) => name);

    expect(offenders).toEqual([]);
  });

  it('accepts posY alongside every payload-root y read', () => {
    const offenders = graphHandlerSources
      .filter(({ source }) => source.includes(PAYLOAD_ROOT_Y))
      .filter(({ source }) => !source.includes(PAYLOAD_ROOT_POS_Y))
      .map(({ name }) => name);

    expect(offenders).toEqual([]);
  });

  it('keeps posX/posY the required node-placement spelling on the native surface', () => {
    for (const id of NODE_PLACEMENT_CAPABILITIES) {
      const schema = findRecord(id).schemas.input;
      const properties = Object.keys((schema.properties ?? {}) as Record<string, unknown>);

      expect(schema.required).toContain('posX');
      expect(schema.required).toContain('posY');
      expect(schema.additionalProperties).toBe(false);
      // x/y are not part of these contracts, so the C++ fallback is the only
      // way a native caller's coordinates can reach the handler.
      expect(properties).not.toContain('x');
      expect(properties).not.toContain('y');
    }
  });

  it('rejects x/y payloads that the pre-fallback handler used to read', () => {
    const ajv = new Ajv({ strict: false, allErrors: true });
    const validate = ajv.compile(findRecord('blueprint.create_node').schemas.input);

    const withPosPrefix = validate({
      action: 'create_node',
      blueprintPath: '/Game/Blueprints/BP_Test',
      graphName: 'EventGraph',
      nodeType: 'CallFunction',
      memberName: 'PrintString',
      posX: 600,
      posY: 6300,
    });
    expect(withPosPrefix).toBe(true);

    const withBareXY = validate({
      action: 'create_node',
      blueprintPath: '/Game/Blueprints/BP_Test',
      graphName: 'EventGraph',
      nodeType: 'CallFunction',
      memberName: 'PrintString',
      x: 600,
      y: 6300,
    });
    expect(withBareXY).toBe(false);
  });
});
