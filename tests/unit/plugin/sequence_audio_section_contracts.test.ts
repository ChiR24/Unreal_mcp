// Wiring contract of manage_sequence add_section with soundPath: a sound goes onto an audio
// track. The handler needs an editor, so this reads the C++ as text; the behaviour itself is
// covered by the integration cases in tests/mcp-tools/utility/manage-sequence.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

const SECTIONS = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Domains', 'Sequence', 'McpAutomationBridge_SequenceHandlersSections.cpp');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
function stripComments(source: string): string {
  return source.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
}

function property(name: string): { readonly description: string; readonly topics: readonly string[] } {
  const record = capabilityIndex().byId.get('sequence.edit_sequence_tracks');
  const properties = record?.schemas.input.properties;
  const entry = isRecord(properties) ? properties[name] : undefined;
  return {
    description: isRecord(entry) && typeof entry.description === 'string' ? entry.description : '',
    topics: record?.discovery.topics ?? [],
  };
}

describe('sequence add_section soundPath: a sound on an audio track', () => {
  const source = (): string => stripComments(readFileSync(SECTIONS, 'utf8'));

  it('reads soundPath and tells an explicit end from the 0-100 default', () => {
    const code = source();

    expect(code).toMatch(/TryGetStringField\(TEXT\("soundPath"\), SoundPath\)/u);
    expect(code).toMatch(/const bool bHasEnd = Payload->TryGetNumberField\(TEXT\("endFrame"\), EndFrame\) \|\|\s*Payload->TryGetNumberField\(TEXT\("end"\), EndFrame\);/u);
    expect(code).toMatch(/if \(End <= Start && \(bHasEnd \|\| SoundPath\.IsEmpty\(\)\)\)/u);
  });

  it('refuses a non-audio track and an unloadable sound before any section exists', () => {
    const code = source();
    const audioCheck = code.indexOf('if (!AudioTrack)');
    const notFound = code.indexOf('TEXT("ASSET_NOT_FOUND")');
    const create = code.indexOf('AddNewSoundOnRow(');

    expect(code).toMatch(/Cast<UMovieSceneAudioTrack>\(Track\)/u);
    expect(code).toMatch(/LoadObject<USoundBase>\(/u);
    expect(audioCheck).toBeGreaterThan(-1);
    expect(code.slice(audioCheck, notFound)).toContain('TEXT("INVALID_ARGUMENT")');
    expect(notFound).toBeGreaterThan(audioCheck);
    expect(create).toBeGreaterThan(notFound);
    expect(code.indexOf('Track->CreateNewSection()')).toBeGreaterThan(notFound);
  });

  it('makes the section with the engine\'s AddNewSoundOnRow, which adds it to the track itself', () => {
    const code = source();

    expect(code).toMatch(/Sound \? AudioTrack->AddNewSoundOnRow\(Sound, Start, INDEX_NONE\) : Track->CreateNewSection\(\)/u);
    expect(code).toMatch(/if \(!Sound\) \{\s*Track->AddSection\(\*NewSection\);\s*\}/u);
  });

  it('keeps the sound\'s own length without an end, and the given range with one', () => {
    const code = source();

    expect(code).toMatch(/if \(Sound && !bHasEnd\) \{\s*EndFrame = FFrameRate::TransformTime\(FFrameTime\(NewSection->GetExclusiveEndFrame\(\)\)/u);
    expect(code).toMatch(/\} else \{\s*NewSection->SetRange\(TRange<FFrameNumber>\(Start, End\)\);/u);
  });

  it('names the sound in the reply', () => {
    const code = source();

    expect(code).toMatch(/SetStringField\(TEXT\("soundPath"\), Sound->GetPathName\(\)\)/u);
    expect(code).toMatch(/SetStringField\(TEXT\("soundName"\), Sound->GetName\(\)\)/u);
  });

  it('is discoverable: soundPath is declared, trackType says Audio, music and cutscene sound are topics', () => {
    expect(property('soundPath').description).toMatch(/^Audio track only/u);
    expect(property('trackType').description).toContain('"Audio" (music or sound');
    const { topics } = property('soundPath');
    expect(topics).toContain('add music to sequence');
    expect(topics).toContain('add sound to cutscene');
  });
});
