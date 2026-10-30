// Behavior Tree edits are one undo step each, on both routes: the asset route (add_composite_node, add_task_node,
// add_decorator, add_service, configure_bt_node) and the graph route (add_node, add_subnode, connect_nodes,
// break_connections, remove_node, set_node_properties). They changed the tree, its graph and its nodes with no
// transaction open, so Ctrl+Z undid the edit before them and the reply said nothing about undo. Behaviour belongs to
// the integration cases in tests/mcp-tools/gameplay/manage-ai.test.mjs and utility/behavior-tree-get-tree.test.mjs,
// which assert undo.undoable.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const TREE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'AI', 'BehaviorTree');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (file: string): string =>
  readFileSync(join(TREE, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

const EDITS: Record<string, readonly [string, string][]> = {
  'McpAIBehaviorTreeAssets.cpp': [
    ['Add Behavior Tree Composite', 'Holder->Children.Add(Child);'],
    ['Add Behavior Tree Task', 'Parent->Children.Add(Child);'],
  ],
  'McpAIBehaviorTreeDecorators.cpp': [
    ['Add Behavior Tree Decorator', 'BT->RootDecorators.Add(NewDecorator);'],
    ['Add Behavior Tree Service', 'BT->RootNode->Services.Add(NewService);'],
  ],
  'McpAIBehaviorTreeNodeConfig.cpp': [
    ['Configure Behavior Tree Node', 'ApplyJsonValueToProperty(TargetNode, Property, Pair.Value, ApplyError)'],
  ],
};

describe('asset-route Behavior Tree edits are one undo step', () => {
  for (const [file, edits] of Object.entries(EDITS)) {
    for (const [title, change] of edits) {
      it(`${file}: ${title} opens before the change and the save follows it`, () => {
        const source = read(file);
        const edit = source.indexOf(`const FMcpAssetEdit Edit(TEXT("${title}")`);
        expect(edit).toBeGreaterThan(0);
        const at = source.indexOf(change, edit);
        expect(at).toBeGreaterThan(edit);
        expect(source.indexOf('McpSafeAssetSave(BT)', at)).toBeGreaterThan(at);
      });
    }
  }

  it('makes the node subobjects it records transactional first', () => {
    expect(read('McpAIBehaviorTreeAssets.cpp')).toMatch(/Holder->SetFlags\(RF_Transactional\);[\s\S]*?Parent->SetFlags\(RF_Transactional\);/u);
    const decorators = read('McpAIBehaviorTreeDecorators.cpp');
    expect(decorators).toContain('Owner->SetFlags(RF_Transactional);');
    expect(decorators).toContain('BT->RootNode->SetFlags(RF_Transactional);');
    expect(read('McpAIBehaviorTreeNodeConfig.cpp')).toMatch(/TargetNode->SetFlags\(RF_Transactional\);\s*const FMcpAssetEdit Edit/u);
  });

  // The graph route: each edit opens its step before the change (connect used to Modify after linking, which recorded
  // the linked state), the graph rebuilds the tree inside it, and the save follows it.
  const GRAPH = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains', 'BehaviorTree');
  const graph = (file: string): string =>
    readFileSync(join(GRAPH, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
  for (const [file, title, change] of [
    ['McpBTGraph.cpp', 'Connect Behavior Tree Nodes', 'TryCreateConnection(OutputPin, InputPin)'],
    ['McpBTGraphEdits.cpp', 'Remove Behavior Tree Node', 'BreakNodeLinks(*TargetNode)'],
    ['McpBTNodeCreation.cpp', 'Add Behavior Tree Node', 'GraphContext.Graph->AddNode(NewNode, true, false);'],
    ['McpBTNodeProps.cpp', 'Set Behavior Tree Node Properties', 'ApplyNodeEdits(Context, GraphContext, TargetNode, BTNode, bModified, Error)'],
    ['McpBTSubnodes.cpp', 'Add Behavior Tree Subnode', 'ParentNode->AddSubNode(NewSubnode, GraphContext.Graph);'],
  ] as const) {
    it(`${file}: ${title} opens before the change; the save follows the step`, () => {
      const source = graph(file);
      const edit = source.indexOf(`TEXT("${title}")`);
      expect(edit).toBeGreaterThan(0);
      const at = source.indexOf(change, edit);
      expect(at).toBeGreaterThan(edit);
      expect(source.indexOf('McpSafeAssetSave(GraphContext.BehaviorTree);', at)).toBeGreaterThan(at);
      expect(source).not.toMatch(/GraphContext\.(BehaviorTree|Graph)->Modify\(\);/u);
    });
  }

  it('the graph rebuild saves nothing itself, and a new graph is transactional', () => {
    const update = graph('McpBTGraph.cpp');
    const body = update.slice(update.indexOf('void UpdateBehaviorTreeAsset('), update.indexOf('UEdGraphNode* FindGraphNodeByIdOrName('));
    expect(body).not.toContain('McpSafeAssetSave');
    expect(update).toContain('NewObject<UBehaviorTreeGraph>(BehaviorTree, TEXT("BehaviorTree"), RF_Transactional)');
    expect(graph('McpBTNodeProps.cpp')).toContain('TEXT("PROPERTY_NOT_FOUND")');
  });

  it('configure_bt_node refuses a call with no properties before it changes anything', () => {
    const config = read('McpAIBehaviorTreeNodeConfig.cpp');
    const refusal = config.indexOf('(*PropertiesObject)->Values.Num() == 0');
    expect(refusal).toBeGreaterThan(0);
    expect(config.indexOf('TEXT("INVALID_ARGUMENT")', refusal)).toBeLessThan(config.indexOf('const FMcpAssetEdit Edit'));
    expect(config).not.toContain('no properties supplied');
  });
});
