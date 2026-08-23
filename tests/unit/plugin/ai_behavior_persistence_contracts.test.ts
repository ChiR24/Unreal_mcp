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
const _btNodeConfig = () => readCpp('Domains/AI/BehaviorTree/McpAutomationBridge_AIHandlersBehaviorTreeNodeConfig.cpp');
const btGraph = () => readCpp('Domains/BehaviorTree/McpAutomationBridge_BehaviorTreeHandlersGraph.cpp');
const btSerializers = () => readCpp('Domains/BehaviorTree/McpAutomationBridge_BehaviorTreeSerializers.cpp');
const blackboardValues = () => readCpp('Domains/AI/Blackboard/McpAutomationBridge_AIHandlersBlackboardValues.cpp');
const controlActorLookup = () => readCpp('Domains/ControlActor/McpAutomationBridge_ControlActorLookup.cpp');
const _inventoryInfo = () => readCpp('Domains/Inventory/McpAutomationBridge_InventoryHandlersInfo.cpp');
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
    const slice = s.slice(idx, idx + 600);
    expect(slice).toMatch(/tree/);
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
