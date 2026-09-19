import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import fs from 'node:fs/promises';
import { mkdtempSync, rmSync, symlinkSync, writeFileSync } from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { readOutputLog } from './log-reader.js';

/**
 * Can this host create a FILE symlink at all?
 *
 * Windows refuses one without Developer Mode or elevation. The two symlink-escape
 * tests below used to swallow that failure and `return`, so on every Windows run
 * two SECURITY assertions reported green having tested nothing. Probing once and
 * gating with `it.runIf` makes the gap show up as a skip instead.
 */
const SYMLINKS_AVAILABLE = ((): boolean => {
  const probe = mkdtempSync(path.join(os.tmpdir(), 'ue-mcp-symlink-probe-'));
  try {
    writeFileSync(path.join(probe, 'target'), '');
    symlinkSync(path.join(probe, 'target'), path.join(probe, 'link'));
    return true;
  } catch {
    return false;
  } finally {
    rmSync(probe, { recursive: true, force: true });
  }
})();

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

  it.runIf(SYMLINKS_AVAILABLE)('does not follow log-directory symlinks outside the project', async () => {
    const outsideLog = path.join(tmpDir, 'secret.log');
    const linkedLog = path.join(tmpDir, 'Project', 'Saved', 'Logs', 'linked.log');
    await fs.writeFile(outsideLog, 'Secret: Log: hidden message\n', 'utf8');
    await fs.symlink(outsideLog, linkedLog);

    const result = await readOutputLog({ logPath: linkedLog, lines: 5 });

    expect(result.success).toBe(false);
    expect(JSON.stringify(result)).not.toContain('hidden message');
  });

  it.runIf(SYMLINKS_AVAILABLE)('does not auto-discover symlinked log files outside the project', async () => {
    const outsideLog = path.join(tmpDir, 'secret.log');
    const linkedLog = path.join(tmpDir, 'Project', 'Saved', 'Logs', 'latest.log');
    await fs.writeFile(outsideLog, 'Secret: Log: hidden message\n', 'utf8');
    await fs.symlink(outsideLog, linkedLog);

    const result = await readOutputLog({ lines: 5 });

    expect(result.success).toBe(false);
    expect(JSON.stringify(result)).not.toContain('hidden message');
  });
});
