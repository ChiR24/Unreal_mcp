// The prompts served by prompts/list and prompts/get are hand-written C++ data (MCP/Primitives/McpPromptCatalog.cpp
// and McpPromptPlaybooks.cpp): every step must name a capability the catalog has, under a tool and action that describe
// resolves, or a prompt hands an agent a call that answers UNKNOWN_CAPABILITY. Nothing else checks them.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIMITIVES = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'MCP', 'Primitives');
const read = (file: string): string => readFileSync(join(PRIMITIVES, file), 'utf8');
const SOURCES = ['McpPromptCatalog.cpp', 'McpPromptPlaybooks.cpp'].map(read).join('\n');

type Registry = { records: { id: string; aliases: string[]; legacyIds: { tool: string; action: string }[] }[] };
const REGISTRY = JSON.parse(
  readFileSync(join('src', 'tools', 'catalog', 'capabilities', 'generated', 'canonical-registry.generated.json'), 'utf8'),
) as Registry;

// A step is {summary, capabilityId, parentTool, action, resourceUri, safety}; its id has a dot, unlike an argument's
// allowed values ({"fbx", "obj", ...}), which have the same shape.
const STEP = /\{TEXT\("(?:[^"\\]|\\.)*"\), TEXT\("([a-z_]+(?:\.[a-z_]+)+)"\), TEXT\("([a-z_]+)"\), TEXT\("([a-z_]+)"\),/gu;
const steps = [...SOURCES.matchAll(STEP)].map(([, id, tool, action]) => ({ id, tool, action }));

describe('prompt catalog steps name real capabilities', () => {
  it('finds the steps of every prompt', () => {
    expect(steps.length).toBeGreaterThanOrEqual(60);
  });

  it('every step capability is an id or an alias in the catalog, and its tool and action are one of its names', () => {
    const problems: string[] = [];
    for (const step of steps) {
      const record = REGISTRY.records.find((r) => r.id === step.id || r.aliases.includes(step.id));
      if (!record) {
        problems.push(`${step.id}: no such capability`);
      } else if (!record.legacyIds.some((pair) => pair.tool === step.tool && pair.action === step.action)) {
        problems.push(`${step.id}: describe ${step.tool}/${step.action} is not one of ${record.id}'s names`);
      }
    }
    expect(problems).toEqual([]);
  });

  it('prompt ids are unique', () => {
    const ids = [...SOURCES.matchAll(/FMcpWorkflowPrompt\{ TEXT\("([a-z-]+)"\)/gu)].map(([, id]) => id);
    expect(ids.length).toBeGreaterThanOrEqual(15);
    expect(new Set(ids).size).toBe(ids.length);
  });

  it('the playbooks follow the workflows, and a prompt renders its traps before its steps', () => {
    expect(read('McpPromptCatalog.cpp')).toContain('All.Append(McpPlaybookPrompts());');
    const render = read('McpPromptRender.cpp');
    expect(render.indexOf('Traps in this domain:')).toBeGreaterThan(-1);
    expect(render.indexOf('Traps in this domain:')).toBeLessThan(render.indexOf('TEXT("Steps:")'));
  });
});
