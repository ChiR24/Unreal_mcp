/**
 * Source contracts for Blueprint flow macros (ForLoop, DoOnce, Do N, Gate, FlipFlop ...).
 *
 * They are graphs in the engine StandardMacros library, not UK2Node classes. Aliases to K2Node_FlipFlop and
 * friends named classes that do not exist, so create_node answered NODE_TYPE_NOT_FOUND for node types its own
 * contract lists, and add_node kept a second list that missed Do N (whose graph name has a space).
 */

import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const domains = 'plugins/EngineWire/Source/EngineWire/Private/Domains';
const read = (file: string): string => readFileSync(resolve(process.cwd(), domains, file), 'utf8');
const creation = read('BlueprintGraph/McpBPGraphNodeCreation.cpp');
const catalog = read('BlueprintGraph/McpBPGraphNodeCatalog.cpp');
const addNode = read('Blueprint/Graph/McpBPAddNodeGraph.cpp');

describe('Blueprint flow macros', () => {
  it('resolve to their StandardMacros graph names', () => {
    for (const [nodeType, graph] of [
      ['ForLoop', 'ForLoop'],
      ['DoOnce', 'DoOnce'],
      ['DoN', 'Do N'],
      ['Gate', 'Gate'],
      ['FlipFlop', 'FlipFlop'],
    ]) {
      expect(creation).toContain(`{TEXT("${nodeType}"), TEXT("${graph}")}`);
    }
  });

  it('are never aliased to a K2Node class that does not exist', () => {
    for (const missing of ['"K2Node_DoOnce"', '"K2Node_DoN"', '"K2Node_FlipFlop"', '"K2Node_Gate"']) {
      expect(catalog).not.toContain(missing);
    }
  });

  it('share one table between create_node and add_node', () => {
    expect(addNode).toContain('McpBlueprintGraphHandlers::StandardMacroGraphName(NodeType)');
    expect(addNode).not.toContain('StandardMacroNamesForMcp');
  });
});
