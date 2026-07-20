import { CATALOG_REVISION } from '../tools/catalog/capabilities/generated/canonical-registry.generated.js';
import { dynamicToolManager, type ToolCategory } from '../tools/dynamic/dynamic-tool-manager.js';

const VALID_TOOL_CATEGORIES: ToolCategory[] = ['core', 'world', 'gameplay', 'utility', 'all'];
export const TOOL_LIST_CHANGED_ACTIONS = new Set(['enable_tools', 'disable_tools', 'enable_category', 'disable_category', 'reset']);

function getStringArray(args: Record<string, unknown>, key: string): string[] {
    const val = args[key];
    if (Array.isArray(val)) {
        return val.filter((v): v is string => typeof v === 'string');
    }
    return [];
}

function getString(args: Record<string, unknown>, key: string): string | undefined {
    const val = args[key];
    return typeof val === 'string' ? val : undefined;
}

function getToolNames(args: Record<string, unknown>): string[] {
    const tools = getStringArray(args, 'tools');
    return tools.length > 0 ? tools : getStringArray(args, 'toolNames');
}

export async function handleManageToolsCall(args: Record<string, unknown>): Promise<Record<string, unknown>> {
    const action = args.action as string;

    switch (action) {
        case 'list_tools':
            return listTools();
        case 'list_categories':
            return listCategories();
        case 'enable_tools':
            return enableTools(args);
        case 'disable_tools':
            return disableTools(args);
        case 'enable_category':
            return setCategoryEnabled(args, true);
        case 'disable_category':
            return setCategoryEnabled(args, false);
        case 'get_status':
            return getStatus();
        case 'reset': {
            const result = dynamicToolManager.reset();
            return {
                success: true,
                enabled: result.enabled,
                message: `Reset complete. ${result.enabled} tools re-enabled.`
            };
        }
        default:
            return {
                success: false,
                error: `Unknown action: ${action}. Available: list_tools, list_categories, enable_tools, disable_tools, enable_category, disable_category, get_status, reset`,
                errorCode: 'UNKNOWN_ACTION'
            };
    }
}

function listTools(): Record<string, unknown> {
    const toolStates = dynamicToolManager.listTools();
    const tools = toolStates.map(state => ({
        name: state.name,
        enabled: dynamicToolManager.isToolEnabled(state.name),
        category: state.category,
        description: state.description.substring(0, 100) + (state.description.length > 100 ? '...' : '')
    }));
    const status = dynamicToolManager.getStatus();
    return {
        success: true,
        tools,
        totalTools: status.totalTools,
        enabledCount: status.enabledTools,
        disabledCount: status.disabledTools,
        message: `Listed ${tools.length} tools (${status.enabledTools} enabled, ${status.disabledTools} disabled)`
    };
}

function listCategories(): Record<string, unknown> {
    const categories = dynamicToolManager.listCategories();
    return {
        success: true,
        categories: categories.map(cat => ({
            name: cat.name,
            enabled: cat.enabled,
            toolCount: cat.toolCount,
            enabledCount: cat.enabledCount
        })),
        totalCategories: categories.length,
        message: `Listed ${categories.length} categories`
    };
}

function enableTools(args: Record<string, unknown>): Record<string, unknown> {
    const toolNames = getToolNames(args);
    if (toolNames.length === 0) {
        return { success: false, error: 'No tools specified. Provide tools array.', errorCode: 'MISSING_TOOLS' };
    }
    const result = dynamicToolManager.enableTools(toolNames);
    return {
        success: true,
        enabled: result.enabled,
        notFound: result.notFound,
        message: result.notFound.length > 0
            ? `Enabled ${result.enabled.length} tools. ${result.notFound.length} not found.`
            : `Enabled ${result.enabled.length} tools`
    };
}

function disableTools(args: Record<string, unknown>): Record<string, unknown> {
    const toolNames = getToolNames(args);
    if (toolNames.length === 0) {
        return { success: false, error: 'No tools specified. Provide tools array.', errorCode: 'MISSING_TOOLS' };
    }
    const result = dynamicToolManager.disableTools(toolNames);
    if (result.protected.length > 0 && result.disabled.length === 0) {
        return {
            success: false,
            error: `Cannot disable protected tools: ${result.protected.join(', ')}`,
            errorCode: 'PROTECTED_TOOLS'
        };
    }
    const messages: string[] = [];
    if (result.disabled.length > 0) messages.push(`Disabled ${result.disabled.length} tools`);
    if (result.notFound.length > 0) messages.push(`${result.notFound.length} not found`);
    if (result.protected.length > 0) messages.push(`${result.protected.length} protected`);
    return {
        success: true,
        disabled: result.disabled,
        notFound: result.notFound,
        protected: result.protected,
        message: messages.join('. ')
    };
}

function setCategoryEnabled(args: Record<string, unknown>, enabled: boolean): Record<string, unknown> {
    const category = getString(args, 'category') as ToolCategory | undefined;
    if (!category) {
        return { success: false, error: 'No category specified.', errorCode: 'MISSING_CATEGORY' };
    }
    if (!VALID_TOOL_CATEGORIES.includes(category)) {
        return {
            success: false,
            error: `Invalid category '${category}'. Valid: ${VALID_TOOL_CATEGORIES.join(', ')}`,
            errorCode: 'INVALID_CATEGORY'
        };
    }

    if (enabled) {
        const result = dynamicToolManager.enableCategory(category);
        if (result.notFound) {
            return { success: false, error: `Category '${category}' not found`, errorCode: 'CATEGORY_NOT_FOUND' };
        }
        return { success: true, category, enabled: result.enabled, message: `Enabled category '${category}' (${result.enabled.length} tools)` };
    }

    const result = dynamicToolManager.disableCategory(category);
    if (result.notFound) {
        return { success: false, error: `Category '${category}' not found`, errorCode: 'CATEGORY_NOT_FOUND' };
    }
    if (result.protected.length > 0 && result.disabled.length === 0) {
        return {
            success: false,
            error: `Cannot fully disable protected category '${category}'. Protected tools: ${result.protected.join(', ')}`,
            errorCode: 'PROTECTED_CATEGORY'
        };
    }
    return { success: true, category, disabled: result.disabled, protected: result.protected, message: `Disabled category '${category}' (${result.disabled.length} tools disabled)` };
}

function getStatus(): Record<string, unknown> {
    const status = dynamicToolManager.getStatus();
    return {
        success: true,
        totalTools: status.totalTools,
        enabledTools: status.enabledTools,
        disabledTools: status.disabledTools,
        categories: status.categories.map(cat => ({
            name: cat.name,
            enabled: cat.enabled,
            toolCount: cat.toolCount,
            enabledCount: cat.enabledCount
        })),
        catalogRevision: CATALOG_REVISION,
        catalogStateRevision: status.catalogStateRevision,
        message: `${status.enabledTools}/${status.totalTools} tools enabled`
    };
}
