// src/utils/serialization/ordering.ts
// The one deterministic ordering primitive for generated and emitted artifacts.
//
// Byte-order (ASCII/UTF-16 code unit) comparison, NEVER `localeCompare`: the
// generated registry, the gateway manifest, the docs tables and the native C++
// shards must all agree byte-for-byte across machines, and `localeCompare` is
// locale- and ICU-dependent (it also treats some separators as ignorable, which
// makes it non-total over NUL-joined keys). A drift here shows up as a
// generated-artifact hash mismatch, not a compile error, so it is centralised.
//
// This replaced eleven hand-rolled copies of the same three lines across
// src/server/gateway, src/tools/catalog and scripts/.

/** Total, locale-independent order over two strings. */
export function compareAscii(left: string, right: string): number {
  if (left < right) return -1;
  if (left > right) return 1;
  return 0;
}

/** Comparator over any record carrying a string `id`. */
export function compareById<T extends { readonly id: string }>(left: T, right: T): number {
  return compareAscii(left.id, right.id);
}

/** A new id-sorted array; never mutates the input. */
export function sortById<T extends { readonly id: string }>(items: readonly T[]): T[] {
  return [...items].sort(compareById);
}

/** Comparator over `[key, value]` entry tuples, ordered by key. */
export function compareEntryKey(
  left: readonly [string, unknown],
  right: readonly [string, unknown]
): number {
  return compareAscii(left[0], right[0]);
}
