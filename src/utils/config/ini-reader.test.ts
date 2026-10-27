/// <reference types="node" />

import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import { readProjectIniValue } from './ini-reader.js';
import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';

describe('readProjectIniValue', () => {
    let tmpDir: string;
    let projectDir: string;

    beforeEach(async () => {
        tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), 'ue-mcp-test-'));
        projectDir = path.join(tmpDir, 'MyProject');
        await fs.mkdir(path.join(projectDir, 'Config'), { recursive: true });
        // A file outside the project that a traversing category would reach.
        await fs.writeFile(path.join(tmpDir, 'secret.ini'), '[SecretSection]\nKey=SuperSecretValue');
        await fs.writeFile(path.join(projectDir, 'Config', 'DefaultEngine.ini'), '[Core.System]\nVersion=1.0');
    });

    afterEach(async () => {
        await fs.rm(tmpDir, { recursive: true, force: true });
    });

    it('reads a key from the project Default<Category>.ini', () => {
        expect(readProjectIniValue(projectDir, 'Engine', 'Core.System', 'Version')).toBe('1.0');
        expect(readProjectIniValue(path.join(projectDir, 'MyProject.uproject'), 'Engine', 'Core.System', 'Version')).toBe('1.0');
    });

    it.each(['/../../../secret', 'Eng/ine', '..\\secret'])('refuses a category that is not a plain identifier: %j', (category) => {
        expect(readProjectIniValue(projectDir, category, 'SecretSection', 'Key')).toBeUndefined();
    });

    it('reads prototype-named sections and keys as plain data', async () => {
        await fs.writeFile(path.join(projectDir, 'Config', 'DefaultProto.ini'), '[__proto__]\nKey=SafeValue\n[constructor]\nprototype=Ignored');

        expect(readProjectIniValue(projectDir, 'Proto', '__proto__', 'Key')).toBe('SafeValue');
        expect(readProjectIniValue(projectDir, 'Proto', '__proto__', 'toString')).toBeUndefined();
        expect(Object.prototype).not.toHaveProperty('Key');
        expect(Object.prototype).not.toHaveProperty('prototype');
    });
});
