/// <reference types="node" />

import { existsSync, readFileSync, readdirSync, statSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';

import { CANONICAL_CAPABILITY_RECORDS } from '../../src/tools/catalog/capabilities/generated/canonical-registry.generated.js';

import { buildResolverIndex, executeReference, type CapabilityLike, type DispatchResult } from './task-27-suite/execute-reference.js';
import { minimalValidParams } from './task-27-suite/case-builder.js';

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

const records = CANONICAL_CAPABILITY_RECORDS as readonly CapabilityLike[];
const index = buildResolverIndex(records);

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
const filesMentioning = (token: string): readonly string[] =>
  handlerSources
    .filter((file) => readFileSync(file, 'utf8').includes(token))
    .map((file) => file.slice(pluginPrivate.length + 1))
    .sort();

const TASK_21_CAPABILITIES = [
  'inspect.get_component_details',
  'inspect.get_project_settings',
  'system_control.get_project_settings',
  'system_control.set_project_setting',
] as const;

const validOutputFor = (record: CapabilityLike): Record<string, unknown> => {
  const schema = record.schemas.output as { properties?: Record<string, Record<string, unknown>>; required?: string[] };
  const output: Record<string, unknown> = {};
  for (const name of schema.required ?? []) {
    const declared = schema.properties?.[name]?.type;
    output[name] = declared === 'boolean' ? true
      : declared === 'number' || declared === 'integer' ? 1
      : declared === 'array' ? []
      : declared === 'object' ? {}
      : 'ok';
  }
  return output;
};

describe('Task 27 / Task 21: the four deferred capabilities are canonically reachable', () => {
  it('resolves every deferred capability from the canonical registry', () => {
    for (const id of TASK_21_CAPABILITIES) {
      expect(index.byId.get(id), `${id} must exist as a canonical record`).toBeDefined();
    }
  });

  it('normalizes the canonical and legacy forms of each to one validated dispatch', () => {
    for (const id of TASK_21_CAPABILITIES) {
      const record = index.byId.get(id);
      expect(record).toBeDefined();
      if (!record) continue;
      const params = minimalValidParams(record);
      const legacy = record.legacyIds[0];
      expect(legacy, `${id} must carry a generated legacy id`).toBeDefined();

      const deps = (queued: string[]) => ({
        index,
        isEnabled: () => true,
        dispatch: (dispatched: CapabilityLike): DispatchResult => {
          queued.push(dispatched.id);
          return { ok: true, data: validOutputFor(dispatched) };
        },
      });

      const canonicalQueue: string[] = [];
      const canonical = executeReference({ capability: id, params }, deps(canonicalQueue));
      const legacyQueue: string[] = [];
      const viaLegacy = executeReference(
        { tool: legacy?.tool, action: legacy?.action, params },
        deps(legacyQueue),
      );

      expect(canonical.status, `${id} canonical form`).toBe('success');
      expect(viaLegacy.status, `${id} legacy form`).toBe('success');
      if (canonical.status !== 'success' || viaLegacy.status !== 'success') continue;
      expect(viaLegacy.capabilityId).toBe(canonical.capabilityId);
      expect(viaLegacy.dispatch).toEqual(canonical.dispatch);
      expect(canonicalQueue).toEqual([id]);
      expect(legacyQueue).toEqual([id]);
    }
  });

  it('rejects an undeclared parameter on each before any dispatch', () => {
    for (const id of TASK_21_CAPABILITIES) {
      const record = index.byId.get(id);
      if (!record) continue;
      const queued: string[] = [];
      const receipt = executeReference(
        { capability: id, params: { ...minimalValidParams(record), task27Undeclared: true } },
        {
          index,
          isEnabled: () => true,
          dispatch: (dispatched): DispatchResult => {
            queued.push(dispatched.id);
            return { ok: true, data: {} };
          },
        },
      );
      expect(receipt.status).toBe('error');
      if (receipt.status === 'error') expect(receipt.error.gatewayCode).toBe('UNDECLARED_PARAMETER');
      expect(queued, `${id} must not reach the queue`).toEqual([]);
    }
  });
});

describe('Task 27 / Task 21: the residual native handler divergence stays visible', () => {
  // Task 21 sublane 4: TS carries component logic locally; native has no
  // distinct body and falls through to generic object inspection. Task 27 does
  // not own Private/Domains, so this is pinned, not silently repaired.
  it('records that get_component_details still has no distinct native handler branch', () => {
    expect(filesMentioning('get_component_details')).toEqual([]);
  }, 60_000);

  // Task 21 sublane 2: set_project_setting is implemented only in the Ui shim.
  it('records that set_project_setting is still owned solely by the Ui domain shim', () => {
    const owners = filesMentioning('set_project_setting');
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
    expect(index.byId.get('inspect.get_project_settings')?.routing.parentTool).toBe('inspect');
    expect(index.byId.get('system_control.get_project_settings')?.routing.parentTool).toBe('system_control');
    expect(index.byId.get('system_control.set_project_setting')?.routing.parentTool).toBe('system_control');
  });
});
