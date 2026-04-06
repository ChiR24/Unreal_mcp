
import { spawn } from 'child_process';
import path from 'path';
import { fileURLToPath } from 'url';
import { consolidatedToolDefinitions } from '../src/tools/consolidated-tool-definitions.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const serverPath = path.join(__dirname, '../dist/cli.js');

console.log('🚬 Running Smoke Test (Mock Mode)...');
console.log(`🔌 Server Path: ${serverPath}`);

const env = { ...process.env, MOCK_UNREAL_CONNECTION: 'true' };

const child = spawn('node', [serverPath], {
    env,
    stdio: ['pipe', 'pipe', 'inherit'] // pipe stdin/stdout, inherit stderr
});

const requests = [
    {
        jsonrpc: '2.0',
        id: 1,
        method: 'initialize',
        params: {
            protocolVersion: '2024-11-05',
            capabilities: {},
            clientInfo: { name: 'smoke-test', version: '1.0' }
        }
    },
    {
        jsonrpc: '2.0',
        id: 2,
        method: 'tools/list',
        params: {}
    }
];

function getToolActionEnum(toolName: string): string[] {
    const tool = consolidatedToolDefinitions.find((definition) => definition.name === toolName);
    if (!tool) {
        throw new Error(`Missing consolidated tool definition for ${toolName}`);
    }

    const inputSchema = tool.inputSchema as { properties?: Record<string, unknown> };
    const actionSchema = inputSchema.properties?.action as { enum?: unknown } | undefined;
    if (!Array.isArray(actionSchema?.enum)) {
        throw new Error(`Tool ${toolName} is missing an action enum in the consolidated definitions`);
    }

    return actionSchema.enum.filter((entry): entry is string => typeof entry === 'string');
}

function selectRequiredActions(toolName: string, predicate: (action: string) => boolean): string[] {
    const actions = getToolActionEnum(toolName).filter(predicate);
    if (actions.length === 0) {
        throw new Error(`No required packaged actions selected for ${toolName}`);
    }

    return actions;
}

const REQUIRED_TOOLS = {
    manage_pipeline: {
        requiredActions: selectRequiredActions('manage_pipeline', (action) => action === 'list_categories' || action === 'get_status'),
        requiresOutputSchema: true
    },
    manage_blueprint: {
        requiredActions: selectRequiredActions('manage_blueprint', (action) => action === 'get_graph_details' || action === 'get_node_details_batch' || action === 'get_pin_details' || action === 'get_graph_review_summary'),
        requiresOutputSchema: true
    },
    manage_audio: {
        requiredActions: selectRequiredActions('manage_audio', (action) => action === 'create_metasound' || action === 'get_audio_info'),
        requiresOutputSchema: true
    },
    manage_effect: {
        requiredActions: selectRequiredActions('manage_effect', (action) => action === 'create_niagara_system' || action === 'get_niagara_info'),
        requiresOutputSchema: true
    },
    animation_physics: {
        requiredActions: selectRequiredActions('animation_physics', (action) => action === 'create_animation_blueprint' || action === 'create_control_rig'),
        requiresOutputSchema: true
    },
    manage_widget_authoring: {
        requiredActions: selectRequiredActions(
            'manage_widget_authoring',
            (action) => action === 'get_widget_tree' || action === 'get_widget_designer_state' || action === 'create_property_binding' || action.startsWith('bind_')
        ),
        requiresOutputSchema: true
    },
    manage_ui: {
        requiredActions: selectRequiredActions('manage_ui', (action) => action === 'list_visible_windows' || action === 'resolve_ui_target'),
        requiresOutputSchema: true
    },
    control_editor: {
        requiredActions: selectRequiredActions(
            'control_editor',
            (action) => [
                'screenshot',
                'simulate_input',
                'fit_blueprint_graph',
                'set_blueprint_graph_view',
                'jump_to_blueprint_node',
                'capture_blueprint_graph_review',
                'set_widget_blueprint_mode',
                'fit_widget_designer',
                'set_widget_designer_view',
                'select_widget_in_designer',
                'focus_editor_surface'
            ].includes(action)
        ),
        requiresOutputSchema: true
    }
};

function getActionEnum(tool: Record<string, unknown>): string[] {
    const inputSchema = tool.inputSchema as { properties?: Record<string, unknown> } | undefined;
    const actionSchema = inputSchema?.properties?.action as { enum?: unknown } | undefined;
    return Array.isArray(actionSchema?.enum)
        ? actionSchema.enum.filter((entry): entry is string => typeof entry === 'string')
        : [];
}

function hasActionableOutputSchema(tool: Record<string, unknown>): boolean {
    const outputSchema = tool.outputSchema;
    if (!outputSchema || typeof outputSchema !== 'object' || Array.isArray(outputSchema)) {
        return false;
    }

    const schema = outputSchema as { type?: unknown; properties?: unknown };
    return schema.type === 'object' && typeof schema.properties === 'object' && schema.properties !== null;
}

function validateToolsList(tools: unknown): void {
    if (!Array.isArray(tools)) {
        throw new Error('tools/list did not return a tools array');
    }

    for (const [toolName, requirements] of Object.entries(REQUIRED_TOOLS)) {
        const tool = tools.find((entry) => {
            return typeof entry === 'object' && entry !== null && (entry as { name?: unknown }).name === toolName;
        }) as Record<string, unknown> | undefined;

        if (!tool) {
            throw new Error(`Missing required tool from tools/list: ${toolName}`);
        }

        if (!tool.inputSchema || typeof tool.inputSchema !== 'object') {
            throw new Error(`Tool ${toolName} is missing an actionable inputSchema`);
        }

        const actionEnum = getActionEnum(tool);
        for (const action of requirements.requiredActions) {
            if (!actionEnum.includes(action)) {
                throw new Error(`Tool ${toolName} is missing required public action: ${action}`);
            }
        }

        if (requirements.requiresOutputSchema && !hasActionableOutputSchema(tool)) {
            throw new Error(`Tool ${toolName} is missing an actionable outputSchema`);
        }
    }
}

let buffer = '';
let passed = false;

child.stdout.on('data', (data) => {
    const chunk = data.toString();
    buffer += chunk;

    // Try to parse JSON lines
    const lines = buffer.split('\n');
    buffer = lines.pop() || ''; // Keep the incomplete last line

    for (const line of lines) {
        if (!line.trim()) continue;
        try {
            const msg = JSON.parse(line);
            console.log('Received:', JSON.stringify(msg).substring(0, 100) + '...');

            if (msg.id === 1 && msg.result) {
                console.log('✅ Initialize success');
                // Send list tools request
                child.stdin.write(JSON.stringify(requests[1]) + '\n');
            }

            if (msg.id === 2 && msg.result) {
                try {
                    validateToolsList(msg.result.tools);
                    console.log(`✅ Tools check success: Found ${msg.result.tools?.length || 0} tools`);
                    console.log('✅ Required public tool and action surface verified');
                    passed = true;
                    child.kill();
                } catch (error) {
                    console.error(`❌ Tools check failed: ${error instanceof Error ? error.message : String(error)}`);
                    child.kill();
                }
            }

        } catch (_e) {
            // Ignore non-JSON output (logs)
        }
    }
});

child.on('exit', (_code) => {
    if (passed) {
        console.log('🎉 Smoke Test PASSED');
        process.exit(0);
    } else {
        console.error('❌ Smoke Test FAILED - Server exited without passing checks');
        process.exit(1);
    }
});

// Start by sending initialize
console.log('Sending initialize...');
child.stdin.write(JSON.stringify(requests[0]) + '\n');

// Timeout safety
setTimeout(() => {
    if (!passed) {
        console.error('❌ Timeout waiting for smoke test');
        console.error('Buffer contents:', buffer);
        child.kill();
        process.exit(1);
    }
}, 15000);
