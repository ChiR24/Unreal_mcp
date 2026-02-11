import { describe, it, expect, vi, beforeEach } from 'vitest';
import { LevelTools } from '../../../src/tools/level';
import { UnrealBridge } from '../../../src/unreal-bridge';

describe('LevelTools Security', () => {
    let bridge: UnrealBridge;
    let levelTools: LevelTools;

    beforeEach(() => {
        bridge = {
            executeConsoleCommand: vi.fn().mockImplementation((cmd: string) => {
                // GetLevelPath returns valid path for existing levels
                if (cmd.startsWith('GetLevelPath')) {
                    return Promise.resolve({ success: true, message: '/Game/Maps/MyMap' });
                }
                return Promise.resolve({ success: true, message: 'OK' });
            }),
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

        // First call is GetLevelPath validation, second is Open
        const calls = executeCommandMock.mock.calls;
        const openCall = calls.find((call: string[]) => call[0].startsWith('Open'));
        expect(openCall).toBeDefined();
        expect(openCall[0]).toBe(`Open ${expectedPath}`);
    });

    it('should validate level path before falling back to console', async () => {
        const maliciousPath = '../../Secret/Map';

        // sanitizePath should throw on '..'
        await expect(levelTools.loadLevel({ levelPath: maliciousPath }))
            .rejects.toThrow(/Security validation failed/);

        const executeCommandMock = bridge.executeConsoleCommand as any;
        expect(executeCommandMock).not.toHaveBeenCalled();
    });

    it('should return error when level not found via GetLevelPath', async () => {
        // Override mock to simulate level not found
        (bridge.executeConsoleCommand as any).mockResolvedValueOnce({ 
            success: false, 
            message: 'Level not found: /Game/Maps/NonExistent' 
        });

        const result = await levelTools.loadLevel({ levelPath: '/Game/Maps/NonExistent' });

        expect(result.success).toBe(false);
        expect(result.error).toBe('LEVEL_NOT_FOUND');
    });
});
