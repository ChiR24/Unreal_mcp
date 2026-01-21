import { describe, it, expect, vi, beforeEach } from 'vitest';
import { FoliageTools } from '../../../src/tools/foliage';
import { UnrealBridge } from '../../../src/unreal-bridge';
import { AutomationBridge } from '../../../src/automation';

describe('FoliageTools Security', () => {
    let bridge: UnrealBridge;
    let automationBridge: AutomationBridge;
    let foliageTools: FoliageTools;

    beforeEach(() => {
        bridge = {
            executeConsoleCommand: vi.fn().mockResolvedValue({ success: true, message: 'OK' }),
            executeConsoleCommands: vi.fn().mockResolvedValue([{ success: true }])
        } as unknown as UnrealBridge;

        automationBridge = {
             sendAutomationRequest: vi.fn().mockResolvedValue({ success: true })
        } as unknown as AutomationBridge;

        foliageTools = new FoliageTools(bridge, automationBridge);
    });

    it('should sanitize name and meshPath in createInstancedMesh to prevent command injection', async () => {
        const maliciousName = 'MyMesh; Quit';
        const maliciousPath = '/Game/Mesh" | calc.exe';

        await foliageTools.createInstancedMesh({
            name: maliciousName,
            meshPath: maliciousPath,
            instances: []
        });

        const executeCommandsMock = bridge.executeConsoleCommands as any;
        expect(executeCommandsMock).toHaveBeenCalled();
        const commands = executeCommandsMock.mock.calls[0][0];

        const createCmd = commands[0];
        // Expect sanitization
        expect(createCmd).not.toContain(';');
        expect(createCmd).not.toContain('|');
        expect(createCmd).toContain('MyMesh_Quit');
    });
});
