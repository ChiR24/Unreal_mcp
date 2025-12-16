
import { AutomationBridge } from '../automation/index.js';

interface ToolDefinition {
    name: string;
    description: string;
    inputSchema: object;
    outputSchema: object;
}

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
        return this.automationBridge.sendAutomationRequest('manage_input', {
            action: 'add_mapping',
            contextPath,
            actionPath,
            key
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
