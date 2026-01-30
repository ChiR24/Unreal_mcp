
import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import { readIniFile } from './ini-reader.js';
import fs from 'fs/promises';
import path from 'path';
import os from 'os';

describe('readIniFile Security', () => {
    let tmpDir: string;
    let txtFile: string;
    let iniFile: string;

    beforeEach(async () => {
        tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), 'ue-mcp-test-ini-sec-'));
        txtFile = path.join(tmpDir, 'test.txt');
        iniFile = path.join(tmpDir, 'test.ini');

        await fs.writeFile(txtFile, 'This is a text file');
        await fs.writeFile(iniFile, '[Section]\nKey=Value');
    });

    afterEach(async () => {
        await fs.rm(tmpDir, { recursive: true, force: true });
    });

    it('should block reading non-ini files', async () => {
        await expect(readIniFile(txtFile)).rejects.toThrow(/Invalid file extension/);
    });

    it('should allow reading valid ini files', async () => {
        const result = await readIniFile(iniFile);
        expect(result).toBeDefined();
        expect(result['Section']).toBeDefined();
        expect(result['Section']['Key']).toBe('Value');
    });
});
