import { describe, expect, it } from 'vitest';

import { describeGatewayCapability } from '../../../src/server/gateway/gateway-describe.js';
import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { executeTargetIndex, resolveExecuteTarget } from '../../../src/server/gateway/gateway-execute-resolve.js';

// MCPBB-081 — 343 capabilities are published under an ID prefix that is not a
// routable tool name (`blueprint.*`, `material.*`, `sequence.*`, ...). A caller
// who read that prefix off a search row and used it as `tool` was refused
// UNKNOWN_TOOL, so the namespace the catalog advertises was not addressable.
//
// The mapping is derived here the same way the fix derives it, rather than
// restated as a literal, so this test cannot drift from the registry: a prefix
// is an alias only when it is not itself a parent tool and every capability
// carrying it routes to the same parent.

function orphanNamespaces(): ReadonlyMap<string, string> {
  const records = capabilityIndex().records;
  const parentTools = new Set(records.map((record) => record.routing.parentTool));
  const owners = new Map<string, Set<string>>();
  for (const record of records) {
    const prefix = record.id.split('.')[0];
    if (prefix === undefined || parentTools.has(prefix)) continue;
    const bucket = owners.get(prefix) ?? new Set<string>();
    bucket.add(record.routing.parentTool);
    owners.set(prefix, bucket);
  }
  const resolved = new Map<string, string>();
  for (const [prefix, bucket] of owners) {
    const only = [...bucket];
    if (only.length === 1 && only[0] !== undefined) resolved.set(prefix, only[0]);
  }
  return resolved;
}

describe('MCPBB-081 — an advertised capability namespace is addressable as a tool', () => {
  it('the registry still publishes orphan namespaces worth resolving', () => {
    expect(orphanNamespaces().size).toBeGreaterThan(0);
  });

  it('describe resolves every orphan namespace to its owning parent tool', () => {
    const refused: string[] = [];
    for (const [prefix, parent] of orphanNamespaces()) {
      const result = describeGatewayCapability({ tool: prefix });
      if (result.success !== true || result.tool !== parent) {
        refused.push(`${prefix} -> ${String(result.errorCode ?? result.tool)}`);
      }
    }
    expect(refused, `namespaces still unaddressable: ${refused.join(', ')}`).toEqual([]);
  });

  it('describe discloses that the tool name was resolved from a namespace', () => {
    const [prefix] = [...orphanNamespaces().keys()];
    expect(prefix).toBeDefined();
    const result = describeGatewayCapability({ tool: prefix as string });
    expect(result.resolvedFromNamespace).toBe(prefix);
  });

  it('execute resolves a namespace-qualified tool to a real dispatch target', () => {
    const index = executeTargetIndex();
    const failures: string[] = [];
    for (const [prefix, parent] of orphanNamespaces()) {
      const actions = index.actionsByParentTool.get(parent) ?? [];
      const action = actions[0];
      if (action === undefined) continue;
      const resolution = resolveExecuteTarget({ tool: prefix, action }, index);
      if (!resolution.ok) failures.push(`${prefix}.${action} -> ${resolution.failure.errorCode}`);
    }
    expect(failures, `namespaces unroutable through execute: ${failures.join(', ')}`).toEqual([]);
  });

  it('a namespace-resolved execute reaches the same record as the parent tool', () => {
    const index = executeTargetIndex();
    const actions = index.actionsByParentTool.get('manage_blueprint') ?? [];
    const action = actions[0] as string;
    const viaNamespace = resolveExecuteTarget({ tool: 'blueprint', action }, index);
    const viaParent = resolveExecuteTarget({ tool: 'manage_blueprint', action }, index);
    expect(viaNamespace.ok).toBe(true);
    expect(viaParent.ok).toBe(true);
    if (viaNamespace.ok && viaParent.ok) {
      expect(viaNamespace.target.record.id).toBe(viaParent.target.record.id);
    }
  });

  it('a name that is neither a tool nor a published namespace is still refused', () => {
    const result = describeGatewayCapability({ tool: 'definitely_not_a_namespace' });
    expect(result.success).toBe(false);
    expect(result.errorCode).toBe('UNKNOWN_TOOL');

    const resolution = resolveExecuteTarget(
      { tool: 'definitely_not_a_namespace', action: 'create' },
      executeTargetIndex()
    );
    expect(resolution.ok).toBe(false);
  });

  it('a real parent tool is unaffected', () => {
    const result = describeGatewayCapability({ tool: 'manage_asset' });
    expect(result.success).toBe(true);
    expect(result.tool).toBe('manage_asset');
    expect(result.resolvedFromNamespace).toBeUndefined();
  });
});
