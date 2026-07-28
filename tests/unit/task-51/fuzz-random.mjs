// @ts-check
// tests/unit/task-51/fuzz-random.mjs
// Task 51 — the deterministic randomness every adversarial case is built from.
//
// A fuzz finding that cannot be replayed is a rumour. Everything in this suite is
// therefore generated from a NAMED SEED through this file and nothing else:
// `Math.random()`, `Date.now()` and `crypto.randomUUID()` are banned inside
// generators, because each of them turns a reproducible failure into a story about
// one machine on one afternoon.
//
// TWO DESIGN CHOICES CARRY THE REPRODUCIBILITY, and both are load-bearing:
//
//  1. SPLIT STREAMS, NOT ONE TAPE. A single shared stream means adding a generator
//     shifts every later draw, so last week's failing seed now produces a different
//     case and the minimized artifact in evidence stops reproducing. `fork(label)`
//     derives an independent stream from (seed, label), so streams are addressed by
//     NAME. New generators are additive; old seeds keep their meaning.
//  2. INTEGER STATE, EXPLICITLY MASKED. SplitMix32 over uint32 with `>>> 0` after
//     every step. No float accumulator, so two Node versions cannot disagree about
//     the low bits and quietly desynchronise a corpus.
//
// The PRNG is deliberately NOT crypto: it must be fast, tiny, and above all
// re-runnable from a written-down number.

/** Mixing constant from SplitMix32 (Steele/Lea/Flood), the 32-bit variant. */
const GOLDEN_GAMMA = 0x9e3779b9;

/**
 * FNV-1a over UTF-16 code units. Turns a human-written seed label
 * ("protocol-fuzz/v1") into the uint32 the generator actually consumes, so
 * evidence can record a name a person can retype rather than an opaque number.
 * @param {string} text
 * @returns {number} uint32
 */
export function hashSeed(text) {
  let hash = 0x811c9dc5;
  for (let index = 0; index < text.length; index += 1) {
    hash ^= text.charCodeAt(index);
    // 16777619, expressed as shifts so the result stays inside uint32 without
    // going through a float multiply.
    hash = (hash + ((hash << 1) + (hash << 4) + (hash << 7) + (hash << 8) + (hash << 24))) >>> 0;
  }
  return hash >>> 0;
}

/** Normalize a seed given as either a number or a label. @param {number|string} seed */
export function normalizeSeed(seed) {
  if (typeof seed === 'number') {
    if (!Number.isInteger(seed)) throw new TypeError(`seed must be an integer, got ${seed}`);
    return seed >>> 0;
  }
  return hashSeed(seed);
}

/**
 * SplitMix32. Small, fast, and — the only property that matters here — identical
 * on every platform for a given seed.
 */
export class Rng {
  /** @param {number|string} seed @param {string} [label] */
  constructor(seed, label = 'root') {
    this.seed = normalizeSeed(seed);
    this.label = label;
    this.state = this.seed;
    this.draws = 0;
  }

  /** @returns {number} the next uint32 */
  next() {
    this.draws += 1;
    this.state = (this.state + GOLDEN_GAMMA) >>> 0;
    let z = this.state;
    z = Math.imul(z ^ (z >>> 16), 0x21f0aaad) >>> 0;
    z = Math.imul(z ^ (z >>> 15), 0x735a2d97) >>> 0;
    return (z ^ (z >>> 15)) >>> 0;
  }

  /** Uniform in [0, 1). @returns {number} */
  float() {
    // 2**-32, applied once, so the mantissa never accumulates rounding drift.
    return this.next() * 2.3283064365386963e-10;
  }

  /**
   * Uniform integer in [min, max] INCLUSIVE. Rejection-free modulo is biased, so
   * for the tiny ranges this suite uses the bias is below the resolution of any
   * assertion made about it; the ranges are asserted small in the tests.
   * @param {number} min @param {number} max
   */
  int(min, max) {
    if (max < min) throw new RangeError(`int(${min}, ${max}): empty range`);
    return min + (this.next() % (max - min + 1));
  }

  /** @param {number} [probability] @returns {boolean} */
  bool(probability = 0.5) {
    return this.float() < probability;
  }

  /** @template T @param {readonly T[]} items @returns {T} */
  pick(items) {
    if (items.length === 0) throw new RangeError('pick() on an empty list');
    return /** @type {T} */ (items[this.int(0, items.length - 1)]);
  }

  /**
   * Weighted choice over `[weight, value]` pairs. Used so a corpus can be tuned
   * ("more malformed frames, fewer legal ones") without changing which STREAM a
   * generator draws from.
   * @template T @param {readonly (readonly [number, T])[]} table @returns {T}
   */
  weighted(table) {
    let total = 0;
    for (const [weight] of table) {
      if (weight < 0) throw new RangeError('negative weight');
      total += weight;
    }
    if (total <= 0) throw new RangeError('weighted() needs a positive total weight');
    let roll = this.float() * total;
    for (const [weight, value] of table) {
      roll -= weight;
      if (roll < 0) return value;
    }
    return /** @type {T} */ (table[table.length - 1][1]);
  }

  /** Fisher-Yates on a COPY; the input is never mutated. @template T @param {readonly T[]} items */
  shuffle(items) {
    const out = [...items];
    for (let index = out.length - 1; index > 0; index -= 1) {
      const swap = this.int(0, index);
      const held = out[index];
      out[index] = /** @type {T} */ (out[swap]);
      out[swap] = /** @type {T} */ (held);
    }
    return out;
  }

  /** @template T @param {number} count @param {(rng: Rng, index: number) => T} make @returns {T[]} */
  list(count, make) {
    const out = [];
    for (let index = 0; index < count; index += 1) out.push(make(this, index));
    return out;
  }

  /**
   * An INDEPENDENT stream named by `label`. Two forks with different labels never
   * share a sequence, and the same label always yields the same sequence, which is
   * what lets a new generator be added without invalidating recorded seeds.
   * @param {string} label
   */
  fork(label) {
    const derived = (this.seed ^ hashSeed(label)) >>> 0;
    // One mix so `fork('a')` of seed S and `fork('b')` of seed S' cannot collide
    // merely because S ^ h(a) === S' ^ h(b) is easy to arrange by hand.
    const mixed = Math.imul(derived ^ (derived >>> 16), 0x45d9f3b) >>> 0;
    return new Rng(mixed, `${this.label}/${label}`);
  }

  /** Restart this stream at its seed. Replay of one case must not need a new object. */
  reset() {
    this.state = this.seed;
    this.draws = 0;
    return this;
  }
}

/**
 * Build the stream for one (seedLabel, streamLabel) pair.
 * The single entry point generators use, so no generator can accidentally read
 * the root tape and couple itself to its neighbours.
 * @param {number|string} seed @param {string} stream
 */
export function streamFor(seed, stream) {
  return new Rng(seed, 'seed').fork(stream);
}
