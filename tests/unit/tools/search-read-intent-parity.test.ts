// A query that opens with a read word ("get actor location") must rank a
// read-effect capability first: before the rule, both doors answered it with
// set_transform, and a small model that takes the first row moved the actor
// instead of reading it. The two doors rank differently (BM25 here, word rules
// in the native gateway), so this pins what they must share: the word list and
// the rule, plus the rankings it exists for.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';

import { RETRIEVAL_READ_INTENT_WORDS, RETRIEVAL_SCORE_CONSTANTS } from '../../../src/tools/catalog/capabilities/retrieval/constants.js';
import { searchGatewayCapabilities } from '../../../src/server/gateway/gateway-search.js';

const GATEWAY = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'MCP', 'Gateway');
const nativeMatch = readFileSync(join(GATEWAY, 'McpNativeGatewaySearchMatch.cpp'), 'utf8');
const nativeHeader = readFileSync(join(GATEWAY, 'McpNativeGatewaySearch.h'), 'utf8');

function topCapability(query: string): string | undefined {
  const reply = searchGatewayCapabilities({ operation: 'search', query, limit: 3 }) as { results: { capability: string }[] };
  return reply.results[0]?.capability;
}

describe('read intent: both doors share the rule', () => {
  it('the native gateway lists exactly the same read words', () => {
    const block = nativeMatch.slice(nativeMatch.indexOf('ReadIntentWords[]'), nativeMatch.indexOf('};', nativeMatch.indexOf('ReadIntentWords[]')));
    const nativeWords = [...block.matchAll(/TEXT\("([a-z]+)"\)/gu)].map((match) => match[1]);
    expect([...nativeWords].sort()).toEqual([...RETRIEVAL_READ_INTENT_WORDS].sort());
  });

  it('both doors give the rule a weight and apply it to read-effect records only', () => {
    expect(RETRIEVAL_SCORE_CONSTANTS.readIntentBonus).toBeGreaterThan(0);
    expect(nativeHeader).toMatch(/constexpr int32 McpSearchReadIntentBonus = [1-9]\d*;/u);
    expect(nativeMatch).toMatch(/Record\.Effect\.Equals\(TEXT\("read"\)[^;]*OpensWithReadWord\(Query\)/u);
  });
});

describe('read intent: rankings on the real catalog', () => {
  it.each([
    ['get actor location', 'control_actor.get_transform'],
    ['show blueprint graph', 'blueprint.inspect_graph'],
    ['read datatable rows', 'datatable.inspect_data_table'],
    ['how many actors in the level', 'control_actor.list'],
  ])('"%s" ranks %s first', (query, expected) => {
    expect(topCapability(query)).toBe(expected);
  });

  it.each([
    ['set actor location', 'control_actor.set_transform'],
    ['show hidden actor', 'control_actor.set_visibility'],
    ['stop music', 'manage_audio.stop_sound'],
  ])('a request to change something still ranks the writer first: "%s" -> %s', (query, expected) => {
    expect(topCapability(query)).toBe(expected);
  });
});
