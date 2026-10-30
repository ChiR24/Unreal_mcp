// A user enum's value edits, and a property set on an enum, are one undo step each on every UE 5.x. add_enum_value
// called Modify with no transaction open, which records nothing, and reorder_enum_values made one engine step per
// move. From UE 5.7 restoring an enum's saved state from the undo buffer asserts (UEnum::FNameData::Initialize,
// check(NumValues == 0)) and crashed the editor on Ctrl+Z, so the enum is never recorded that way: while the edit runs
// it is not transactional (the engine's own Modify calls record nothing), and as the edit closes its values and
// properties before and after go into the open step as one change undo writes back with SetEnums. Behaviour belongs to
// the integration cases in tests/mcp-tools/core/enum.test.mjs (undo and redo of each edit).

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (...parts: string[]): string =>
  readFileSync(join(PRIVATE, ...parts), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

const values = read('Domains', 'AssetWorkflow', 'Enums', 'Values.cpp');

/** The body of one action's branch, up to the next action. */
const branch = (action: string): string => {
  const start = values.indexOf(`Action == TEXT("${action}")`);
  const next = values.indexOf('Action == TEXT("', start + 1);
  return values.slice(start, next < 0 ? undefined : next);
};

const STEPS: Record<string, string> = {
  add_enum_value: 'Add Enum Value',
  remove_enum_value: 'Remove Enum Value',
  rename_enum_value: 'Rename Enum Value',
  reorder_enum_values: 'Reorder Enum Values',
};

describe('enum value edits: one undo step on every 5.x, never the enum\'s saved state', () => {
  for (const [action, title] of Object.entries(STEPS)) {
    it(`${action}: ${title}, committed inside the edit and saved after it closes`, () => {
      const body = branch(action);
      const edit = body.indexOf(`const FMcpEnumEdit Edit(TEXT("${title}"), Enum, Result);`);
      expect(edit).toBeGreaterThan(0);
      const commit = body.indexOf('CommitEnum(Enum);', edit);
      expect(commit).toBeGreaterThan(edit);
      // A save inside the edit is a durable write the undo cannot take back; a Modify outside it records nothing.
      expect(body).not.toMatch(/FinalizeEnum\(|Enum->Modify\(\);/u);
      expect(body.indexOf('McpSafeAssetSave(Enum)')).toBeGreaterThan(commit);
    });
  }

  it('the engine moves of a reorder run inside its one edit', () => {
    const body = branch('reorder_enum_values');
    expect(body.indexOf('FEnumEditorUtils::MoveEnumeratorInUserDefinedEnum(')).toBeGreaterThan(body.indexOf('const FMcpEnumEdit Edit('));
  });

  it('the edit opens its step with nothing recorded and closes the enum change before the step, on every version', () => {
    const shared = read('Domains', 'AssetWorkflow', 'Enums', 'Shared.h');
    expect(shared).not.toMatch(/ENGINE_MINOR_VERSION|Cancel\(\)|UNDO_UNAVAILABLE_ENUM_RESTORE/u);
    const edit = shared.slice(shared.indexOf('struct FMcpEnumEdit'));
    // Members close in reverse order: the change is stored while the transaction is still open.
    expect(edit).toMatch(/FMcpScopedEditorTransaction Transaction;\s*McpSafeOperations::FMcpEnumUndo Undo;/u);
    expect(edit).toContain('Transaction(FText::FromString(Title), EMcpMutationDurability::EditorStateOnly, {}), Undo(Enum)');
  });
});

describe('the enum change: recorded in the open step, applied by SetEnums', () => {
  const undo = read('Safety', 'McpSafeOperationsEnumUndo.h');

  it('the enum is not transactional while the edit runs, and gets its flag back', () => {
    const open = undo.slice(undo.indexOf('explicit FMcpEnumUndo(UEnum* InEnum)'));
    expect(open).toMatch(/bWasTransactional\(InEnum->HasAnyFlags\(RF_Transactional\)\)[\s\S]*?Enum->ClearFlags\(RF_Transactional\);/u);
    const close = undo.slice(undo.indexOf('~FMcpEnumUndo()'));
    expect(close.indexOf('Enum->SetFlags(RF_Transactional);')).toBeGreaterThan(0);
  });

  it('a change is stored only when the enum changed, and its step is marked as one undo can apply', () => {
    const close = undo.slice(undo.indexOf('~FMcpEnumUndo()'));
    const store = close.indexOf('GUndo->StoreUndo(Enum, MakeUnique<FMcpEnumChange>(MoveTemp(Before), MoveTemp(After)));');
    expect(store).toBeGreaterThan(close.indexOf('if (GUndo && !(After == Before))'));
    expect(close.indexOf('McpEnumChangeSteps().Add(GUndo->GetContext().TransactionId);')).toBeGreaterThan(store);
  });

  it('undo writes the state before back and redo the state after, through SetEnums, never Serialize', () => {
    expect(undo).toMatch(/class FMcpEnumChange final : public FCommandChange/u);
    expect(undo).toMatch(/Apply\(UObject\* Object\) override[\s\S]*?After\.Write\(Enum\);/u);
    expect(undo).toMatch(/Revert\(UObject\* Object\) override[\s\S]*?Before\.Write\(Enum\);/u);
    expect(undo).toContain('MCP_SET_ENUMS(Enum, Values, Enum->GetCppForm());');
    expect(undo).not.toMatch(/->Serialize\(|FObjectReader|FObjectWriter/u);
  });

  it('a property set on an enum records it the same way, inside the Set Property step', () => {
    const set = read('Domains', 'Property', 'McpPropObjectSet.cpp');
    const open = set.indexOf('Transaction.Emplace(FText::FromString(TEXT("Set Property")));');
    const emplace = set.indexOf('EnumUndo.Emplace(Enum);');
    expect(emplace).toBeGreaterThan(open);
    expect(set.indexOf('RootObject->Modify(bMarkDirty);')).toBeGreaterThan(emplace);
    expect(set).toMatch(/EnumUndo\.Reset\(\);\s*Transaction\.Reset\(\);/u);
  });
});
