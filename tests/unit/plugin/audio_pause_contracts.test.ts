// Wiring contracts of manage_audio pause_sound / resume_sound, which no unit test can reach by
// running them (they need an editor). Behaviour itself belongs to the integration cases in
// tests/mcp-tools/utility/manage-audio.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';

const AUDIO = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Domains', 'Audio');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (file: string): string =>
  readFileSync(join(AUDIO, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('pause_sound and resume_sound act on the sounds stop_sound stops', () => {
  const pause = (): string => read('McpAutomationBridge_AudioHandlersPause.cpp');

  it('fold into the stop_sound record: soundOp picks the bridge action, stop stays the default', () => {
    const record = capabilityIndex().byId.get('manage_audio.stop_sound');
    expect(record?.routing.dispatchBy).toEqual({
      param: 'soundOp',
      actions: { stop: 'stop_sound', pause: 'pause_sound', resume: 'resume_sound' },
    });
    expect(record?.schemas.input.required).toEqual(['action']);
    expect(Object.keys(record?.schemas.input.properties ?? {})).toEqual(expect.arrayContaining(['soundPath', 'all', 'soundOp']));
  });

  it('are routed by the audio dispatcher before it gives up on the action', () => {
    const dispatcher = read('McpAutomationBridge_AudioHandlers.cpp');
    expect(dispatcher).toContain('Lower != TEXT("pause_sound")');
    expect(dispatcher).toContain('Lower != TEXT("resume_sound")');
    expect(dispatcher).toContain('&McpAudioHandlers::HandlePauseActions');
    expect(read('McpAutomationBridge_AudioHandlersPrivate.h')).toContain('bool HandlePauseActions(');
  });

  it('start from the 2D sounds play_sound started, and scan every audio component only with all', () => {
    expect(read('McpAutomationBridge_AudioHandlersPrivate.h')).toContain('McpPreviewSounds();');
    const source = pause();
    expect(source).toContain('McpPreviewSounds()');
    expect(source).toMatch(/if \(bAll\) \{\s*for \(TObjectIterator<UAudioComponent> It; It; \+\+It\)/u);
  });

  it('refuse a missing sound asset before touching any component', () => {
    const source = pause();
    const refusal = source.indexOf('TEXT("ASSET_NOT_FOUND")');
    expect(refusal).toBeGreaterThan(-1);
    expect(refusal).toBeLessThan(source.indexOf('SetPaused('));
  });

  it('change only a playing component that is not already in the requested state, and count it', () => {
    const source = pause();
    expect(source).toContain('IsPlaying()');
    expect(source).toContain('(Only && Component->Sound != Only)');
    expect(source).toContain('(Component->GetPlayState() == EAudioComponentPlayState::Paused) != bPause');
    expect(source).toMatch(/SetPaused\(bPause\);\s*\+\+Changed;/u);
    expect(source).toContain('SetNumberField(bPause ? TEXT("paused") : TEXT("resumed"), Changed)');
  });

  it('reply with a zero count, not an error, when nothing was playing (as stop_sound does)', () => {
    const source = pause();
    expect(source).toMatch(/Resp->SetBoolField\(TEXT\("success"\), true\);/u);
    expect(source).toContain('SendAutomationResponse(RequestingSocket, RequestId, true, Message, Resp)');
  });
});
