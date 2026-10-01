// describe names the variants that read a parameter. A folded family's input
// schema is the union of its variants' parameters (add_content_widget lists 30),
// so a caller adding a slider could not tell minValue from isMarquee. Both doors
// read routing.dispatchBy.declaredBy, so this pins the TypeScript reply and the
// native source that mirrors it.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { inferSelector, unreadVariantParams } from '../../../src/server/gateway/gateway-dispatch-by.js';
import { describeGatewayCapability } from '../../../src/server/gateway/gateway-describe.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

type Row = { name: string; variants?: string[] };

const PRIVATE = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private');
const nativeDescribe = readFileSync(join(PRIVATE, 'MCP', 'Gateway', 'McpNativeGatewayDescribe.cpp'), 'utf8');

describe('describe names the variants that read a parameter', () => {
  const contract = describeGatewayCapability({ operation: 'describe', capability: 'blueprint.add_content_widget' });
  const rows = contract.parameters as Row[];

  it('lists the variants on a parameter only some variants read', () => {
    expect(rows.find((row) => row.name === 'minValue')?.variants).toEqual(['slider', 'spin_box']);
    expect(rows.find((row) => row.name === 'isMarquee')?.variants).toEqual(['progress_bar']);
  });

  it('leaves it off a parameter every variant reads, and off a record that is not a family', () => {
    expect(rows.find((row) => row.name === 'widgetPath')).not.toHaveProperty('variants');
    const plain = describeGatewayCapability({ operation: 'describe', capability: 'blueprint.compile' });
    expect((plain.parameters as Row[]).some((row) => row.variants !== undefined)).toBe(false);
  });

  it('the single-parameter describe carries it too', () => {
    const one = describeGatewayCapability({ operation: 'describe', capability: 'blueprint.add_content_widget', param: 'stepSize' });
    expect(one.variants).toEqual(['slider']);
  });

  it('the native describe emits it from the same declaredBy data on both levels', () => {
    expect(nativeDescribe).toMatch(/Record\.DispatchByDeclaredBy\.Find\(Name\)[\s\S]*SetArrayField\(TEXT\("variants"\)/u);
    expect(nativeDescribe).toMatch(/SetVariants\(View, Record, Name\);/u);
    expect(nativeDescribe).toMatch(/SetVariants\(Out, \*Record, Input\.Param\);/u);
  });
});

// A path the handlers read under two spellings was declared under one spelling per variant, so the other
// spelling looked like a parameter of only those variants: get_material_info info=node_properties with the
// assetPath it had read was told "assetPath is read only when info is material or function or ...", and a bare
// systemPath on get_niagara_info selected validate (only validate declared it), so an inspector call ran the
// validator. Every variant declares what its handler reads, and the warning is left for a real mismatch.
describe('a path the handlers read under either spelling is declared by every variant of the family', () => {
  const record = (id: string) => {
    const found = capabilityIndex().byId.get(id);
    if (found === undefined) throw new Error(`${id} is not in the catalogue`);
    return found;
  };
  const code = (...segments: readonly string[]): string =>
    readFileSync(join(PRIVATE, 'Domains', ...segments), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
  const variants = (id: string): readonly string[] => Object.keys(record(id).routing.dispatchBy?.actions ?? {});

  it('material info: no variant is the only reader of assetPath or materialPath, so none is warned', () => {
    const info = record('material.get_material_info');
    const declaredBy = info.routing.dispatchBy?.declaredBy ?? {};

    expect(variants(info.id)).toEqual(['material', 'function', 'find_node', 'node_details', 'node_properties', 'node_connections', 'node_chain', 'subgraph']);
    expect(declaredBy).not.toHaveProperty('assetPath');
    expect(declaredBy).not.toHaveProperty('materialPath');
    for (const variant of variants(info.id)) {
      expect(unreadVariantParams(info, ['assetPath', 'materialPath'], { info: variant }), variant).toEqual([]);
    }
  });

  it('the warning stays for a parameter another variant alone reads', () => {
    const info = record('material.get_material_info');

    expect(unreadVariantParams(info, ['assetPath', 'orphansOnly'], { info: 'material' })).toEqual([
      'orphansOnly is read only when info is subgraph; this info=material call did not use it.',
    ]);
  });

  it('either spelling is accepted by every variant, and one is required', () => {
    for (const id of ['material.get_material_info', 'material.compile_material']) {
      const input = record(id).schemas.input;
      expect(Object.keys(isRecord(input.properties) ? input.properties : {}), id).toEqual(expect.arrayContaining(['assetPath', 'materialPath']));
      expect(input.requiredOneOf, id).toEqual(['assetPath', 'materialPath']);
    }
  });

  it('compile and rebuild read both spellings too', () => {
    const compile = record('material.compile_material');

    expect(variants(compile.id)).toEqual(['compile', 'rebuild']);
    expect(compile.routing.dispatchBy?.declaredBy ?? {}).toEqual({});
    for (const variant of variants(compile.id)) {
      expect(unreadVariantParams(compile, ['assetPath', 'materialPath'], { compileOp: variant }), variant).toEqual([]);
    }
  });

  it('niagara info: systemPath and system are read by both variants, and a bare systemPath no longer selects validate', () => {
    const info = record('manage_effect.get_niagara_info');
    const declaredBy = info.routing.dispatchBy?.declaredBy ?? {};
    const selector = isRecord(info.schemas.input.properties) ? info.schemas.input.properties.info : undefined;

    expect(declaredBy).not.toHaveProperty('systemPath');
    expect(declaredBy).not.toHaveProperty('system');
    for (const params of [{ systemPath: '/Game/NS_Fire' }, { system: '/Game/NS_Fire' }, { assetPath: '/Game/NS_Fire' }]) {
      expect(inferSelector(info, params), JSON.stringify(params)).toEqual(params);
    }
    expect(isRecord(selector) ? selector.default : undefined, 'the call runs the inspector when it names no variant').toBe('info');
    expect(info.routing.dispatchBy?.actions.info).toBe('get_niagara_info');
    for (const variant of variants(info.id)) {
      expect(unreadVariantParams(info, ['assetPath', 'systemPath', 'system'], { info: variant }), variant).toEqual([]);
    }
  });

  it('describe leaves the variant list off those paths', () => {
    for (const [capability, names] of [['material.get_material_info', ['assetPath', 'materialPath']], ['manage_effect.get_niagara_info', ['assetPath', 'systemPath', 'system']]] as const) {
      const rows = describeGatewayCapability({ operation: 'describe', capability }).parameters as Row[];
      for (const name of names) {
        expect(rows.find((row) => row.name === name), `${capability} ${name}`).not.toHaveProperty('variants');
      }
    }
  });

  it('the handlers read the spellings the records declare', () => {
    const loader = code('MaterialAuthoring', 'McpAutomationBridge_MaterialAuthoringHandlersPrivate.h');
    expect(loader).toMatch(/#define LOAD_MATERIAL_OR_FUNCTION_OR_RETURN\(\)[\s\S]*?TEXT\("assetPath"\)[\s\S]*?TEXT\("materialPath"\)/u);
    // find_node, the connections, properties, chain and subgraph reads, and node details, all through the loader
    for (const file of ['FindNode', 'GetNodeConnections', 'GetNodeProperties', 'GetNodeChain', 'GetConnectedSubgraph']) {
      expect(code('MaterialAuthoring', 'Queries', `McpAutomationBridge_MaterialAuthoringHandlers${file}.cpp`), file).toContain('LOAD_MATERIAL_OR_FUNCTION_OR_RETURN();');
    }
    for (const file of [['Queries', 'McpAutomationBridge_MaterialAuthoringHandlersGetMaterialInfo.cpp'], ['Queries', 'McpAutomationBridge_MaterialAuthoringHandlersGetMaterialNodeDetails.cpp'], ['Properties', 'McpAutomationBridge_MaterialAuthoringHandlersCompileMaterial.cpp']]) {
      const source = code('MaterialAuthoring', ...file);
      expect(source, file.join('/')).toContain('TEXT("assetPath")');
      expect(source, file.join('/')).toContain('TEXT("materialPath")');
    }
    expect(code('MaterialAuthoring', 'McpAutomationBridge_MaterialAuthoringHandlers.cpp')).toContain('SubAction = TEXT("get_material_info");');
  });

  it('the niagara handlers resolve the asset from assetPath, systemPath or system', () => {
    const context = code('NiagaraAuthoring', 'McpAutomationBridge_NiagaraAuthoringHandlersContext.cpp');
    const info = code('NiagaraAuthoring', 'McpAutomationBridge_NiagaraAuthoringHandlersInfoValidation.cpp');

    expect(context).toMatch(/Context\.SystemPath = GetJsonStringField\(Payload, TEXT\("systemPath"\)\);\s*if \(Context\.SystemPath\.IsEmpty\(\)\)\s*\{\s*Context\.SystemPath = GetJsonStringField\(Payload, TEXT\("system"\)\);\s*\}\s*if \(Context\.SystemPath\.IsEmpty\(\)\)\s*\{\s*Context\.SystemPath = Context\.AssetPath;\s*\}/u);
    expect(info).toContain('if (Context.AssetPath.IsEmpty() && Context.SystemPath.IsEmpty())');
    expect(info).toContain('const FString TargetPath = Context.AssetPath.IsEmpty() ? Context.SystemPath : Context.AssetPath;');
  });
});
