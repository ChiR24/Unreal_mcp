import { describe, it, expect, vi, beforeEach } from 'vitest';
import { LevelTools } from '../../../src/tools/level.js';
import { UnrealBridge } from '../../../src/unreal-bridge.js';

describe('LevelTools Injection Security', () => {
    let bridge: UnrealBridge;
    let levelTools: LevelTools;

    beforeEach(() => {
        bridge = {
            executeConsoleCommand: vi.fn().mockResolvedValue({ success: true, message: 'OK' }),
            executeConsoleCommands: vi.fn().mockResolvedValue({ success: true, message: 'OK' }),
            sendAutomationRequest: vi.fn().mockRejectedValue(new Error('Automation unavailable')),
            isConnected: false
        } as unknown as UnrealBridge;
        levelTools = new LevelTools(bridge);
    });

    it('should NOT allow command injection in createSubLevel via name', async () => {
        const maliciousName = 'MyLevel;Quit';
        await levelTools.createSubLevel({ name: maliciousName, type: 'Persistent' });

        const executeCommandMock = vi.mocked(bridge.executeConsoleCommand);
        expect(executeCommandMock).toHaveBeenCalled();
        const command = executeCommandMock.mock.calls[0][0];

        // Expectation: The command should either be sanitized (no ;) or quoted such that it doesn't execute 'Quit'
        // OR the function should throw before executing.
        // Currently it does: `CreateSubLevel ${params.name} ...`

        // If vulnerable: "CreateSubLevel MyLevel;Quit Persistent None"
        // If secured: "CreateSubLevel MyLevel_Quit Persistent None" (if we use replace)

        expect(command).not.toContain(';Quit');
    });

    it('should NOT allow command injection in setLevelVisibility via levelName', async () => {
        const maliciousName = 'MyLevel;Quit';
        await levelTools.setLevelVisibility({ levelName: maliciousName, visible: true });

        const executeCommandMock = vi.mocked(bridge.executeConsoleCommand);
        expect(executeCommandMock).toHaveBeenCalled();
        const command = executeCommandMock.mock.calls[0][0];

        expect(command).not.toContain(';Quit');
    });

    it('should NOT allow command injection in streaming load fallback', async () => {
        await levelTools.loadLevel({ levelPath: '/Game/Maps/MyLevel;Quit', streaming: true });

        const executeCommandMock = vi.mocked(bridge.executeConsoleCommand);
        const command = executeCommandMock.mock.calls[0][0];
        expect(command).toBe('StreamLevel MyLevel_Quit Load Show');
    });
});
