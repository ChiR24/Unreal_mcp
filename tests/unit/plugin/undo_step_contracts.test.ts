// Edits that told the caller they were done but could not be undone as one step. paint_foliage and
// add_foliage_instances called Modify() on the foliage actor after the instances were added, so undo recorded the
// state with them already in it; arrange_nodes saved the Blueprint inside its own transaction; the material
// set_node_position called Modify() with no transaction open. Wiring contracts only: behaviour needs an editor.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { sliceBetween } from './plugin-contract-fixtures.js';

const DOMAINS = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (...segments: readonly string[]): string =>
  readFileSync(join(DOMAINS, ...segments), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('foliage placement is one undo step, opened before any instance is added', () => {
  for (const [file, title] of [['McpFoliagePaint.cpp', 'Paint Foliage'], ['McpFoliageInsts.cpp', 'Add Foliage Instances']] as const) {
    it(`${file} records the foliage actor before the first AddFoliageInstance`, () => {
      const source = read('Foliage', file);
      const open = source.indexOf(`FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("${title}"))`);
      const add = source.indexOf('McpFoliageHandlers::AddFoliageInstance(IFA, FoliageType, Instance);');
      expect(open, 'transaction opened').toBeGreaterThan(-1);
      expect(add, 'instances added').toBeGreaterThan(open);
      expect(source.slice(open, add)).toContain('EMcpMutationDurability::EditorStateOnly, {IFA}');
      expect(source, 'no Modify() after the adds').not.toContain('IFA->Modify();');
      expect(source).toContain('Transaction.DescribeInto(Resp);');
    });
  }

  it('paint_foliage refuses NO_SURFACE before the transaction opens, so a refusal leaves no undo entry', () => {
    const source = read('Foliage', 'McpFoliagePaint.cpp');
    expect(source.indexOf('TEXT("NO_SURFACE")')).toBeLessThan(source.indexOf('FMcpScopedEditorTransaction Transaction('));
    expect(source).toContain('Placed.Add(Instance);');
  });
});

describe('arrange_nodes records its moves in a transaction that closes before the save', () => {
  const arrange = (): string => sliceBetween(read('BlueprintGraph', 'McpBPGraphDeleteNodes.cpp'), 'bool ArrangeNodes(', '\n}\n');

  it('resolves every id before the transaction, so an unknown id changes nothing', () => {
    const source = arrange();
    expect(source.indexOf('Context.SendNodeNotFound(')).toBeLessThan(source.indexOf('FMcpScopedEditorTransaction Transaction('));
    expect(source, 'the plain transaction that wrapped the save is gone').not.toContain('FScopedTransaction Transaction(');
  });

  it('closes the transaction scope before SaveLoadedAssetThrottled', () => {
    const source = arrange();
    const describeAt = source.indexOf('Transaction.DescribeInto(Result);');
    const saveAt = source.indexOf('SaveLoadedAssetThrottled(Context.Blueprint);');
    expect(describeAt).toBeGreaterThan(-1);
    expect(saveAt).toBeGreaterThan(describeAt);
    expect(source.slice(describeAt, saveAt)).toMatch(/\}\s*$/u);
  });

  it('claims an undo step only when a node moved: the buffer drops a transaction that changed nothing', () => {
    const source = arrange();
    expect(source).toMatch(/if \(Moved\.Num\(\) > 0\)\s*\{\s*Transaction\.DescribeInto\(Result\);\s*\}/u);
    expect(source).toContain('InPlace.Add(');
  });
});

describe('a Blueprint compile that clears the undo history says so in the undo block', () => {
  it('rewrites undo to undoable false with UNDO_CLEARED_BY_COMPILE when it resets the buffer', () => {
    const source = readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Foundation', 'BridgeHelpers', 'Blueprints', 'McpBPDiag.h'), 'utf8')
      .replace(/\r\n/gu, '\n').replace(/\/\/[^\n]*/gu, ' ');
    const reset = source.indexOf('Out->SetBoolField(TEXT("undoBufferReset"), true);');
    const rewrite = source.indexOf('TEXT("UNDO_CLEARED_BY_COMPILE")');
    expect(reset).toBeGreaterThan(-1);
    expect(rewrite).toBeGreaterThan(reset);
    expect(source.slice(reset, rewrite)).toContain('Undo->SetBoolField(TEXT("undoable"), false);');
    expect(source.slice(rewrite)).toMatch(/Out->SetObjectField\(TEXT\("undo"\), Undo\);/u);
  });
});

describe('an asset created without RF_Transactional still records undo', () => {
  // The skeletons EngineWire created carried RF_Public | RF_Standalone only, so the gate refused every later bone edit
  // and the replies never said so; undo then reverted the previous step instead.
  it('the gate gives an asset the flag before it refuses objects without it', () => {
    const source = readFileSync(join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Foundation', 'McpScopedEditorTransaction.cpp'), 'utf8')
      .replace(/\/\/[^\n]*/gu, ' ');
    const flag = source.indexOf('Object->SetFlags(RF_Transactional);');
    expect(flag).toBeGreaterThan(-1);
    expect(source.slice(0, flag)).toMatch(/if \(Object && Object->IsAsset\(\)\)\s*\{\s*$/u);
    expect(source.indexOf('!Object->HasAnyFlags(RF_Transactional)')).toBeGreaterThan(flag);
  });

  for (const [file, title] of [
    ['McpSkeletonStructureCreation.cpp', 'Add Bone'],
    ['McpSkeletonStructureEditing.cpp', 'Remove Bone'],
    ['McpSkeletonStructureEditing.cpp', 'Set Bone Parent'],
  ] as const) {
    it(`${title} describes its undo step before the save`, () => {
      const source = read('Skeleton', 'Bones', file);
      const open = source.indexOf(`FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("${title}"))`);
      const describe = source.indexOf('Transaction.DescribeInto(Result);', open);
      expect(open).toBeGreaterThan(-1);
      expect(describe).toBeGreaterThan(open);
      expect(source.indexOf('SaveAndReport(Skeleton, Payload, Result);', open)).toBeGreaterThan(describe);
    });
  }

  it('data table row edits describe their undo step before the save', () => {
    const source = read('AssetWorkflow', 'DataTables', 'Rows.cpp');
    for (const title of ['Add Data Table Row', 'Update Data Table Row', 'Delete Data Table Row']) {
      const open = source.indexOf(`FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("${title}"))`);
      const describe = source.indexOf('Transaction.DescribeInto(OutResult);', open);
      expect(open, title).toBeGreaterThan(-1);
      expect(describe, title).toBeGreaterThan(open);
      expect(source.indexOf('McpDataTableSaveIfRequested(Params, Table);', open), title).toBeGreaterThan(describe);
    }
  });
});

describe('the material set_node_position move is one undo step', () => {
  it('opens the gated transaction on the expression and describes it in the reply', () => {
    const source = read('MaterialAuthoring', 'Nodes', 'McpMatSetNodePosition.cpp');
    const open = source.indexOf('FMcpScopedEditorTransaction Transaction(FText::FromString(TEXT("Move Material Node"))');
    expect(open).toBeGreaterThan(-1);
    expect(source.indexOf('Expr->MaterialExpressionEditorX =')).toBeGreaterThan(open);
    expect(source).toContain('EMcpMutationDurability::EditorStateOnly, {Expr}');
    expect(source).toContain('Transaction.DescribeInto(Result);');
    expect(source, 'no bare Modify() outside a transaction').not.toContain('Expr->Modify();');
  });
});
