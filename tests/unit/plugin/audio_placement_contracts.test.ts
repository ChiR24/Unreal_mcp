// Wiring contracts for sounds the MCP places in a level and for the attenuation extents it writes. Both need an
// editor to run, so these read the C++ as text; behaviour belongs to tests/mcp-tools/utility/manage-audio.test.mjs.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

const DOMAINS = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Domains');

/** Block and line comments removed, so no assertion can be satisfied by prose. */
const read = (file: string): string =>
  readFileSync(join(DOMAINS, file), 'utf8').replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');

describe('placed sounds play when the game starts', () => {
  // bIsActive is transient, so the editor-side Activate is never saved: only bAutoActivate reaches the game.
  it('an ambient sound keeps bAutoActivate on', () => {
    const ambient = read('Audio/McpAutomationBridge_AudioHandlersAmbient.cpp');
    expect(ambient).not.toContain('bAutoActivate = false');
    expect(ambient).toMatch(/AudioComp->bAutoActivate = true;\s*AudioComp->Activate\(true\);/u);
  });

  it('an audio component takes bAutoActivate from autoPlay', () => {
    const component = read('Audio/McpAutomationBridge_AudioHandlersComponentsAndFades.cpp');
    expect(component).toContain('AudioComp->bAutoActivate = GetJsonBoolField(Payload, TEXT("autoPlay"), true);');
    expect(component).toMatch(/if \(AudioComp->bAutoActivate\)\s*AudioComp->Activate\(true\);/u);
  });
});

describe('innerRadius fills the extents the attenuation shape reads', () => {
  it('sets a Box on every axis and a Capsule radius in Y', () => {
    const helper = read('AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersPrivate.h');
    expect(helper).toContain('inline void SetAttenuationInnerRadius(FBaseAttenuationSettings& Settings, double Radius)');
    expect(helper).toMatch(/EAttenuationShape::Box\) \{ Extents = FVector\(Radius\); \}/u);
    expect(helper).toMatch(/EAttenuationShape::Capsule\) \{ Extents\.Y = Radius;/u);
  });

  it('is the only writer of the extents in the three attenuation handlers', () => {
    const preset = read('Audio/McpAutomationBridge_AudioHandlersOcclusionAndAttenuation.cpp');
    const authoring = read('AudioAuthoring/McpAutomationBridge_AudioAuthoringHandlersAttenuation.cpp');
    expect(preset).not.toContain('AttenuationShapeExtents');
    expect(authoring).not.toContain('AttenuationShapeExtents');
    expect(authoring.match(/SetAttenuationInnerRadius\(/gu)).toHaveLength(2);
    // The preset sets the shape first, because the shape decides which axes the radius fills.
    expect(preset.indexOf('McpAudioAuthoring::SetAttenuationInnerRadius(Atten->Attenuation, InnerRadius)'))
      .toBeGreaterThan(preset.lastIndexOf('EAttenuationShape::Sphere'));
  });
});
