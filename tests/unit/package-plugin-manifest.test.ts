import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';

import { describe, expect, it } from 'vitest';
import { z } from 'zod';

import {
  buildManifest,
  serializeManifest,
} from '../../scripts/package-plugin.mjs';

const pluginDescriptorSchema = z.object({ VersionName: z.string() });
// package.json is the canonical version source (see
// tests/unit/version-consistency.test.ts); comparing against a literal
// here just meant every release bump broke this test.
const CANONICAL_VERSION = JSON.parse(
  readFileSync(resolve(process.cwd(), 'package.json'), 'utf8'),
).version as string;

describe('plugin package manifest', () => {
  it('builds a stable sorted manifest with the descriptor version', () => {
    // Given
    const pluginDescriptor = pluginDescriptorSchema.parse(
      JSON.parse(
        readFileSync(
          resolve(process.cwd(), 'plugins/McpAutomationBridge/McpAutomationBridge.uplugin'),
          'utf8',
        ),
      ),
    );

    // When
    const manifest = buildManifest({
      archives: [
        {
          filename: 'McpAutomationBridge-v0.5.30-UE5.7-Linux.zip',
          sha256: 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad',
        },
      ],
      engineTarget: 'UE5.7-Linux',
      generatedAt: '2026-07-14T12:34:56.000Z',
      pluginName: 'McpAutomationBridge',
      ueRoot: '/opt/UnrealEngine/UE_5.7',
      version: pluginDescriptor.VersionName,
    });

    // Then
    expect(pluginDescriptor.VersionName).toBe(CANONICAL_VERSION);
    expect(Object.keys(manifest)).toEqual([...Object.keys(manifest)].sort());
    expect(Object.keys(manifest.archives[0] ?? {})).toEqual(['filename', 'sha256']);
    expect(manifest.archives).toEqual([
      {
        filename: 'McpAutomationBridge-v0.5.30-UE5.7-Linux.zip',
        sha256: 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad',
      },
    ]);
    expect(manifest.version).toBe(CANONICAL_VERSION);
    expect(manifest.generatedAt).toMatch(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$/);
    expect(serializeManifest(manifest)).toBe(`${JSON.stringify(manifest, null, 2)}\n`);
  });
});
