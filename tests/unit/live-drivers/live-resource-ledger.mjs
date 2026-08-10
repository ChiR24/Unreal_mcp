// @ts-check
// tests/unit/live-drivers/live-resource-ledger.mjs
// Task 49 — the QA resource ledger and the secret redactor.
//
// CLEANUP IS AN ACCEPTANCE CRITERION, NOT HYGIENE. A live run that leaves a child
// `node dist/cli.js`, an undeleted `/mcp` session, or a seeded asset does not just
// litter: it changes the result of the NEXT run. Task 42's asset-registry reads are
// only conclusive because the folder they read was empty when the run began.
//
// So every resource this suite creates is registered the instant it exists, and
// teardown emits a RECEIPT: an explicit per-resource record of what was released
// and how that release was observed. "We called kill()" is not a receipt; "pgrep
// returns 0 for this pid" is.
//
// The redactor lives here because the ledger is what serializes evidence to disk.
// The capability token is read from the environment and must never reach a log
// line, a receipt or an evidence file — so the ONE function that writes evidence
// is also the one that scrubs it, and nothing has to remember to call it.

import { writeFileSync, mkdirSync } from 'node:fs';
import { dirname, resolve } from 'node:path';

/** Resource classes the ledger knows how to account for. */
export const RESOURCE_KINDS = Object.freeze(['process', 'session', 'port', 'tempdir', 'content']);

/**
 * Environment variables whose VALUES are secrets. Their values are replaced
 * everywhere they appear in serialized evidence, however deeply nested and
 * whether or not the key that carries them looks sensitive — a token pasted into
 * a free-text error message is still a leaked token.
 */
export const SECRET_ENV_VARS = Object.freeze([
  'MCP_QA_TOKEN',
  'MCP_AUTOMATION_CAPABILITY_TOKEN',
  'MCP_CAPABILITY_TOKEN',
  'MCP_METRICS_TOKEN',
]);

export const REDACTED = '[REDACTED]';

/**
 * Collect the live secret values from the environment. Short values are ignored:
 * a one- or two-character token would turn the redactor into a text shredder and
 * destroy the evidence it is meant to protect.
 * @param {NodeJS.ProcessEnv} [env]
 * @returns {string[]}
 */
export function secretValues(env = process.env) {
  /** @type {string[]} */
  const values = [];
  for (const name of SECRET_ENV_VARS) {
    const value = env[name];
    if (typeof value === 'string' && value.length >= 4 && !values.includes(value)) values.push(value);
  }
  // Longest first, so a token that contains another token's text is masked whole.
  return values.sort((a, b) => b.length - a.length);
}

/**
 * Deep-redact every occurrence of every secret value in a structure, returning a
 * new value. Keys are redacted too: a secret used as an object key is as leaked
 * as one used as a value.
 * @param {unknown} value @param {readonly string[]} secrets @returns {unknown}
 */
export function redact(value, secrets) {
  if (secrets.length === 0) return value;
  if (typeof value === 'string') {
    let out = value;
    for (const secret of secrets) out = out.split(secret).join(REDACTED);
    return out;
  }
  if (Array.isArray(value)) return value.map((entry) => redact(entry, secrets));
  if (value !== null && typeof value === 'object') {
    /** @type {Record<string, unknown>} */
    const out = {};
    for (const [key, entry] of Object.entries(value)) {
      out[String(redact(key, secrets))] = redact(entry, secrets);
    }
    return out;
  }
  return value;
}

/** @param {unknown} error @returns {string} */
export function errorText(error) {
  if (error instanceof Error) return error.message;
  return String(error);
}

/**
 * @typedef {{ kind: string, id: string, detail: Record<string, unknown>,
 *   release: () => Promise<void>, verify: () => Promise<{ released: boolean, observed: string }>,
 *   registeredAt: string }} LedgerEntry
 */

/**
 * Tracks every QA-owned resource and proves its teardown.
 */
export class ResourceLedger {
  constructor() {
    /** @type {LedgerEntry[]} */
    this.entries = [];
    /** @type {Array<Record<string, unknown>>} */
    this.receipts = [];
  }

  /**
   * @param {string} kind @param {string} id @param {Record<string, unknown>} detail
   * @param {() => Promise<void>} release
   * @param {() => Promise<{ released: boolean, observed: string }>} verify
   */
  register(kind, id, detail, release, verify) {
    if (!RESOURCE_KINDS.includes(kind)) throw new Error(`unknown resource kind "${kind}"`);
    this.entries.push({ kind, id, detail, release, verify, registeredAt: new Date().toISOString() });
    return this;
  }

  /** Resources still outstanding, newest first — teardown order is reverse of creation. */
  outstanding() {
    return [...this.entries].reverse();
  }

  /**
   * Release everything and verify each release independently. A release that
   * THROWS is still recorded and still verified: a teardown that fails silently
   * is exactly the residue this ledger exists to surface.
   * @returns {Promise<{ total: number, released: number, leaked: number, receipts: Array<Record<string, unknown>> }>}
   */
  async teardown() {
    for (const entry of this.outstanding()) {
      /** @type {string|null} */
      let releaseError = null;
      try {
        await entry.release();
      } catch (error) {
        releaseError = errorText(error);
      }
      /** @type {{ released: boolean, observed: string }} */
      let verdict;
      try {
        verdict = await entry.verify();
      } catch (error) {
        verdict = { released: false, observed: `verification threw: ${errorText(error)}` };
      }
      this.receipts.push({
        kind: entry.kind,
        id: entry.id,
        detail: entry.detail,
        registeredAt: entry.registeredAt,
        releasedAt: new Date().toISOString(),
        releaseError,
        released: verdict.released,
        observed: verdict.observed,
      });
    }
    const released = this.receipts.filter((receipt) => receipt.released === true).length;
    return {
      total: this.receipts.length,
      released,
      leaked: this.receipts.length - released,
      receipts: this.receipts,
    };
  }
}

/**
 * Write evidence to disk with every secret removed on the way out. This is the
 * ONLY sanctioned way this suite persists a report, so a token cannot reach a
 * file by someone forgetting a redaction call at one of several call sites.
 * @param {string} path @param {unknown} report @param {NodeJS.ProcessEnv} [env]
 */
export function writeRedactedEvidence(path, report, env = process.env) {
  const absolute = resolve(path);
  mkdirSync(dirname(absolute), { recursive: true });
  const clean = redact(report, secretValues(env));
  writeFileSync(absolute, `${JSON.stringify(clean, null, 2)}\n`);
  return absolute;
}

/**
 * Read the capability token from the environment. Never defaulted to a literal
 * in a committed file: a hardcoded token is a credential in version control even
 * when it is "only" the loopback test one, and it silently makes an unauthenticated
 * run look authenticated.
 * @param {NodeJS.ProcessEnv} [env]
 * @returns {{ token: string|null, source: string }}
 */
export function readCapabilityToken(env = process.env) {
  for (const name of ['MCP_QA_TOKEN', 'MCP_AUTOMATION_CAPABILITY_TOKEN', 'MCP_CAPABILITY_TOKEN']) {
    const value = env[name];
    if (typeof value === 'string' && value.length > 0) return { token: value, source: name };
  }
  return { token: null, source: 'unset' };
}
