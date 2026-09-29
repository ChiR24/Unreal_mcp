// Wiring contracts of the Widget Blueprint authoring handlers that no unit test can
// reach by running them (they need an editor): who passes which flag to the shared
// tree funnel, and what a reply names. Behaviour itself belongs to the integration
// cases in tests/mcp-tools/core/manage-blueprint*.test.mjs.

import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const DOMAIN = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Domains', 'WidgetAuthoring');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
function stripComments(source: string): string {
  return source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
}

function read(...segments: readonly string[]): string {
  return stripComments(readFileSync(join(DOMAIN, ...segments), 'utf8'));
}

function sources(dir: string = DOMAIN): string[] {
  return readdirSync(dir, { withFileTypes: true }).flatMap((entry) =>
    entry.isDirectory() ? sources(join(dir, entry.name)) : /\.(cpp|h)$/u.test(entry.name) ? [join(dir, entry.name)] : []);
}

describe('SafeAddWidgetToTree: only an add that re-uses a slotName warns that the widget was already seated', () => {
  it('the warning is gated on the caller not moving the widget on purpose', () => {
    const tree = read('Support', 'McpAutomationBridge_WidgetAuthoringTree.cpp');

    expect(tree).toMatch(/if \(bDetached && bWarnOnReuse\)/u);
    expect(tree).toMatch(/DetachFromOwningPanel\(WidgetBP, NewWidget, OldSlot, OldParent, OldIndex, !bMove\);/u);
  });

  it('reparent_widget is the one caller that says the move is intended', () => {
    const calls: Array<{ readonly file: string; readonly args: string }> = [];
    for (const file of sources()) {
      if (/WidgetAuthoringTree(Mutation)?\.(cpp|h)$/u.test(file)) continue;
      for (const match of stripComments(readFileSync(file, 'utf8')).matchAll(/SafeAddWidgetToTree\(([^;{]*)/gu)) {
        calls.push({ file: file.replace(/^.*[\\/]/u, ''), args: match[1] ?? '' });
      }
    }
    const moving = calls.filter((call) => /,\s*true\s*\)/u.test(call.args));

    expect(calls.length, 'every add path still goes through the funnel').toBeGreaterThanOrEqual(4);
    expect(moving.map((call) => call.file)).toEqual(['McpAutomationBridge_WidgetAuthoringManipulation.cpp']);

    const manipulation = read('Support', 'McpAutomationBridge_WidgetAuthoringManipulation.cpp');
    const reparent = manipulation.slice(manipulation.indexOf('reparent_widget'), manipulation.indexOf('get_widget_slot_info'));
    expect(reparent).toMatch(/SafeAddWidgetToTree\(WidgetBP, TargetWidget, NewParentWidget->GetName\(\),[^;{]*,\s*true\)/u);
  });
});
