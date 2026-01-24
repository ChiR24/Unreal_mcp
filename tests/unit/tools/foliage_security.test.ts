import { describe, it, expect, vi, beforeEach } from 'vitest';
import { FoliageTools } from '../../../src/tools/foliage.js';
import { UnrealBridge } from '../../../src/unreal-bridge.js';

describe('FoliageTools Security', () => {
    let bridge: UnrealBridge;
    let foliageTools: FoliageTools;

    beforeEach(() => {
        bridge = {
            executeConsoleCommand: vi.fn().mockResolvedValue({ success: true }),
            executeConsoleCommands: vi.fn().mockResolvedValue({ success: true }),
        } as unknown as UnrealBridge;
        foliageTools = new FoliageTools(bridge);
    });

    it('should sanitize foliage type to prevent command injection in removeFoliageInstances', async () => {
        await foliageTools.removeFoliageInstances({
            foliageType: 'Tree; Quit',
            position: [0, 0, 0],
            radius: 100
        });

        // We expect the command NOT to contain the semicolon
        expect(bridge.executeConsoleCommand).toHaveBeenCalledWith(
            expect.not.stringMatching(/Tree; Quit/)
        );

        // We expect it to be sanitized (likely replacing special chars with _)
        const lastCall = (bridge.executeConsoleCommand as any).mock.calls[0][0];
        expect(lastCall).not.toContain(';');
    });

    it('should sanitize inputs in createInstancedMesh', async () => {
        await foliageTools.createInstancedMesh({
            name: 'MyMesh; Delete',
            meshPath: '/Game/Mesh; rm -rf',
            instances: [{ position: [0,0,0] }]
        });

        expect(bridge.executeConsoleCommands).toHaveBeenCalled();
        const calls = (bridge.executeConsoleCommands as any).mock.calls[0][0];

        // Check all commands in the batch
        calls.forEach((cmd: string) => {
            expect(cmd).not.toContain(';');
            expect(cmd).not.toContain('rm -rf');
        });
    });

    it('should sanitize inputs in selectFoliageInstances', async () => {
        await foliageTools.selectFoliageInstances({
            foliageType: 'Tree" | calc.exe',
            selectAll: true
        });

        const lastCall = (bridge.executeConsoleCommand as any).mock.calls[0][0];
        expect(lastCall).not.toContain('"');
        expect(lastCall).not.toContain('|');
    });
});
