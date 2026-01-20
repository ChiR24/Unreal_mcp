import { describe, it, expect, vi, beforeEach } from 'vitest';
import { UITools } from '../../../src/tools/ui';
import { UnrealBridge } from '../../../src/unreal-bridge';
import { AutomationBridge } from '../../../src/automation';

describe('UITools Security', () => {
    let bridge: UnrealBridge;
    let automationBridge: AutomationBridge;
    let uiTools: UITools;

    beforeEach(() => {
        bridge = {
            executeConsoleCommand: vi.fn().mockResolvedValue({ success: true, message: 'OK' }),
            executeConsoleCommands: vi.fn().mockResolvedValue([{ success: true }]),
            executeEditorFunction: vi.fn().mockResolvedValue({ success: true })
        } as unknown as UnrealBridge;

        automationBridge = {
             sendAutomationRequest: vi.fn().mockResolvedValue({ success: true })
        } as unknown as AutomationBridge;

        uiTools = new UITools(bridge, automationBridge);
    });

    it('should sanitize tooltip text to prevent argument injection', async () => {
        const maliciousText = 'Hello" 0.0 SetWidgetTooltip Other "Malicious';

        await uiTools.createTooltip({
            widgetName: 'TestWidget',
            componentName: 'TestComp',
            text: maliciousText
        });

        const executeCommandMock = bridge.executeConsoleCommand as any;
        expect(executeCommandMock).toHaveBeenCalled();
        const command = executeCommandMock.mock.calls[0][0];

        // We expect the double quotes to be replaced by single quotes (or escaped/removed)
        // so that they don't terminate the string argument.
        expect(command).toContain('"Hello\' 0.0 SetWidgetTooltip Other \'Malicious"');
    });

    it('should sanitize menu button text to prevent command injection', async () => {
        const maliciousText = '"; Quit; "';

        await uiTools.createMenu({
            name: 'TestMenu',
            menuType: 'Main',
            buttons: [{
                text: maliciousText,
                action: 'DoSomething'
            }]
        });

        const executeCommandsMock = bridge.executeConsoleCommands as any;
        expect(executeCommandsMock).toHaveBeenCalled();
        const commands = executeCommandsMock.mock.calls[0][0];

        const addBtnCmd = commands.find((cmd: string) => cmd.startsWith('AddMenuButton'));
        expect(addBtnCmd).toBeDefined();

        // We expect the double quotes to be replaced by single quotes.
        // Input: "; Quit; "
        // Output: '; Quit; '
        // Command: AddMenuButton ... "'; Quit; '" ...
        expect(addBtnCmd).toContain('"\'; Quit; \'"');
    });
});
