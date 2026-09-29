// Wiring contract of manage_sequence add_section with soundPath: a sound goes onto an audio
// track. The handler needs an editor, so this reads the C++ as text; the behaviour itself is
// covered by the integration cases in tests/mcp-tools/utility/manage-sequence.test.mjs.

import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import { capabilityIndex } from '../../../src/server/gateway/gateway-capability-index.js';
import { isRecord } from '../../../src/utils/validation/type-guards.js';

const PRIVATE = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private');
const SEQUENCE_DIR = join(PRIVATE, 'Domains', 'Sequence');
const SECTIONS = join(SEQUENCE_DIR, 'McpAutomationBridge_SequenceHandlersSections.cpp');
const TRACK_CREATION = join(SEQUENCE_DIR, 'McpAutomationBridge_SequenceHandlersTrackCreation.cpp');

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

  it('runs the sound path through the shared sanitizer before it loads anything', () => {
    const code = source();
    const sanitize = code.indexOf('SanitizeProjectRelativePath(SoundObjectPath)');
    const load = code.indexOf('LoadObject<USoundBase>(');

    expect(code).toMatch(/MapContentRootInline\(SoundObjectPath\)/u);
    expect(sanitize).toBeGreaterThan(code.indexOf('if (!AudioTrack)'));
    expect(load).toBeGreaterThan(sanitize);
    expect(code.slice(sanitize, load)).toContain('TEXT("INVALID_PATH")');
    expect(code.slice(load)).not.toMatch(/LoadObject<USoundBase>\(\s*nullptr,\s*\*\(SoundPath/u);
  });

  it('makes the section with the engine\'s AddNewSoundOnRow, which adds it to the track itself', () => {
    const code = source();

    expect(code).toMatch(/Sound \? AudioTrack->AddNewSoundOnRow\(Sound, Start, INDEX_NONE\) : Track->CreateNewSection\(\)/u);
    expect(code).toMatch(/if \(!Sound\) \{\s*Track->AddSection\(\*NewSection\);\s*\}/u);
  });

  it('keeps the sound\'s own length without an end, and the given range with one', () => {
    const code = source();

    expect(code).toMatch(/const FFrameNumber SoundEnd = NewSection->GetExclusiveEndFrame\(\);/u);
    expect(code).toMatch(/bHasEnd \? End - Start\s*: \(Start < SoundEnd \? SoundEnd - Start\s*: MovieScene->GetTickResolution\(\)\.AsFrameNumber\(1\.0\)\)/u);
    expect(code).toMatch(/if \(!bHasEnd\) \{\s*EndFrame = FFrameRate::TransformTime\(FFrameTime\(Start \+ Length\),/u);
    expect(code).toMatch(/\} else \{\s*NewSection->SetRange\(TRange<FFrameNumber>\(Start, End\)\);/u);
  });

  it('places the section again for a range other than the sound\'s, so its row fits and nothing overlaps', () => {
    const code = source();

    expect(code).toMatch(/if \(Start \+ Length != SoundEnd\) \{\s*NewSection->InitialPlacementOnRow\(Track->GetAllSections\(\), Start, Length\.Value, INDEX_NONE\);/u);
  });

  it('gives a zero-length sound one second on every version, as 5.3 and later already do', () => {
    const code = source();
    const oneSecond = code.indexOf('AsFrameNumber(1.0)');

    expect(oneSecond).toBeGreaterThan(code.indexOf('SoundEnd = NewSection->GetExclusiveEndFrame()'));
    expect(oneSecond).toBeLessThan(code.indexOf('InitialPlacementOnRow('));
    expect(code.slice(code.indexOf('Start < SoundEnd'), oneSecond)).toContain('SoundEnd - Start');
  });

  it('names the sound in the reply', () => {
    const code = source();

    expect(code).toMatch(/SetStringField\(TEXT\("soundPath"\), Sound->GetPathName\(\)\)/u);
    expect(code).toMatch(/SetStringField\(TEXT\("soundName"\), Sound->GetName\(\)\)/u);
  });

  it('adds an unbound Audio track on 5.0 and 5.1 too, through AddMasterTrack', () => {
    const code = stripComments(readFileSync(TRACK_CREATION, 'utf8'));

    expect(code).toContain('NewTrack = MCP_ADD_MOVIESCENE_TRACK(MovieScene, TrackClass);');
    expect(code).not.toContain('NOT_SUPPORTED');
  });

  // Master tracks became root tracks in 5.2: 5.0 and 5.1 have only GetMasterTracks, AddMasterTrack and
  // RemoveMasterTrack, so a raw GetTracks/AddTrack(Class)/RemoveTrack on a UMovieScene fails there.
  it('reaches root tracks through the version macros only', () => {
    const compat = stripComments(readFileSync(join(PRIVATE, 'Core', 'Compatibility', 'McpVersionCompatibility.h'), 'utf8'));
    expect(compat).toMatch(/#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 2\s*#define MCP_GET_MOVIESCENE_TRACKS\(MovieScene\) \(MovieScene\)->GetTracks\(\)/u);
    expect(compat).toContain('#define MCP_ADD_MOVIESCENE_TRACK(MovieScene, TrackClass) (MovieScene)->AddMasterTrack(TrackClass)');
    expect(compat).toContain('(MovieScene)->RemoveMasterTrack(Track) || (MovieScene)->RemoveTrack(Track)');
    const sequenceDir = join(PRIVATE, 'Domains', 'Sequence');
    const offenders = readdirSync(sequenceDir, { recursive: true })
      .map(String)
      .filter((file) => file.endsWith('.cpp'))
      .filter((file) => /MovieScene->(GetTracks\(\)|AddTrack\(TrackClass\)|RemoveTrack\()/u.test(stripComments(readFileSync(join(SEQUENCE_DIR, file), 'utf8'))));
    expect(offenders).toEqual([]);
  });

  it('is discoverable: soundPath is declared, trackType says Audio, music and cutscene sound are topics', () => {
    expect(property('soundPath').description).toMatch(/^Audio track only/u);
    expect(property('trackType').description).toContain('"Audio" (music or sound');
    const { topics } = property('soundPath');
    expect(topics).toContain('add music to sequence');
    expect(topics).toContain('add sound to cutscene');
  });
});
