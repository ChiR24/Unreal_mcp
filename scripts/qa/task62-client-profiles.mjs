// @ts-check
// scripts/qa/task62-client-profiles.mjs
// Task 62 — read the full and minimal MCP client profiles from Task 35's source
// of truth instead of restating them.
//
// The six structural booleans live in
// `src/server/mcp-primitives/session-capability-profile.ts`. Copying them here
// would create a second definition that silently drifts the first time a seventh
// capability is added, and a compatibility record that lists five of six client
// capabilities is worse than one that lists none. So they are parsed out of the
// declared interface, and a parse that finds nothing throws rather than returning
// an empty profile that would read as "this client can do nothing".

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

export const PROFILE_SOURCE = 'src/server/mcp-primitives/session-capability-profile.ts';

/**
 * The capability keys the structural profile declares, in source order.
 * @param {string} [projectRoot]
 * @returns {string[]}
 */
export function profileKeys(projectRoot = process.cwd()) {
  const source = readFileSync(resolve(projectRoot, PROFILE_SOURCE), 'utf8');
  const start = source.indexOf('export interface ClientCapabilityProfile');
  if (start < 0) throw new Error(`${PROFILE_SOURCE} no longer declares ClientCapabilityProfile; the client axis of the compatibility record cannot be derived`);
  const body = source.slice(start, source.indexOf('}', start));
  const keys = [...body.matchAll(/readonly\s+(\w+)\s*:\s*boolean/gu)].map((match) => match[1]);
  if (keys.length === 0) throw new Error(`${PROFILE_SOURCE} declares ClientCapabilityProfile with no boolean members; refusing to report an empty client profile`);
  return keys;
}

/** A client that declared nothing: every structural capability absent. */
export const MINIMAL_PROFILE = Object.freeze(Object.fromEntries(profileKeys().map((key) => [key, false])));

/** A client that declared everything: the upper bound of the same six booleans. */
export const FULL_PROFILE = Object.freeze(Object.fromEntries(profileKeys().map((key) => [key, true])));
