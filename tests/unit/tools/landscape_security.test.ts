import { describe, it, expect, vi, beforeEach } from 'vitest';
import { LandscapeTools } from '../../../src/tools/landscape';
import { UnrealBridge } from '../../../src/unreal-bridge';
import { AutomationBridge } from '../../../src/automation';

describe('LandscapeTools Security', () => {
    let bridge: UnrealBridge;
    let automationBridge: AutomationBridge;
    let landscapeTools: LandscapeTools;

    beforeEach(() => {
        bridge = {
            executeConsoleCommand: vi.fn().mockResolvedValue({ success: true, message: 'OK' }),
            executeConsoleCommands: vi.fn().mockResolvedValue([{ success: true }])
        } as unknown as UnrealBridge;

        automationBridge = {
             sendAutomationRequest: vi.fn().mockResolvedValue({ success: true })
        } as unknown as AutomationBridge;

        landscapeTools = new LandscapeTools(bridge, automationBridge);
    });

    it('should sanitize landscape name in createLandscapeGrass to prevent command injection', async () => {
        const maliciousName = 'MyLandscape; quit';
        const grassType = 'Grass_Type';

        await landscapeTools.createLandscapeGrass({
            landscapeName: maliciousName,
            grassType: grassType
        });

        const executeCommandsMock = bridge.executeConsoleCommands as any;
        expect(executeCommandsMock).toHaveBeenCalled();
        const commands = executeCommandsMock.mock.calls[0][0];

        // Should be quoted: CreateLandscapeGrass "MyLandscape; quit" "Grass_Type"
        expect(commands[0]).toContain('"MyLandscape; quit"');
    });

    it('should sanitize grass type in createLandscapeGrass', async () => {
        const landscapeName = 'MyLandscape';
        const maliciousGrassType = 'GrassType; quit';

        await landscapeTools.createLandscapeGrass({
            landscapeName: landscapeName,
            grassType: maliciousGrassType
        });

        const executeCommandsMock = bridge.executeConsoleCommands as any;
        expect(executeCommandsMock).toHaveBeenCalled();
        const commands = executeCommandsMock.mock.calls[0][0];

        // Should be quoted: CreateLandscapeGrass "MyLandscape" "GrassType; quit"
        expect(commands[0]).toContain('"GrassType; quit"');
    });

    it('should sanitize inputs in createWaterBody', async () => {
        const maliciousName = 'WaterBody; quit';

        await landscapeTools.createWaterBody({
            type: 'Lake',
            name: maliciousName
        });

        const executeCommandMock = bridge.executeConsoleCommand as any;
        expect(executeCommandMock).toHaveBeenCalled();
        const command = executeCommandMock.mock.calls[0][0];

        // Should be quoted: CreateWaterBody "Lake" "WaterBody; quit" ...
        expect(command).toContain('"WaterBody; quit"');
    });

    it('should sanitize landscape name in updateLandscapeCollision', async () => {
        const maliciousName = 'Landscape; quit';

        await landscapeTools.updateLandscapeCollision({
            landscapeName: maliciousName,
            simpleCollision: true
        });

        const executeCommandsMock = bridge.executeConsoleCommands as any;
        expect(executeCommandsMock).toHaveBeenCalled();
        const commands = executeCommandsMock.mock.calls[0][0];

        // Check the UpdateLandscapeCollision command
        const updateCmd = commands.find((cmd: string) => cmd.startsWith('UpdateLandscapeCollision'));
        expect(updateCmd).toBeDefined();
        // Should be quoted: UpdateLandscapeCollision "Landscape; quit"
        expect(updateCmd).toContain('"Landscape; quit"');
    });

    it('should sanitize material path in setLandscapeMaterial', async () => {
        const landscapeName = 'Landscape';
        const maliciousPath = '/Game/Materials/MyMaterial; quit';

        await landscapeTools.setLandscapeMaterial({
            landscapeName,
            materialPath: maliciousPath
        });

        const automationSpy = automationBridge.sendAutomationRequest as any;
        expect(automationSpy).toHaveBeenCalled();
        const payload = automationSpy.mock.calls[0][1];
        // setLandscapeMaterial uses sanitizePath which replaces invalid chars with _
        expect(payload.materialPath).toBe('/Game/Materials/MyMaterial_quit');
    });

    it('should sanitize type in createWaterBody', async () => {
         const maliciousType = 'Ocean; quit' as any;

         await landscapeTools.createWaterBody({
             type: maliciousType,
             name: 'Water'
         });

         const executeCommandMock = bridge.executeConsoleCommand as any;
         expect(executeCommandMock).toHaveBeenCalled();
         const command = executeCommandMock.mock.calls[0][0];

         // Should be quoted: CreateWaterBody "Ocean; quit" ...
         expect(command).toContain('"Ocean; quit"');
    });
});
