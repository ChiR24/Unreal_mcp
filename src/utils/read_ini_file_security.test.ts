import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import { readIniFile } from './ini-reader.js';
import fs from 'fs/promises';
import path from 'path';
import os from 'os';

describe('readIniFile Security', () => {
    let tmpDir: string;
    let textFile: string;

    beforeEach(async () => {
        tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), 'ue-mcp-test-ini-'));
        textFile = path.join(tmpDir, 'sensitive.txt');
        await fs.writeFile(textFile, 'This is a sensitive file');
    });

    afterEach(async () => {
        await fs.rm(tmpDir, { recursive: true, force: true });
    });

    it('should reject files without .ini extension', async () => {
        await expect(readIniFile(textFile)).rejects.toThrow(/must be \.ini/i);
    });
});
