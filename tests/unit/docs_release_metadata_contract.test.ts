import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

// Reads README.md from the repository root resolved against the current working
// directory so the assertions stay tied to the workspace the test runs in.
const README = readFileSync(resolve(process.cwd(), 'README.md'), 'utf8');

/** package.json is the canonical version; the release workflow rewrites it. */
const PACKAGE_VERSION = (JSON.parse(
  readFileSync(resolve(process.cwd(), 'package.json'), 'utf8'),
) as { version: string }).version;

/**
 * A `v`-prefixed release token, e.g. `v0.6.0-beta-a`. The `v` is what makes this
 * unambiguous: a bare `x.y.z` in this README is just as likely to be an engine
 * version or the client build in a sample log line.
 */
const RELEASE_TOKEN = /(?<![\w.])v(\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?)(?![\w.])/g;

describe('docs release metadata contract', () => {
  it('names the current package version and pins no superseded one', () => {
    // This used to assert the literal `0.5.30`, and it kept passing off a stale
    // packaging EXAMPLE long after the package moved on -- README is not one of
    // the seven files the version workflow rewrites, so a literal here always
    // rots. Derive it, and refuse any other release version in the document.
    expect(README).toContain(PACKAGE_VERSION);
    const stale = [...README.matchAll(RELEASE_TOKEN)]
      .map((match) => match[1])
      // startsWith, not equality: a real zip name is `v<version>-UE5.7-Linux.zip`,
      // and the pre-release suffix pattern swallows the engine part with it.
      .filter((token) => !token.startsWith(PACKAGE_VERSION));
    expect([...new Set(stale)], 'README pins a superseded release version').toEqual([]);
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

  it('names the UE 5.0–5.8 range with a simple support statement', () => {
    const rangeNamed = /5\.0[\s.\-–]*(?:to|through|[-–—])?\s*5\.8/i.test(README);
    expect(rangeNamed, 'expected the UE 5.0-5.8 range to be named').toBe(true);

    const supportStated =
      /5\.0[\s.\-–]*(?:to|through|[-–—])?\s*5\.8[^\n]{0,120}(?:supported|working)/i.test(README) ||
      /(?:supported|working)[^\n]{0,120}5\.0[\s.\-–]*(?:to|through|[-–—])?\s*5\.8/i.test(README);
    expect(
      supportStated,
      'expected a simple supported/working statement for the 5.0-5.8 range',
    ).toBe(true);
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
