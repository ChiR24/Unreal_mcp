// Shared source-contract test helpers for the plugin contract suites.
//
// These were duplicated verbatim across the `tests/unit/plugin/*contracts.test.ts`
// suites; both countPureLines (10 copies) and sliceBetween (3 copies) asserted
// the same 250-line ceiling and the same brace-slicing behavior, so a drift in
// one suite's copy could silently weaken the contract gate for that file. A
// single implementation keeps the gate identical everywhere it runs.

/** Pure (non-blank, non-comment) line count, mirroring the CI source-contract ceiling. */
export function countPureLines(source: string): number {
  return source
    .split(/\r?\n/u)
    .filter((line) => !/^\s*$/u.test(line) && !/^\s*(?:#|\/\/)/u.test(line))
    .length;
}

/** Slices between the first occurrence of `start` and the `end` that follows it. */
export function sliceBetween(source: string, start: string, end: string): string {
  const from = source.indexOf(start);
  if (from === -1) {
    return '';
  }
  const to = source.indexOf(end, from + start.length);
  return source.slice(from, to === -1 ? undefined : to);
}
