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
            executeConsoleCommands: vi.fn().mockResolvedValue([{ success: true }]),
            executeEditorFunction: vi.fn().mockResolvedValue({ success: true })
        } as unknown as UnrealBridge;

        automationBridge = {
             sendAutomationRequest: vi.fn().mockResolvedValue({ success: true })
        } as unknown as AutomationBridge;

        foliageTools = new FoliageTools(bridge, automationBridge);
    });

    it('createInstancedMesh should sanitize inputs to prevent command injection', async () => {
        const maliciousName = 'MyMesh"; Quit; "';
        const maliciousPath = '/Game/Mesh"; DeleteAll; "';

        await foliageTools.createInstancedMesh({
            name: maliciousName,
            meshPath: maliciousPath,
            instances: [{ position: [0, 0, 0] }]
        });

        const executeCommandsMock = bridge.executeConsoleCommands as any;
        expect(executeCommandsMock).toHaveBeenCalled();
        const commands = executeCommandsMock.mock.calls[0][0];

        const createCmd = commands.find((cmd: string) => cmd.startsWith('CreateInstancedStaticMesh'));
        expect(createCmd).toBeDefined();

        // Expect sanitization
        // The injection vector (";) should be gone
        expect(createCmd).not.toContain('";');
        expect(createCmd).not.toContain('; Quit');

        // The resulting command should contain the sanitized versions
        // sanitizeAssetName: MyMesh"; Quit; " -> MyMesh_Quit
        expect(createCmd).toContain('MyMesh_Quit');

        // sanitizePath: /Game/Mesh"; DeleteAll; " -> /Game/Mesh_DeleteAll
        // Note: sanitizePath might treat the whole thing as a path segment or split it.
        // Given '/Game/Mesh"; DeleteAll; "', it splits by /.
        // Segments: 'Game', 'Mesh"; DeleteAll; "'
        // 'Mesh"; DeleteAll; "' sanitized becomes 'Mesh_DeleteAll'
        expect(createCmd).toContain('/Game/Mesh_DeleteAll');
    });

    it('setFoliageLOD should sanitize foliageType', async () => {
        const maliciousType = 'MyFoliage"; Quit; "';

        await foliageTools.setFoliageLOD({
            foliageType: maliciousType,
            lodDistances: [100, 200]
        });

        const executeCommandsMock = bridge.executeConsoleCommands as any;
        expect(executeCommandsMock).toHaveBeenCalled();
        const commands = executeCommandsMock.mock.calls[0][0];

        const lodCmd = commands.find((cmd: string) => cmd.startsWith('SetFoliageLODDistances'));
        expect(lodCmd).toBeDefined();

        // Expect injection vector to be gone
        expect(lodCmd).not.toContain('";');
        expect(lodCmd).not.toContain('; Quit');

        // Expect sanitized output. sanitizePath will ensure it starts with /Game if it looks like a relative path
        // MyFoliage"; Quit; " -> /Game/MyFoliage_Quit
        expect(lodCmd).toContain('/Game/MyFoliage_Quit');
    });
});
