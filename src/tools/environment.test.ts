import { describe, it, expect } from 'vitest';
import { validateSnapshotPath } from './environment.js';

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

    it('should reject null input', () => {
      const result = validateSnapshotPath(null as unknown as string);
      expect(result.isValid).toBe(false);
    });

    it('should reject undefined input', () => {
      const result = validateSnapshotPath(undefined as unknown as string);
      expect(result.isValid).toBe(false);
    });
  });
});
