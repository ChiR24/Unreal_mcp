// describe names the variants that read a parameter. A folded family's input
// schema is the union of its variants' parameters (add_content_widget lists 30),
// so a caller adding a slider could not tell minValue from isMarquee. Both doors
// read routing.dispatchBy.declaredBy, so this pins the TypeScript reply and the
// native source that mirrors it.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';

import { describeGatewayCapability } from '../../../src/server/gateway/gateway-describe.js';

type Row = { name: string; variants?: string[] };

const nativeDescribe = readFileSync(join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'MCP',
  'Gateway', 'McpNativeGatewayDescribe.cpp'), 'utf8');

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
