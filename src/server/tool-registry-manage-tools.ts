import { CATALOG_REVISION } from '../tools/catalog/capabilities/generated/canonical-registry.generated.js';
import { dynamicToolManager, type ToolCategory } from '../tools/dynamic/dynamic-tool-manager.js';
import type {
    CategoryDisableResult,
    CategoryEnableResult,
    CategoryState,
    DisableToolsResult,
    EnableToolsResult,
    ToolState
} from '../tools/dynamic/dynamic-tool-types.js';
import { SessionConfigureStore } from './mcp-primitives/session-configure-store.js';
import type { ClientCapabilityProfile } from './mcp-primitives/session-capability-profile.js';

const VALID_TOOL_CATEGORIES: ToolCategory[] = ['core', 'world', 'gameplay', 'utility', 'all'];
export const TOOL_LIST_CHANGED_ACTIONS = new Set(['enable_tools', 'disable_tools', 'enable_category', 'disable_category', 'reset']);

// The stable session key for the single stdio transport. Task 37 injects real
// per-session ids for the native transport through the resolver seam below; until
// then every stdio call resolves to this key, which is deliberately mapped to the
// unchanged global dynamic manager so the existing gateway coupling is preserved.
export const STDIO_SESSION_ID = 'stdio';

// The per-session configure overlay store (implements the C1 CatalogRevisionReader
// write side). Only injected non-default sessions reach it; the global manager
// stays the immutable seed the overlays are cloned from.
export const sessionConfigureStore = new SessionConfigureStore();

// Injected seam: how manage_tools resolves the caller's session id WITHOUT the
// frozen gateway call site changing. Defaults to STDIO_SESSION_ID; Task 37
// replaces it with a native per-session resolver.
let resolveSessionId: () => string = () => STDIO_SESSION_ID;

export function setManageToolsSessionResolver(resolver: () => string): void {
    resolveSessionId = resolver;
}

export function resetManageToolsSessionResolver(): void {
    resolveSessionId = () => STDIO_SESSION_ID;
}

export function clearManageToolsSession(sessionId: string): boolean {
    return sessionConfigureStore.clearSession(sessionId);
}

// Task 37 seams. The primitive wiring injects (a) a hook fired after a session's
// visibility-changing configure action so the coalescer can fold an effective
// catalog revision advance into one resources/updated, and (b) a resolver so
// configure get_status can report the session's derived structural client
// profile. Both default to no-op/undefined, so a direct manage_tools call outside
// the wired server behaves exactly as before.
let onConfigureVisibilityChanged: (sessionId: string) => void = () => undefined;
export function setConfigureVisibilityHook(hook: (sessionId: string) => void): void {
    onConfigureVisibilityChanged = hook;
}

let clientProfileResolver: (sessionId: string) => ClientCapabilityProfile | undefined = () => undefined;
export function setClientProfileResolver(resolver: (sessionId: string) => ClientCapabilityProfile | undefined): void {
    clientProfileResolver = resolver;
}

/**
 * Uninstall both Task 37 seams, restoring the pre-wiring defaults.
 *
 * These are module-level singletons with last-writer-wins setters, so a second
 * wirePrimitives() silently took ownership of the first server's configure hook
 * and profile resolver — and tearing a server down left both pointing at its
 * disposed driver. `createServer` is public API and server-factory.test.ts
 * already builds two in one process, so this is reachable today.
 */
export function resetManageToolsHooks(): void {
    onConfigureVisibilityChanged = () => undefined;
    clientProfileResolver = () => undefined;
}

// The uniform visibility surface both targets expose. The global manager already
// satisfies it structurally (no session arg); the store is bound to a session id.
interface ConfigureTarget {
    listTools(): ToolState[];
    listCategories(): CategoryState[];
    enableTools(toolNames: string[]): EnableToolsResult;
    disableTools(toolNames: string[]): DisableToolsResult;
    enableCategory(category: ToolCategory): CategoryEnableResult;
    disableCategory(category: ToolCategory): CategoryDisableResult;
    reset(): { enabled: number };
    getStatus(): {
        totalTools: number;
        enabledTools: number;
        disabledTools: number;
        categories: CategoryState[];
        catalogStateRevision: number;
    };
    isToolEnabled(toolName: string): boolean;
}

function targetFor(sessionId: string): ConfigureTarget {
    if (sessionId === STDIO_SESSION_ID) {
        return dynamicToolManager;
    }
    return {
        listTools: () => sessionConfigureStore.listTools(sessionId),
        listCategories: () => sessionConfigureStore.listCategories(sessionId),
        enableTools: (toolNames) => sessionConfigureStore.enableTools(sessionId, toolNames),
        disableTools: (toolNames) => sessionConfigureStore.disableTools(sessionId, toolNames),
        enableCategory: (category) => sessionConfigureStore.enableCategory(sessionId, category),
        disableCategory: (category) => sessionConfigureStore.disableCategory(sessionId, category),
        reset: () => sessionConfigureStore.reset(sessionId),
        getStatus: () => sessionConfigureStore.getStatus(sessionId),
        isToolEnabled: (toolName) => sessionConfigureStore.isToolEnabled(sessionId, toolName)
    };
}

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
    const sessionId = resolveSessionId();
    const target = targetFor(sessionId);
    const result = dispatchManageToolsAction(action, target, sessionId, args);
    // A visibility-changing action may advance the session's catalog state
    // revision; notify the primitive wiring so a subscribed session receives a
    // single coalesced resources/updated (syncCatalog no-ops when nothing moved).
    if (TOOL_LIST_CHANGED_ACTIONS.has(action)) {
        onConfigureVisibilityChanged(sessionId);
    }
    return result;
}

function dispatchManageToolsAction(
    action: string,
    target: ConfigureTarget,
    sessionId: string,
    args: Record<string, unknown>
): Record<string, unknown> {
    switch (action) {
        case 'list_tools':
            return listTools(target);
        case 'list_categories':
            return listCategories(target);
        case 'enable_tools':
            return enableTools(target, args);
        case 'disable_tools':
            return disableTools(target, args);
        case 'enable_category':
            return setCategoryEnabled(target, args, true);
        case 'disable_category':
            return setCategoryEnabled(target, args, false);
        case 'get_status':
            return getStatus(target, sessionId);
        case 'reset': {
            const result = target.reset();
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

function listTools(target: ConfigureTarget): Record<string, unknown> {
    const toolStates = target.listTools();
    const tools = toolStates.map(state => ({
        name: state.name,
        enabled: target.isToolEnabled(state.name),
        category: state.category,
        description: state.description.substring(0, 100) + (state.description.length > 100 ? '...' : '')
    }));
    const status = target.getStatus();
    return {
        success: true,
        tools,
        totalTools: status.totalTools,
        enabledCount: status.enabledTools,
        disabledCount: status.disabledTools,
        message: `Listed ${tools.length} tools (${status.enabledTools} enabled, ${status.disabledTools} disabled)`
    };
}

function listCategories(target: ConfigureTarget): Record<string, unknown> {
    const categories = target.listCategories();
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

function enableTools(target: ConfigureTarget, args: Record<string, unknown>): Record<string, unknown> {
    const toolNames = getToolNames(args);
    if (toolNames.length === 0) {
        return { success: false, error: 'No tools specified. Provide tools array.', errorCode: 'MISSING_TOOLS' };
    }
    const result = target.enableTools(toolNames);
    return {
        success: true,
        enabled: result.enabled,
        notFound: result.notFound,
        message: result.notFound.length > 0
            ? `Enabled ${result.enabled.length} tools. ${result.notFound.length} not found.`
            : `Enabled ${result.enabled.length} tools`
    };
}

function disableTools(target: ConfigureTarget, args: Record<string, unknown>): Record<string, unknown> {
    const toolNames = getToolNames(args);
    if (toolNames.length === 0) {
        return { success: false, error: 'No tools specified. Provide tools array.', errorCode: 'MISSING_TOOLS' };
    }
    const result = target.disableTools(toolNames);
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

function setCategoryEnabled(target: ConfigureTarget, args: Record<string, unknown>, enabled: boolean): Record<string, unknown> {
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
        const result = target.enableCategory(category);
        if (result.notFound) {
            return { success: false, error: `Category '${category}' not found`, errorCode: 'CATEGORY_NOT_FOUND' };
        }
        return { success: true, category, enabled: result.enabled, message: `Enabled category '${category}' (${result.enabled.length} tools)` };
    }

    const result = target.disableCategory(category);
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

function getStatus(target: ConfigureTarget, sessionId: string): Record<string, unknown> {
    const status = target.getStatus();
    const clientProfile = clientProfileResolver(sessionId);
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
        ...(clientProfile !== undefined ? { clientProfile } : {}),
        message: `${status.enabledTools}/${status.totalTools} tools enabled`
    };
}
