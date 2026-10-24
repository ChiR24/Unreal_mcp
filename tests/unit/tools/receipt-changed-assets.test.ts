// A receipt's changes[] is inferred from the asset and actor fields a handler echoes,
// so a widget preview (which only draws the widget) and a played sound (which only
// uses its asset) claimed to have changed the asset they named. A result that carries
// a changedAssets array now states every asset it changed, an empty one included, and
// the single asset fields are not read; the actor fields still are, because a handler
// can change an actor while changing no asset.
//
// Both doors carry the rule, so the native source is held to the TypeScript lists and
// to the same gate, and the handlers that only look at an asset are pinned to state it.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { describe, expect, it } from 'vitest';

import { Logger } from '../../../src/utils/logging/logger.js';
import type { ITools } from '../../../src/types/tools/tool-interfaces.js';
import { handleUnrealGatewayCall, type GatewayContext } from '../../../src/server/tool-registry-gateway.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';
import {
  AFFECTED_ACTOR_HANDLE_LIMIT,
  CHANGE_ACTOR_SINGLE_FIELDS,
  CHANGE_ASSET_SINGLE_FIELDS,
  extractChanges,
  extractHandles
} from '../../../src/tools/catalog/capabilities/semantic/receipt-outcome.js';
import { sliceBetween } from '../plugin/plugin-contract-fixtures.js';

const PRIVATE = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
function stripComments(source: string): string {
  return source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
}

function nativeSource(...segments: readonly string[]): string {
  return stripComments(readFileSync(join(PRIVATE, ...segments), 'utf8'));
}

const WIDGET = '/Game/UI/WBP_HUD';

describe('extractChanges: an explicit changedAssets replaces the asset single-field inference', () => {
  it.each(CHANGE_ASSET_SINGLE_FIELDS)('an empty changedAssets keeps %s out of changes', (field) => {
    expect(extractChanges({ success: true, [field]: WIDGET, changedAssets: [] })).toEqual([]);
  });

  it.each(CHANGE_ASSET_SINGLE_FIELDS)('%s is still a change when the handler states nothing', (field) => {
    expect(extractChanges({ success: true, [field]: WIDGET })).toEqual([WIDGET]);
  });

  it('a stated list is the whole list: the echoed path is not added to it', () => {
    expect(extractChanges({ widgetPath: WIDGET, assetPath: '/Game/Other', changedAssets: ['/Game/Changed'] }))
      .toEqual(['/Game/Changed']);
  });

  it.each(CHANGE_ACTOR_SINGLE_FIELDS)('the actor field %s is still a change beside an empty changedAssets', (field) => {
    expect(extractChanges({ success: true, [field]: 'Actor_1', assetPath: '/Game/Sound', changedAssets: [] }))
      .toEqual(['Actor_1']);
  });

  it('the other explicit arrays are still read', () => {
    expect(extractChanges({ widgetPath: WIDGET, changedAssets: [], changedEntities: ['track A'] })).toEqual(['track A']);
  });

  it('is read from data and details like every other field', () => {
    expect(extractChanges({ data: { widgetPath: WIDGET, changedAssets: [] } })).toEqual([]);
    expect(extractChanges({ widgetPath: WIDGET, details: { changedAssets: [] } })).toEqual([]);
  });

  it('a changedAssets that is not an array states nothing', () => {
    expect(extractChanges({ widgetPath: WIDGET, changedAssets: 'none' })).toEqual([WIDGET]);
  });

  it('the asset handle is identity, not a change, so it survives', () => {
    expect(extractHandles({ widgetPath: WIDGET, changedAssets: [] })).toContainEqual({ kind: 'asset', path: WIDGET });
  });
});

describe('native McpExtractReceiptChanges mirrors the same rule', () => {
  const outcome = (): string => nativeSource('MCP', 'Execute', 'McpNativeReceiptOutcome.cpp');
  const list = (name: string): string[] => {
    const match = new RegExp(`${name}\\[\\]\\s*=\\s*\\{([^}]*)\\}`, 'u').exec(outcome());
    return [...(match?.[1] ?? '').matchAll(/TEXT\("(\w+)"\)/gu)].map((entry) => entry[1] ?? '');
  };

  it('holds the same asset and actor fields, in the same order', () => {
    expect(list('CHANGE_ASSET_SINGLES')).toEqual([...CHANGE_ASSET_SINGLE_FIELDS]);
    expect(list('CHANGE_ACTOR_SINGLES')).toEqual([...CHANGE_ACTOR_SINGLE_FIELDS]);
  });

  it('reads the asset singles only when no changedAssets array was stated, the actor ones always', () => {
    const body = outcome().slice(outcome().indexOf('TArray<FString> McpExtractReceiptChanges('));
    expect(body).toMatch(/ReadField\(RawResult,\s*TEXT\("changedAssets"\)\)/u);
    expect(body).toMatch(
      /if\s*\(!StatedAssets\.IsValid\(\)\s*\|\|\s*!StatedAssets->TryGetArray\(StatedArray\)\)\s*\{\s*AddSingleChanges\(RawResult,\s*CHANGE_ASSET_SINGLES,\s*Changes\);\s*\}[^;]*AddSingleChanges\(RawResult,\s*CHANGE_ACTOR_SINGLES,\s*Changes,\s*true\);/u
    );
    expect(outcome(), 'the first actor field found is the change').toMatch(/Changes\.AddUnique\(Text\);\s*if \(bFirstOnly\) return;/u);
  });
});

// A widget tree checked with dryRun listed its Widget Blueprint as changed: a dry run names what it checked and
// changes nothing.
describe('a dry run lists no change', () => {
  it.each([...CHANGE_ASSET_SINGLE_FIELDS, ...CHANGE_ACTOR_SINGLE_FIELDS])('%s beside dryRun: true is not a change', (field) => {
    expect(extractChanges({ success: true, [field]: WIDGET, dryRun: true })).toEqual([]);
    expect(extractChanges({ success: true, changedEntities: ['track A'], details: { [field]: WIDGET, dryRun: true } })).toEqual([]);
  });

  it('dryRun: false keeps the inference', () => {
    expect(extractChanges({ success: true, widgetPath: WIDGET, dryRun: false })).toEqual([WIDGET]);
  });

  it('the native extractor returns before it reads any change field', () => {
    const source = nativeSource('MCP', 'Execute', 'McpNativeReceiptOutcome.cpp');
    const body = source.slice(source.indexOf('TArray<FString> McpExtractReceiptChanges('));
    expect(body).toMatch(/ReadField\(RawResult,\s*TEXT\("dryRun"\)\)[\s\S]*?DryRun->Type == EJson::Boolean && DryRun->AsBool\(\)\)\)\s*\{\s*return Changes;/u);
    expect(body.indexOf('TEXT("dryRun")')).toBeLessThan(body.indexOf('for (const TCHAR* Field : CHANGE_ARRAYS)'));
  });
});

// A move listed its actor twice, by label and by object path, as two changes.
describe('one actor is one change', () => {
  it('names it by actorName, or by actorPath when there is no name', () => {
    const path = '/Game/Maps/Demo.Demo:PersistentLevel.StaticMeshActor_3';
    expect(extractChanges({ success: true, actorName: 'Crate', actorPath: path })).toEqual(['Crate']);
    expect(extractChanges({ success: true, actorPath: path })).toEqual([path]);
  });
});

describe('the handlers that only look at an asset state that they changed none', () => {
  const helper = (): string => nativeSource('Foundation', 'HandlerUtils', 'McpHandlerUtilsResponses.h');

  it('MarkNoAssetsChanged writes an empty changedAssets array', () => {
    expect(helper()).toMatch(
      /inline void MarkNoAssetsChanged\([^)]*\)\s*\{[^}]*SetArrayField\(TEXT\("changedAssets"\),\s*TArray<TSharedPtr<FJsonValue>>\(\)\)/u
    );
  });

  it('preview_widget marks the widget it draws as unchanged before it replies', () => {
    const source = nativeSource('Domains', 'WidgetAuthoring', 'Support', 'McpWidgetPreview.cpp');
    const mark = source.indexOf('McpHandlerUtils::MarkNoAssetsChanged(ResultJson)');
    expect(mark).toBeGreaterThan(-1);
    expect(mark).toBeLessThan(source.indexOf('SendAutomationResponse('));
  });

  const playback: ReadonlyArray<readonly [string, readonly string[], string, string]> = [
    ['play_sound_at_location', ['Domains', 'Audio', 'McpAudioPlayback.cpp'], 'Lower == TEXT("play_sound_at_location")', 'Lower == TEXT('],
    ['play_sound_2d', ['Domains', 'Audio', 'McpAudioPlayback.cpp'], 'Lower == TEXT("play_sound_2d")', 'Lower == TEXT('],
    ['play_sound_attached', ['Domains', 'Audio', 'McpAudioPlayback.cpp'], 'Lower == TEXT("play_sound_attached")', 'return false;'],
    ['spawn_sound_at_location', ['Domains', 'Audio', 'McpAudioAmbient.cpp'], 'Lower == TEXT("spawn_sound_at_location")', 'return false;'],
    ['prime_sound', ['Domains', 'Audio', 'McpAudioComponentsAndFades.cpp'], 'Lower == TEXT("prime_sound")', 'create_audio_component']
  ];

  it.each(playback)('%s marks the sound it plays as unchanged before it replies', (_action, file, start, end) => {
    const block = sliceBetween(nativeSource(...file), start, end);
    const mark = block.indexOf('McpHandlerUtils::MarkNoAssetsChanged(');
    expect(mark, 'the handler must state that it changed no asset').toBeGreaterThan(-1);
    expect(mark).toBeLessThan(block.indexOf('SendAutomationResponse('));
  });
});

describe('over the gateway: a preview lists no change, an edit still does, a many form names its actors', () => {
  let bridgeResult: unknown = { success: true };

  function context(): GatewayContext {
    const tools: ITools = {
      automationBridge: {
        isConnected: () => true,
        sendAutomationRequest: async () => bridgeResult
      }
    };
    return { tools, logger: new Logger('receipt-changed-assets', 'error'), ensureConnected: async () => true };
  }

  async function receiptOf(capability: string, params: Record<string, unknown>): Promise<Record<string, unknown>> {
    const reply = await handleUnrealGatewayCall({ operation: 'execute', capability, params }, context());
    const receipt: unknown = reply.receipt;
    if (!isRecord(receipt)) throw new Error(`no receipt in ${JSON.stringify(reply).slice(0, 400)}`);
    return receipt;
  }

  it('a widget preview names the widget it drew without listing it as changed', async () => {
    bridgeResult = {
      success: true, widgetPath: WIDGET, changedAssets: [], width: 1280, height: 720,
      mimeType: 'image/png', sizeBytes: 4, imageBase64: 'AAAA', editorOpened: false
    };
    const receipt = await receiptOf('blueprint.preview_widget', { widgetPath: WIDGET });

    expect(receipt.status).toBe('success');
    expect(receipt.changes).toEqual([]);
    expect(receipt.handles).toContainEqual({ kind: 'asset', path: WIDGET });
  });

  it('a reparent that states nothing still lists the widget it edited', async () => {
    bridgeResult = { success: true, widgetPath: WIDGET, slotName: 'PlayButton', newParent: 'Column', index: 0 };
    const receipt = await receiptOf('blueprint.edit_widget_blueprint', {
      edit: 'reparent_widget', widgetPath: WIDGET, slotName: 'PlayButton', newParent: 'Column'
    });

    expect(receipt.changes).toEqual([WIDGET]);
  });

  it('an add_tag many-form reply lists each actor it tagged as a change and as an actor handle', async () => {
    bridgeResult = { success: true, tag: 'Pickup', taggedCount: 2, missing: ['Gone'], affectedActors: ['Sign_1', 'Sign_2'] };
    const receipt = await receiptOf('control_actor.add_tag', { actorNames: ['Sign_1', 'Sign_2', 'Gone'], tag: 'Pickup' });

    expect(receipt.changes).toEqual(['Sign_1', 'Sign_2']);
    expect(receipt.handles).toEqual([{ kind: 'actor', ref: 'Sign_1' }, { kind: 'actor', ref: 'Sign_2' }]);
  });
});

// A level save answered only details.verifiedPath (and save-as levelPath), neither of which the
// receipt reads, so saving a level returned "handles": [] and "changes": [].
describe('a level save names the level it saved', () => {
  const LEVEL = '/Game/Maps/L_Main';

  it('savedAssetPath gives the receipt a handle and a change', () => {
    expect(extractChanges({ success: true, details: { savedAssetPath: LEVEL, verifiedPath: LEVEL } })).toEqual([LEVEL]);
    expect(extractHandles({ success: true, details: { savedAssetPath: LEVEL } })).toContainEqual({ kind: 'asset', path: LEVEL });
  });

  it('save and save-as both answer savedAssetPath', () => {
    const lifecycle = ['Domains', 'Level', 'Lifecycle'];
    expect(nativeSource(...lifecycle, 'McpLevelSaveCurrent.cpp'))
      .toMatch(/Resp->SetStringField\(TEXT\("savedAssetPath"\), LevelPath\);/u);
    expect(nativeSource(...lifecycle, 'McpLevelSaveAs.cpp'))
      .toMatch(/Resp->SetStringField\(TEXT\("savedAssetPath"\), SavePath\);/u);
  });
});

// delete_output_file answered only `path` (a disk file, not an asset field), so deleting a
// screenshot returned "changes": []. It now names every file it deleted in changedEntities.
describe('an output-file delete names the files it deleted', () => {
  it('changedEntities is read into changes', () => {
    const FILE = 'Saved/Screenshots/check.png';
    expect(extractChanges({ success: true, details: { path: FILE, deleted: true, changedEntities: [FILE] } })).toEqual([FILE]);
  });

  it('the single and the list form both answer changedEntities', () => {
    const source = nativeSource('Domains', 'SystemControl', 'McpSystemOutputFiles.cpp');
    expect(source).toMatch(/SetArrayField\(TEXT\("changedEntities"\),\s*TArray<TSharedPtr<FJsonValue>>\{MakeShared<FJsonValueString>/u);
    expect(source).toMatch(/SetArrayField\(TEXT\("changedEntities"\), DeletedPaths\);/u);
  });
});

// set_property on the PIE GameInstance answered handles [{kind: asset, path: /Engine/Transient}] and
// changes ["/Engine/Transient"]: the running game's objects live in the transient package, and no
// asset changed.
describe('the transient package is never a changed asset', () => {
  it('extractHandles and extractChanges skip it, and still read the actor fields', () => {
    const result = { success: true, details: { assetPath: '/Engine/Transient', actorName: 'BP_RiderGM0' } };
    expect(extractHandles(result)).toEqual([{ kind: 'actor', ref: 'BP_RiderGM0' }]);
    expect(extractChanges(result)).toEqual(['BP_RiderGM0']);
    expect(extractChanges({ assetPath: '/Engine/Transient.BP_MyGameInstance_C_0' })).toEqual([]);
  });

  it('the native extraction skips it on the same fields', () => {
    const source = nativeSource('MCP', 'Execute', 'McpNativeReceiptOutcome.cpp');
    expect(source).toContain('Path == TEXT("/Engine/Transient") || Path.StartsWith(TEXT("/Engine/Transient."))');
    expect(source).toContain('if (!Text.IsEmpty() && !IsTransientPackagePath(Text))');
    expect(source).toContain('if (IsAllowedPath(Path) && !IsTransientPackagePath(Path))');
  });
});

// The many forms of set_visibility, set_actor_collision and add_tag (actorNames) name every actor they
// changed in affectedActors and carry no single actorName, so a receipt listed them in changes[] but held no
// actor handle. Both extractors derive one handle per entry rather than each handler building a handles array.
describe('the actors a many form names each get a typed actor handle, on both doors', () => {
  const actor = (ref: string): { kind: 'actor'; ref: string } => ({ kind: 'actor', ref });
  const named = (count: number): string[] => Array.from({ length: count }, (_, index) => `Sign_${index + 1}`);

  it('one handle per entry, in the order the reply named them', () => {
    expect(extractHandles({ success: true, taggedCount: 2, affectedActors: ['Sign_1', 'Sign_2'] }))
      .toEqual([actor('Sign_1'), actor('Sign_2')]);
  });

  it('only the first AFFECTED_ACTOR_HANDLE_LIMIT entries become handles, while changes still lists every one', () => {
    const affectedActors = named(AFFECTED_ACTOR_HANDLE_LIMIT + 5);

    expect(extractHandles({ affectedActors })).toEqual(affectedActors.slice(0, AFFECTED_ACTOR_HANDLE_LIMIT).map(actor));
    expect(extractChanges({ affectedActors })).toEqual(affectedActors);
  });

  it('a repeated entry, or the actor a single field already named, is one handle', () => {
    expect(extractHandles({ actorName: 'Sign_1', affectedActors: ['Sign_1', 'Sign_2', 'Sign_2'] }))
      .toEqual([actor('Sign_1'), actor('Sign_2')]);
  });

  it('an entry that is not a valid actor ref is dropped, not coerced, and still counts toward the bound', () => {
    expect(extractHandles({ affectedActors: ['Sign_1', '', 42, null, { name: 'x' }, 'x'.repeat(513), 'Sign_2'] }))
      .toEqual([actor('Sign_1'), actor('Sign_2')]);
    expect(extractHandles({ affectedActors: [...Array.from({ length: 5 }, () => ''), ...named(AFFECTED_ACTOR_HANDLE_LIMIT)] }))
      .toEqual(named(AFFECTED_ACTOR_HANDLE_LIMIT - 5).map(actor));
  });

  it('is read from data and details like every other field, and a value that is not an array names nothing', () => {
    expect(extractHandles({ data: { affectedActors: ['Sign_1'] } })).toEqual([actor('Sign_1')]);
    expect(extractHandles({ details: { affectedActors: ['Sign_1'] } })).toEqual([actor('Sign_1')]);
    expect(extractHandles({ affectedActors: 'Sign_1' })).toEqual([]);
  });

  describe('the native McpExtractReceiptHandles', () => {
    const source = (): string => nativeSource('MCP', 'Execute', 'McpNativeReceiptOutcome.cpp');

    it('takes the same bound over the same entries through the same gate', () => {
      const body = source().slice(source().indexOf('McpExtractReceiptHandles('));

      expect(source()).toContain(`const int32 MAX_AFFECTED_ACTOR_HANDLES = ${AFFECTED_ACTOR_HANDLE_LIMIT};`);
      expect(body).toMatch(/ReadField\(RawResult,\s*TEXT\("affectedActors"\)\)/u);
      expect(body).toMatch(
        /for \(int32 Index = 0; Index < AffectedArray->Num\(\) && Index < MAX_AFFECTED_ACTOR_HANDLES; \+\+Index\)\s*\{\s*FString Ref;\s*if \(McpHandlerUtils::TryGetJsonValueString\(\(\*AffectedArray\)\[Index\], Ref\) && !Ref\.IsEmpty\(\) && Ref\.Len\(\) <= 512\)\s*\{\s*AddHandle\(TEXT\("actor"\), TEXT\("ref"\), Ref\);/u
      );
    });

    it('derives them after the single actor field, as extractHandles does', () => {
      const body = source().slice(source().indexOf('McpExtractReceiptHandles('));

      expect(body.indexOf('for (const TCHAR* Field : ACTOR_FIELDS)')).toBeGreaterThan(-1);
      expect(body.indexOf('for (const TCHAR* Field : ACTOR_FIELDS)')).toBeLessThan(body.indexOf('TEXT("affectedActors")'));
    });

    it('reads a field from the root, then data, details and data.details, the order makeReader uses', () => {
      const reader = sliceBetween(source(), 'TSharedPtr<FJsonValue> ReadField(', 'FString OutcomeReadString(');

      expect(reader).toMatch(
        /Holders\[\] = \{\s*Result,\s*Data,\s*OutcomeChildObject\(Result, TEXT\("details"\)\),\s*OutcomeChildObject\(Data, TEXT\("details"\)\)\};/u
      );
      expect(reader).toMatch(/Data = OutcomeChildObject\(Result, TEXT\("data"\)\);/u);
    });
  });
});
