// tests/eval/manifest-model.ts
// Load and query the neutral gateway manifest as the source of available
// capabilities. The manifest is the single source of truth the gateway
// discovery surface consumes (TS + native share it). The scorer treats it as
// the universe of reachable capabilities and validates corpus references
// against it so corpus/manifest drift is caught fail-closed.

import { readFileSync } from 'node:fs';
import { ManifestError } from './errors.js';
import type { CapabilityRef, ManifestModel, ManifestTool } from './types.js';

export type { ManifestModel } from './types.js';

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === 'object' && value !== null && !Array.isArray(value);
}

export function loadManifestModel(path: string): ManifestModel {
  let raw: string;
  try {
    raw = readFileSync(path, 'utf8');
  } catch (error) {
    if (error instanceof Error) {
      throw new ManifestError(`cannot read manifest at ${path}`, error.message);
    }
    throw new ManifestError(`cannot read manifest at ${path}`);
  }
  let parsed: unknown;
  try {
    parsed = JSON.parse(raw);
  } catch (error) {
    if (error instanceof Error) {
      throw new ManifestError('manifest is not valid JSON', error.message);
    }
    throw new ManifestError('manifest is not valid JSON');
  }
  if (!isRecord(parsed)) {
    throw new ManifestError('manifest root is not an object');
  }
  if (!Array.isArray(parsed.tools)) {
    throw new ManifestError('manifest missing tools array');
  }
  const tools: ManifestTool[] = [];
  for (const entry of parsed.tools as unknown[]) {
    if (!isRecord(entry)) {
      throw new ManifestError('manifest tool entry is not an object');
    }
    const name = entry.name;
    const actions = entry.actions;
    const parameterNames = entry.parameterNames;
    if (typeof name !== 'string') {
      throw new ManifestError('manifest tool missing string name');
    }
    if (!Array.isArray(actions) || !actions.every((a) => typeof a === 'string')) {
      throw new ManifestError(`manifest tool ${name} has invalid actions`);
    }
    if (!Array.isArray(parameterNames) || !parameterNames.every((p) => typeof p === 'string')) {
      throw new ManifestError(`manifest tool ${name} has invalid parameterNames`);
    }
    tools.push({
      name,
      category: typeof entry.category === 'string' ? (entry.category as string) : null,
      description: typeof entry.description === 'string' ? (entry.description as string) : '',
      actions: actions as readonly string[],
      parameterNames: parameterNames as readonly string[],
    });
  }
  const version = typeof parsed.version === 'number' ? (parsed.version as number) : 0;
  const source = typeof parsed.source === 'string' ? (parsed.source as string) : '';
  return { version, source, tools };
}

export function findTool(model: ManifestModel, name: string): ManifestTool | null {
  for (const tool of model.tools) {
    if (tool.name === name) return tool;
  }
  return null;
}

export function hasCapability(model: ManifestModel, ref: CapabilityRef): boolean {
  const tool = findTool(model, ref.tool);
  if (tool === null) return false;
  return tool.actions.includes(ref.action);
}

export function availableManifest(model: ManifestModel, unavailableTool?: string): ManifestModel {
  if (unavailableTool === undefined) return model;
  return {
    version: model.version,
    source: model.source,
    tools: model.tools.filter((tool) => tool.name !== unavailableTool),
  };
}

export function allCapabilities(model: ManifestModel): readonly CapabilityRef[] {
  const out: CapabilityRef[] = [];
  for (const tool of model.tools) {
    for (const action of tool.actions) {
      out.push({ tool: tool.name, action });
    }
  }
  return out;
}
