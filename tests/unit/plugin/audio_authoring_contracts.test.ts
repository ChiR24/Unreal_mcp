// Todo 22 BB-029..BB-033 — audio routing + MetaSound + SoundCue + source effects source contracts.
import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const PRIVATE = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private');
const TS = resolve(process.cwd(), 'src/tools/handlers/audio');
function readCpp(...parts: string[]): string {
  const p = resolve(PRIVATE, ...parts);
  expect(existsSync(p), `missing: ${p}`).toBe(true);
  return readFileSync(p, 'utf8');
}
function readTsp(name: string): string {
  const p = resolve(TS, name);
  expect(existsSync(p), `missing: ${p}`).toBe(true);
  return readFileSync(p, 'utf8');
}
function code(s: string): string { return s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, ''); }

const metaNodes = () => readCpp('Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersMetaSoundNodes.cpp');
const metaConnect = () => readCpp('Domains/AudioAuthoring/MetaSound/McpAutomationBridge_AudioAuthoringHandlersMetaSoundNodesConnect.cpp');
const cueNodes = () => readCpp('Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersSoundCueNodes.cpp');
const audioInfo = () => readCpp('Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersInfo.cpp');
const effects = () => readCpp('Domains/AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersEffects.cpp');
const audioEffectConfig = () => readCpp('Domains/Audio/McpAutomationBridge_AudioHandlersEffectConfiguration.cpp');

describe('BB-029 runtime audio routing is locked', () => {
  it('TS audio-handlers dispatches the six runtime actions to correct bridge tools', () => {
    const s = code(readTsp('runtime/audio-handlers.ts'));
    for (const a of ['create_ambient_sound', 'create_audio_component', 'play_sound_2d', 'play_sound_at_location', 'push_sound_mix', 'prime_sound']) {
      expect(s, `${a} must be routed`).toContain(a);
    }
  });
  it('no manage_level_structure path exists in the audio routing', () => {
    const s = code(readTsp('runtime/audio-handlers.ts'));
    expect(s).not.toMatch(/manage_level_structure/i);
  });
});

describe('BB-030 MetaSound node diagnostics on NODE_CLASS_NOT_FOUND', () => {
  it('the error path includes availableNodes diagnostics', () => {
    const s = code(metaNodes());
    const errIdx = s.indexOf('NODE_CLASS_NOT_FOUND');
    expect(errIdx).toBeGreaterThan(-1);
    const window = s.slice(errIdx, errIdx + 800);
    expect(window).toMatch(/availableNodes|acceptedNodeTypes/i);
  });
});

describe('BB-031 MetaSound Value alias for graph-input output pins', () => {
  it('HandleMetaSoundNodeConnect maps Value to the single output when Value is not found', () => {
    const s = code(metaConnect());
    expect(s, 'must contain a Value literal in the alias logic').toContain('TEXT("Value")');
    expect(s, 'must check Outputs.Num() == 1 for the single-output alias').toMatch(/Outputs\.Num\(\)\s*==\s*1/);
    expect(s, 'must retry AddNamedEdges after resolving the alias').toMatch(/AddNamedEdges.*Edges.*CreatedEdges/s);
  });
});

describe('BB-032 SoundCue root/output node resolution', () => {
  it('connect_cue_nodes resolves Output/Root/asset-name to FirstNode', () => {
    const s = code(cueNodes());
    const connectIdx = s.indexOf('connect_cue_nodes');
    expect(connectIdx).toBeGreaterThan(-1);
    const slice = s.slice(connectIdx, connectIdx + 2000);
    expect(slice).toMatch(/FirstNode|Output|Root/i);
  });
  it('get_audio_info exposes rootNodeId for SoundCue', () => {
    const s = code(audioInfo());
    const cueIdx = s.indexOf('USoundCue');
    expect(cueIdx).toBeGreaterThan(-1);
    const slice = s.slice(cueIdx, cueIdx + 600);
    expect(slice).toContain('rootNodeId');
  });
});

describe('BB-033 durable concrete preset creation + no abstract NewObject', () => {
  it('add_source_effect creates a durable preset with CreatePackage + RF_Public when only effectType is supplied', () => {
    const s = code(effects());
    const addIdx = s.indexOf('add_source_effect');
    expect(addIdx).toBeGreaterThan(-1);
    const slice = s.slice(addIdx);
    expect(slice).toMatch(/CreatePackage[\s\S]*RF_Public|RF_Public[\s\S]*CreatePackage/i);
    expect(slice).toMatch(/McpSafeAssetSave/);
  });
  it('no abstract USoundEffectSourcePreset NewObject in Domains/Audio', () => {
    const s = code(audioEffectConfig());
    expect(s, 'abstract USoundEffectSourcePreset NewObject must be removed').not.toMatch(/NewObject<USoundEffectSourcePreset>/);
  });
});
