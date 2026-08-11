/**
 * Routing contract for the promoted GAS and AI routes.
 *
 * Seven operations are implemented and natively dispatchable — four under
 * `Private/Domains/GAS/`, three under `Private/Domains/AI/` — but neither parent
 * reaches them from MCP. `manage_gas` and `manage_ai` both register with
 * MCP_REGISTER_DIRECT, so no native predicate array gates them and no native
 * edit is needed; the gap is entirely on the TypeScript side, where each parent
 * screens actions before dispatch and answers `createUnknownActionResponse`.
 *
 * The two parents screen differently, so both halves are pinned here: `manage_gas`
 * looks the action up in a route table, while `manage_ai` matches `case` arms in
 * a switch whose `default` rejects. The third case asserts the native chain still
 * carries all seven, so a future edit that removes the native dispatch while
 * leaving the TypeScript gate open fails loudly instead of silently answering
 * UNKNOWN_SUBACTION from the editor.
 */

import { readdirSync, readFileSync, statSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';
import { getGASActionRoute } from '../../../src/tools/handlers/gas/gas-action-routes.js';

const GAS_PROMOTED = [
  'create_ability_set',
  'add_ability',
  'grant_ability',
  'create_execution_calculation',
] as const;

const AI_PROMOTED = [
  'create_nav_modifier',
  'set_ai_movement',
  'set_ai_perception',
] as const;

const AI_HANDLERS = resolve(
  process.cwd(),
  'src/tools/handlers/ai/ai-handlers.ts',
);

const NATIVE_DISPATCH = [
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/GAS',
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains/AI',
].map((dir) => resolve(process.cwd(), dir));

function nativeSources(): string {
  const out: string[] = [];
  const walk = (dir: string): void => {
    for (const entry of readdirSync(dir)) {
      const full = resolve(dir, entry);
      if (statSync(full).isDirectory()) walk(full);
      else if (entry.endsWith('.cpp')) out.push(readFileSync(full, 'utf8'));
    }
  };
  for (const dir of NATIVE_DISPATCH) walk(dir);
  return out.join('\n');
}

describe('promoted GAS and AI routes are named on the surfaces that gate them', () => {
  it('the manage_gas route table resolves every promoted GAS action', () => {
    const missing = GAS_PROMOTED.filter((action) => getGASActionRoute(action) === undefined);

    expect(
      missing,
      'absent from GAS_ACTION_ROUTES, so handleGASTools answers '
      + 'createUnknownActionResponse and the native handler is never reached',
    ).toEqual([]);
  });

  it('the manage_ai switch carries a case arm for every promoted AI action', () => {
    const source = readFileSync(AI_HANDLERS, 'utf8');
    const missing = AI_PROMOTED.filter((action) => !source.includes(`case '${action}':`));

    expect(
      missing,
      'no case arm in handleAITools, so the switch falls to its default and '
      + 'answers createUnknownActionResponse',
    ).toEqual([]);
  });

  it('the native domains still dispatch every promoted action', () => {
    const source = nativeSources();
    const missing = [...GAS_PROMOTED, ...AI_PROMOTED].filter(
      (action) => !source.includes(`TEXT("${action}")`),
    );

    expect(
      missing,
      'no native dispatch, so opening the TypeScript gate would forward to a '
      + 'subAction the editor answers UNKNOWN_SUBACTION for',
    ).toEqual([]);
  });
});
