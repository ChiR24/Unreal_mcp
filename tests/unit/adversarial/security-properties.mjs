// @ts-check
// tests/unit/adversarial/security-properties.mjs
// Task 51 — the adversarial RUNTIME properties: path/symlink containment, the
// idempotency ledger under interleaving, and settle-once cancellation.
//
// These are deliberately NOT source-contract checks. The plugin's constant-time
// token compare and its fail-closed non-loopback bind are already pinned by
// tests/unit/plugin/security_contracts.test.ts, which reads the C++ and asserts the
// required and forbidden patterns; re-asserting the same strings here would add a
// second place to update and no new detection. This file drives the REAL objects
// with seeded hostile input instead, which is the coverage the static checks cannot
// give: a rule can be present in the source and still be walked past at run time.
//
// This module holds no `src/` imports so plain node can load it; the bindings to
// the real sanitizePath and IdempotencyLedger live in security-properties-source.mjs,
// and an offline CLI supplies the same two from `dist/`. The split is about module
// resolution only — both paths drive the one production implementation.
//
// THE SYMLINK CHECK USES A REAL SYMLINK. A containment test that only concatenates
// strings proves the string logic and nothing about the filesystem: the escape that
// matters is a path that is textually inside the owned root and resolves outside it.
// So the fixtures are created on disk, under a directory this suite owns, and the
// predicate is asked about the RESOLVED path.

import { mkdtempSync, mkdirSync, symlinkSync, writeFileSync, realpathSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve, sep } from 'node:path';


/** Roots `sanitizePath` accepts by default. */
export const UE_ROOTS = Object.freeze(['/Game', '/Engine', '/Script', '/Temp', '/Niagara']);

/**
 * The UE-path verdict, as a value rather than an exception, so a fuzz loop can
 * classify thousands of inputs without a try/catch at every call site.
 * @param {(path: string, allowedRoots?: string[]) => string} sanitize
 */
export function uePathVerdictWith(sanitize) {
  /** @param {string} candidate
   * @returns {{ accepted: boolean, normalized: string|null, reason: string }} */
  return (candidate) => {
    try {
      return { accepted: true, normalized: sanitize(candidate), reason: 'ACCEPTED' };
    } catch (error) {
      return { accepted: false, normalized: null, reason: error instanceof Error ? error.message : String(error) };
    }
  };
}

/**
 * The invariant every accepted UE path must satisfy, checked INDEPENDENTLY of the
 * function that accepted it. Restating the rule here is the point: if `sanitizePath`
 * ever returns something that starts outside a root, or still carries a traversal
 * segment, this catches it without asking `sanitizePath` whether it was right.
 * @param {string} normalized
 */
export function normalizedPathIsContained(normalized) {
  if (!normalized.startsWith('/')) return { ok: false, why: 'does not start at a root' };
  if (normalized.includes('..')) return { ok: false, why: 'retains a traversal segment' };
  if (normalized.includes('//')) return { ok: false, why: 'retains an empty segment' };
  const lower = normalized.toLowerCase();
  const inRoot = UE_ROOTS.some((root) => lower === root.toLowerCase() || lower.startsWith(`${root.toLowerCase()}/`));
  if (!inRoot) return { ok: false, why: `resolves outside every allowed root: ${normalized}` };
  return { ok: true, why: 'inside an allowed root with no traversal or empty segment' };
}

/**
 * A disposable host-filesystem fixture with a REAL symlink escape in it.
 * `owned` is the root this suite may delete; `outside` is deliberately a sibling,
 * so a predicate that merely does `startsWith` on the unresolved string will pass a
 * path this returns as an escape.
 */
export function makeSymlinkFixture() {
  const base = mkdtempSync(join(realpathSync(tmpdir()), 'task51-paths-'));
  const owned = join(base, 'owned');
  const outside = join(base, 'outside');
  mkdirSync(owned, { recursive: true });
  mkdirSync(outside, { recursive: true });
  writeFileSync(join(outside, 'secret.txt'), 'not yours\n');
  writeFileSync(join(owned, 'mine.txt'), 'ok\n');
  const escapeLink = join(owned, 'escape');
  symlinkSync(outside, escapeLink, 'dir');
  const selfLink = join(owned, 'self');
  symlinkSync(owned, selfLink, 'dir');
  return {
    base,
    owned,
    outside,
    /** Textually inside `owned`, resolves into `outside`. */
    escapingPath: join(escapeLink, 'secret.txt'),
    /** Textually and really inside `owned`. */
    containedPath: join(owned, 'mine.txt'),
    /** A symlink that stays inside the owned root: containment must still ACCEPT it. */
    loopbackLinkPath: join(selfLink, 'mine.txt'),
    dispose: () => { rmSync(base, { recursive: true, force: true }); },
  };
}

/**
 * Host-path containment decided on the RESOLVED path.
 * A missing path is reported as undecided rather than contained: answering
 * "contained" for something that does not exist is how a check passes for a target
 * it never looked at.
 * @param {string} root @param {string} candidate
 */
export function hostPathContained(root, candidate) {
  let realRoot;
  let realCandidate;
  try {
    realRoot = realpathSync(resolve(root));
  } catch {
    return { contained: false, decided: false, why: 'the owned root does not resolve' };
  }
  try {
    realCandidate = realpathSync(resolve(candidate));
  } catch {
    return { contained: false, decided: false, why: 'the candidate does not resolve; nothing was proven about it' };
  }
  const contained = realCandidate === realRoot || realCandidate.startsWith(`${realRoot}${sep}`);
  return {
    contained,
    decided: true,
    why: contained
      ? `${realCandidate} resolves inside ${realRoot}`
      : `${realCandidate} resolves OUTSIDE ${realRoot}; the unresolved spelling was inside it`,
  };
}

/** A manual clock, so ledger TTL is exercised without sleeping.
 * @param {number} [start] */
export function manualClock(start = 1_000_000) {
  let now = start;
  return { now: () => now, advance: (/** @type {number} */ ms) => { now += ms; } };
}

/**
 * Drive the REAL idempotency ledger through a seeded interleaving of begin /
 * complete / abandon across two principals, and report the invariants it must never
 * break. Returns the violations rather than asserting, so the caller can attach a
 * minimized replay artifact to each one.
 * @param {any} LedgerClass the real IdempotencyLedger constructor
 * @param {import('./fuzz-random.mjs').Rng} rng
 * @param {{ operations?: number, maxEntries?: number }} [options]
 */
export function exerciseIdempotencyLedgerWith(LedgerClass, rng, options = {}) {
  const clock = manualClock();
  const ledger = new LedgerClass({ clock: clock.now, maxEntries: options.maxEntries ?? 8, ttlMs: 60_000 });
  /** @type {Array<{ handle: { slot: string }, principal: string, capabilityId: string, key: string, fingerprint: string }>} */
  const inFlight = [];
  /** @type {string[]} */
  const violations = [];
  /** @type {Map<string, string>} */
  const completedFingerprints = new Map();
  const principals = ['alice', 'bob'];
  const capabilities = ['manage_asset.create_material', 'control_actor.spawn_actor'];

  for (let step = 0; step < (options.operations ?? 200); step += 1) {
    const action = rng.weighted([[4, 'begin'], [3, 'complete'], [2, 'abandon'], [1, 'tick']]);
    if (action === 'tick') { clock.advance(rng.int(1, 5_000)); continue; }

    if (action === 'begin') {
      const principal = rng.pick(principals);
      const capabilityId = rng.pick(capabilities);
      const key = `k${rng.int(0, 5)}`;
      const fingerprint = `f${rng.int(0, 2)}`;
      const scopeKey = `${principal}|${capabilityId}|${key}`;
      const outcome = ledger.begin({ principal, capabilityId, key }, fingerprint);
      if (outcome.kind === 'first') {
        inFlight.push({ handle: outcome.handle, principal, capabilityId, key, fingerprint });
      } else if (outcome.kind === 'replay') {
        const expected = completedFingerprints.get(scopeKey);
        if (expected !== fingerprint) {
          violations.push(`replay returned a receipt for ${scopeKey} whose recorded fingerprint was ${String(expected)} but the request carried ${fingerprint}`);
        }
      } else if (outcome.kind === 'conflict') {
        // A conflict must disclose nothing. The outcome shape has no receipt field
        // at all, which is the strongest form of that guarantee; assert it anyway,
        // because a future field added "for debugging" is exactly how it leaks.
        if (Object.prototype.hasOwnProperty.call(outcome, 'receipt')) {
          violations.push(`a conflict for ${scopeKey} carried a receipt, disclosing another principal's result`);
        }
      }
      continue;
    }

    if (inFlight.length === 0) continue;
    const index = rng.int(0, inFlight.length - 1);
    const entry = /** @type {typeof inFlight[number]} */ (inFlight.splice(index, 1)[0]);
    const scopeKey = `${entry.principal}|${entry.capabilityId}|${entry.key}`;
    if (action === 'complete') {
      ledger.complete(entry.handle, { ok: true, capability: entry.capabilityId });
      completedFingerprints.set(scopeKey, entry.fingerprint);
    } else {
      ledger.abandon(entry.handle);
      completedFingerprints.delete(scopeKey);
      // A FAILURE IS NEVER CACHED: after abandon the same key must be admissible.
      const retry = ledger.begin({ principal: entry.principal, capabilityId: entry.capabilityId, key: entry.key }, entry.fingerprint);
      if (retry.kind !== 'first') {
        violations.push(`after abandon, ${scopeKey} returned "${retry.kind}" instead of being retryable; a failure was cached`);
      } else {
        ledger.abandon(retry.handle);
      }
    }
  }

  // EVICTION MUST NEVER DROP AN IN-FLIGHT ENTRY: every outstanding handle must still
  // block a duplicate, no matter how far over cap the completed population went.
  for (const entry of inFlight) {
    const duplicate = ledger.begin({ principal: entry.principal, capabilityId: entry.capabilityId, key: entry.key }, entry.fingerprint);
    if (duplicate.kind !== 'in-flight') {
      violations.push(`an in-flight slot for ${entry.principal}|${entry.capabilityId}|${entry.key} was evicted: a duplicate got "${duplicate.kind}" and would have dispatched a second real mutation`);
    }
  }

  return { violations, size: ledger.size(), debugState: ledger.debugState(), inFlight: inFlight.length };
}

/**
 * A settle-once box: the shape a cancellation race must obey. Returns how many
 * terminal results were ATTEMPTED and how many were delivered; more than one
 * delivery is the bug.
 */
export function settleOnce() {
  /** @type {unknown} */
  let value;
  let delivered = 0;
  let attempted = 0;
  return {
    settle(/** @type {unknown} */ candidate) {
      attempted += 1;
      if (delivered > 0) return false;
      delivered += 1;
      value = candidate;
      return true;
    },
    get state() { return { attempted, delivered, value }; },
  };
}
