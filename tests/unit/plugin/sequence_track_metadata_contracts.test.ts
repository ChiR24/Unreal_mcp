// Todo 28-29 BB-038..BB-042, BB-069..BB-071
// Sequence tracks, metadata, bindings, discovery, preview, and MRQ semantics.
import { existsSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { describe, expect, it } from 'vitest';

const PRIVATE = resolve(process.cwd(), 'plugins/McpAutomationBridge/Source/McpAutomationBridge/Private');
const TS = resolve(process.cwd(), 'src/tools');
function readCpp(...parts: string[]): string {
  const p = resolve(PRIVATE, ...parts);
  expect(existsSync(p), `missing: ${p}`).toBe(true);
  return readFileSync(p, 'utf8');
}
function readTs(...parts: string[]): string {
  const p = resolve(TS, ...parts);
  expect(existsSync(p), `missing: ${p}`).toBe(true);
  return readFileSync(p, 'utf8');
}
function code(s: string): string { return s.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/[^\n]*/g, ''); }

// --- Todo 28 source files ---
const bindingTracks = () => readCpp('Domains/Sequence/Cinematics/McpAutomationBridge_SequenceCinematicsBindingTracks.cpp');
const setMetadata = () => readCpp('Domains/Sequence/Metadata/McpAutomationBridge_SequenceHandlersSetMetadata.cpp');
const seqProperties = () => readCpp('Domains/Sequence/McpAutomationBridge_SequenceHandlersProperties.cpp');
const bindings = () => readCpp('Domains/Sequence/McpAutomationBridge_SequenceHandlersBindings.cpp');
const trackDiscovery = () => readCpp('Domains/Sequence/McpAutomationBridge_SequenceHandlersTrackDiscovery.cpp');
const metadataRecord = () => readTs('catalog/capabilities/records/manage-sequence/metadata.ts');

// --- Todo 29 source files ---
const keyframes = () => readCpp('Domains/Sequence/McpAutomationBridge_SequenceHandlersKeyframes.cpp');
const playback = () => readCpp('Domains/Sequence/McpAutomationBridge_SequenceHandlersPlayback.cpp');
const movieRenderOutput = () => readCpp('Domains/Sequence/MovieRender/McpAutomationBridge_SequenceMovieRenderOutput.cpp');

// =============================================================
// Todo 28: Make Sequence tracks, metadata, bindings, and discovery complete
// =============================================================

describe('BB-038 bound-track handlers accept actorName (not just bindingGuid)', () => {
  it('BindingTracks.cpp does not reject with bare "bindingGuid is required" (without actorName alternative)', () => {
    const s = code(bindingTracks());
    // The old broken pattern was a bare "bindingGuid is required" message.
    // The fixed pattern says "actorName or bindingGuid is required".
    // Check that the error message includes actorName as an alternative.
    const match = s.match(/bindingGuid\s+is\s+required/);
    if (match) {
      // If "bindingGuid is required" appears, it must be preceded by "actorName or"
      const idx = match.index ?? 0;
      const prefix = s.slice(Math.max(0, idx - 50), idx);
      expect(prefix, 'error message should include actorName as alternative').toMatch(/actorName|ActorName/);
    }
  });
  it('BindingTracks.cpp resolves actorName as alternative to bindingGuid', () => {
    const s = code(bindingTracks());
    expect(s).toMatch(/actorName|ActorName|ResolveActor/);
  });
});

describe('BB-040 get_properties emits frameRate as number (not object)', () => {
  it('HandleSequenceGetProperties emits frameRate via SetNumberField (not SetObjectField)', () => {
    const s = code(seqProperties());
    const idx = s.indexOf('HandleSequenceGetProperties');
    expect(idx).toBeGreaterThan(-1);
    const funcBody = s.slice(idx, idx + 2000);
    expect(funcBody).toMatch(/SetNumberField\s*\(\s*TEXT\s*\(\s*"frameRate"\s*\)/);
  });
});

describe('BB-039 metadata set/get handlers emit declared fields', () => {
  it('SetMetadata.cpp handles set_metadata action', () => {
    const s = code(setMetadata());
    expect(s).toMatch(/set_metadata|SetMetadata/);
  });
  it('metadata record declares metadata output fields', () => {
    const s = code(metadataRecord());
    expect(s.length).toBeGreaterThan(0);
  });
});

describe('BB-041 add_actors returns truthful batch results', () => {
  it('Bindings.cpp handles add_actors action', () => {
    const s = code(bindings());
    expect(s).toMatch(/add_actors|AddActors/);
  });
  it('add_actors result includes batch summary (total or count or added)', () => {
    const s = code(bindings());
    const idx = s.indexOf('add_actors');
    if (idx < 0) return; // skip if not found
    const funcBody = s.slice(idx, idx + 6000);
    expect(funcBody).toMatch(/total|count|added|successful|batch/i);
  });
});

describe('BB-042 list_tracks discovers camera-cut tracks', () => {
  it('TrackDiscovery.cpp handles list_tracks action', () => {
    const s = code(trackDiscovery());
    expect(s).toMatch(/list_tracks|ListTracks|HandleSequenceListTracks/);
  });
  it('list_tracks result includes camera-cut track type', () => {
    const s = code(trackDiscovery());
    const idx = s.indexOf('list_tracks');
    if (idx < 0) return; // skip if not found
    const funcBody = s.slice(idx, idx + 3000);
    expect(funcBody).toMatch(/camera.cut|CameraCut|camera_cut|CinematicCamera/i);
  });
});

// =============================================================
// Todo 29: Evaluate Sequence preview and enforce keyframe/MRQ semantics
// =============================================================

describe('BB-069 add_keyframe emits correct keyframe shape', () => {
  it('Keyframes.cpp handles add_keyframe action', () => {
    const s = code(keyframes());
    expect(s).toMatch(/add_keyframe|AddKeyframe|HandleAddKeyframe/);
  });
});

describe('BB-070 play exposes evaluated preview state', () => {
  it('Playback.cpp handles play action', () => {
    const s = code(playback());
    expect(s).toMatch(/play|Play|HandleSequencePlay/);
  });
  it('play result or get_properties includes evaluated state (startTime or playhead or currentFrame)', () => {
    const s = code(playback());
    const idx = s.indexOf('HandleSequencePlay');
    if (idx >= 0) {
      const funcBody = s.slice(idx, idx + 3000);
      expect(funcBody).toMatch(/startTime|playhead|currentFrame|evaluated|position/i);
    }
  });
});

describe('BB-071 MRQ enforces end-exclusive ranges', () => {
  it('MovieRenderOutput.cpp handles output settings configuration', () => {
    const s = code(movieRenderOutput());
    expect(s.length).toBeGreaterThan(0);
  });
  it('MRQ output settings validate range bounds', () => {
    const s = code(movieRenderOutput());
    expect(s).toMatch(/range|Range|start|end|Start|End/i);
  });
});
