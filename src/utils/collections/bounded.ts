// src/utils/collections/bounded.ts
// Insertion-order eviction for the bounded in-memory stores.
//
// `Map` and `Set` both iterate in insertion order and both expose
// `size`/`keys()`/`delete()`, so one helper bounds either. This was written out
// three times (subscription store, telemetry registry, progress sink registry)
// with three slightly different shapes — an `if` in two of them and a `while` in
// the third — which is exactly the kind of divergence that turns "bounded" into
// "bounded except on the path nobody re-checked".

/** The read/delete surface `Map` and `Set` share. */
export interface InsertionOrdered<K> {
  readonly size: number;
  delete(key: K): boolean;
  keys(): IterableIterator<K>;
}

/**
 * Evict oldest-first until `size` is below `maxSize`, returning how many went.
 *
 * Call BEFORE inserting, with the capacity the collection must not exceed once
 * the new entry lands. `onEvict` runs after each removal so a caller can release
 * whatever the entry owned.
 */
export function evictOldestUntilUnder<K>(
  collection: InsertionOrdered<K>,
  maxSize: number,
  onEvict?: (key: K) => void
): number {
  let evicted = 0;
  while (collection.size >= maxSize) {
    const oldest = collection.keys().next();
    if (oldest.done === true) break;
    collection.delete(oldest.value);
    evicted += 1;
    onEvict?.(oldest.value);
  }
  return evicted;
}
