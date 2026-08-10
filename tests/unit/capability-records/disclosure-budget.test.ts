import { describe, expect, it } from 'vitest';
import { searchGatewayCapabilities } from '../../../src/server/gateway/gateway-search.js';
import { describeGatewayCapability } from '../../../src/server/gateway/gateway-describe.js';
import {
  DEFAULT_SEARCH_MAX_BYTES,
  MAX_SEARCH_MAX_BYTES,
} from '../../../src/server/gateway/gateway-search-filters.js';
import { ALL_CAPABILITY_RECORDS } from '../../../src/tools/catalog/capabilities/records/aggregate.js';

const EXPECTED_RECORDS = 1373;
const sizeOf = (value: unknown): number => Buffer.byteLength(JSON.stringify(value), 'utf8');

describe('Task 29 - search disclosure stays bounded and deterministic', () => {
  it('an unbudgeted full-catalog browse never approaches the 1,373-record payload', () => {
    const result = searchGatewayCapabilities({});
    const size = sizeOf(result);

    expect(size).toBeLessThanOrEqual(DEFAULT_SEARCH_MAX_BYTES);
    expect(size).toBeLessThan(sizeOf(ALL_CAPABILITY_RECORDS) / 10);
  });

  it('an explicit byte budget is honoured and the truncation is reported honestly', () => {
    for (const maxBytes of [1024, 2048, 8192]) {
      const result = searchGatewayCapabilities({ limit: 25, maxBytes });
      expect(sizeOf(result), `budget ${maxBytes} exceeded`).toBeLessThanOrEqual(maxBytes);
      expect(result.maxBytes).toBe(maxBytes);
    }
  });

  it('even the largest permitted budget cannot dump the whole catalog', () => {
    const result = searchGatewayCapabilities({ limit: 1000, maxBytes: MAX_SEARCH_MAX_BYTES });
    expect(sizeOf(result)).toBeLessThanOrEqual(MAX_SEARCH_MAX_BYTES);
    expect(sizeOf(result)).toBeLessThan(sizeOf(ALL_CAPABILITY_RECORDS));
  });

  it('identical search calls are byte-for-byte deterministic', () => {
    for (const args of [{}, { query: 'asset' }, { domain: 'asset', limit: 10 }, { limit: 25, maxBytes: 4096 }]) {
      const first = JSON.stringify(searchGatewayCapabilities({ ...args }));
      const second = JSON.stringify(searchGatewayCapabilities({ ...args }));
      const third = JSON.stringify(searchGatewayCapabilities({ ...args }));
      expect(second, `nondeterministic for ${JSON.stringify(args)}`).toBe(first);
      expect(third, `nondeterministic for ${JSON.stringify(args)}`).toBe(first);
    }
  });

  it('a search response never inlines a full input schema', () => {
    const serialized = JSON.stringify(searchGatewayCapabilities({ limit: 25 }));
    expect(serialized).not.toContain('https://json-schema.org/draft/2020-12/schema');
    expect(serialized).not.toContain('additionalProperties');
  });
});

describe('Task 29 - describe discloses progressively and never dumps a union', () => {
  const sampleId = String(ALL_CAPABILITY_RECORDS[0]?.id ?? '');

  it('a capability describe is bounded and deterministic', () => {
    const first = describeGatewayCapability({ capability: sampleId });
    const second = describeGatewayCapability({ capability: sampleId });

    expect(JSON.stringify(second)).toBe(JSON.stringify(first));
    expect(sizeOf(first)).toBeLessThanOrEqual(MAX_SEARCH_MAX_BYTES);
  });

  it('no describe response for any of the 23 parents dumps every action schema', () => {
    const parents = [...new Set(ALL_CAPABILITY_RECORDS.map((r) => String(r.routing.parentTool)))];
    expect(parents.length).toBe(23);

    for (const parent of parents) {
      const result = describeGatewayCapability({ tool: parent });
      const serialized = JSON.stringify(result);
      const schemaMarkers = serialized.split('https://json-schema.org/draft/2020-12/schema').length - 1;

      expect(
        schemaMarkers,
        `describe(tool=${parent}) inlined ${schemaMarkers} full schemas; progressive disclosure allows at most one`,
      ).toBeLessThanOrEqual(1);
      expect(sizeOf(result), `describe(tool=${parent}) exceeded the ceiling`).toBeLessThanOrEqual(MAX_SEARCH_MAX_BYTES);
    }
  });

  it('an unknown capability is refused with bounded guidance, not a catalog dump', () => {
    const result = describeGatewayCapability({ capability: '__task29_no_such_capability__' });
    const serialized = JSON.stringify(result);

    expect(sizeOf(result)).toBeLessThan(4096);
    expect(serialized).not.toContain('https://json-schema.org/draft/2020-12/schema');
  });

  it('the describe budget is independent of catalog size at every drill-down level', () => {
    const record = ALL_CAPABILITY_RECORDS[0];
    if (record === undefined) throw new Error('need a record');
    const legacy = record.legacyIds[0];
    if (legacy === undefined) throw new Error('need a legacy pair');

    const levels = [
      describeGatewayCapability({ tool: String(legacy.tool) }),
      describeGatewayCapability({ tool: String(legacy.tool), action: String(legacy.action) }),
      describeGatewayCapability({ capability: String(record.id) }),
    ];

    for (const [index, level] of levels.entries()) {
      expect(sizeOf(level), `drill-down level ${index} exceeded the ceiling`).toBeLessThanOrEqual(MAX_SEARCH_MAX_BYTES);
    }
    expect(new Set(levels.map((l) => JSON.stringify(l))).size).toBeGreaterThan(1);
  });

  it('the catalog really is the full 1,373 universe behind these bounded views', () => {
    expect(ALL_CAPABILITY_RECORDS.length).toBe(EXPECTED_RECORDS);
  });
});
