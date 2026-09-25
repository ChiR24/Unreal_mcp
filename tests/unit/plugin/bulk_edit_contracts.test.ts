// List forms that turn N consented round trips into one: set_material takes
// actorNames, remove_scs_component takes componentNames. Each item runs the
// ordinary single-item path and is reported; a partial result fails naming the
// items that did not apply.
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';
import { CONTROL_ACTOR_RECORDS } from '../../../src/tools/catalog/capabilities/records/control-actor/index.js';
import { MANAGE_BLUEPRINT_RECORDS } from '../../../src/tools/catalog/capabilities/records/manage-blueprint/index.js';
import { MANAGE_ASSET_RECORD_SPECS } from '../../../src/tools/catalog/capabilities/records/manage-asset/index.js';
import { MANAGE_ASSET_FOLDS } from '../../../src/tools/catalog/capabilities/records/folds/manage-asset.folds.js';

const DOMAINS = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Domains');
const source = (path: string): string => readFileSync(resolve(DOMAINS, path), 'utf8');

describe('set_material actorNames', () => {
  it('runs every name through the single-actor handler and reports each', () => {
    const s = source('ControlActor/McpAutomationBridge_ControlActorMaterials.cpp');
    expect(s).toContain('TryGetArrayField(TEXT("actorNames"), Names)');
    expect(s).toMatch(/Capture\.Begin\(ItemId\);\s*HandleControlActorSetMaterial\(ItemId, One, Socket\);\s*const FMcpCapturedResponse Reply = Capture\.End\(ItemId\);/);
    expect(s).toContain('TEXT("MATERIAL_BATCH_INCOMPLETE")');
  });

  it('the record accepts actorName or actorNames', () => {
    const record = CONTROL_ACTOR_RECORDS.find((r) => r.legacyIds[0].action === 'set_material');
    expect(record).toBeDefined();
    expect(record?.schemas.input.properties).toHaveProperty('actorNames');
    expect(record?.schemas.input.required).not.toContain('actorName');
    expect(record?.schemas.input.requiredOneOf).toEqual(['actorName', 'actorNames']);
  });
});

describe('material parameters in one consented call', () => {
  it('set_material_parameter runs each entry through the single-value path and fails naming misses', () => {
    const s = source('MaterialAuthoring/Parameters/McpAutomationBridge_MaterialAuthoringHandlersSetMaterialParameter.cpp');
    expect(s).toMatch(/Capture\.Begin\(ItemId\);\s*HandleSetMaterialParameter\(Bridge, ItemId, TEXT\("set_material_parameter"\), One, Socket\);/);
    expect(s).toContain('One->RemoveField(TEXT("parameters"))');
    expect(s).toContain('TEXT("PARAMETER_BATCH_INCOMPLETE")');
  });

  it('create_material_instance applies parameters to the new instance before answering', () => {
    const s = source('MaterialAuthoring/Creation/McpAutomationBridge_MaterialAuthoringHandlersCreateMaterialInstance.cpp');
    expect(s).toMatch(/ApplyMaterialParameterList\(Bridge, RequestId, NewInstance->GetOutermost\(\)->GetName\(\)/);
    expect(s).toContain('TEXT("PARAMETER_BATCH_INCOMPLETE")');
  });
});

describe('remove_scs_component componentNames', () => {
  it('removes each name and fails naming the ones that stayed', () => {
    const s = source('Blueprint/Components/McpAutomationBridge_BlueprintHandlersScsWrappers.cpp');
    expect(s).toContain('TryGetArrayField(TEXT("componentNames"), Names)');
    expect(s).toContain('FSCSHandlers::RemoveSCSComponent(BPPath, One)');
    expect(s).toContain('TEXT("SCS_REMOVE_INCOMPLETE")');
  });

  it('the record accepts componentName or componentNames', () => {
    const record = MANAGE_BLUEPRINT_RECORDS.find((r) => r.legacyIds[0].action === 'remove_scs_component');
    expect(record).toBeDefined();
    expect(record?.schemas.input.properties).toHaveProperty('componentNames');
    expect(record?.schemas.input.required).not.toContain('componentName');
    expect(record?.schemas.input.requiredOneOf).toEqual(['componentName', 'componentNames']);
  });
});

describe('build_material_graph', () => {
  const batch = (): string => source('MaterialAuthoring/McpAutomationBridge_MaterialAuthoringGraphBatch.cpp');

  it('runs every step back through the manage_material_authoring entry with its reply captured', () => {
    expect(source('MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlers.cpp'))
      .toMatch(/SubAction == TEXT\("build_material_graph"\)[\s\S]*HandleManageMaterialAuthoringAction\(StepId, Action, Step, Socket\)/);
    expect(batch()).toMatch(/Capture\.Begin\(StepId\);\s*RunStep\(StepId, StepPayload\);\s*const FMcpCapturedResponse Reply = Capture\.End\(StepId\);/);
  });

  it('batches additive edits only, and compiles and saves once at the end', () => {
    const s = batch();
    const allowList = s.slice(s.indexOf('bool IsBatchableMaterialEdit('), s.indexOf('bool CreatesNode('));
    for (const destructive of ['delete_node', 'remove_material_node', 'disconnect_nodes', 'break_material_connections', 'create_material']) {
      expect(allowList).not.toContain(destructive);
    }
    expect(s).toContain('Compile->SetStringField(TEXT("subAction"), TEXT("compile_material"));');
  });

  it('stacks auto-placed nodes by their reported height, not a fixed pitch', () => {
    // A vector parameter is ~320 tall; a 220 pitch overlapped every one of them.
    const s = batch();
    expect(s).toMatch(/TryGetNumberField\(TEXT\("estimatedHeight"\), Height\)/);
    expect(s).toMatch(/CursorY \+= FMath::Max\(Height, 64\.0\) \+ 48\.0;/);
  });

  it('is routed on both surfaces and published as the add_material_node batch member', () => {
    expect(readFileSync(resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/MCP/Routing/McpConsolidatedActionRoutingAssets.h'), 'utf8'))
      .toContain('TEXT("build_material_graph")');
    const spec = MANAGE_ASSET_RECORD_SPECS.find((r) => r.action === 'build_material_graph');
    expect(spec?.input.required).toEqual(['materialPath', 'operations']);
    const fold = MANAGE_ASSET_FOLDS.find((f) => f.primary === 'add_material_node');
    expect(fold?.members).toMatchObject({ batch: 'build_material_graph' });
  });
});

// A step that named its own asset was edited outside the batch's pre-check,
// compile and save, and stayed unsaved while the batch reported success.
describe('batch steps edit only the batch asset', () => {
  it('build_graph, build_material_graph and build_metasound drop a step asset path', () => {
    expect(source('BlueprintGraph/McpAutomationBridge_BlueprintGraphHandlersBatchSteps.cpp'))
      .toContain('if (Pair.Key != TEXT("blueprintPath") && Pair.Key != TEXT("assetPath"))');
    expect(source('MaterialAuthoring/McpAutomationBridge_MaterialAuthoringGraphBatch.cpp'))
      .toContain('if (Pair.Key != TEXT("assetPath") && Pair.Key != TEXT("materialPath")) {');
    expect(source('AudioAuthoring/MetaSound/McpAutomationBridge_AudioAuthoringHandlersMetaSoundBatch.cpp'))
      .toContain('if (Pair.Key != TEXT("assetPath")) { Step->SetField(Pair.Key, Pair.Value); }');
  });

  it('build_material_graph says so when the save failed', () => {
    expect(source('MaterialAuthoring/McpAutomationBridge_MaterialAuthoringGraphBatch.cpp'))
      .toContain('bSaved ? TEXT(" and was saved") : TEXT(", but it was NOT saved")');
  });
});

describe('actor batch bounds', () => {
  it('set_transform and set_blueprint_variables refuse more than 500 actors, like spawn_batch', () => {
    for (const file of ['ControlActor/McpAutomationBridge_ControlActorTransform.cpp', 'ControlActor/McpAutomationBridge_ControlActorAdvanced.cpp']) {
      expect(source(file)).toMatch(/if \(Items->Num\(\) > 500\) \{\s*SendStandardErrorResponse\(this, Socket, RequestId, TEXT\("INVALID_ARGUMENT"\),/);
    }
  });
});
