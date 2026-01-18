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
import { CustomToolDefinition, CustomToolSchema, CUSTOM_TOOL_CONSTRAINTS } from '../types/custom-tool.js';

function isPlainObject(value: unknown): value is Record<string, unknown> {
    return Boolean(value) && typeof value === 'object' && !Array.isArray(value);
}

const TOOLLIST_SCHEMA_DROP_KEYS = new Set([
    // pure decoration
    'title',
    'examples',
    'default',
    '$comment',
    // draft-specific or noisy
    'deprecated',
    'readOnly',
    'writeOnly',
    // validation constraints (LLM doesn't need these for tool selection)
    'minimum',
    'maximum',
    'minLength',
    'maxLength',
    'pattern',
    'format',
    'minItems',
    'maxItems',
    'uniqueItems',
    'exclusiveMinimum',
    'exclusiveMaximum',
    'multipleOf',
    // JSON Schema meta
    '$schema',
    '$id',
    '$ref',
    '$defs',
    'definitions',
    'additionalProperties',
    'additionalItems',
    'propertyNames',
    // AGGRESSIVE: Remove outputSchema entirely - LLM only needs input
    'outputSchema',
    // ULTRA-AGGRESSIVE: Remove items spec from arrays, category (internal)
    'category'
]);

function shouldKeepDescription(parentKey: string | undefined): boolean {
    if (!parentKey) return false;
    // ONLY keep action descriptions - critical for LLM to understand what actions do
    if (parentKey === 'action') return true;
    return false;
}

// ULTRA-AGGRESSIVE: Simplify schema objects for minimal token usage
// NOTE: MAX_ACTIONS_PER_TOOL truncation REMOVED - all actions must be visible to LLM

function simplifySchemaObject(schema: Record<string, unknown>, _parentKey?: string): Record<string, unknown> {
    const result = { ...schema };
    
    // Remove 'type: string' when enum is present (redundant)
    if (result['enum'] && result['type'] === 'string') {
        delete result['type'];
    }
    
    // ULTRA-AGGRESSIVE: Remove primitive types - LLM can infer from param names
    // Only keep type for arrays and objects, remove for string/number/boolean/integer
    const primitiveTypes = ['string', 'number', 'boolean', 'integer'];
    if (primitiveTypes.includes(result['type'] as string) && !result['enum']) {
        delete result['type'];
    }
    
    // Simplify arrays - just { type: 'array' } without items detail
    if (result['type'] === 'array') {
        delete result['items'];
        return { type: 'array' };
    }
    
    // Simplify location/rotation/scale/color objects - just { type: 'object' }
    if (result['type'] === 'object' && result['properties']) {
        const props = result['properties'] as Record<string, unknown>;
        const propKeys = Object.keys(props);
        // Common vector/color patterns - simplify to just type: object
        if ((propKeys.length === 3 && 
            (propKeys.every(k => ['x', 'y', 'z'].includes(k)) ||
             propKeys.every(k => ['r', 'g', 'b'].includes(k)) ||
             propKeys.every(k => ['pitch', 'yaw', 'roll'].includes(k)))) ||
            (propKeys.length === 4 && propKeys.every(k => ['r', 'g', 'b', 'a'].includes(k))) ||
            (propKeys.length === 2 && propKeys.every(k => ['x', 'y'].includes(k))) ||
            (propKeys.length === 6 && propKeys.every(k => ['x', 'y', 'z', 'pitch', 'yaw', 'roll'].includes(k)))) {
            return { type: 'object' };
        }
    }
    
    // Remove 'required' array if empty or only has 'action' (obvious requirement)
    if (result['required']) {
        const req = result['required'] as string[];
        if (req.length === 0 || (req.length === 1 && req[0] === 'action')) {
            delete result['required'];
        }
    }
    
    // Remove empty objects
    if (Object.keys(result).length === 0) {
        return {};
    }
    
    return result;
}

function pruneSchemaForToolList(schema: unknown, parentKey?: string): unknown {
    if (Array.isArray(schema)) {
        return schema.map(v => pruneSchemaForToolList(v, parentKey));
    }

    if (!isPlainObject(schema)) return schema;

    const out: Record<string, unknown> = {};
    for (const [k, v] of Object.entries(schema)) {
        if (TOOLLIST_SCHEMA_DROP_KEYS.has(k)) continue;

        if (k === 'description' && !shouldKeepDescription(parentKey)) {
            continue;
        }

        // Recurse with current key as parent
        out[k] = pruneSchemaForToolList(v, k);
    }

    // Apply simplification after pruning, passing parentKey for action truncation
    const simplified = simplifySchemaObject(out, parentKey);
    
    // SAFETY: Ensure root inputSchema ALWAYS has type: 'object'
    // This is required by strict MCP clients (Cursor/Trae)
    if (parentKey === undefined && isPlainObject(simplified)) {
        (simplified as Record<string, unknown>)['type'] = 'object';
    }
    
    return simplified;
}

function safeJsonByteLength(value: unknown): number {
    try {
        return Buffer.byteLength(JSON.stringify(value), 'utf8');
    } catch {
        return -1;
    }
}

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
    private customTools: Map<string, CustomToolDefinition> = new Map();

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

    /**
     * Register a custom tool with validation
     * @throws Error if validation fails or security constraints violated
     */
    public registerCustomTool(definition: CustomToolDefinition): void {
        const result = CustomToolSchema.safeParse(definition);
        if (!result.success) {
            throw new Error(`Invalid custom tool schema: ${result.error.message}`);
        }

        if (this.customTools.size >= CUSTOM_TOOL_CONSTRAINTS.maxTools) {
            throw new Error(`Maximum custom tools limit (${CUSTOM_TOOL_CONSTRAINTS.maxTools}) reached`);
        }

        for (const reserved of CUSTOM_TOOL_CONSTRAINTS.reservedNames) {
            if (definition.name.startsWith(reserved)) {
                throw new Error(`Tool name cannot start with reserved prefix: ${reserved}`);
            }
        }

        if (this.customTools.has(definition.name)) {
            throw new Error(`Custom tool already registered: ${definition.name}`);
        }

        this.customTools.set(definition.name, definition);
        this.logger.info(`Registered custom tool: ${definition.name}`);
    }

    /**
     * Get all registered custom tools
     */
    public getCustomTools(): CustomToolDefinition[] {
        return Array.from(this.customTools.values());
    }
    
    private normalizeToolArgs(_toolName: string, args: Record<string, unknown>): Record<string, unknown> {
        // Normalize common path-like parameters and accept a few common aliases.
        // This is intentionally conservative to avoid surprising behavior.
        const out: Record<string, unknown> = { ...args };

        const coerceStringPath = (value: unknown): string | undefined => {
            if (typeof value !== 'string') return undefined;
            let normalized = value.trim();
            if (normalized.length === 0) return undefined;

            normalized = normalized.replace(/\\/g, '/');
            normalized = normalized.replace(/\/+/g, '/');
            // Prefer Unreal roots; convert Content -> Game when obvious
            if (normalized.startsWith('/Content/')) {
                normalized = '/Game/' + normalized.slice('/Content/'.length);
            }
            if (!normalized.startsWith('/')) {
                normalized = '/' + normalized;
            }
            if (normalized.startsWith('/Game') && normalized !== '/Game' && !normalized.startsWith('/Game/')) {
                normalized = '/Game/' + normalized.slice('/Game'.length);
            }
            return normalized;
        };

        const pathKeys = [
            'assetPath', 'blueprintPath', 'meshPath', 'texturePath', 'materialPath', 'soundPath', 'animationPath', 'levelPath', 'sequencePath',
            'directoryPath', 'directory', 'path', 'packagePath', 'exportPath', 'sourcePath', 'destinationPath', 'outputPath', 'savePath',
            'actorPath', 'systemPath', 'emitterPath', 'wavePath'
        ];

        for (const k of pathKeys) {
            const v = out[k];
            const normalized = coerceStringPath(v);
            if (normalized) out[k] = normalized;
        }

        // Normalize arrays of paths when present
        const arrayPathKeys = ['assetPaths', 'levelPaths', 'packagePaths', 'classNames'];
        for (const k of arrayPathKeys) {
            const v = out[k];
            if (Array.isArray(v)) {
                out[k] = v.map(item => coerceStringPath(item) ?? item);
            }
        }

        // Aliases: map common alternative names to canonical ones when canonical not provided
        // Example: some callers use "uePath" or "asset"; accept as assetPath.
        if (out.assetPath === undefined) {
            const alias = out.uePath ?? out.asset ?? out.asset_path;
            const normalized = coerceStringPath(alias);
            if (normalized) out.assetPath = normalized;
        }

        return out;
    }

    private async handlePipelineCall(args: Record<string, unknown>) {
        const action = args.action as string;
        const VALID_CATEGORIES = ['core', 'world', 'authoring', 'gameplay', 'utility', 'all'] as const;
        
        if (action === 'set_categories') {
            const newCats = Array.isArray(args.categories) ? args.categories as string[] : [];
            
            // Validate category names to prevent self-lock bug
            const invalidCats = newCats.filter(c => !VALID_CATEGORIES.includes(c as typeof VALID_CATEGORIES[number]));
            if (invalidCats.length > 0) {
                this.logger.warn(`Invalid categories rejected: ${invalidCats.join(', ')}`);
                return { 
                    success: false, 
                    error: 'INVALID_CATEGORIES', 
                    message: `Invalid category names: ${invalidCats.join(', ')}. Valid categories: ${VALID_CATEGORIES.join(', ')}`,
                    validCategories: [...VALID_CATEGORIES]
                };
            }
            
            this.currentCategories = newCats.length > 0 ? newCats : ['all'];
            this.logger.info(`MCP Categories updated to: ${this.currentCategories.join(', ')}`);
            
            // Trigger list_changed notification
            this.server.notification({
                method: 'notifications/tools/list_changed',
                params: {}
            }).catch(err => this.logger.error('Failed to send list_changed notification', err));

            return { success: true, message: `Categories updated to ${this.currentCategories.join(', ')}`, categories: this.currentCategories };
        } else if (action === 'list_categories') {
            return { 
                success: true, 
                categories: this.currentCategories, 
                available: [...VALID_CATEGORIES] 
            };
        } else if (action === 'get_status') {
            return { 
                success: true, 
                categories: this.currentCategories,
                toolCount: consolidatedToolDefinitions.length,
                filteredCount: consolidatedToolDefinitions.filter((t: ToolDefinition) => !t.category || this.currentCategories.includes(t.category) || this.currentCategories.includes('all')).length
            };
        }
        return { success: false, error: `Unknown pipeline action: ${action}` };
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
                } catch (e: unknown) {
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
                // Phase E3: Robust client info detection with fallback chain
                // Try documented APIs first, then fall back to private property
                const serverObj = this.server as unknown as Record<string, unknown>;
                
                // Try getClientInfo() - future documented API
                if (typeof serverObj.getClientInfo === 'function') {
                    const clientInfo = (serverObj.getClientInfo as () => { name?: string } | undefined)();
                    clientName = clientInfo?.name;
                }
                // Try getClientVersion() - alternate API name
                else if (typeof serverObj.getClientVersion === 'function') {
                    const clientInfo = (serverObj.getClientVersion as () => { name?: string } | undefined)();
                    clientName = clientInfo?.name;
                }
                // Fallback: access private _clientVersion (current SDK behavior)
                else if ('_clientVersion' in serverObj) {
                    const clientInfo = serverObj._clientVersion as { name?: string } | undefined;
                    clientName = clientInfo?.name;
                }
                // Last resort: check _clientInfo
                else if ('_clientInfo' in serverObj) {
                    const clientInfo = serverObj._clientInfo as { name?: string } | undefined;
                    clientName = clientInfo?.name;
                }
                
                supportsListChanged = clientSupportsListChanged(clientName);
                this.logger.debug(`Client detection: name=${clientName}, supportsListChanged=${supportsListChanged}`);
            } catch (_e) {
                // Graceful degradation - assume no listChanged support for unknown clients
                supportsListChanged = false;
                this.logger.debug('Client version detection failed, assuming no listChanged support');
            }

            // If client doesn't support dynamic loading, show ALL tools (backward compatibility)
            // If client supports it AND categories don't include 'all', apply filtering
            const effectiveCategories = (!supportsListChanged || this.currentCategories.includes('all'))
                ? ['all']
                : this.currentCategories;

            this.logger.info(`Serving tools for categories: ${effectiveCategories.join(', ')} (client=${clientName || 'unknown'}, supportsListChanged=${supportsListChanged})`);
            
            // Filter by category AND hide configure_tools from clients that can't use it
            // AND hide experimental tools unless EXPERIMENTAL_TOOLS is enabled
            // SAFETY: Always expose configure_tools to prevent self-lock when invalid categories are set
            const filtered = consolidatedToolDefinitions
                .filter((t: ToolDefinition) => 
                    t.name === 'configure_tools' || !t.category || effectiveCategories.includes(t.category) || effectiveCategories.includes('all')
                )
                .filter((t: ToolDefinition) => 
                    supportsListChanged || t.name !== 'configure_tools'
                )
                .filter((t: ToolDefinition) =>
                    !t.experimental || config.EXPERIMENTAL_TOOLS
                );
            
             const sanitized = filtered.map((t: ToolDefinition) => {
                 try {
                     const copy = JSON.parse(JSON.stringify(t)) as Record<string, unknown>;
                     delete copy.outputSchema;

                     if (copy.inputSchema) {
                         copy.inputSchema = pruneSchemaForToolList(copy.inputSchema) as Record<string, unknown>;
                         // SAFETY: Force type: 'object' on the root inputSchema
                         // This is required by strict MCP clients and overrides any pruning logic
                         if (copy.inputSchema && typeof copy.inputSchema === 'object') {
                             (copy.inputSchema as Record<string, unknown>)['type'] = 'object';
                         }
                     }
                     
                      // DEBUG: Log the first tool's schema to see what's wrong
                      if (t.name === 'configure_tools' && this.logger.isDebugEnabled()) {
                          this.logger.debug(`configure_tools schema: ${JSON.stringify(copy.inputSchema)}`);
                      }

                     return copy;
                 } catch (_e) {
                     return t;
                 }
             });

             const totalBytes = safeJsonByteLength({ tools: sanitized });
             if (totalBytes > 0) {
                 this.logger.info(`tools/list payload size: ${totalBytes} bytes (tools=${sanitized.length})`);
             }

             return { tools: sanitized };

        });

        this.server.setRequestHandler(CallToolRequestSchema, async (request) => {
             const { name } = request.params;
             let args: Record<string, unknown> = request.params.arguments || {};

             // ---------------------------------
             // Argument normalization (quality):
             // - reduce invalid calls with corrected /Game paths and common aliases
             // ---------------------------------
             args = this.normalizeToolArgs(name, args);

             if (name === 'configure_tools') {
                 return { content: [{ type: 'text', text: JSON.stringify(await this.handlePipelineCall(args)) }] };
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
                } catch (e: unknown) {
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
                } catch { /* Success extraction failed - use fallback */ }

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
            } catch (error: unknown) {
                this.healthMonitor.trackPerformance(startTime, false);
                const errorResponse = ErrorHandler.createErrorResponse(error, name, { ...args, scope: `tool-call/${name}` });
                this.logger.error(`Tool execution failed: ${name}`, errorResponse);
                this.healthMonitor.recordError(errorResponse);

                const sanitizedError = cleanObject(errorResponse);
                try {
                    sanitizedError.isError = true;
                } catch { /* isError assignment failed - continue */ }
                return responseValidator.wrapResponse(name, sanitizedError);
            }
        });
    }
}
