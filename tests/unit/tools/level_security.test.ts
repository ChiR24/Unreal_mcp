import { describe, it, expect, vi, beforeEach } from 'vitest';
import { LevelTools } from '../../../src/tools/level';
import { UnrealBridge } from '../../../src/unreal-bridge';

describe('LevelTools Security', () => {
    let bridge: UnrealBridge;
    let levelTools: LevelTools;
    let executeCommandMock: any;

    beforeEach(() => {
        executeCommandMock = vi.fn().mockResolvedValue({ success: true, message: 'OK' });
        bridge = {
            executeConsoleCommand: executeCommandMock,
            sendAutomationRequest: vi.fn().mockRejectedValue(new Error('Automation unavailable')),
            executeConsoleCommands: vi.fn().mockResolvedValue({ success: true }),
            isConnected: false
        } as unknown as UnrealBridge;
        levelTools = new LevelTools(bridge);
    });

    describe('Command Injection Prevention', () => {
        it('should sanitize level name in createSubLevel', async () => {
            const maliciousName = 'MyLevel"; Quit; "';
            await levelTools.createSubLevel({
                name: maliciousName,
                type: 'Persistent'
            });

            expect(executeCommandMock).toHaveBeenCalled();
            const command = executeCommandMock.mock.calls[0][0];
            // Should NOT contain the raw quote and semicolon sequence
            expect(command).not.toContain('"; Quit; "');
            // sanitizeAssetName replaces invalid chars with _ and collapses them, and trims leading/trailing
            expect(command).toContain('MyLevel_Quit');
        });

        it('should sanitize level name in setLevelVisibility', async () => {
            const maliciousName = 'Level" && calc';
            await levelTools.setLevelVisibility({
                levelName: maliciousName,
                visible: true
            });

            const command = executeCommandMock.mock.calls[0][0];
            expect(command).not.toContain('" && calc');
            // sanitizeAssetName replaces " and & and spaces with _ and collapses them
            expect(command).toContain('Level_calc');
        });

        it('should sanitize game mode in setWorldSettings', async () => {
            const maliciousGameMode = 'MyGameMode; DestroyAll';
            await levelTools.setWorldSettings({
                gameMode: maliciousGameMode
            });

            // check executeConsoleCommands was called
            const executeCommandsMock = bridge.executeConsoleCommands as any;
            expect(executeCommandsMock).toHaveBeenCalled();
            const commands = executeCommandsMock.mock.calls[0][0];
            expect(commands[0]).not.toContain('; DestroyAll');
            expect(commands[0]).toContain('MyGameMode_DestroyAll');
        });

        it('should allow valid asset paths in setWorldSettings', async () => {
            const validGameMode = '/Game/Blueprints/MyMode.MyMode_C';
            await levelTools.setWorldSettings({
                gameMode: validGameMode
            });

            const executeCommandsMock = bridge.executeConsoleCommands as any;
            expect(executeCommandsMock).toHaveBeenCalled();
            const commands = executeCommandsMock.mock.calls[0][0];
            expect(commands[0]).toContain(`SetGameMode ${validGameMode}`);
        });
    });

    describe('Type Validation', () => {
        it('should validate vector inputs for setWorldOrigin', async () => {
            // Bypass typescript to pass invalid input
            await expect(levelTools.setWorldOrigin({
                location: ['1', '2; rm -rf', '3'] as any
            })).rejects.toThrow();
        });

        it('should validate inputs for createStreamingVolume', async () => {
            await expect(levelTools.createStreamingVolume({
                levelName: 'Level1',
                position: 'invalid' as any,
                size: [100, 100, 100]
            })).rejects.toThrow();
        });
    });
});
