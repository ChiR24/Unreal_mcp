import { readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join, resolve } from 'node:path';

import { afterEach, describe, expect, it } from 'vitest';
import { z } from 'zod';

import {
  buildManifest,
  serializeManifest,
  sha256File,
} from '../../scripts/lib/package-manifest.mjs';

const fixturePath = join(tmpdir(), `unreal-mcp-package-manifest-${process.pid}.zip`);
const pluginDescriptorSchema = z.object({ VersionName: z.string() });

afterEach(() => {
  rmSync(fixturePath, { force: true });
});

describe('plugin package manifest', () => {
  it('hashes an archive with lowercase SHA-256', async () => {
    // Given
    writeFileSync(fixturePath, 'abc', { mode: 0o600 });

    // When
    const sha256 = await sha256File(fixturePath);

    // Then
    expect(sha256).toBe('ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad');
    expect(sha256).toMatch(/^[a-f0-9]{64}$/);
  });

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
    expect(pluginDescriptor.VersionName).toBe('0.5.30');
    expect(Object.keys(manifest)).toEqual([...Object.keys(manifest)].sort());
    expect(Object.keys(manifest.archives[0] ?? {})).toEqual(['filename', 'sha256']);
    expect(manifest.archives).toEqual([
      {
        filename: 'McpAutomationBridge-v0.5.30-UE5.7-Linux.zip',
        sha256: 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad',
      },
    ]);
    expect(manifest.version).toBe('0.5.30');
    expect(manifest.generatedAt).toMatch(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$/);
    expect(serializeManifest(manifest)).toBe(`${JSON.stringify(manifest, null, 2)}\n`);
  });

  it('wires Linux and Windows packaging through the shared helper', () => {
    // Given
    const scripts = ['scripts/package-plugin.sh', 'scripts/package-plugin.bat'].map((path) =>
      readFileSync(resolve(process.cwd(), path), 'utf8'),
    );

    // When
    const helperInvocations = scripts.map((script) =>
      script.replaceAll('\\', '/').includes('lib/package-manifest.mjs'),
    );

    // Then
    expect(helperInvocations).toEqual([true, true]);
    for (const script of scripts) {
      expect(script).toContain('MANIFEST_PATH');
      expect(script).toContain('PLUGIN_VER');
      expect(script).toContain('ZIP_PATH');
    }
  });
});
