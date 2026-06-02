import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { readOutputLog } from './log-reader.js';

describe('readOutputLog path safety', () => {
  let tmpDir: string;
  let originalProjectPath: string | undefined;

  beforeEach(async () => {
    tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), 'ue-mcp-log-test-'));
    originalProjectPath = process.env.UE_PROJECT_PATH;
    process.env.UE_PROJECT_PATH = path.join(tmpDir, 'Project', 'TestProject.uproject');
    await fs.mkdir(path.join(tmpDir, 'Project', 'Saved', 'Logs'), { recursive: true });
  });

  afterEach(async () => {
    if (originalProjectPath === undefined) {
      delete process.env.UE_PROJECT_PATH;
    } else {
      process.env.UE_PROJECT_PATH = originalProjectPath;
    }
    await fs.rm(tmpDir, { recursive: true, force: true });
  });

  it('reads real log files under the project log directory', async () => {
    const logPath = path.join(tmpDir, 'Project', 'Saved', 'Logs', 'Project.log');
    await fs.writeFile(logPath, 'LogMcp: Log: visible message\n', 'utf8');

    const result = await readOutputLog({ logPath, lines: 5 });

    expect(result.success).toBe(true);
    expect(result.entries).toEqual([
      { category: 'LogMcp', level: 'Log', message: 'visible message' }
    ]);
  });

  it('discovers logs when UE_PROJECT_PATH points at the project directory', async () => {
    process.env.UE_PROJECT_PATH = path.join(tmpDir, 'Project');
    const logPath = path.join(tmpDir, 'Project', 'Saved', 'Logs', 'Project.log');
    await fs.writeFile(logPath, 'LogMcp: Log: project directory message\n', 'utf8');

    const result = await readOutputLog({ lines: 5 });

    expect(result.success).toBe(true);
    expect(result.entries).toEqual([
      { category: 'LogMcp', level: 'Log', message: 'project directory message' }
    ]);
  });

  it('does not reuse a cached log path after UE_PROJECT_PATH changes', async () => {
    const firstLogPath = path.join(tmpDir, 'Project', 'Saved', 'Logs', 'Project.log');
    await fs.writeFile(firstLogPath, 'LogMcp: Log: first project message\n', 'utf8');
    await expect(readOutputLog({ logPath: firstLogPath, lines: 5 })).resolves.toMatchObject({ success: true });

    const secondProjectDir = path.join(tmpDir, 'SecondProject');
    process.env.UE_PROJECT_PATH = path.join(secondProjectDir, 'SecondProject.uproject');
    await fs.mkdir(path.join(secondProjectDir, 'Saved', 'Logs'), { recursive: true });
    await fs.writeFile(path.join(secondProjectDir, 'Saved', 'Logs', 'SecondProject.log'), 'LogMcp: Log: second project message\n', 'utf8');

    const result = await readOutputLog({ lines: 5 });

    expect(result.success).toBe(true);
    expect(JSON.stringify(result)).not.toContain('first project message');
    expect(result.entries).toEqual([
      { category: 'LogMcp', level: 'Log', message: 'second project message' }
    ]);
  });

  it('does not follow log-directory symlinks outside the project', async () => {
    const outsideLog = path.join(tmpDir, 'secret.log');
    const linkedLog = path.join(tmpDir, 'Project', 'Saved', 'Logs', 'linked.log');
    await fs.writeFile(outsideLog, 'Secret: Log: hidden message\n', 'utf8');

    try {
      await fs.symlink(outsideLog, linkedLog);
    } catch {
      return;
    }

    const result = await readOutputLog({ logPath: linkedLog, lines: 5 });

    expect(result.success).toBe(false);
    expect(JSON.stringify(result)).not.toContain('hidden message');
  });

  it('does not auto-discover symlinked log files outside the project', async () => {
    const outsideLog = path.join(tmpDir, 'secret.log');
    const linkedLog = path.join(tmpDir, 'Project', 'Saved', 'Logs', 'latest.log');
    await fs.writeFile(outsideLog, 'Secret: Log: hidden message\n', 'utf8');

    try {
      await fs.symlink(outsideLog, linkedLog);
    } catch {
      return;
    }

    const result = await readOutputLog({ lines: 5 });

    expect(result.success).toBe(false);
    expect(JSON.stringify(result)).not.toContain('hidden message');
  });
});
