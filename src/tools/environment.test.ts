import fs from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { describe, it, expect } from 'vitest';
import { exportEnvironmentSnapshot, importEnvironmentSnapshot, validateSnapshotPath } from '../utils/environment-snapshot.js';

describe('validateSnapshotPath Security', () => {
  describe('Windows absolute paths', () => {
    it('should reject Windows drive letter paths (X:\\etc\\passwd)', () => {
      const result = validateSnapshotPath('X:\\etc\\passwd\\test.json');
      expect(result.isValid).toBe(false);
      if (!result.isValid) {
        expect(result.error).toContain('SECURITY_VIOLATION');
        expect(result.error).toContain('drive letters');
      }
    });

    it('should reject Windows drive letter paths with forward slashes (C:/Windows)', () => {
      const result = validateSnapshotPath('C:/Windows/System32');
      expect(result.isValid).toBe(false);
      if (!result.isValid) {
        expect(result.error).toContain('SECURITY_VIOLATION');
      }
    });

    it('should reject paths containing colon anywhere', () => {
      const result = validateSnapshotPath('path:with:colons');
      expect(result.isValid).toBe(false);
      if (!result.isValid) {
        expect(result.error).toContain('SECURITY_VIOLATION');
      }
    });
  });

  describe('Unix system paths', () => {
    it('should reject /etc/passwd paths', () => {
      const result = validateSnapshotPath('/etc/passwd');
      expect(result.isValid).toBe(false);
      if (!result.isValid) {
        expect(result.error).toContain('SECURITY_VIOLATION');
        expect(result.error).toContain('System directory');
      }
    });

    it('should reject /var/log paths', () => {
      const result = validateSnapshotPath('/var/log/test.log');
      expect(result.isValid).toBe(false);
      if (!result.isValid) {
        expect(result.error).toContain('SECURITY_VIOLATION');
      }
    });

    it('should reject /usr/bin paths', () => {
      const result = validateSnapshotPath('/usr/bin/test');
      expect(result.isValid).toBe(false);
      if (!result.isValid) {
        expect(result.error).toContain('SECURITY_VIOLATION');
      }
    });

    it('should reject /tmp paths (security risk)', () => {
      const result = validateSnapshotPath('/tmp/snapshot.json');
      expect(result.isValid).toBe(false);
      if (!result.isValid) {
        expect(result.error).toContain('SECURITY_VIOLATION');
        expect(result.error).toContain('System directory');
      }
    });

    it('should reject exact /tmp path', () => {
      const result = validateSnapshotPath('/tmp');
      expect(result.isValid).toBe(false);
      if (!result.isValid) {
        expect(result.error).toContain('SECURITY_VIOLATION');
      }
    });

    it('should reject arbitrary absolute paths', () => {
      const result = validateSnapshotPath('/arbitrary/path/file.json');
      expect(result.isValid).toBe(false);
      if (!result.isValid) {
        expect(result.error).toContain('SECURITY_VIOLATION');
        expect(result.error).toContain('UE project-relative paths');
      }
    });
  });

  describe('UE project-relative paths', () => {
    it('should accept /Temp paths and map to project temp directory', () => {
      const result = validateSnapshotPath('/Temp/snapshot.json');
      expect(result.isValid).toBe(true);
      if (result.isValid) {
        expect(result.safePath).toContain('temp');
        expect(result.safePath).not.toContain('/Temp');
      }
    });

    it('should accept /Saved paths and map to project Saved directory', () => {
      const result = validateSnapshotPath('/Saved/backup.json');
      expect(result.isValid).toBe(true);
      if (result.isValid) {
        expect(result.safePath).toContain('Saved');
      }
    });

    it('should accept /Game paths and map to project Content directory', () => {
      const result = validateSnapshotPath('/Game/Data/config.json');
      expect(result.isValid).toBe(true);
      if (result.isValid) {
        expect(result.safePath).toContain('Content');
      }
    });

    it('should accept case-insensitive UE paths (/temp, /TEMP)', () => {
      const result1 = validateSnapshotPath('/temp/test.json');
      expect(result1.isValid).toBe(true);

      const result2 = validateSnapshotPath('/TEMP/test.json');
      expect(result2.isValid).toBe(true);
    });
  });

  describe('Path traversal', () => {
    it('should reject path traversal with ../..', () => {
      const result = validateSnapshotPath('../../../etc/passwd');
      expect(result.isValid).toBe(false);
      if (!result.isValid) {
        expect(result.error).toContain('SECURITY_VIOLATION');
        expect(result.error).toContain('Path traversal');
      }
    });

    it('should reject path traversal in middle of path', () => {
      const result = validateSnapshotPath('./tmp/../../etc/passwd');
      expect(result.isValid).toBe(false);
      if (!result.isValid) {
        expect(result.error).toContain('SECURITY_VIOLATION');
      }
    });

    it('should reject path traversal with encoded slashes', () => {
      const result = validateSnapshotPath('..\\..\\etc\\passwd');
      expect(result.isValid).toBe(false);
      if (!result.isValid) {
        expect(result.error).toContain('SECURITY_VIOLATION');
      }
    });

    it('should reject traversal before normalization collapses segments', () => {
      for (const snapshotPath of ['/Game/../Saved/foo.json', '/Game/../../Unreal_mcp_main/package.json', 'foo/../bar.json']) {
        const result = validateSnapshotPath(snapshotPath);

        expect(result.isValid).toBe(false);
        if (!result.isValid) {
          expect(result.error).toContain('SECURITY_VIOLATION');
          expect(result.error).toContain('Path traversal');
        }
      }
    });
  });

  describe('Valid paths', () => {
    it('should accept relative paths within project', () => {
      const result = validateSnapshotPath('./tmp/snapshot.json');
      expect(result.isValid).toBe(true);
      if (result.isValid) {
        expect(result.safePath).toContain('tmp');
      }
    });

    it('should accept tmp directory paths', () => {
      const result = validateSnapshotPath('tmp/unreal-mcp/snapshot.json');
      expect(result.isValid).toBe(true);
    });

    it('should accept temp directory paths', () => {
      const result = validateSnapshotPath('temp/snapshot.json');
      expect(result.isValid).toBe(true);
    });

    it('should accept nested relative paths', () => {
      const result = validateSnapshotPath('./output/snapshots/env.json');
      expect(result.isValid).toBe(true);
    });

    it('should accept filenames containing consecutive dots', () => {
      const result = validateSnapshotPath('./output/snapshots/env..backup.json');
      expect(result.isValid).toBe(true);
    });
  });

  describe('Edge cases', () => {
    it('should reject empty string', () => {
      const result = validateSnapshotPath('');
      expect(result.isValid).toBe(false);
      if (!result.isValid) {
        expect(result.error).toContain('Path is required');
      }
    });

    it('should reject whitespace-only string', () => {
      const result = validateSnapshotPath('   ');
      expect(result.isValid).toBe(false);
      if (!result.isValid) {
        expect(result.error).toContain('empty');
      }
    });

    it('should reject paths containing null bytes', () => {
      const result = validateSnapshotPath('tmp/snapshot\0.json');
      expect(result.isValid).toBe(false);
      if (!result.isValid) {
        expect(result.error).toContain('SECURITY_VIOLATION');
        expect(result.error).toContain('null bytes');
      }
    });

    it('should reject null input', () => {
      const result = validateSnapshotPath(null);
      expect(result.isValid).toBe(false);
    });

    it('should reject undefined input', () => {
      const result = validateSnapshotPath(undefined);
      expect(result.isValid).toBe(false);
    });
  });

  describe('Filesystem symlink safety', () => {
    it('should reject exporting through symlinked project path components', async () => {
      const previousCwd = process.cwd();
      const projectDir = await fs.mkdtemp(path.join(os.tmpdir(), 'mcp-env-project-'));
      const outsideDir = await fs.mkdtemp(path.join(os.tmpdir(), 'mcp-env-outside-'));

      try {
        process.chdir(projectDir);
        await fs.symlink(outsideDir, path.join(projectDir, 'escape'), process.platform === 'win32' ? 'junction' : 'dir');

        const result = await exportEnvironmentSnapshot({ path: 'escape/snapshot.json' });

        expect(result.success).toBe(false);
        expect(String(result.error)).toContain('SECURITY_VIOLATION');
        await expect(fs.access(path.join(outsideDir, 'snapshot.json'))).rejects.toThrow();
      } finally {
        process.chdir(previousCwd);
        await fs.rm(projectDir, { recursive: true, force: true });
        await fs.rm(outsideDir, { recursive: true, force: true });
      }
    });

    it('should reject exporting through nested symlinked project path components even when they resolve inside the project', async () => {
      const previousCwd = process.cwd();
      const projectDir = await fs.mkdtemp(path.join(os.tmpdir(), 'mcp-env-project-'));

      try {
        process.chdir(projectDir);
        await fs.mkdir(path.join(projectDir, 'real', 'sub'), { recursive: true });
        await fs.symlink(path.join(projectDir, 'real'), path.join(projectDir, 'link'), process.platform === 'win32' ? 'junction' : 'dir');

        const result = await exportEnvironmentSnapshot({ path: 'link/sub/snapshot.json' });

        expect(result.success).toBe(false);
        expect(String(result.error)).toContain('SECURITY_VIOLATION');
        await expect(fs.access(path.join(projectDir, 'real', 'sub', 'snapshot.json'))).rejects.toThrow();
      } finally {
        process.chdir(previousCwd);
        await fs.rm(projectDir, { recursive: true, force: true });
      }
    });

    it('should reject importing through symlinked project path components', async () => {
      const previousCwd = process.cwd();
      const projectDir = await fs.mkdtemp(path.join(os.tmpdir(), 'mcp-env-project-'));
      const outsideDir = await fs.mkdtemp(path.join(os.tmpdir(), 'mcp-env-outside-'));

      try {
        process.chdir(projectDir);
        await fs.writeFile(path.join(outsideDir, 'secret.json'), '{"timeOfDay": 12}', 'utf8');
        await fs.symlink(outsideDir, path.join(projectDir, 'escape'), process.platform === 'win32' ? 'junction' : 'dir');

        const result = await importEnvironmentSnapshot({ path: 'escape/secret.json' });

        expect(result.success).toBe(false);
        expect(String(result.error)).toContain('SECURITY_VIOLATION');
      } finally {
        process.chdir(previousCwd);
        await fs.rm(projectDir, { recursive: true, force: true });
        await fs.rm(outsideDir, { recursive: true, force: true });
      }
    });

    it('should reject filename parameters containing drive or stream separators', async () => {
      for (const filename of ['foo:bar.json', 'C:foo.json']) {
        const result = await exportEnvironmentSnapshot({ path: 'tmp/unreal-mcp', filename });

        expect(result.success).toBe(false);
        expect(String(result.error)).toContain('SECURITY_VIOLATION');
      }
    });

    it('should reject filename parameters containing null bytes', async () => {
      const result = await exportEnvironmentSnapshot({ path: 'tmp/unreal-mcp', filename: 'env\0snapshot.json' });

      expect(result.success).toBe(false);
      expect(String(result.error)).toContain('SECURITY_VIOLATION');
      expect(String(result.error)).toContain('null bytes');
    });
  });

  describe('Snapshot import details', () => {
    it('should only expose parsed object snapshots as details', async () => {
      const previousCwd = process.cwd();
      const projectDir = await fs.mkdtemp(path.join(os.tmpdir(), 'mcp-env-project-'));

      try {
        process.chdir(projectDir);
        await fs.mkdir(path.join(projectDir, 'tmp', 'unreal-mcp'), { recursive: true });
        await fs.writeFile(path.join(projectDir, 'tmp', 'unreal-mcp', 'array.json'), '[1,2,3]', 'utf8');

        const result = await importEnvironmentSnapshot({ path: 'tmp/unreal-mcp/array.json' });

        expect(result.success).toBe(true);
        expect(result.details).toBeUndefined();
      } finally {
        process.chdir(previousCwd);
        await fs.rm(projectDir, { recursive: true, force: true });
      }
    });
  });
});
