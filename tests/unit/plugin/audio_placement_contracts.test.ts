// Wiring contracts for sounds the MCP places in a level and for the attenuation extents it writes. Both need an
// editor to run, so these read the C++ as text; behaviour belongs to tests/mcp-tools/utility/manage-audio.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const DOMAINS = join('plugins', 'EngineWire', 'Source', 'EngineWire', 'Private', 'Domains');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (file: string): string =>
  readFileSync(join(DOMAINS, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('placed sounds play when the game starts', () => {
  // bIsActive is transient, so the editor-side Activate is never saved: only bAutoActivate reaches the game.
  it('an ambient sound keeps bAutoActivate on', () => {
    const ambient = read('Audio/McpAudioAmbient.cpp');
    expect(ambient).not.toContain('bAutoActivate = false');
    expect(ambient).toMatch(/AudioComp->bAutoActivate = true;\s*AudioComp->Activate\(true\);/u);
  });

  it('an audio component takes bAutoActivate from autoPlay', () => {
    const component = read('Audio/McpAudioComponentsAndFades.cpp');
    expect(component).toContain('AudioComp->bAutoActivate = GetJsonBoolField(Payload, TEXT("autoPlay"), true);');
    expect(component).toMatch(/if \(AudioComp->bAutoActivate\)\s*AudioComp->Activate\(true\);/u);
  });
});

describe('innerRadius fills the extents the attenuation shape reads', () => {
  it('sets a Box on every axis and a Capsule radius in Y', () => {
    const helper = read('AudioAuthoring/McpAudioAuthoringPrivate.h');
    expect(helper).toContain('inline void SetAttenuationInnerRadius(FBaseAttenuationSettings& Settings, double Radius)');
    expect(helper).toMatch(/EAttenuationShape::Box\) \{ Extents = FVector\(Radius\); \}/u);
    expect(helper).toMatch(/EAttenuationShape::Capsule\) \{ Extents\.Y = Radius;/u);
  });

  it('is the only writer of the extents in the three attenuation handlers', () => {
    const preset = read('Audio/McpAudioOcclusionAndAttenuation.cpp');
    const authoring = read('AudioAuthoring/McpAudioAttenuation.cpp');
    expect(preset).not.toContain('AttenuationShapeExtents');
    expect(authoring).not.toContain('AttenuationShapeExtents');
    expect(authoring.match(/SetAttenuationInnerRadius\(/gu)).toHaveLength(2);
    // The preset sets the shape first, because the shape decides which axes the radius fills.
    expect(preset.indexOf('McpAudioAuthoring::SetAttenuationInnerRadius(Atten->Attenuation, InnerRadius)'))
      .toBeGreaterThan(preset.lastIndexOf('EAttenuationShape::Sphere'));
  });

  // A box zone for a long hall had to be a cube as wide and tall as the hall was long.
  it('extent sizes a box per axis, checked before anything is made or changed and written after innerRadius', () => {
    const helper = read('AudioAuthoring/McpAudioAuthoringPrivate.h');
    expect(helper).toContain('inline bool ReadAttenuationBoxExtent(const TSharedPtr<FJsonObject>& Params, bool bBox, TOptional<FVector>& Out, FString& OutError)');
    expect(helper).toContain('Size.GetMin() <= 0.0');
    const preset = read('Audio/McpAudioOcclusionAndAttenuation.cpp');
    expect(preset.indexOf('McpAudioAuthoring::ReadAttenuationBoxExtent(')).toBeLessThan(preset.indexOf('CreatePackage(*FullPath)'));
    expect(preset.indexOf('McpAudioAuthoring::ApplyAttenuationBoxExtent(Atten->Attenuation, BoxExtent)'))
      .toBeGreaterThan(preset.indexOf('McpAudioAuthoring::SetAttenuationInnerRadius(Atten->Attenuation, InnerRadius)'));
    const authoring = read('AudioAuthoring/McpAudioAttenuation.cpp');
    expect(authoring.indexOf('ReadAttenuationBoxExtent(Params,'))
      .toBeLessThan(authoring.indexOf('Atten->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear'));
    expect(authoring).toContain('AddAttenuationBoxExtent(Response, Atten->Attenuation);');
    expect(preset, 'the preset reply reads the size back').toContain('Resp->SetNumberField(TEXT("innerRadius"), McpAudioAuthoring::GetAttenuationInnerRadius(Atten->Attenuation));');
  });
});

// A distant engine was muffled by writing Attenuation.bAttenuateWithLPF and the LPF fields through set_property.
describe('distance attenuation sets air absorption', () => {
  it('reads the four lpf values and turns the filter on when one is sent', () => {
    const authoring = read('AudioAuthoring/McpAudioAttenuation.cpp');
    for (const [key, field] of [['lpfRadiusMin', 'LPFRadiusMin'], ['lpfRadiusMax', 'LPFRadiusMax'],
      ['lpfFrequencyAtMin', 'LPFFrequencyAtMin'], ['lpfFrequencyAtMax', 'LPFFrequencyAtMax']]) {
      expect(authoring).toContain(`{TEXT("${key}"), &Atten->Attenuation.${field}}`);
    }
    expect(authoring).toContain('if (bLowPassSent || Params->HasField(TEXT("attenuateWithLPF"))) { Atten->Attenuation.bAttenuateWithLPF = GetJsonBoolField(Params, TEXT("attenuateWithLPF"), true); }');
  });

  // It answered only "Operation complete", while occlusion, reverb send and spatialization echo what they set.
  it('reads back what it set, the inner radius from the axis the shape keeps it on', () => {
    const authoring = read('AudioAuthoring/McpAudioAttenuation.cpp');
    const distance = authoring.slice(authoring.indexOf('"configure_distance_attenuation"'), authoring.indexOf('"configure_spatialization"'));
    expect(distance).toContain('Response->SetNumberField(TEXT("innerRadius"), GetAttenuationInnerRadius(Atten->Attenuation));');
    expect(distance).toContain('Response->SetNumberField(TEXT("falloffDistance"), Atten->Attenuation.FalloffDistance);');
    expect(distance).toContain('for (const TPair<const TCHAR*, float*>& Field : LowPass) { Response->SetNumberField(Field.Key, *Field.Value); }');
    expect(read('AudioAuthoring/McpAudioAuthoringPrivate.h'))
      .toContain('return Settings.AttenuationShape == EAttenuationShape::Capsule ? Settings.AttenuationShapeExtents.Y : Settings.AttenuationShapeExtents.X;');
  });
});
