import { promises as fs } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { afterEach, describe, expect, it } from 'vitest';
import { DebugArtifactRegistry } from './artifact-registry.js';

const temporaryRoots: string[] = [];

afterEach(async () => {
  await Promise.all(temporaryRoots.splice(0).map((root) => fs.rm(root, { recursive: true, force: true })));
});

describe('DebugArtifactRegistry', () => {
  it('keeps large content on disk and returns verifiable metadata', async () => {
    const projectRoot = await fs.mkdtemp(path.join(os.tmpdir(), 'ue-mcp-artifacts-'));
    temporaryRoots.push(projectRoot);
    const registry = new DebugArtifactRegistry(projectRoot);
    const artifact = await registry.createJson('session-one', 'manifest', 'manifest.json', { status: 'failed' });

    expect(artifact.absolutePath).toContain(path.join('Saved', 'McpDebug', 'session-one'));
    expect(artifact.size).toBeGreaterThan(0);
    expect(artifact.sha256).toMatch(/^[a-f0-9]{64}$/);
    expect(await registry.refreshDiskUsage()).toBe(artifact.size);
    expect(registry.health().totalBytes).toBe(artifact.size);
  });
});
