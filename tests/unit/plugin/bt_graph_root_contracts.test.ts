// The Behavior Tree graph route works on a tree with no root composite, as the editor does while its Root node is not
// linked yet: add_node, then connect_nodes from BehaviorTreeGraphNode_Root_0 makes the root. Every graph-route call on
// such a tree was refused ("requires UE 5.3+" on every version, then NO_ROOT). The one state still refused is root
// decorators with no root composite to hang on: UBehaviorTreeGraph::SpawnMissingNodes dereferences the missing
// composite's node for them (SpawnMissingDecoratorNodes, dogfood #63), and the graph's rebuild discards them.
// Behaviour belongs to tests/mcp-tools/gameplay/manage-ai.test.mjs and tests/mcp-tools/utility/behavior-tree-get-tree.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const graph = readFileSync(
  join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'BehaviorTree', 'McpBTGraph.cpp'),
  'utf8',
).replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

const ensure = graph.slice(graph.indexOf('bool EnsureBehaviorTreeGraph('), graph.indexOf('UEdGraph* CreateBehaviorTreeGraph('));
const load = graph.slice(graph.indexOf('bool LoadBehaviorTreeForGraph('), graph.indexOf('void UpdateBehaviorTreeAsset('));

describe('the Behavior Tree graph route on a tree with no root composite', () => {
  it('refuses only root decorators with no root composite, before the graph is touched', () => {
    const guard = ensure.indexOf('if (!BehaviorTree->RootNode && BehaviorTree->RootDecorators.Num() > 0)');
    expect(guard).toBeGreaterThan(0);
    expect(ensure).not.toMatch(/if \(!BehaviorTree->RootNode\)\s*\{/u);
    expect(ensure.indexOf('OutGraph = BehaviorTree->BTGraph;')).toBeGreaterThan(guard);
    expect(ensure.indexOf('SpawnMissingNodes()')).toBeGreaterThan(guard);
  });

  it('names why: NO_ROOT for loose root decorators, GRAPH_NOT_READY for a graph with no nodes, and the version only below 5.3', () => {
    expect(load).toContain('const bool bLooseDecorators = !BehaviorTree->RootNode && BehaviorTree->RootDecorators.Num() > 0;');
    expect(load).toContain('bLooseDecorators ? TEXT("NO_ROOT") : TEXT("GRAPH_NOT_READY")');
    const below = load.slice(load.indexOf('#else'));
    expect(below).toContain('TEXT("NOT_SUPPORTED")');
    expect(load.indexOf('TEXT("NOT_SUPPORTED")')).toBeGreaterThan(load.indexOf('#else'));
  });
});
