/**
 * tests/unit/capability-records/support.ts
 *
 * Task 29 support: deterministic structural diff that reports the FIRST leaf
 * mismatch as an exact `{ id, pointer }` pair (RFC 6901 JSON pointer).
 *
 * Task 29 must never report "something drifted"; every failure names the exact
 * capability and the exact field inside it. Traversal is canonical (sorted
 * object keys, ascending array indices) so the reported pointer is stable and
 * reproducible across runs and machines.
 */

export interface PointerDiff {
  readonly id: string;
  readonly pointer: string;
  readonly expected: string;
  readonly actual: string;
}

const isContainer = (v: unknown): v is Record<string, unknown> | unknown[] =>
  typeof v === 'object' && v !== null;

/** RFC 6901 token escaping: `~` -> `~0`, `/` -> `~1`. */
const escapeToken = (token: string): string => token.replace(/~/g, '~0').replace(/\//g, '~1');

const preview = (v: unknown): string => {
  const text = JSON.stringify(v) ?? String(v);
  return text.length > 160 ? `${text.slice(0, 157)}...` : text;
};

/**
 * Collect leaf differences between `expected` and `actual`, deepest-first, and
 * stop after `limit` findings so a wholesale mismatch cannot flood the report.
 */
export const diffPointers = (
  id: string,
  expected: unknown,
  actual: unknown,
  pointer = '',
  found: PointerDiff[] = [],
  limit = 5,
): readonly PointerDiff[] => {
  if (found.length >= limit) return found;

  if (!isContainer(expected) || !isContainer(actual)) {
    if (JSON.stringify(expected) !== JSON.stringify(actual)) {
      found.push({ id, pointer: pointer === '' ? '/' : pointer, expected: preview(expected), actual: preview(actual) });
    }
    return found;
  }

  const expectedIsArray = Array.isArray(expected);
  if (expectedIsArray !== Array.isArray(actual)) {
    found.push({ id, pointer: pointer === '' ? '/' : pointer, expected: preview(expected), actual: preview(actual) });
    return found;
  }

  if (expectedIsArray) {
    const e = expected as unknown[];
    const a = actual as unknown[];
    const shared = Math.min(e.length, a.length);
    for (let i = 0; i < shared; i += 1) {
      diffPointers(id, e[i], a[i], `${pointer}/${i}`, found, limit);
      if (found.length >= limit) return found;
    }
    if (e.length !== a.length) {
      found.push({
        id,
        pointer: `${pointer}/${shared}`,
        expected: e.length > shared ? preview(e[shared]) : '<absent>',
        actual: a.length > shared ? preview(a[shared]) : '<absent>',
      });
    }
    return found;
  }

  const e = expected as Record<string, unknown>;
  const a = actual as Record<string, unknown>;
  const keys = [...new Set([...Object.keys(e), ...Object.keys(a)])].sort();
  for (const key of keys) {
    const child = `${pointer}/${escapeToken(key)}`;
    const inE = Object.hasOwn(e, key);
    const inA = Object.hasOwn(a, key);
    if (!inE || !inA) {
      found.push({
        id,
        pointer: child,
        expected: inE ? preview(e[key]) : '<absent>',
        actual: inA ? preview(a[key]) : '<absent>',
      });
      if (found.length >= limit) return found;
      continue;
    }
    diffPointers(id, e[key], a[key], child, found, limit);
    if (found.length >= limit) return found;
  }
  return found;
};

/** Render findings as one stable, greppable assertion message. */
export const formatDiffs = (label: string, diffs: readonly PointerDiff[]): string =>
  diffs.length === 0
    ? `${label}: clean`
    : `${label}: ${diffs.length} mismatch(es)\n` +
      diffs
        .map((d) => `  capability=${d.id} pointer=${d.pointer}\n    expected=${d.expected}\n    actual  =${d.actual}`)
        .join('\n');
