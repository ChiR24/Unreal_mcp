// Wiring contracts of the Widget Blueprint authoring handlers that no unit test can
// reach by running them (they need an editor): who passes which flag to the shared
// tree funnel, and what a reply names. Behaviour itself belongs to the integration
// cases in tests/mcp-tools/core/manage-blueprint*.test.mjs.

import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

const DOMAIN = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Domains', 'WidgetAuthoring');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
function stripComments(source: string): string {
  return source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
}

function read(...segments: readonly string[]): string {
  return stripComments(readFileSync(join(DOMAIN, ...segments), 'utf8'));
}

function sources(dir: string = DOMAIN): string[] {
  return readdirSync(dir, { withFileTypes: true }).flatMap((entry) =>
    entry.isDirectory() ? sources(join(dir, entry.name)) : /\.(cpp|h)$/u.test(entry.name) ? [join(dir, entry.name)] : []);
}

const UI_WIDGET_AUTHORING = join(
  'plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Domains', 'Ui',
  'McpAutomationBridge_UiHandlersWidgetAuthoring.cpp'
);

describe('widgetPath is the canonical /Game package path, never the object path', () => {
  const objectPathWrite = /SetStringField\(\s*TEXT\("widgetPath"\)\s*,[^;]*GetPathName\(\)/u;

  it('no widget handler derives the widgetPath it answers from GetPathName()', () => {
    const files = [...sources(), UI_WIDGET_AUTHORING];
    const offenders = files.filter((file) => objectPathWrite.test(stripComments(readFileSync(file, 'utf8'))));

    expect(files.length).toBeGreaterThan(30);
    expect(offenders.map((file) => file.replace(/^.*[\\/]/u, ''))).toEqual([]);
  });

  it('the shared layout reply, the creators and the template builders name the package path', () => {
    const viaHelper = /SetStringField\(TEXT\("widgetPath"\), WidgetBlueprintPackagePath\((?:WidgetBP|WidgetBlueprint)\)\)/u;
    for (const segments of [
      ['Support', 'McpAutomationBridge_WidgetAuthoringSlotReadback.cpp'],
      ['Support', 'McpAutomationBridge_WidgetAuthoringCreation.cpp'],
      ['Animation', 'McpAutomationBridge_WidgetAuthoringAnimationCore.cpp'],
      ['Bindings', 'McpAutomationBridge_WidgetAuthoringPropertyBindings.cpp'],
      ['Templates', 'McpAutomationBridge_WidgetAuthoringHudElements.cpp'],
      ['Templates', 'McpAutomationBridge_WidgetAuthoringScreenParts.cpp']
    ] as const) {
      expect(read(...segments), segments.join('/')).toMatch(viaHelper);
    }
    expect(stripComments(readFileSync(UI_WIDGET_AUTHORING, 'utf8'))).toMatch(/const FString CreatedPath = WidgetBlueprint->GetOutermost\(\)->GetName\(\);/u);
  });

  it('WidgetBlueprintPackagePath is the package name of the asset', () => {
    expect(read('Support', 'McpAutomationBridge_WidgetAuthoringLoading.cpp')).toMatch(
      /FString WidgetBlueprintPackagePath\(const UWidgetBlueprint\* WidgetBP\)\s*\{\s*return WidgetBP && WidgetBP->GetOutermost\(\) \? WidgetBP->GetOutermost\(\)->GetName\(\) : FString\(\);/u
    );
  });
});

describe('set_style and set_clipping name the widget they saved into', () => {
  const styling = (): string => read('Styling', 'McpAutomationBridge_WidgetAuthoringStyleClipping.cpp');

  it('widgetPath is on the reply before either variant branches', () => {
    const source = styling();
    const named = source.indexOf('ResultJson->SetStringField(TEXT("widgetPath"), WidgetBlueprintPackagePath(WidgetBP));');

    expect(named).toBeGreaterThan(-1);
    expect(named).toBeLessThan(source.indexOf('SubAction.Equals(TEXT("set_clipping")', named));
  });

  it('a property read names the widget without claiming a change', () => {
    const source = styling();
    const readBranch = source.slice(source.indexOf('TEXT("read")'), source.indexOf('bool bWriteSuccess'));

    expect(readBranch.length).toBeGreaterThan(0);
    expect(readBranch).toContain('McpHandlerUtils::MarkNoAssetsChanged(ResultJson)');
    expect(readBranch).not.toContain('bWriteSuccess');
  });
});

describe('a tree edit that adds or renames a variable widget leaves the generated class current', () => {
  it('RefreshWidgetBlueprintClass compiles, but never while Play-In-Editor runs', () => {
    expect(read('Support', 'McpAutomationBridge_WidgetAuthoringLoading.cpp')).toMatch(
      /bool RefreshWidgetBlueprintClass\(UWidgetBlueprint\* WidgetBP\)\s*\{\s*return WidgetBP && !\(GEditor && GEditor->PlayWorld\) && McpSafeCompileBlueprint\(WidgetBP\);/u
    );
  });

  it.each([
    ['add_* widgets', ['Support', 'McpAutomationBridge_WidgetAuthoringAddWidget.cpp'], 'ValidateWidgetCreation('],
    ['add_widget_component', ['Components', 'McpAutomationBridge_WidgetAuthoringGenericComponent.cpp'], 'McpSafeAssetSave(WidgetBP)'],
    ['duplicate_widget', ['Support', 'McpAutomationBridge_WidgetAuthoringDuplicate.cpp'], 'MarkWidgetBlueprintModifiedAndSave(WidgetBP)'],
    ['add_game_widget', ['Templates', 'McpAutomationBridge_WidgetAuthoringHudElements.cpp'], 'ValidateWidgetCreation(']
  ] as const)('%s refreshes the class after the edit it saved', (_name, file, after) => {
    const source = read(...file);
    const refresh = source.indexOf('RefreshWidgetBlueprintClass(WidgetBP);');

    expect(refresh, 'the handler refreshes the class').toBeGreaterThan(-1);
    expect(refresh).toBeGreaterThan(source.indexOf(after));
  });

  it('rename_widget refreshes it inside its own block, not remove_widget\'s', () => {
    const source = read('Support', 'McpAutomationBridge_WidgetAuthoringManipulation.cpp');
    const rename = source.slice(source.indexOf('"rename_widget"'), source.indexOf('"reparent_widget"'));
    const remove = source.slice(source.indexOf('"remove_widget"'), source.indexOf('"rename_widget"'));

    expect(rename).toContain('WidgetAuthoringHelpers::RefreshWidgetBlueprintClass(WidgetBP);');
    expect(remove).not.toContain('RefreshWidgetBlueprintClass');
  });
});

describe('SafeAddWidgetToTree: only an add that re-uses a slotName warns that the widget was already seated', () => {
  it('the warning is gated on the caller not moving the widget on purpose', () => {
    const tree = read('Support', 'McpAutomationBridge_WidgetAuthoringTree.cpp');

    expect(tree).toMatch(/if \(bDetached && bWarnOnReuse\)/u);
    expect(tree).toMatch(/DetachFromOwningPanel\(WidgetBP, NewWidget, OldSlot, OldParent, OldIndex, !bMove\);/u);
  });

  it('reparent_widget is the one caller that says the move is intended', () => {
    const calls: Array<{ readonly file: string; readonly args: string }> = [];
    for (const file of sources()) {
      if (/WidgetAuthoringTree(Mutation)?\.(cpp|h)$/u.test(file)) continue;
      for (const match of stripComments(readFileSync(file, 'utf8')).matchAll(/SafeAddWidgetToTree\(([^;{]*)/gu)) {
        calls.push({ file: file.replace(/^.*[\\/]/u, ''), args: match[1] ?? '' });
      }
    }
    const moving = calls.filter((call) => /,\s*true\s*\)/u.test(call.args));

    expect(calls.length, 'every add path still goes through the funnel').toBeGreaterThanOrEqual(4);
    expect(moving.map((call) => call.file)).toEqual(['McpAutomationBridge_WidgetAuthoringManipulation.cpp']);

    const manipulation = read('Support', 'McpAutomationBridge_WidgetAuthoringManipulation.cpp');
    const reparent = manipulation.slice(manipulation.indexOf('reparent_widget'), manipulation.indexOf('get_widget_slot_info'));
    expect(reparent).toMatch(/SafeAddWidgetToTree\(WidgetBP, TargetWidget, NewParentWidget->GetName\(\),[^;{]*,\s*true\)/u);
  });
});

describe('duplicate_widget: the record says how the copies are named', () => {
  const newName = (): string => {
    const properties = capabilityIndex().byId.get('blueprint.duplicate_widget')?.schemas.input.properties;
    const entry = isRecord(properties) ? properties.newName : undefined;
    return isRecord(entry) && typeof entry.description === 'string' ? entry.description : '';
  };

  it('states the _Copy naming, made unique with a number, and no longer claims the children keep their names', () => {
    expect(newName()).toMatch(/<its own name>_Copy/u);
    expect(newName()).toMatch(/_1, _2/u);
    expect(newName()).not.toMatch(/keep their names/u);
  });

  it('is what the handler does: every child is copied under its own name plus _Copy', () => {
    const handler = read('Support', 'McpAutomationBridge_WidgetAuthoringDuplicate.cpp');

    expect(handler).toContain('Source->GetName() + TEXT("_Copy")');
    expect(handler).toMatch(/McpCopySubtree\(Tree, Child, FString\(\), Names\)/u);
    expect(handler).toMatch(/TEXT\("%s_%d"\), \*Wanted, Suffix/u);
  });
});

describe('set_size sizes a box child by rule, and the record declares what the handler reads', () => {
  const record = capabilityIndex().byId.get('blueprint.set_widget_layout');
  const properties = isRecord(record?.schemas.input.properties) ? record.schemas.input.properties : {};

  it('declares sizeRule and fillValue on the layout family, and infers the size variant from them', () => {
    expect(Object.keys(properties)).toEqual(expect.arrayContaining(['size', 'sizeRule', 'fillValue']));
    expect(record?.routing.dispatchBy?.declaredBy?.sizeRule).toEqual(['size']);
    expect(record?.routing.dispatchBy?.declaredBy?.fillValue).toEqual(['size']);
    expect(record?.schemas.input.required).not.toContain('size');
  });

  it('the handler reads both, through the property the layout readback reports', () => {
    const geometry = read('Layout', 'McpAutomationBridge_WidgetAuthoringCanvasSlotGeometry.cpp');
    const readback = read('Support', 'McpAutomationBridge_WidgetAuthoringSlotReadback.cpp');

    expect(geometry).toContain('TEXT("sizeRule")');
    expect(geometry).toContain('TEXT("fillValue")');
    for (const source of [geometry, readback]) {
      expect(source).toMatch(/FindFProperty<FStructProperty>\(\w+->GetClass\(\), TEXT\("Size"\)\)/u);
      expect(source).toContain('FSlateChildSize::StaticStruct()');
    }
  });
});

// set_style propertyName "Font.OutlineSettings.OutlineSize" answered PROPERTY_NOT_FOUND: the writer looked
// the name up flat, so no struct member (a text outline) could be written.
describe('set_style propertyName walks a dotted path into a struct', () => {
  it('resolves through the shared property path resolver and writes into the resolved container', () => {
    const source = read('Styling', 'McpAutomationBridge_WidgetAuthoringStyleClipping.cpp');
    expect(source).toContain('McpResolvePropertyPath(Widget, PropertyName, Container, ResolvedPath, ResolveError)');
    expect(source).toContain('Prop->ContainerPtrToValuePtr<void>(Container)');
  });
});
