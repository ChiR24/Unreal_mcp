/// <reference types="node" />

import { existsSync, readFileSync, readdirSync, statSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../src/server/gateway/gateway-capability-index.js';


// Task 21 deferred two native divergences to Task 23, which re-deferred them to
// Task 27 (see .omo/evidence/task-23-*.json task21DivergenceDisposition):
//   sublane 2 — project-setting misroutes
//   sublane 4 — inspect.get_component_details has no distinct native body
//
// Task 27 owns native validation/envelope/routing, NOT the editor domain
// handlers under Private/Domains. So the part Task 27 can resolve is resolved
// here (one canonical, schema-validated, deterministically dispatched path for
// all four capabilities), and the part it cannot is pinned by these tests so it
// can never be silently claimed as fixed.

const pluginPrivate = resolve(
  process.cwd(),
  'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private',
);

function sourceFilesUnder(...relativeDirs: readonly string[]): readonly string[] {
  const files: string[] = [];
  const walk = (dir: string): void => {
    if (!existsSync(dir)) return;
    for (const entry of readdirSync(dir)) {
      const full = resolve(dir, entry);
      if (statSync(full).isDirectory()) walk(full);
      else if (entry.endsWith('.cpp') || entry.endsWith('.h')) files.push(full);
    }
  };
  for (const relative of relativeDirs) walk(resolve(pluginPrivate, relative));
  return files;
}

const handlerSources = sourceFilesUnder('Domains', 'Core', 'Foundation');
// Read once: re-reading every source file per call took 14.7 s under a loaded machine and timed
// out the 10 s tests after the first (which alone has 60 s for this first read).
let handlerTexts: readonly (readonly [string, string])[] | undefined;
const filesMentioning = (token: string): readonly string[] =>
  (handlerTexts ??= handlerSources.map((file) => [file, readFileSync(file, 'utf8')] as const))
    .filter(([, text]) => text.includes(token))
    .map(([file]) => file.slice(pluginPrivate.length + 1).replaceAll('\\', '/'))
    .sort();

describe('Task 27 / Task 21: the residual native handler divergence stays visible', () => {
  // Task 21 sublane 4: TS carries component logic locally; native has no
  // distinct body and falls through to generic object inspection. Task 27 does
  // not own Private/Domains, so this is pinned, not silently repaired.
  it('records that get_component_details now has a distinct native handler branch (dogfood #146)', () => {
    expect(filesMentioning('get_component_details')).toContain(
      'Domains/Environment/Inspection/McpAutomationBridge_EnvironmentHandlersInspectComponent.cpp',
    );
  }, 60_000);

  // Task 21 sublane 2: set_project_setting is implemented only in the Ui shim.
  // A file that only NAMES it, in a nextCall sending the caller there, owns nothing:
  // McpBlueprintBehaviour points at it when a project's input classes are not Enhanced Input.
  const namesOnly = new Set([
    'Domains/BlueprintGraph/Behaviour/McpAutomationBridge_BlueprintBehaviourInputAssets.cpp',
  ]);
  it('records that set_project_setting is still owned solely by the Ui domain shim', () => {
    const owners = filesMentioning('set_project_setting').filter((file) => !namesOnly.has(file));
    expect(owners.length).toBeGreaterThan(0);
    for (const owner of owners) {
      expect(owner.startsWith('Domains/Ui/'), `${owner} should be the Ui shim`).toBe(true);
    }
  });

  it('records the exact native owners of get_project_settings', () => {
    expect(filesMentioning('get_project_settings')).toEqual([
      'Domains/Environment/Inspection/McpAutomationBridge_EnvironmentHandlersInspect.cpp',
      'Domains/Environment/Inspection/McpAutomationBridge_EnvironmentHandlersInspectSettings.cpp',
      'Domains/Ui/McpAutomationBridge_UiHandlersProjectSettings.cpp',
    ]);
  });

  it('keeps both project-setting capabilities advertised under their own parent tools', () => {
    expect(capabilityIndex().byId.get('inspect.get_editor_state')?.routing.parentTool).toBe('inspect');
    expect(capabilityIndex().byId.get('system_control.get_project_settings')?.routing.parentTool).toBe('system_control');
    expect(capabilityIndex().byId.get('system_control.set_project_setting')?.routing.parentTool).toBe('system_control');
  });
});
