// Todo 24 BB-046..BB-051 — AI behavior persistence, graph unification, and output schema source contracts.
import { existsSync, readFileSync, readdirSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const PRIVATE = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private');
const TS = resolve(process.cwd(), 'src');
function readCpp(...parts: string[]): string {
  const p = resolve(PRIVATE, ...parts);
  expect(existsSync(p), `missing: ${p}`).toBe(true);
  return readFileSync(p, 'utf8');
}
function readTs(...parts: string[]): string {
  const p = resolve(TS, ...parts);
  expect(existsSync(p), `missing: ${p}`).toBe(true);
  return readFileSync(p, 'utf8');
}
function code(s: string): string { return s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, ''); }

const btAssets = () => readCpp('Domains/AI/BehaviorTree/McpAutomationBridge_AIHandlersBehaviorTreeAssets.cpp');
const btDecorators = () => readCpp('Domains/AI/BehaviorTree/McpAutomationBridge_AIHandlersBehaviorTreeDecorators.cpp');
const btGraph = () => readCpp('Domains/BehaviorTree/McpAutomationBridge_BehaviorTreeHandlersGraph.cpp');
const btSerializers = () => readCpp('Domains/BehaviorTree/McpAutomationBridge_BehaviorTreeSerializers.cpp');
const blackboardValues = () => readCpp('Domains/AI/Blackboard/McpAutomationBridge_AIHandlersBlackboardValues.cpp');
const controlActorLookup = () => readCpp('Domains/ControlActor/McpAutomationBridge_ControlActorLookup.cpp');
const aiRecords = () => readTs('tools/catalog/capabilities/records/gameplay/manage-ai/create-read-actions.data.ts');
const aiAddRecords = () => readTs('tools/catalog/capabilities/records/gameplay/manage-ai/add-actions.data.ts');
const inventorySchema = () => readTs('tools/catalog/capabilities/records/gameplay/manage-inventory/schema.ts');
const inventoryRecords = () => readTs('tools/catalog/capabilities/records/gameplay/manage-inventory/inventory-2.data.ts');
const inspectRecords = () => readTs('tools/catalog/capabilities/records/inspect/component-actor.data.ts');

function dirFiles(dir: string): string[] {
  return readdirSync(resolve(PRIVATE, ...dir.split('/')), { encoding: 'utf8' }).filter(f => f.endsWith('.cpp'));
}

describe('BB-046 AI mutation handlers persist with McpSafeAssetSave', () => {
  it('no handler in Domains/AI/** calls MarkPackageDirty without McpSafeAssetSave in the same file', () => {
    const aiDirs = ['Domains/AI/BehaviorTree', 'Domains/AI/Blackboard', 'Domains/AI/Controllers', 'Domains/AI/EQS', 'Domains/AI/Navigation', 'Domains/AI/Perception', 'Domains/AI/Runtime', 'Domains/AI/SmartObjects', 'Domains/AI/StateTree'];
    for (const dir of aiDirs) {
      for (const file of dirFiles(dir)) {
        const src = code(readCpp(dir, file));
        if (src.includes('MarkPackageDirty')) {
          expect(src, `${dir}/${file}: MarkPackageDirty without McpSafeAssetSave`).toContain('McpSafeAssetSave');
        }
      }
    }
  });
  it('no handler in Domains/BehaviorTree/** calls MarkPackageDirty without McpSafeAssetSave', () => {
    for (const file of dirFiles('Domains/BehaviorTree')) {
      const src = code(readCpp('Domains/BehaviorTree', file));
      if (src.includes('MarkPackageDirty')) {
        expect(src, `Domains/BehaviorTree/${file}: MarkPackageDirty without McpSafeAssetSave`).toContain('McpSafeAssetSave');
      }
    }
  });
});

describe('BB-047/048/049 record output schemas declare emitted fields', () => {
  it('get_tree record declares tree output', () => {
    const s = code(aiRecords());
    const idx = s.indexOf('get_tree');
    expect(idx).toBeGreaterThan(-1);
    // Reaches past the record's doc comment to its `out:` block. A bare
    // /tree/ against a slice that STARTS with "get_tree" matched the anchor
    // itself, so the case could not fail for any input.
    const slice = s.slice(idx, idx + 1200);
    expect(slice).toMatch(/out:\s*\{[\s\S]*?\btree:\s*\{/);
  });
  it('get_ai_info record declares rootDecoratorClasses and other aiInfo fields', () => {
    const s = code(aiRecords());
    const idx = s.indexOf('get_ai_info');
    expect(idx).toBeGreaterThan(-1);
    const slice = s.slice(idx, idx + 1200);
    expect(slice).toMatch(/rootDecoratorClasses/);
    expect(slice).toMatch(/rootDecorators/);
    expect(slice).toMatch(/childDecorators/);
    expect(slice).toMatch(/services/);
    expect(slice).toMatch(/keyCount/);
    expect(slice).toMatch(/blackboardKeys/);
  });
  it('get_blackboard_value record declares valueAvailable', () => {
    const s = code(aiRecords());
    const idx = s.indexOf('get_blackboard_value');
    expect(idx).toBeGreaterThan(-1);
    const slice = s.slice(idx, idx + 400);
    expect(slice).toMatch(/valueAvailable/);
  });
});

describe('BB-049 get_blackboard_value emits value or valueAvailable', () => {
  it('HandleGetBlackboardValue emits value or valueAvailable:false', () => {
    const s = code(blackboardValues());
    expect(s).toMatch(/valueAvailable|SetValueAsString|GetPropertyValueAsString/i);
  });
});

describe('BB-050 asset-route node handlers emit nodeId and attach to tree', () => {
  it('add_task_node/add_decorator/add_service emit nodeId', () => {
    const s1 = code(btAssets());
    const s2 = code(btDecorators());
    const combined = s1 + s2;
    expect(combined).toMatch(/nodeId/);
  });
  it('CreateBehaviorTreeAsset ensures a graph (EnsureBehaviorTreeGraph)', () => {
    const s = code(btAssets());
    expect(s).toMatch(/EnsureBehaviorTreeGraph/i);
  });
  it('LoadBehaviorTreeForGraph auto-creates a graph instead of GRAPH_NOT_FOUND', () => {
    const s = code(btGraph());
    expect(s).toMatch(/EnsureBehaviorTreeGraph/i);
    expect(s, 'must not return GRAPH_NOT_FOUND without trying to create a graph').not.toMatch(/GRAPH_NOT_FOUND/);
  });
  it('SerializeBTNode emits graph NodeGuid nodeId when a graph exists', () => {
    const s = code(btSerializers());
    expect(s).toMatch(/NodeGuid|nodeId/i);
  });
  it('add_task_node record accepts optional parentNodeId', () => {
    const s = code(aiAddRecords());
    const idx = s.indexOf('add_task_node');
    expect(idx).toBeGreaterThan(-1);
    const slice = s.slice(idx, idx + 400);
    expect(slice).toMatch(/parentNodeId/);
  });
});

describe('BB-051 list_objects record declares count/totalCount/isPieWorld/worldName', () => {
  it('inspect component-actor record declares extras for list_objects', () => {
    const s = code(inspectRecords());
    const idx = s.indexOf('list_objects');
    expect(idx).toBeGreaterThan(-1);
    const slice = s.slice(idx, idx + 600);
    expect(slice).toMatch(/count/);
    expect(slice).toMatch(/totalCount/);
    expect(slice).toMatch(/isPieWorld/);
    expect(slice).toMatch(/worldName/);
  });
  it('ControlActorLookup always emits actors array', () => {
    const s = code(controlActorLookup());
    expect(s).toContain('SetArrayField(TEXT("actors")');
  });
});

describe('BB-054 get_inventory_info declares type-specific outputs', () => {
  it('inventory schema supports outputProps override', () => {
    const s = code(inventorySchema());
    expect(s).toMatch(/outputProps|outputRequired/i);
  });
  it('inventory-2.data.ts declares get_inventory_info outputs', () => {
    const s = code(inventoryRecords());
    const idx = s.indexOf('get_inventory_info');
    expect(idx).toBeGreaterThan(-1);
    const slice = s.slice(idx, idx + 800);
    expect(slice).toMatch(/assetType|className|itemPath/i);
  });
});

// Live-sweep ID-015 / ID-018 / ID-049 — Behavior Tree graph invariants.
//
// These three fixes are deletions and re-orderings whose correct state is the
// ABSENCE of a call. A live probe cannot see any of them: the only observable
// effect they ever had was an editor crash or silently deleted nodes. A source
// contract is the only thing that can hold them, so the guard lives here.
const btGraphSync = () => readCpp('Domains/BehaviorTree/McpAutomationBridge_BehaviorTreeHandlersGraphSync.cpp');

describe('live-sweep ID-015/018/049 Behavior Tree graph invariants', () => {
  it('ID-018: add_decorator and add_service do not call EnsureBehaviorTreeGraph', () => {
    // They declared `UEdGraph* Graph = nullptr`, called it, then never read the
    // result. The side effect was ID-015's crash.
    const s = code(btDecorators());
    expect(s, 'BehaviorTreeDecorators.cpp must not call EnsureBehaviorTreeGraph').not.toContain('EnsureBehaviorTreeGraph');
  });

  it('ID-015: EnsureBehaviorTreeGraph sanitizes DecoratorOps at the choke point', () => {
    const s = code(btGraph());
    expect(s).toContain('SanitizeDecoratorOps');
    expect(s).toContain('DropUnindexableDecoratorOps');
  });

  it('ID-015: SpawnMissingNodes is never called on an already-populated graph', () => {
    // UBehaviorTreeGraph::SpawnMissingNodes() is an OnCreated()-only API: its
    // worker spawns a node per asset node unconditionally, so calling it on a
    // populated graph duplicates the whole tree. SyncBehaviorTreeGraphFromAsset
    // is the idempotent equivalent and is what the existing-graph path must use.
    const s = code(btGraph());
    const spawnCalls = s.match(/SpawnMissingNodes\s*\(/gu) ?? [];
    expect(spawnCalls.length, 'SpawnMissingNodes may only appear on the freshly-created-graph path').toBeLessThanOrEqual(1);
    expect(s).toContain('SyncBehaviorTreeGraphFromAsset');
  });

  it('ID-049: the asset->graph sync mirrors decorators and services', () => {
    // CreateBTFromGraph empties RootDecorators and re-collects them FROM the
    // graph, so anything the graph does not know about is deleted on the next
    // graph-route edit.
    const s = code(btGraphSync());
    expect(s).toContain('SyncSubnodes');
    expect(s).toContain('RootDecorators');
  });

  it('ID-049: the sync attaches subnodes without UAIGraphNode::AddSubNode', () => {
    // AddSubNode ends with GetAIGraph()->UpdateAsset(), which rebuilds the asset
    // from the graph. Called from inside the asset walk it dangles the UBTNode
    // pointers being iterated — an access violation, observed live.
    const s = code(btGraphSync());
    expect(s, 'GraphSync must not call AddSubNode; it re-enters UpdateAsset mid-walk').not.toContain('AddSubNode(');
    expect(s).toContain('AttachSubnode');
  });

  it('ID-049: the graph node walk guards against garbage instances', () => {
    const s = code(btGraph());
    expect(s).toContain('IsValid(AINode->NodeInstance)');
  });
});
