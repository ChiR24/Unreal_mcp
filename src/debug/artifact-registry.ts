import { createHash, randomUUID } from 'node:crypto';
import { promises as fs, type Dirent } from 'node:fs';
import path from 'node:path';
import type { DebugArtifactRecord } from './types.js';

const WARNING_BYTES = 5 * 1024 * 1024 * 1024;

export class DebugArtifactRegistry {
  private readonly artifacts = new Map<string, DebugArtifactRecord>();
  private diskBytes = 0;
  readonly root: string;

  constructor(projectPath = process.env.UE_PROJECT_PATH ?? process.cwd()) {
    const projectRoot = path.extname(projectPath).toLowerCase() === '.uproject'
      ? path.dirname(path.resolve(projectPath))
      : path.resolve(projectPath);
    this.root = path.join(projectRoot, 'Saved', 'McpDebug');
    void this.refreshDiskUsage().catch(() => undefined);
  }

  async createJson(
    sessionId: string,
    kind: string,
    fileName: string,
    value: unknown,
    metadata?: Record<string, unknown>
  ): Promise<DebugArtifactRecord> {
    const sessionRoot = path.join(this.root, this.safeSegment(sessionId));
    await fs.mkdir(sessionRoot, { recursive: true });
    const absolutePath = path.join(sessionRoot, this.safeSegment(fileName));
    await fs.writeFile(absolutePath, `${JSON.stringify(value, null, 2)}\n`, 'utf8');
    return this.registerFile(absolutePath, kind, sessionId, metadata);
  }

  async registerFile(
    absolutePath: string,
    kind: string,
    sessionId?: string,
    metadata?: Record<string, unknown>
  ): Promise<DebugArtifactRecord> {
    const resolved = path.resolve(absolutePath);
    const data = await fs.readFile(resolved);
    const artifact: DebugArtifactRecord = {
      artifactId: randomUUID(),
      kind,
      absolutePath: resolved,
      size: data.byteLength,
      sha256: createHash('sha256').update(data).digest('hex'),
      createdAt: new Date().toISOString(),
      ...(sessionId ? { sessionId } : {}),
      ...(metadata ? { metadata } : {})
    };
    this.artifacts.set(artifact.artifactId, artifact);
    void this.refreshDiskUsage().catch(() => undefined);
    return { ...artifact };
  }

  get(artifactId: string): DebugArtifactRecord | undefined {
    const artifact = this.artifacts.get(artifactId);
    return artifact ? { ...artifact } : undefined;
  }

  list(sessionId?: string): DebugArtifactRecord[] {
    return Array.from(this.artifacts.values())
      .filter((artifact) => !sessionId || artifact.sessionId === sessionId)
      .map((artifact) => ({ ...artifact }));
  }

  health(): { root: string; artifactCount: number; totalBytes: number; warning: string | null } {
    const registeredBytes = Array.from(this.artifacts.values()).reduce((sum, artifact) => sum + artifact.size, 0);
    const totalBytes = Math.max(registeredBytes, this.diskBytes);
    return {
      root: this.root,
      artifactCount: this.artifacts.size,
      totalBytes,
      warning: totalBytes >= WARNING_BYTES
        ? 'Debug artifacts exceed 5 GiB. They are retained until explicitly removed by an authorized operator.'
        : null
    };
  }

  async refreshDiskUsage(): Promise<number> {
    const measure = async (directory: string): Promise<number> => {
      let entries: Dirent[];
      try { entries = await fs.readdir(directory, { withFileTypes: true }); } catch { return 0; }
      let bytes = 0;
      for (const entry of entries) {
        const entryPath = path.join(directory, entry.name);
        if (entry.isDirectory()) bytes += await measure(entryPath);
        else if (entry.isFile()) {
          try { bytes += (await fs.stat(entryPath)).size; } catch { /* artifact may be moving */ }
        }
      }
      return bytes;
    };
    this.diskBytes = await measure(this.root);
    return this.diskBytes;
  }

  private safeSegment(value: string): string {
    const safe = value.replace(/[^A-Za-z0-9._-]/g, '_');
    if (!safe || safe === '.' || safe === '..') throw new Error('Invalid artifact path segment');
    return safe;
  }
}
