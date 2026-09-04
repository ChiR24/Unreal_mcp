import { describe, expect, it } from 'vitest';

import { searchGatewayCapabilities } from '../../../src/server/gateway/gateway-search.js';

function rows(result: Record<string, unknown>): Array<Record<string, unknown>> {
  return result.results as Array<Record<string, unknown>>;
}

describe('Todo 11 gateway search truthfulness', () => {
  it('discloses rows omitted by the effective limit', () => {
    // Given more matching capabilities than the requested page size
    // When the first page is requested
    const result = searchGatewayCapabilities({ query: 'manage_asset', limit: 1, offset: 0 });

    // Then every omission and the executable continuation are disclosed
    expect(result.truncated).toBe(true);
    expect(result.truncationReason).toBe('limit');
    expect(result.effectiveLimit).toBe(1);
    expect(result.servedCount).toBe(1);
    expect(result.hasMore).toBe(true);
    expect(typeof result.nextCursor).toBe('string');
  });

  it('discloses the clamped effective limit', () => {
    // Given a limit above the supported maximum
    // When search clamps it
    const result = searchGatewayCapabilities({ query: 'manage_asset', limit: 100 });

    // Then the applied value and reason remain machine-readable
    expect(result.limit).toBe(25);
    expect(result.effectiveLimit).toBe(25);
    expect(result.coercions).toEqual(
      expect.arrayContaining([
        expect.objectContaining({ parameter: 'limit', requested: 100, applied: 25 })
      ])
    );
    // 'byte-budget' joined the accepted set when manage_asset gained
    // add_fab_asset_to_project: the Fab and content-source records all carry long
    // summaries, so this page is now cut by bytes before the clamped limit binds.
    // What this case guards is that the clamp is disclosed -- limit, effectiveLimit
    // and the coercion entry above -- and those still hold; the reason is honestly
    // reporting which ceiling actually applied.
    expect(['limit', 'none', 'byte-budget']).toContain(result.truncationReason);
  });

  it('identifies byte-budget truncation', () => {
    // Given a bounded response budget smaller than the full asset domain page
    // When search drops rows to fit
    const result = searchGatewayCapabilities({ domain: 'asset', limit: 25, maxBytes: 1500 });

    // Then byte pressure takes precedence over the coincident limit omission
    expect(result.truncated).toBe(true);
    expect(result.truncationReason).toBe('byte-budget');
    expect(JSON.stringify(result).length).toBeLessThanOrEqual(1500);
    expect(result.hasMore).toBe(true);
  });

  it('keeps cursor continuation executable to exhaustion', () => {
    // Given a one-row first page
    // When its cursors are followed until the catalog says there is nothing left
    const first = searchGatewayCapabilities({ query: 'manage_asset', limit: 1 });
    const seen = new Set(rows(first).map((row) => String(row.capability)));
    let cursor = first.nextCursor;
    let page: Record<string, unknown> = first;
    let hops = 0;
    while (cursor !== undefined && hops < 16) {
      page = searchGatewayCapabilities({ query: 'manage_asset', limit: 25, cursor });
      // Then every continuation advances without repeating rows
      for (const row of rows(page)) {
        expect(seen.has(String(row.capability))).toBe(false);
        seen.add(String(row.capability));
      }
      // A byte-budget cut is legitimate mid-way as long as it hands out a cursor
      expect(['none', 'byte-budget', 'limit']).toContain(page.truncationReason);
      cursor = page.nextCursor as string | undefined;
      hops += 1;
    }
    // and the last page is honest about the end of the result set
    expect(page.hasMore).toBe(false);
    expect(page.nextCursor).toBeUndefined();
    expect(page.truncationReason).toBe('none');
    expect(seen.size).toBe(page.total);
  });

  it('returns a typed refusal for a malformed cursor', () => {
    // Given a cursor outside the gateway encoding
    // When search parses it
    const result = searchGatewayCapabilities({ query: 'manage_asset', cursor: 'not-a-cursor' });

    // Then it refuses with executable restart guidance
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('INVALID_CURSOR');
    expect(result.nextCall).toEqual({ operation: 'search' });
  });

  it('discloses coercions for malformed bounds', () => {
    // Given out-of-range pagination bounds
    // When search normalizes them
    const result = searchGatewayCapabilities({ query: 'manage_asset', limit: 0, offset: -1 });

    // Then both coercions and the effective limit are reported
    expect(result.success).toBe(true);
    expect(result.effectiveLimit).toBe(1);
    expect(result.coercions).toEqual(expect.arrayContaining([
      expect.objectContaining({ parameter: 'offset', requested: -1, applied: 0 }),
      expect.objectContaining({ parameter: 'limit', requested: 0, applied: 1 })
    ]));
  });
});
