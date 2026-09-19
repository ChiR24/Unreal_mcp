// tests/ordering.mjs
// Byte-order comparison for the test and audit modules that plain `node` runs.
//
// This is the runtime twin of src/utils/serialization/ordering.ts, and exists
// only because several audits are invoked as `node tests/<audit>.mjs` (see
// package.json "test:native-parity" / "test:params") with no TypeScript loader,
// so they cannot import the .ts module. TypeScript test files must import
// compareAscii from src/utils/serialization/ordering.js directly rather than
// from here.
//
// Behaviour must stay identical to that module: a total, locale-independent
// order that returns 0 for equal inputs. `localeCompare` is banned repo-wide
// for ordering because it is locale- and ICU-dependent, so two machines can
// disagree — which silently weakens any test that uses it to express "the
// sorted order" while asserting a byte-order contract.

/** Total, locale-independent order over two strings. */
export function compareAscii(left, right) {
  if (left < right) return -1;
  if (left > right) return 1;
  return 0;
}

/** Comparator over objects carrying a string field, by that field. */
export function compareByField(field) {
  return (left, right) => compareAscii(left[field], right[field]);
}
