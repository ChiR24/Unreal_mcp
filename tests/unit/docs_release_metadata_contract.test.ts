import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

// Reads README.md from the repository root resolved against the current working
// directory so the assertions stay tied to the workspace the test runs in.
const README = readFileSync(resolve(process.cwd(), 'README.md'), 'utf8');

const lower = README.toLowerCase();

/**
 * Find the region around the first "certif" mention (a generous window so the
 * check is robust to surrounding wording). Falls back to the whole document
 * when no certification heading exists yet.
 */
const certRegion = (() => {
  const idx = lower.indexOf('certif');
  if (idx < 0) return README;
  return README.slice(Math.max(0, idx - 400), idx + 1400);
})();

describe('docs release metadata contract', () => {
  it('states the release version 0.5.30', () => {
    expect(README).toMatch(/0\.5\.30/);
  });

  it('states a Node.js runtime floor of 20.19 and does not claim Node 18 support', () => {
    // Positive: a clear minimum statement for 20.19 exists. Tolerate markdown
    // emphasis markers (e.g. "**Node.js** 20.19.0 or later") between the token
    // and the version.
    const floorStated =
      /node\.?js[*\s]*20\.19(?:\.0)?\s*(\+|or later|or above|minimum|>=?)/i.test(README) ||
      /node\.?js[*\s]*(?:>=?|version)[*\s]*20\.19/i.test(README);
    expect(floorStated, 'expected a Node.js 20.19 minimum to be stated').toBe(true);

    // Negative: Node 18 must not be presented as the supported/runtime floor.
    const node18FloorClaim = /node\.?js\s*1?8\s*(\+|or later|or above|minimum|>=?|and above|supported)/i.test(
      README,
    );
    expect(node18FloorClaim, 'README must not claim Node.js 18 support as the floor').toBe(false);
  });

  it('names the UE 5.0–5.8 range with an honest (incomplete) verification status', () => {
    const rangeNamed = /5\.0[\s.\-–]*(?:to|through|[-–—])?\s*5\.8/i.test(README);
    expect(rangeNamed, 'expected the UE 5.0-5.8 range to be named').toBe(true);

    const statusWords = /(ongoing|not yet|pending|in progress|incomplete)/i;
    expect(
      statusWords.test(certRegion),
      'expected an honest verification-status phrase near UE certification',
    ).toBe(true);
    expect(/5\.0[\s.\-–]*(?:to|through|[-–—])?\s*5\.8/i.test(certRegion), 'expected the 5.0-5.8 range near UE certification').toBe(
      true,
    );
  });

  it('does not contradict the asymmetric protocol version list (soft check)', () => {
    // Native /mcp supports exactly the three modern versions; the TypeScript
    // server also accepts the two legacy versions; native deliberately does NOT
    // implement the later 2026-07-28 RC.
    expect(README).toMatch(/2025-11-25/);
    expect(README).toMatch(/2025-06-18/);
    expect(README).toMatch(/2025-03-26/);
    expect(README).toMatch(/2024-11-05/);
    expect(README).toMatch(/2024-10-07/);
    expect(README).toMatch(/not\s+(implement|support)[^\n]*2026-07-28/i);
  });
});
