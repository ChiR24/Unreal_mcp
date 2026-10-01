// What create_light says it takes, and what it says it did. Wiring contracts only; the behaviour needs an editor.
//
// build_environment.create_light searched poorly ("add spot light intensity color" listed no light capability on the
// native door), its summary named no type and no property, and its reply was "Light spawned" whether or not a single
// setting had taken. Three records create lights; the contracts keep their words and the C++ in step.

import { readFileSync } from 'node:fs';
import { join } from 'node:path';

import { describe, expect, it } from 'vitest';

import {
  LIGHT_COLOR_TEXT,
  LIGHT_PROPERTIES_TEXT,
  LIGHT_PROPERTY_KEYS,
} from '../../../src/tools/catalog/capabilities/records/shared/light-text.js';

const LIGHTING = join('plugins', 'McpAutomationBridge', 'Source', 'McpAutomationBridge', 'Private', 'Domains', 'Lighting');
const strip = (text: string): string => text.replace(/\/\*[\s\S]*?\*\//gu, ' ').replace(/\/\/[^\n]*/gu, ' ');
const cpp = (file: string): string => strip(readFileSync(join(LIGHTING, file), 'utf8'));

interface Rec {
  readonly id: string;
  readonly discovery: { readonly summary: string; readonly topics: readonly string[]; readonly whenNotToUse: readonly string[] };
  readonly schemas: { readonly input: { readonly properties: Record<string, { readonly description?: string }> } };
}
const registry = JSON.parse(
  readFileSync(join('src', 'tools', 'catalog', 'capabilities', 'generated', 'canonical-registry.generated.json'), 'utf8'),
) as { readonly records: readonly Rec[] };
const record = (id: string): Rec => {
  const found = registry.records.find((entry) => entry.id === id);
  if (found === undefined) throw new Error(`no record ${id}`);
  return found;
};

describe('create_light names every setting the C++ reads', () => {
  const properties = cpp('McpAutomationBridge_LightingHandlersLightProperties.cpp');
  const known = [...(/Known\[\] = \{([^}]*)\}/u.exec(properties)?.[1] ?? '').matchAll(/TEXT\("(\w+)"\)/gu)].map((match) => match[1]);

  it('has a list of the keys it accepts, and no other key is read', () => {
    expect(known.length).toBeGreaterThan(5);
    const read = [...properties.matchAll(/(?:ReadNumber|HasField|TryGetBoolField|Fits\([^,]+,)\s*\(?TEXT\("(\w+)"\)/gu)].map((match) => match[1]);
    expect([...new Set(read)].filter((key) => !known.includes(key ?? '')).sort()).toEqual([]);
  });

  it('is the list the records document, in both directions', () => {
    expect([...known].sort()).toEqual([...LIGHT_PROPERTY_KEYS].sort());
    for (const key of LIGHT_PROPERTY_KEYS) expect(LIGHT_PROPERTIES_TEXT, key).toContain(key);
  });

  it('puts the same words on the two records that run the handler', () => {
    for (const id of ['build_environment.create_light', 'manage_level.create_light']) {
      const input = record(id).schemas.input.properties;
      expect(input.properties?.description, id).toBe(LIGHT_PROPERTIES_TEXT);
      expect(input.lightType?.description, id).toMatch(/point, spot, directional, rect or sky/u);
      expect(input.color?.description, id).toContain(LIGHT_COLOR_TEXT);
    }
  });

  it('says a color is linear and stored as sRGB, with the worked example', () => {
    expect(LIGHT_COLOR_TEXT).toContain('Linear 0-1');
    expect(LIGHT_COLOR_TEXT).toContain('sRGB');
    expect(LIGHT_COLOR_TEXT).toContain('(255, 218, 173)');
    expect(record('manage_effect.create_dynamic_light').schemas.input.properties.color?.description).toContain(LIGHT_COLOR_TEXT);
  });
});

describe('create_light says what it did', () => {
  const spawn = cpp('McpAutomationBridge_LightingHandlersLightSpawn.cpp');
  const properties = cpp('McpAutomationBridge_LightingHandlersLightProperties.cpp');

  it('lists what was applied, refused and adjusted, and says so in the message', () => {
    expect(spawn).toContain('Say(TEXT("applied"), Report.Applied);');
    expect(spawn).toContain('Say(TEXT("refused"), Report.Refused);');
    expect(spawn).toContain('Say(TEXT("adjusted"), Report.Adjusted);');
    expect(spawn).toContain('Subsystem.SendAutomationResponse(RequestingSocket, RequestId, true, Message, Resp);');
    expect(spawn).not.toContain('TEXT("Light spawned"), Resp)');
  });

  it('records the top-level intensity and color too, on both light kinds', () => {
    expect(spawn.match(/Report\.Applied\.AddUnique\(TEXT\("intensity"\)\)/gu)).toHaveLength(2);
    expect(spawn.match(/Report\.Applied\.AddUnique\(TEXT\("color"\)\)/gu)).toHaveLength(2);
  });

  it('refuses a key that is unknown, does not fit the light, or has the wrong type, by name', () => {
    expect(properties).toContain('not a light property this action reads');
    expect(properties).toContain('a sky light takes only the top-level intensity and color');
    expect(properties).toContain('only a spot light has a cone angle');
    expect(properties).toContain('only a rect light has a source size');
    expect(properties).toContain('only a directional light can be the atmosphere sun light');
    expect(properties).toContain('must be a number');
    expect(properties).toContain('must be true or false');
  });

  it('reports a value it replaced for being out of range, instead of logging it alone', () => {
    expect(properties).toMatch(/Report\.Adjusted\.Add\(FString::Printf\(TEXT\("%s: %s must be %s; %s was used"\)/u);
  });
});

describe('the three light records point at each other', () => {
  const when = (id: string): string => record(id).discovery.whenNotToUse.join(' ');

  it('send a caller to the one that fits: PIE to the dynamic light, everything else to create_light', () => {
    expect(when('build_environment.create_light')).toContain('manage_effect.create_dynamic_light');
    expect(when('build_environment.create_light')).toContain('manage_level.create_light');
    expect(when('manage_level.create_light')).toContain('build_environment.create_light');
    expect(when('manage_effect.create_dynamic_light')).toContain('build_environment.create_light');
  });

  it('names the light types and the settings in the line a search shows', () => {
    const summary = record('build_environment.create_light').discovery.summary;
    for (const word of ['point', 'spot', 'directional', 'rect', 'sky', 'intensity', 'color']) expect(summary).toContain(word);
  });

  it('is found by what a caller types', () => {
    const topics = record('build_environment.create_light').discovery.topics;
    for (const phrase of ['add spot light', 'add point light', 'street lamp light', 'light intensity', 'light color']) {
      expect(topics).toContain(phrase);
    }
  });
});
