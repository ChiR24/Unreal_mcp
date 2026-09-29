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
  CHANGE_ACTOR_SINGLE_FIELDS,
  CHANGE_ASSET_SINGLE_FIELDS,
  extractChanges,
  extractHandles
} from '../../../src/tools/catalog/capabilities/semantic/receipt-outcome.js';
import { sliceBetween } from '../plugin/plugin-contract-fixtures.js';

const PRIVATE = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private');

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
      /if\s*\(!StatedAssets\.IsValid\(\)\s*\|\|\s*!StatedAssets->TryGetArray\(StatedArray\)\)\s*\{\s*AddSingleChanges\(RawResult,\s*CHANGE_ASSET_SINGLES,\s*Changes\);\s*\}\s*AddSingleChanges\(RawResult,\s*CHANGE_ACTOR_SINGLES,\s*Changes\);/u
    );
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
    const source = nativeSource('Domains', 'WidgetAuthoring', 'Support', 'McpAutomationBridge_WidgetAuthoringPreview.cpp');
    const mark = source.indexOf('McpHandlerUtils::MarkNoAssetsChanged(ResultJson)');
    expect(mark).toBeGreaterThan(-1);
    expect(mark).toBeLessThan(source.indexOf('SendAutomationResponse('));
  });

  const playback: ReadonlyArray<readonly [string, readonly string[], string, string]> = [
    ['play_sound_at_location', ['Domains', 'Audio', 'McpAutomationBridge_AudioHandlersPlayback.cpp'], 'Lower == TEXT("play_sound_at_location")', 'Lower == TEXT('],
    ['play_sound_2d', ['Domains', 'Audio', 'McpAutomationBridge_AudioHandlersPlayback.cpp'], 'Lower == TEXT("play_sound_2d")', 'Lower == TEXT('],
    ['play_sound_attached', ['Domains', 'Audio', 'McpAutomationBridge_AudioHandlersPlayback.cpp'], 'Lower == TEXT("play_sound_attached")', 'return false;'],
    ['spawn_sound_at_location', ['Domains', 'Audio', 'McpAutomationBridge_AudioHandlersAmbient.cpp'], 'Lower == TEXT("spawn_sound_at_location")', 'return false;'],
    ['prime_sound', ['Domains', 'Audio', 'McpAutomationBridge_AudioHandlersComponentsAndFades.cpp'], 'Lower == TEXT("prime_sound")', 'create_audio_component']
  ];

  it.each(playback)('%s marks the sound it plays as unchanged before it replies', (_action, file, start, end) => {
    const block = sliceBetween(nativeSource(...file), start, end);
    const mark = block.indexOf('McpHandlerUtils::MarkNoAssetsChanged(');
    expect(mark, 'the handler must state that it changed no asset').toBeGreaterThan(-1);
    expect(mark).toBeLessThan(block.indexOf('SendAutomationResponse('));
  });
});

describe('over the gateway: a preview lists no change, an edit still does', () => {
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
});

// A level save answered only details.verifiedPath (and save-as levelPath), neither of which the
// receipt reads, so saving a level returned "handles": [] and "changes": [].
describe('a level save names the level it saved', () => {
  const LEVEL = '/Game/Maps/L_Stage01';

  it('savedAssetPath gives the receipt a handle and a change', () => {
    expect(extractChanges({ success: true, details: { savedAssetPath: LEVEL, verifiedPath: LEVEL } })).toEqual([LEVEL]);
    expect(extractHandles({ success: true, details: { savedAssetPath: LEVEL } })).toContainEqual({ kind: 'asset', path: LEVEL });
  });

  it('save and save-as both answer savedAssetPath', () => {
    const lifecycle = ['Domains', 'Level', 'Lifecycle'];
    expect(nativeSource(...lifecycle, 'McpAutomationBridge_LevelHandlersSaveCurrent.cpp'))
      .toMatch(/Resp->SetStringField\(TEXT\("savedAssetPath"\), LevelPath\);/u);
    expect(nativeSource(...lifecycle, 'McpAutomationBridge_LevelHandlersSaveAs.cpp'))
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
    const source = nativeSource('Domains', 'SystemControl', 'McpAutomationBridge_SystemControlHandlersOutputFiles.cpp');
    expect(source).toMatch(/SetArrayField\(TEXT\("changedEntities"\),\s*TArray<TSharedPtr<FJsonValue>>\{MakeShared<FJsonValueString>/u);
    expect(source).toMatch(/SetArrayField\(TEXT\("changedEntities"\), DeletedPaths\);/u);
  });
});
