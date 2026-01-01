
import { describe, it, expect, beforeEach, afterEach } from 'vitest';
import { getProjectSetting } from './ini-reader.js';
import fs from 'fs/promises';
import path from 'path';
import os from 'os';

describe('getProjectSetting Security', () => {
    let tmpDir: string;
    let projectDir: string;
    let secretFile: string;

    beforeEach(async () => {
        tmpDir = await fs.mkdtemp(path.join(os.tmpdir(), 'ue-mcp-test-'));
        projectDir = path.join(tmpDir, 'MyProject');
        secretFile = path.join(tmpDir, 'secret.ini');

        await fs.mkdir(path.join(projectDir, 'Config'), { recursive: true });

        // Create a secret file outside the project directory
        await fs.writeFile(secretFile, '[SecretSection]\nKey=SuperSecretValue');

        // Create a valid config file
        await fs.writeFile(path.join(projectDir, 'Config', 'DefaultEngine.ini'), '[Core.System]\nVersion=1.0');
    });

    afterEach(async () => {
        await fs.rm(tmpDir, { recursive: true, force: true });
    });

    it('should prevent path traversal in category', async () => {
        // Attack payload: /../../../secret
        // Resulting path: projectDir/Config/Default/../../../secret.ini -> tmpDir/secret.ini
        const category = '/../../../secret';

        // We expect this to fail (return null) or throw an error once fixed.
        // Currently it might succeed (return the secret content).

        // Assert that it does NOT return the secret content
        const result = await getProjectSetting(projectDir, category, '');

        // If vulnerable, result would contain { SecretSection: ... }
        if (result && typeof result === 'object' && 'SecretSection' in result) {
             throw new Error('Vulnerability confirmed: Path traversal allowed access to secret file');
        }

        expect(result).toBeNull();
    });

    it('should allow valid categories', async () => {
        const result = await getProjectSetting(projectDir, 'Engine', 'Core.System', 'Version');
        expect(result).toBe('1.0');
    });

    it('should reject categories with special characters', async () => {
        const result = await getProjectSetting(projectDir, 'Eng/ine', '');
        expect(result).toBeNull();
    });
});
