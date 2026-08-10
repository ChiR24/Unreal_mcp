/**
 * Focused test: the manage_asset `list` record declares the full bounded
 * pagination surface handleListAssets consumes - the flat limit/offset pair,
 * the nested pagination envelope, the opaque cursor, and includeTags - plus
 * the continuation fields the handler emits back.
 *
 * Bounds are grounded in asset-basic-actions.ts: normalizeListLimit clamps to
 * 1..500 defaulting to 50, normalizeListOffset floors at 0, and both read
 * `params.<key> ?? pagination.<key>`.
 */
import { describe, expect, it } from 'vitest';
import { MANAGE_ASSET_RECORDS } from './index.js';

interface NumericSchema {
  readonly type?: string;
  readonly minimum?: number;
  readonly maximum?: number;
  readonly default?: number;
}

interface ObjectSchema {
  readonly type?: string;
  readonly additionalProperties?: boolean;
  readonly properties?: Record<string, NumericSchema>;
}

const listRecord = MANAGE_ASSET_RECORDS.find((record) => record.legacyIds[0].action === 'list');
if (listRecord === undefined) {
  throw new TypeError('manage_asset list record is unavailable');
}

const INPUT = listRecord.schemas.input.properties as Record<string, ObjectSchema>;
const OUTPUT = listRecord.schemas.output.properties as Record<string, unknown>;

describe('manage_asset list record: bounded pagination contract', () => {
  it('Given the list record, When its input is inspected, Then cursor and includeTags remain declared alongside the flat limit/offset pair', () => {
    expect(INPUT).toHaveProperty('cursor');
    expect(INPUT).toHaveProperty('includeTags');
    expect(INPUT).toHaveProperty('limit');
    expect(INPUT).toHaveProperty('offset');
  });

  it('Given the list record, When its input is inspected, Then a closed nested pagination envelope declares only limit and offset', () => {
    const pagination = INPUT.pagination;
    expect(pagination).toBeDefined();
    expect(pagination.type).toBe('object');
    expect(pagination.additionalProperties).toBe(false);
    expect(Object.keys(pagination.properties ?? {}).sort()).toEqual(['limit', 'offset']);
  });

  it('Given the nested pagination envelope, When its bounds are read, Then they match the handler normalization of 1..500 default 50 and a non-negative offset', () => {
    const nested = INPUT.pagination.properties ?? {};

    expect(nested.limit).toMatchObject({ type: 'number', minimum: 1, maximum: 500, default: 50 });
    expect(nested.offset).toMatchObject({ type: 'number', minimum: 0 });
  });

  it('Given the nested pagination envelope, When compared with the flat limit, Then both declare identical bounds so neither form is privileged', () => {
    expect(INPUT.pagination.properties?.limit).toEqual(INPUT.limit);
  });

  it('Given the list record, When its output is inspected, Then every continuation field the handler emits is declared', () => {
    for (const field of ['assets', 'folders', 'totalCount', 'count', 'limit', 'offset', 'hasMore', 'nextOffset', 'cursor', 'nextCursor']) {
      expect(OUTPUT, `list output should declare ${field}`).toHaveProperty(field);
    }
  });
});
