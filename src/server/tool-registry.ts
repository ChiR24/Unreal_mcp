import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { CallToolRequestSchema, ListToolsRequestSchema } from '@modelcontextprotocol/sdk/types.js';
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { Logger } from '../utils/logger.js';
import { HealthMonitor } from '../services/health-monitor.js';
import { consolidatedToolDefinitions, ToolDefinition } from '../tools/consolidated-tool-definitions.js';
import { handleConsolidatedToolCall } from '../tools/consolidated-tool-handlers.js';
import { responseValidator } from '../utils/response-validator.js';
import { ErrorHandler } from '../utils/error-handler.js';
import { cleanObject } from '../utils/safe-json.js';
import { createElicitationHelper, PrimitiveSchema } from '../utils/elicitation.js';
import { AssetResources } from '../resources/assets.js';
import { ActorResources } from '../resources/actors.js';
import { LevelResources } from '../resources/levels.js';
import { ActorTools } from '../tools/actors.js';
import { AssetTools } from '../tools/assets.js';
import { EditorTools } from '../tools/editor.js';
import { MaterialTools } from '../tools/materials.js';
import { AnimationTools } from '../tools/animation.js';
import { PhysicsTools } from '../tools/physics.js';
import { NiagaraTools } from '../tools/niagara.js';
import { BlueprintTools } from '../tools/blueprint.js';
import { LevelTools } from '../tools/level.js';
import { LightingTools } from '../tools/lighting.js';
import { LandscapeTools } from '../tools/landscape.js';
import { FoliageTools } from '../tools/foliage.js';
import { EnvironmentTools } from '../tools/environment.js';
import { DebugVisualizationTools } from '../tools/debug.js';
import { PerformanceTools } from '../tools/performance.js';
import { AudioTools } from '../tools/audio.js';
import { UITools } from '../tools/ui.js';
import { SequenceTools } from '../tools/sequence.js';
import { IntrospectionTools } from '../tools/introspection.js';
import { EngineTools } from '../tools/engine.js';
import { BehaviorTreeTools } from '../tools/behavior-tree.js';
import { InputTools } from '../tools/input.js';

import { LogTools } from '../tools/logs.js';
import { getProjectSetting } from '../utils/ini-reader.js';
import { config } from '../config.js';
import { mcpClients } from 'mcp-client-capabilities';
import { dynamicToolManager } from '../tools/dynamic-tool-manager.js';

// Parse default categories from config
function parseDefaultCategories(): string[] {
    const raw = config.MCP_DEFAULT_CATEGORIES || 'core';
    const cats = raw.split(',').map(c => c.trim().toLowerCase()).filter(c => c.length > 0);
    return cats.length > 0 ? cats : ['core'];
}

// Check if a client supports tools.listChanged based on known client capabilities
function clientSupportsListChanged(clientName: string | undefined): boolean {
    if (!clientName) return false;
    
    // Normalize client name (lowercase, trim)
    const normalizedName = clientName.toLowerCase().trim();
    
    // Check in the mcp-client-capabilities database
    for (const [key, clientInfo] of Object.entries(mcpClients)) {
        if (key.toLowerCase() === normalizedName || 
            (clientInfo.title && clientInfo.title.toLowerCase() === normalizedName)) {
            // Check if tools.listChanged is supported
            const tools = clientInfo.tools as { listChanged?: boolean } | undefined;
            return Boolean(tools?.listChanged);
        }
    }
    
    // Fallback: check for known clients by partial name match
    const knownDynamicClients = ['cursor', 'cline', 'windsurf', 'kilo', 'opencode', 'vscode', 'visual studio code'];
    for (const known of knownDynamicClients) {
        if (normalizedName.includes(known)) return true;
    }
    
    return false;
}

export class ToolRegistry {
    private defaultElicitationTimeoutMs = 60000;
    private currentCategories: string[] = parseDefaultCategories();

    constructor(
        private server: Server,
        private bridge: UnrealBridge,
        private automationBridge: AutomationBridge,
        private logger: Logger,
        private healthMonitor: HealthMonitor,
        private assetResources: AssetResources,
        private actorResources: ActorResources,
        private levelResources: LevelResources,
        private ensureConnected: () => Promise<boolean>
    ) { }
    
    private async handlePipelineCall(args: Record<string, unknown>) {
        const action = args.action as string;
        if (action === 'set_categories') {
            const newCats = Array.isArray(args.categories) ? args.categories as string[] : [];
            this.currentCategories = newCats.length > 0 ? newCats : ['all'];
            this.logger.info(`MCP Categories updated to: ${this.currentCategories.join(', ')}`);
            
            // Trigger list_changed notification
            this.server.notification({
                method: 'notifications/tools/list_changed',
                params: {}
            }).catch(err => this.logger.error('Failed to send list_changed notification', err));

            return { success: true, message: `Categories updated to ${this.currentCategories.join(', ')}`, categories: this.currentCategories };
        } else if (action === 'list_categories') {
            const categories = dynamicToolManager.listCategories();
            return { 
                success: true, 
                categories: this.currentCategories, 
                available: ['core', 'world', 'authoring', 'gameplay', 'utility', 'all'],
                categoryDetails: categories.map(c => ({
                    name: c.name,
                    enabled: c.enabled,
                    toolCount: c.toolCount,
                    enabledCount: c.enabledCount
                }))
            };
        } else if (action === 'get_status') {
            const status = dynamicToolManager.getStatus();
            return { 
                success: true, 
                categories: this.currentCategories,
                toolCount: status.totalTools,
                enabledCount: status.enabledTools,
                disabledCount: status.disabledTools,
                filteredCount: dynamicToolManager.getEnabledToolDefinitions().length
            };
        }
        return { success: false, error: `Unknown pipeline action: ${action}` };
    }

    private async handleManageToolsCall(args: Record<string, unknown>): Promise<Record<string, unknown>> {
        const action = args.action as string;
        type ToolCategory = 'core' | 'world' | 'authoring' | 'gameplay' | 'utility';
        
        // Helper to safely extract string array
        const getStringArray = (key: string): string[] => {
            const val = args[key];
            if (Array.isArray(val)) {
                return val.filter((v): v is string => typeof v === 'string');
            }
            return [];
        };

        // Helper to safely extract string
        const getString = (key: string): string | undefined => {
            const val = args[key];
            return typeof val === 'string' ? val : undefined;
        };

        switch (action) {
            case 'list_tools': {
                const toolStates = dynamicToolManager.listTools();
                const tools = toolStates.map(state => ({
                    name: state.name,
                    enabled: state.enabled,
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

            case 'list_categories': {
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

            case 'enable_tools': {
                const toolNames = getStringArray('tools');
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

            case 'disable_tools': {
                const toolNames = getStringArray('tools');
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

            case 'enable_category': {
                const category = getString('category') as ToolCategory | undefined;
                if (!category) {
                    return { success: false, error: 'No category specified.', errorCode: 'MISSING_CATEGORY' };
                }
                const validCategories: ToolCategory[] = ['core', 'world', 'authoring', 'gameplay', 'utility'];
                if (!validCategories.includes(category)) {
                    return { 
                        success: false, 
                        error: `Invalid category '${category}'. Valid: ${validCategories.join(', ')}`,
                        errorCode: 'INVALID_CATEGORY'
                    };
                }
                const result = dynamicToolManager.enableCategory(category);
                if (result.notFound) {
                    return { success: false, error: `Category '${category}' not found`, errorCode: 'CATEGORY_NOT_FOUND' };
                }
                return {
                    success: true,
                    category,
                    enabled: result.enabled,
                    message: `Enabled category '${category}' (${result.enabled.length} tools)`
                };
            }

            case 'disable_category': {
                const category = getString('category') as ToolCategory | undefined;
                if (!category) {
                    return { success: false, error: 'No category specified.', errorCode: 'MISSING_CATEGORY' };
                }
                const validCategories: ToolCategory[] = ['core', 'world', 'authoring', 'gameplay', 'utility'];
                if (!validCategories.includes(category)) {
                    return { 
                        success: false, 
                        error: `Invalid category '${category}'. Valid: ${validCategories.join(', ')}`,
                        errorCode: 'INVALID_CATEGORY'
                    };
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
                return {
                    success: true,
                    category,
                    disabled: result.disabled,
                    protected: result.protected,
                    message: `Disabled category '${category}' (${result.disabled.length} tools disabled)`
                };
            }

            case 'get_status': {
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
                    message: `${status.enabledTools}/${status.totalTools} tools enabled`
                };
            }

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

    register() {
        // Initialize tools
        const actorTools = new ActorTools(this.bridge);
        const assetTools = new AssetTools(this.bridge);
        const editorTools = new EditorTools(this.bridge);
        const materialTools = new MaterialTools(this.bridge);
        const animationTools = new AnimationTools(this.bridge);
        const physicsTools = new PhysicsTools(this.bridge);
        const niagaraTools = new NiagaraTools(this.bridge);
        const blueprintTools = new BlueprintTools(this.bridge);
        const levelTools = new LevelTools(this.bridge);
        const lightingTools = new LightingTools(this.bridge);
        const landscapeTools = new LandscapeTools(this.bridge);
        const foliageTools = new FoliageTools(this.bridge);
        const environmentTools = new EnvironmentTools(this.bridge);
        const debugTools = new DebugVisualizationTools(this.bridge);
        const performanceTools = new PerformanceTools(this.bridge);
        const audioTools = new AudioTools(this.bridge);
        const uiTools = new UITools(this.bridge);
        const sequenceTools = new SequenceTools(this.bridge);
        const introspectionTools = new IntrospectionTools(this.bridge);
        const engineTools = new EngineTools(this.bridge);
        const behaviorTreeTools = new BehaviorTreeTools(this.bridge);

        const inputTools = new InputTools();
        const logTools = new LogTools(this.bridge);

        // Wire AutomationBridge
        const toolsWithAutomation = [
            materialTools, animationTools, physicsTools, niagaraTools,
            lightingTools, landscapeTools, foliageTools, debugTools,
            performanceTools, audioTools, uiTools, introspectionTools,

            engineTools, environmentTools, inputTools
        ];
        toolsWithAutomation.forEach(t => t.setAutomationBridge(this.automationBridge));

        // Lightweight system tools facade
        const systemTools = {
            executeConsoleCommand: (command: string) => this.bridge.executeConsoleCommand(command),
            getProjectSettings: async (section?: string) => {
                const category = typeof section === 'string' && section.trim().length > 0 ? section.trim() : 'Project';
                if (!this.automationBridge || !this.automationBridge.isConnected()) {
                    // Fallback to reading from disk
                    if (process.env.UE_PROJECT_PATH) {
                        try {
                            const settings = await getProjectSetting(process.env.UE_PROJECT_PATH, category, '');
                            return {
                                success: true as const,
                                section: category,
                                settings: settings || {},
                                source: 'disk'
                            };
                        } catch (_diskErr) {
                            return { success: false as const, error: 'Automation bridge not connected and disk read failed', section: category };
                        }
                    }
                    return { success: false as const, error: 'Automation bridge not connected', section: category };
                }
                try {
                    const resp = await this.automationBridge.sendAutomationRequest('system_control', {
                        action: 'get_project_settings',
                        category
                    }, { timeoutMs: 30000 }) as Record<string, unknown>;

                    const rawError = (resp?.error || '').toString();
                    const msgLower = (resp?.message || '').toString().toLowerCase();

                    const isNotImplemented = rawError.toUpperCase() === 'NOT_IMPLEMENTED' || msgLower.includes('not implemented');

                    if (!resp || resp.success === false) {
                        if (isNotImplemented) {
                            // Fallback to reading from disk
                            if (process.env.UE_PROJECT_PATH) {
                                try {
                                    const settings = await getProjectSetting(process.env.UE_PROJECT_PATH, category, '');
                                    return {
                                        success: true as const,
                                        section: category,
                                        settings: settings || {},
                                        source: 'disk'
                                    };
                                } catch (_diskErr) {
                                    // Ignore and fall through to stub
                                }
                            }

                            return {
                                success: true as const,
                                section: category,
                                settings: {
                                    category,
                                    available: false,
                                    note: 'Project settings are not exposed by the current runtime but validation can proceed.'
                                }
                            };
                        }

                        return {
                            success: false as const,
                            error: rawError || resp?.message || 'Failed to get project settings',
                            section: category,
                            settings: resp?.result
                        };
                    }

                    const result = resp.result && typeof resp.result === 'object' ? (resp.result as Record<string, unknown>) : {};
                    const settings = (result.settings && typeof result.settings === 'object') ? (result.settings as Record<string, unknown>) : result;

                    return {
                        success: true as const,
                        section: category,
                        settings
                    };
                } catch (e) {
                    // Fallback to reading from disk on error
                    if (process.env.UE_PROJECT_PATH) {
                        try {
                            const settings = await getProjectSetting(process.env.UE_PROJECT_PATH, category, '');
                            return {
                                success: true as const,
                                section: category,
                                settings: settings || {},
                                source: 'disk'
                            };
                        } catch (_diskErr) {
                            // Ignore
                        }
                    }
                    return {
                        success: false as const,
                        error: `Failed to get project settings: ${e instanceof Error ? e.message : String(e)}`,
                        section: category
                    };
                }
            }
        };

        const elicitation = createElicitationHelper(this.server, this.logger);

        this.server.setRequestHandler(ListToolsRequestSchema, async () => {
            // Check if client supports listChanged based on client name from initialization
            let supportsListChanged = false;
            let clientName: string | undefined;
            try {
                // Get client info - the server stores this from the initialize request
                // Note: _clientVersion is a private SDK property (fragile but necessary)
                const serverObj = this.server as unknown as Record<string, unknown>;
                const clientInfo = serverObj._clientVersion as { name?: string } | undefined;
                clientName = clientInfo?.name;
                supportsListChanged = clientSupportsListChanged(clientName);
                this.logger.debug(`Client detection: name=${clientName}, supportsListChanged=${supportsListChanged}`);
            } catch (_e) {
                supportsListChanged = false;
            }

            // If client doesn't support dynamic loading, show ALL tools (backward compatibility)
            // If client supports it AND categories don't include 'all', apply filtering
            const effectiveCategories = (!supportsListChanged || this.currentCategories.includes('all'))
                ? ['all']
                : this.currentCategories;

            this.logger.info(`Serving tools for categories: ${effectiveCategories.join(', ')} (client=${clientName || 'unknown'}, supportsListChanged=${supportsListChanged})`);
            
            // Use DynamicToolManager for filtering
            const allTools = dynamicToolManager.getAllToolDefinitions();
            const status = dynamicToolManager.getStatus();
            
            // Filter by: 1) tool enabled in DynamicToolManager, 2) category, 3) hide manage_pipeline from non-dynamic clients
            const filtered = allTools
                .filter((t: ToolDefinition) => {
                    // Check if tool is enabled
                    if (!dynamicToolManager.isToolEnabled(t.name)) return false;
                    
                    // Check category filter
                    const category = t.category;
                    if (category && !effectiveCategories.includes(category) && !effectiveCategories.includes('all')) {
                        return false;
                    }
                    
                    // Hide manage_pipeline from clients that can't use it
                    if (!supportsListChanged && t.name === 'manage_pipeline') return false;
                    
                    return true;
                });
            
            this.logger.debug(`Tool filtering: ${status.enabledTools}/${status.totalTools} enabled, ${filtered.length} visible`);
            
            const sanitized = filtered.map((t: ToolDefinition) => {
                try {
                    const copy = JSON.parse(JSON.stringify(t)) as Record<string, unknown>;
                    delete copy.outputSchema;
                    return copy;
                } catch (_e) {
                    return t;
                }
            });
            return { tools: sanitized };
        });

        this.server.setRequestHandler(CallToolRequestSchema, async (request) => {
            const { name } = request.params;
            let args: Record<string, unknown> = request.params.arguments || {};

            if (name === 'manage_pipeline') {
                return { content: [{ type: 'text', text: JSON.stringify(await this.handlePipelineCall(args)) }] };
            }

            // Handle manage_tools for dynamic tool management
            if (name === 'manage_tools') {
                const result = await this.handleManageToolsCall(args);
                // Trigger list_changed notification if tools were modified
                const action = args.action as string;
                if (['enable_tools', 'disable_tools', 'enable_category', 'disable_category', 'reset'].includes(action || '')) {
                    this.server.notification({
                        method: 'notifications/tools/list_changed',
                        params: {}
                    }).catch(err => this.logger.error('Failed to send list_changed notification', err));
                }
                return { content: [{ type: 'text', text: JSON.stringify(result) }] };
            }

            const startTime = Date.now();

            const connected = await this.ensureConnected();
            if (!connected) {
                // Allow certain tools (pipeline, system checks) to run without connection
                if (name === 'system_control' && args.action === 'get_project_settings') {
                    // Allowed
                } else {
                    this.healthMonitor.trackPerformance(startTime, false);
                    return {
                        content: [{ type: 'text', text: `Cannot execute tool '${name}': Unreal Engine is not connected.` }],
                        isError: true
                    };
                }
            }

            const tools = {
                actorTools, assetTools, materialTools, editorTools, animationTools,
                physicsTools, niagaraTools, blueprintTools, levelTools, lightingTools,
                landscapeTools, foliageTools, environmentTools, debugTools, performanceTools,
                audioTools, systemTools, uiTools, sequenceTools, introspectionTools,

                engineTools, behaviorTreeTools, inputTools, logTools,
                elicit: elicitation.elicit,
                supportsElicitation: elicitation.supports,
                elicitationTimeoutMs: this.defaultElicitationTimeoutMs,
                assetResources: this.assetResources,
                actorResources: this.actorResources,
                levelResources: this.levelResources,
                bridge: this.bridge,
                automationBridge: this.automationBridge
            };

            try {
                this.logger.debug(`Executing tool: ${name}`);

                // ... Elicitation logic ...
                try {
                    const toolDef = (consolidatedToolDefinitions as Array<Record<string, unknown>>).find(t => t.name === name) as Record<string, unknown> | undefined;
                    const inputSchema = toolDef?.inputSchema as Record<string, unknown> | undefined;
                    const elicitFn = tools.elicit;
                    if (inputSchema && typeof elicitFn === 'function') {
                        const props = (inputSchema.properties || {}) as Record<string, Record<string, unknown>>;
                        const required: string[] = Array.isArray(inputSchema.required) ? inputSchema.required as string[] : [];
                        const missing = required.filter((k: string) => {
                            const v = (args as Record<string, unknown>)[k];
                            if (v === undefined || v === null) return true;
                            if (typeof v === 'string' && v.trim() === '') return true;
                            return false;
                        });

                        const primitiveProps: Record<string, PrimitiveSchema> = {};
                        for (const k of missing) {
                            const p = props[k];
                            if (!p || typeof p !== 'object') continue;
                            const t = (p.type || '').toString();
                            const isEnum = Array.isArray(p.enum);
                            if (t === 'string' || t === 'number' || t === 'integer' || t === 'boolean' || isEnum) {
                                // Build schema with proper type casting
                                const schemaType = (isEnum ? 'string' : t) as 'string' | 'number' | 'integer' | 'boolean';
                                primitiveProps[k] = {
                                    type: schemaType,
                                    title: typeof p.title === 'string' ? p.title : undefined,
                                    description: typeof p.description === 'string' ? p.description : undefined,
                                    enum: Array.isArray(p.enum) ? (p.enum as string[]) : undefined,
                                    enumNames: Array.isArray(p.enumNames) ? (p.enumNames as string[]) : undefined,
                                    minimum: typeof p.minimum === 'number' ? p.minimum : undefined,
                                    maximum: typeof p.maximum === 'number' ? p.maximum : undefined,
                                    minLength: typeof p.minLength === 'number' ? p.minLength : undefined,
                                    maxLength: typeof p.maxLength === 'number' ? p.maxLength : undefined,
                                    pattern: typeof p.pattern === 'string' ? p.pattern : undefined,
                                    format: typeof p.format === 'string' ? (p.format as 'email' | 'uri' | 'date' | 'date-time') : undefined,
                                    default: (typeof p.default === 'string' || typeof p.default === 'number' || typeof p.default === 'boolean') ? p.default : undefined
                                } as PrimitiveSchema;
                            }
                        }

                        if (Object.keys(primitiveProps).length > 0) {
                            const elicitOptions: Record<string, unknown> = { fallback: async () => ({ ok: false, error: 'missing-params' }) };
                            if (typeof tools.elicitationTimeoutMs === 'number' && Number.isFinite(tools.elicitationTimeoutMs)) {
                                elicitOptions.timeoutMs = tools.elicitationTimeoutMs;
                            }
                            const elicitRes = await elicitFn(
                                `Provide missing parameters for ${name}`,
                                { type: 'object', properties: primitiveProps, required: Object.keys(primitiveProps) },
                                elicitOptions
                            );
                            if (elicitRes && elicitRes.ok && elicitRes.value) {
                                args = { ...args, ...elicitRes.value };
                            }
                        }
                    }
                } catch (e) {
                    const errObj = e as Record<string, unknown> | null;
                    this.logger.debug('Generic elicitation prefill skipped', { err: errObj?.message ? String(errObj.message) : String(e) });
                }

                let result = await handleConsolidatedToolCall(name, args, tools);
                this.logger.debug(`Tool ${name} returned result`);
                result = cleanObject(result);

                const resultObj = result as Record<string, unknown> | null;
                const explicitSuccess = typeof resultObj?.success === 'boolean' ? Boolean(resultObj.success) : undefined;
                const wrappedResult = await responseValidator.wrapResponse(name, result);

                let wrappedSuccess: boolean | undefined = undefined;
                try {
                    const wrappedObj = wrappedResult as Record<string, unknown>;
                    const sc = wrappedObj.structuredContent as Record<string, unknown> | undefined;
                    if (sc && typeof sc.success === 'boolean') wrappedSuccess = Boolean(sc.success);
                } catch { }

                const wrappedResultObj = wrappedResult as Record<string, unknown>;
                const isErrorResponse = Boolean(wrappedResultObj?.isError === true);
                const tentative = explicitSuccess ?? wrappedSuccess;
                const finalSuccess = tentative === true && !isErrorResponse;

                this.healthMonitor.trackPerformance(startTime, finalSuccess);

                const durationMs = Date.now() - startTime;
                if (finalSuccess) {
                    this.logger.info(`Tool ${name} completed successfully in ${durationMs}ms`);
                } else {
                    this.logger.warn(`Tool ${name} completed with errors in ${durationMs}ms`);
                }

                const responsePreview = JSON.stringify(wrappedResult).substring(0, 100);
                this.logger.debug(`Returning response to MCP client: ${responsePreview}...`);

                return wrappedResult;
            } catch (error) {
                this.healthMonitor.trackPerformance(startTime, false);
                const errorResponse = ErrorHandler.createErrorResponse(error, name, { ...args, scope: `tool-call/${name}` });
                this.logger.error(`Tool execution failed: ${name}`, errorResponse);
                this.healthMonitor.recordError(errorResponse as unknown as Record<string, unknown>);

                const sanitizedError = cleanObject(errorResponse) as unknown as Record<string, unknown>;
                try {
                    sanitizedError.isError = true;
                } catch { }
                return responseValidator.wrapResponse(name, sanitizedError);
            }
        });
    }
}
