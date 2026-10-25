// A clear first search hit carries its signature and execute call so a caller can skip describe. Held here over the
// whole catalog: never for a capability that needs consent, never past the first page, an example that is a legal
// params object for the variant it shows, and a family's selector with the values its schema declares.

import { describe, expect, it } from 'vitest';

import { capabilitySignature } from '../../../src/server/gateway/gateway-capability-view.js';
import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { searchGatewayCapabilities } from '../../../src/server/gateway/gateway-search.js';

const records = capabilityIndex().records;
const byId = new Map(records.map((record) => [String(record.id), record]));
const firstRow = (args: Record<string, unknown>): Record<string, unknown> | undefined =>
  (searchGatewayCapabilities({ operation: 'search', ...args }).results as Record<string, unknown>[])[0];

describe('a search signature', () => {
  it('goes only on a first row that needs no consent, with the execute call describe would give', () => {
    let carried = 0;
    for (const record of records) {
      const query = String(record.id).split('.').pop()?.replace(/_/gu, ' ') ?? '';
      const row = firstRow({ query });
      if (row?.signature === undefined) continue;
      carried += 1;
      const hit = byId.get(String(row.capability));
      expect(hit?.policy.consent, query).toBe('none');
      expect(row.executeCall, query).toMatchObject({ operation: 'execute', capability: row.capability, params: {} });
    }
    expect(carried, 'some action names are clear hits').toBeGreaterThan(100);
  }, 60_000); // a search per record: a cold run took it past the default 10 s

  it('never goes past the first page', () => {
    expect(firstRow({ query: 'save level' })?.signature).toBeDefined();
    expect(firstRow({ query: 'save level', offset: 1 })?.signature).toBeUndefined();
    expect(firstRow({})?.signature).toBeUndefined();
  });

  it('shows an example execute accepts, with the variant its action name pins', () => {
    for (const record of records) {
      const signature = capabilitySignature(record);
      const example = signature.example as Record<string, unknown>;
      expect(Object.keys(example), String(record.id)).not.toContain('action');
      expect(Object.keys(example), String(record.id)).not.toContain('subAction');
      const pinned = record.legacyIds.find((pair) => pair.folded !== undefined && pair.action === record.examples[0]?.input.action);
      for (const [name, value] of Object.entries(pinned?.folded ?? {})) {
        expect(example[name], `${String(record.id)} ${name}`).toEqual(record.examples[0]?.input[name] ?? value);
      }
      const selector = record.routing.dispatchBy?.param;
      if (selector === undefined) {
        expect(signature.selector).toBeUndefined();
        continue;
      }
      const schema = record.schemas.input.properties[selector] as { enum?: unknown[] } | undefined;
      expect(signature.selector, String(record.id)).toMatchObject({ name: selector, values: schema?.enum ?? [] });
      expect(Object.keys(signature.required as object)).not.toContain(selector);
    }
  });
});
