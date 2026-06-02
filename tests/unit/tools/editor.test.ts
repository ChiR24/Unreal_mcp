import { describe, it, expect, vi, beforeEach } from 'vitest';
import { EditorTools } from '../../../src/tools/editor.js';
import { UnrealBridge } from '../../../src/unreal-bridge.js';

describe('EditorTools Security', () => {
    let bridge: UnrealBridge;
    let editorTools: EditorTools;

    beforeEach(() => {
        bridge = {
            executeConsoleCommand: vi.fn().mockResolvedValue({ success: true, message: 'OK' })
        } as unknown as UnrealBridge;
        editorTools = new EditorTools(bridge);
    });

    it('should sanitize screenshot filenames to prevent path traversal', async () => {
        const maliciousFilename = '../../../../Windows/System32/drivers/etc/hosts';

        await editorTools.takeScreenshot(maliciousFilename);

        const executeCommandMock = vi.mocked(bridge.executeConsoleCommand);
        expect(executeCommandMock).toHaveBeenCalled();

        const command = executeCommandMock.mock.calls[0][0];
        // Expect the command to NOT contain traversal characters
        expect(command).not.toContain('..');
        expect(command).not.toContain('/');
        expect(command).not.toContain('\\');

        // It should have stripped the path and kept only the basename (sanitized)
        // basename of '../../../../Windows/System32/drivers/etc/hosts' is 'hosts'
        expect(command).toContain('filename="hosts"');
    });

    it('should allow safe filenames', async () => {
        const safeFilename = 'MyScreenshot';
        await editorTools.takeScreenshot(safeFilename);

        const executeCommandMock = vi.mocked(bridge.executeConsoleCommand);
        const command = executeCommandMock.mock.calls[0][0];
        expect(command).toContain('filename="MyScreenshot"');
    });

    it('should handle filenames with invalid chars by replacing them', async () => {
        const invalidFilename = 'My:Screenshot?.png';
        await editorTools.takeScreenshot(invalidFilename);

        const executeCommandMock = vi.mocked(bridge.executeConsoleCommand);
        const command = executeCommandMock.mock.calls[0][0];
        // : and ? should be replaced by _
        expect(command).toContain('filename="My_Screenshot_.png"');
    });

    it('should fall back when sanitized screenshot filename is empty', async () => {
        await editorTools.takeScreenshot('////');

        const executeCommandMock = vi.mocked(bridge.executeConsoleCommand);
        const command = executeCommandMock.mock.calls[0][0];
        expect(command).toMatch(/filename="Screenshot_\d+"/);
    });
});
