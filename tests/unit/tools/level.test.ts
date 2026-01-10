import { describe, it, expect, vi, beforeEach } from 'vitest';
import { LevelTools } from '../../../src/tools/level';
import { UnrealBridge } from '../../../src/unreal-bridge';

describe('LevelTools Security', () => {
    let bridge: UnrealBridge;
    let levelTools: LevelTools;

    beforeEach(() => {
        bridge = {
            executeConsoleCommand: vi.fn().mockResolvedValue({ success: true, message: 'OK' }),
            sendAutomationRequest: vi.fn().mockRejectedValue(new Error('Automation unavailable')),
            isConnected: false
        } as unknown as UnrealBridge;
        levelTools = new LevelTools(bridge);
    });

    it('should use sanitized path in console fallback for loadLevel', async () => {
        // Input that needs sanitization/normalization
        // sanitizePath allows /Game/..., but we want to check if normalizer changes it.
        // normalizeLevelPath changes backslashes to slashes.
        const rawPath = '\\Game\\Maps\\MyMap';
        const expectedPath = '/Game/Maps/MyMap';

        await levelTools.loadLevel({ levelPath: rawPath });

        const executeCommandMock = bridge.executeConsoleCommand as any;
        expect(executeCommandMock).toHaveBeenCalled();

        const command = executeCommandMock.mock.calls[0][0];

        // This assertion checks if we are using the normalized path.
        expect(command).toBe(`Open ${expectedPath}`);
    });

    it('should validate level path before falling back to console', async () => {
        const maliciousPath = '../../Secret/Map';

        // sanitizePath should throw on '..'
        await expect(levelTools.loadLevel({ levelPath: maliciousPath }))
            .rejects.toThrow(/Security validation failed/);

        const executeCommandMock = bridge.executeConsoleCommand as any;
        expect(executeCommandMock).not.toHaveBeenCalled();
    });
});
