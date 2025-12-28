
import { AutomationBridge } from '../automation/index.js';
import { Logger } from '../utils/logger.js';

const log = new Logger('InputTools');

interface ToolDefinition {
    name: string;
    description: string;
    inputSchema: object;
    outputSchema: object;
}

// Common valid key names for UE5 Enhanced Input (not exhaustive, but covers primary cases)
const VALID_KEY_NAMES = new Set([
    // Keyboard - Letters
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
    'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    // Keyboard - Numbers
    'Zero', 'One', 'Two', 'Three', 'Four', 'Five', 'Six', 'Seven', 'Eight', 'Nine',
    // Keyboard - Function keys
    'F1', 'F2', 'F3', 'F4', 'F5', 'F6', 'F7', 'F8', 'F9', 'F10', 'F11', 'F12',
    // Keyboard - Special
    'SpaceBar', 'Enter', 'Escape', 'Tab', 'BackSpace', 'CapsLock',
    'LeftShift', 'RightShift', 'LeftControl', 'RightControl', 'LeftAlt', 'RightAlt',
    'LeftCommand', 'RightCommand', 'Insert', 'Delete', 'Home', 'End', 'PageUp', 'PageDown',
    'Up', 'Down', 'Left', 'Right',
    // Keyboard - Punctuation
    'Semicolon', 'Equals', 'Comma', 'Hyphen', 'Underscore', 'Period', 'Slash', 'Tilde',
    'LeftBracket', 'Backslash', 'RightBracket', 'Apostrophe', 'Quote',
    // Mouse - Buttons
    'LeftMouseButton', 'RightMouseButton', 'MiddleMouseButton', 'ThumbMouseButton', 'ThumbMouseButton2',
    // Mouse - Axes (must be mapped separately, not as composite 2D)
    'MouseX', 'MouseY', 'MouseWheelAxis', 'MouseScrollUp', 'MouseScrollDown',
    // Gamepad - Face Buttons
    'Gamepad_FaceButton_Bottom', 'Gamepad_FaceButton_Right', 'Gamepad_FaceButton_Left', 'Gamepad_FaceButton_Top',
    // Gamepad - Shoulder/Trigger
    'Gamepad_LeftShoulder', 'Gamepad_RightShoulder', 'Gamepad_LeftTrigger', 'Gamepad_RightTrigger',
    'Gamepad_LeftTriggerAxis', 'Gamepad_RightTriggerAxis',
    // Gamepad - Sticks
    'Gamepad_LeftThumbstick', 'Gamepad_RightThumbstick',
    'Gamepad_LeftStick_Up', 'Gamepad_LeftStick_Down', 'Gamepad_LeftStick_Left', 'Gamepad_LeftStick_Right',
    'Gamepad_RightStick_Up', 'Gamepad_RightStick_Down', 'Gamepad_RightStick_Left', 'Gamepad_RightStick_Right',
    // Gamepad - D-Pad
    'Gamepad_DPad_Up', 'Gamepad_DPad_Down', 'Gamepad_DPad_Left', 'Gamepad_DPad_Right',
    // Gamepad - Special
    'Gamepad_Special_Left', 'Gamepad_Special_Right'
]);

export class InputTools {
    private automationBridge: AutomationBridge | null = null;

    constructor() { }

    setAutomationBridge(bridge: AutomationBridge) {
        this.automationBridge = bridge;
    }

    async createInputAction(name: string, path: string) {
        if (!this.automationBridge) throw new Error('Automation bridge not set');
        return this.automationBridge.sendAutomationRequest('manage_input', {
            action: 'create_input_action',
            name,
            path
        });
    }

    async createInputMappingContext(name: string, path: string) {
        if (!this.automationBridge) throw new Error('Automation bridge not set');
        return this.automationBridge.sendAutomationRequest('manage_input', {
            action: 'create_input_mapping_context',
            name,
            path
        });
    }

    async addMapping(contextPath: string, actionPath: string, key: string) {
        if (!this.automationBridge) throw new Error('Automation bridge not set');

        // Validate key name
        if (!key || typeof key !== 'string' || key.trim().length === 0) {
            return { success: false, error: 'INVALID_ARGUMENT', message: 'Key name is required.' };
        }

        const trimmedKey = key.trim();

        // Check for common mistakes (composite 2D axis names)
        if (trimmedKey === 'MouseXY2D' || trimmedKey === 'Mouse2D' || trimmedKey === 'MouseXY') {
            return {
                success: false,
                error: 'INVALID_ARGUMENT',
                message: `Invalid key name '${trimmedKey}'. For mouse axis input, use separate mappings with 'MouseX' and 'MouseY' keys instead of composite 2D axis names.`
            };
        }

        // Warn if key is not in our known list (but still attempt the mapping)
        if (!VALID_KEY_NAMES.has(trimmedKey)) {
            log.warn(`Key '${trimmedKey}' is not in the standard key list. Attempting mapping anyway.`);
        }

        return this.automationBridge.sendAutomationRequest('manage_input', {
            action: 'add_mapping',
            contextPath,
            actionPath,
            key: trimmedKey
        });
    }

    async removeMapping(contextPath: string, actionPath: string) {
        if (!this.automationBridge) throw new Error('Automation bridge not set');
        return this.automationBridge.sendAutomationRequest('manage_input', {
            action: 'remove_mapping',
            contextPath,
            actionPath
        });
    }
}


export const inputTools: ToolDefinition = {
    name: 'manage_input',
    description: `Enhanced Input management.

Use it when you need to:
- create Input Actions (IA_*)
- create Input Mapping Contexts (IMC_*)
- bind keys to actions in a mapping context.

Supported actions:
- create_input_action: Create a UInputAction asset.
- create_input_mapping_context: Create a UInputMappingContext asset.
- add_mapping: Add a key mapping to a context.
- remove_mapping: Remove a mapping from a context.`,
    inputSchema: {
        type: 'object',
        properties: {
            action: {
                type: 'string',
                enum: [
                    'create_input_action',
                    'create_input_mapping_context',
                    'add_mapping',
                    'remove_mapping'
                ],
                description: 'Action to perform'
            },
            name: { type: 'string', description: 'Name of the asset (for creation).' },
            path: { type: 'string', description: 'Path to save the asset (e.g. /Game/Input).' },
            contextPath: { type: 'string', description: 'Path to the Input Mapping Context.' },
            actionPath: { type: 'string', description: 'Path to the Input Action.' },
            key: { type: 'string', description: 'Key name (e.g. "SpaceBar", "W", "Gamepad_FaceButton_Bottom").' }
        },
        required: ['action']
    },
    outputSchema: {
        type: 'object',
        properties: {
            success: { type: 'boolean' },
            message: { type: 'string' },
            assetPath: { type: 'string' }
        }
    }
};
