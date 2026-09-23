/**
 * Both surfaces decide which domain handler gets an action from a hand-kept
 * list: a Set in consolidated-routing.ts and a TArray in the native
 * McpConsolidatedActionRouting*.h headers. Neither is generated, so an action
 * added to one list only is routed to a different handler by the other surface.
 * That is how build_metasound reached the native "volume" handler.
 */

import { readdirSync, readFileSync } from 'node:fs';
import path from 'node:path';
import { describe, expect, it } from 'vitest';
import * as routing from '../../../src/tools/orchestration/consolidated-routing.js';

const ROUTING_DIR =
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Routing';

const PAIRS: ReadonlyArray<readonly [keyof typeof routing, string]> = [
  ['materialAuthoringActionSet', 'MaterialAuthoring'],
  ['textureActionSet', 'Texture'],
  ['skeletonActionSet', 'Skeleton'],
  ['lightingActionSet', 'Lighting'],
  ['splineActionSet', 'Splines'],
  ['renderActionSet', 'Rendering'],
  ['performanceActionSet', 'Performance'],
  ['behaviorTreeActionSet', 'BehaviorTree'],
  ['navigationActionSet', 'Navigation'],
  ['widgetAuthoringActionSet', 'WidgetAuthoring'],
  ['sessionActionSet', 'Sessions'],
  ['gameFrameworkActionSet', 'GameFramework'],
  ['inputActionSet', 'Input'],
  ['volumeActionSet', 'Volumes'],
  ['animationAuthoringActionSet', 'AnimationAuthoring'],
  ['audioAuthoringActionSet', 'AudioAuthoring'],
];

// Native-only members that TypeScript reaches by another path on purpose:
// add_notify is routed conditionally in consolidated-handler-registration.ts,
// show_fps is a system_control action, and the rest have their own handler case.
const KNOWN_NATIVE_ONLY: Readonly<Record<string, readonly string[]>> = {
  MaterialAuthoring: ['set_node_position'],
  Performance: ['show_fps'],
  GameFramework: ['set_hud_class'],
  AnimationAuthoring: ['add_notify', 'delete_transition'],
};

function nativeLists(): Map<string, Set<string>> {
  const source = readdirSync(ROUTING_DIR)
    .filter((name) => /^McpConsolidatedActionRouting.*\.h$/.test(name))
    .map((name) => readFileSync(path.join(ROUTING_DIR, name), 'utf8'))
    .join('\n');
  const lists = new Map<string, Set<string>>();
  for (const match of source.matchAll(/inline const TArray<FString>& (\w+)\(\)\s*\{([\s\S]*?)\n\}/g)) {
    lists.set(match[1], new Set([...match[2].matchAll(/TEXT\("([^"]+)"\)/g)].map((m) => m[1])));
  }
  return lists;
}

describe('TypeScript and native domain routing lists agree', () => {
  const native = nativeLists();

  it.each(PAIRS)('%s matches native %s()', (tsName, nativeName) => {
    const tsSet = routing[tsName] as Set<string>;
    const nativeSet = native.get(nativeName);
    expect(nativeSet, `no native ${nativeName}() list found`).toBeDefined();
    const onlyTs = [...tsSet].filter((action) => !nativeSet?.has(action)).sort();
    const onlyNative = [...(nativeSet ?? [])].filter((action) => !tsSet.has(action)).sort();
    expect({ onlyTs, onlyNative }).toEqual({ onlyTs: [], onlyNative: KNOWN_NATIVE_ONLY[nativeName] ?? [] });
  });
});
