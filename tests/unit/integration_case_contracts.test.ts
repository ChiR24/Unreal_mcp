import { readdirSync, readFileSync } from 'node:fs';
import { join, relative, resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

// Reusing the runner's own parser is the point: a second implementation of the
// grammar could disagree with the one the runner actually applies, which would
// make this gate lie in either direction.
import { splitExpectedConditions } from '../expectation-utils.mjs';

const PRIMARY_INTENTS = new Set(['success', 'error', 'timeout']);

const suiteFiles = (): readonly string[] => {
  const walk = (dir: string): string[] =>
    readdirSync(dir, { withFileTypes: true }).flatMap((entry) => {
      const full = join(dir, entry.name);
      if (entry.isDirectory()) return walk(full);
      return /\.(test|cases)\.(mjs|cjs)$/.test(entry.name) ? [full] : [];
    });
  return [
    resolve(process.cwd(), 'tests/integration.mjs'),
    ...walk(resolve(process.cwd(), 'tests/mcp-tools')),
  ];
};

// Both expectation forms, and ONLY those. `condition:` is anchored behind
// `expected:` because it is also an ordinary tool argument - `manage_networking`
// takes `condition: 'COND_OwnerOnly'` - and matching it bare made this gate
// report a replication enum as a malformed expectation.
const EXPECTATION_FORMS =
  /\bexpected:\s*'([^']*)'|\bexpected:\s*\{[^}]*?\bcondition:\s*'([^']*)'/;

const expectationsOf = (
  source: string,
): readonly { readonly line: number; readonly text: string }[] =>
  source.split('\n').flatMap((line, index) => {
    const match = EXPECTATION_FORMS.exec(line);
    const text = match?.[1] ?? match?.[2];
    return text === undefined ? [] : [{ line: index + 1, text }];
  });

describe('canonical integration case contracts', () => {
  it('allows destructive asset cleanup to finish dependency-safe deletion', () => {
    const source = readFileSync(
      resolve(process.cwd(), 'tests/integration.mjs'),
      'utf8'
    );

    for (const scenario of [
      'Cleanup: delete test folder',
      'Cleanup: delete advanced test folder'
    ]) {
      const caseLine = source
        .split('\n')
        .find((line) => line.includes(`scenario: '${scenario}'`));

      expect(caseLine).toBeDefined();
      expect(caseLine).toMatch(/timeoutMs:\s*(?:[2-9]\d{4}|\d{6,})/);
    }
  });

  // The grammar is documented for every suite, but it used to be enforced on
  // tests/integration.mjs alone - one file out of forty. The other 39 happened to
  // comply; nothing made them.
  it('holds the expectation grammar across every suite, not just integration.mjs', () => {
    const violations = suiteFiles().flatMap((file) =>
      expectationsOf(readFileSync(file, 'utf8')).flatMap(({ line, text }) => {
        const conditions = splitExpectedConditions(text) as readonly string[];
        const [primary, ...alternatives] = conditions;
        const where = `${relative(process.cwd(), file).replace(/\\/g, '/')}:${line}`;

        if (primary === undefined || !PRIMARY_INTENTS.has(primary)) {
          return [`${where} primary intent must be success/error/timeout, got '${text}'`];
        }
        // A success-primary case that also accepts a bare `error` asserts nothing.
        if (primary === 'success' && alternatives.includes('error')) {
          return [`${where} broad mask '${text}' - success|error can never fail`];
        }
        // A timeout is only ever evidence as the primary condition; behind an
        // error it silently absolves a hang, a crash or a lost bridge.
        if (alternatives.includes('timeout')) {
          return [`${where} 'timeout' is only valid as the primary condition, got '${text}'`];
        }
        return [];
      }),
    );

    expect(violations).toEqual([]);
  }, 60_000);
});
