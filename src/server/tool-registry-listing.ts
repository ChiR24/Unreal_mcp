import { dynamicToolManager } from '../tools/dynamic/dynamic-tool-manager.js';
import type { ToolDefinition } from '../tools/catalog/consolidated-tool-definitions.js';
import { unrealGatewayToolDefinition } from '../tools/catalog/unreal-gateway-definition.js';
import { isRecord } from '../utils/validation/type-guards.js';

export function buildSanitizedToolList(effectiveCategories: string[]) {
    const allTools = dynamicToolManager.getAllToolDefinitions();
    const filtered = allTools.filter((tool: ToolDefinition) => {
        if (!dynamicToolManager.isToolEnabled(tool.name)) return false;

        const category = tool.category;
        if (category && !effectiveCategories.includes(category) && !effectiveCategories.includes('all')) {
            return false;
        }

        return true;
    });

    return filtered.map((tool: ToolDefinition) => sanitizeToolDefinition(tool));
}

function sanitizeToolDefinition(tool: ToolDefinition) {
    const properties: Record<string, unknown> = {};
    const actionValues = new Set<string>();
    let actionDescription = 'Action to perform.';
    const sourceProperties = isRecord(tool.inputSchema.properties) ? tool.inputSchema.properties : {};

    for (const [name, schema] of Object.entries(sourceProperties)) {
        if (properties[name] === undefined) properties[name] = schema;
    }

    const sourceAction = isRecord(sourceProperties.action) ? sourceProperties.action : undefined;
    if (typeof sourceAction?.description === 'string') actionDescription = sourceAction.description;
    if (Array.isArray(sourceAction?.enum)) {
        for (const value of sourceAction.enum) {
            if (typeof value === 'string') actionValues.add(value);
        }
    }

    const actionSchema: Record<string, unknown> = {
        type: 'string',
        description: actionDescription
    };
    if (actionValues.size > 0) {
        actionSchema.enum = Array.from(actionValues);
    }
    properties.action = actionSchema;

    const parameterNames = Object.keys(properties).filter(name => name !== 'action');
    const parameterSummary = parameterNames.length > 0 ? ` Params by action: ${parameterNames.join(', ')}.` : '';
    const actionGuidance = ' Required: action. Select one enum value, then provide only parameters relevant to that action.';
    const sourceRequired = Array.isArray(tool.inputSchema.required)
        ? tool.inputSchema.required.filter((name): name is string => typeof name === 'string')
        : ['action'];
    const required = sourceRequired.includes('action') ? sourceRequired : ['action', ...sourceRequired];

    return {
        name: tool.name,
        description: `${tool.description}${actionGuidance}${parameterSummary}`,
        category: tool.category,
        inputSchema: {
            ...tool.inputSchema,
            type: 'object',
            properties,
            required,
            additionalProperties: tool.inputSchema.additionalProperties ?? true
        }
    };
}

// The single public MCP tool. `unreal` is the only addressable entry point; the
// canonical 23 parent tools stay internal. configure may change internal
// enable/disable state but must never alter this advertised list.
export function buildGatewayToolDefinition(): ToolDefinition {
    return {
        name: unrealGatewayToolDefinition.name,
        description: unrealGatewayToolDefinition.description,
        category: 'core',
        inputSchema: unrealGatewayToolDefinition.inputSchema,
        outputSchema: unrealGatewayToolDefinition.outputSchema
    };
}
