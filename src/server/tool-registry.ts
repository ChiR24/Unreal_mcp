import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { CallToolRequestSchema, ListToolsRequestSchema } from '@modelcontextprotocol/sdk/types.js';
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { Logger } from '../utils/logging/logger.js';
import { HealthMonitor } from '../services/health-monitor.js';
import { handleConsolidatedToolCall } from '../tools/orchestration/consolidated-tool-handlers.js';
import { mergeActionParams } from '../tools/orchestration/consolidated-call-utils.js';
import { responseValidator } from '../utils/responses/response-validator.js';
import { ErrorHandler } from '../utils/responses/error-handler.js';
import { cleanObject } from '../utils/serialization/safe-json.js';
import { isRecord } from '../utils/validation/type-guards.js';
import { redactImagePayloadForLog } from '../utils/logging/log-redaction.js';
import { createElicitationHelper } from '../utils/interaction/elicitation.js';
import { AssetResources } from '../resources/assets.js';
import { ActorResources } from '../resources/actors.js';
import { LevelResources } from '../resources/levels.js';
import { getProjectSetting } from '../utils/config/ini-reader.js';
import { dynamicToolManager } from '../tools/dynamic/dynamic-tool-manager.js';
import {
    clientSupportsListChanged,
    getEffectiveCategories,
    parseDefaultCategories
} from './tool-registry-client.js';
import {
    handleManageToolsCall as handleManageToolsAction,
    TOOL_LIST_CHANGED_ACTIONS
} from './tool-registry-manage-tools.js';
import { buildSanitizedToolList } from './tool-registry-listing.js';
import { maybeElicitMissingArgs } from './tool-registry-elicitation.js';
import { DebugService } from '../debug/index.js';

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
        private debugService: DebugService,
        private ensureConnected: () => Promise<boolean>
    ) { }

    private async readProjectSettingsFromDisk(category: string): Promise<Record<string, unknown> | undefined> {
        if (!process.env.UE_PROJECT_PATH) return undefined;

        try {
            const settings = await getProjectSetting(process.env.UE_PROJECT_PATH, category, '');
            return {
                success: true as const,
                section: category,
                settings: settings || {},
                source: 'disk'
            };
        } catch (error) {
            const message = error instanceof Error ? error.message : String(error);
            this.logger.debug('Disk project settings fallback failed', { error: message, section: category });
            return undefined;
        }
    }

    register() {
        const systemTools = {
            executeConsoleCommand: (command: string) => this.bridge.executeConsoleCommand(command),
            getProjectSettings: async (section?: string) => {
                const category = typeof section === 'string' && section.trim().length > 0 ? section.trim() : 'Project';
                if (!this.automationBridge || !this.automationBridge.isConnected()) {
                    const diskSettings = await this.readProjectSettingsFromDisk(category);
                    if (diskSettings) {
                        return diskSettings;
                    }
                    const error = process.env.UE_PROJECT_PATH
                        ? 'Automation bridge not connected and disk read failed'
                        : 'Automation bridge not connected';
                    return { success: false as const, error, section: category };
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
                            const diskSettings = await this.readProjectSettingsFromDisk(category);
                            if (diskSettings) {
                                return diskSettings;
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
                    const diskSettings = await this.readProjectSettingsFromDisk(category);
                    if (diskSettings) {
                        return diskSettings;
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
            const clientInfo = Reflect.get(this.server, '_clientVersion');
            const clientName = isRecord(clientInfo) && typeof clientInfo.name === 'string' ? clientInfo.name : undefined;
            const supportsListChanged = clientSupportsListChanged(clientName);
            this.logger.debug(`Client detection: name=${clientName}, supportsListChanged=${supportsListChanged}`);

            const effectiveCategories = getEffectiveCategories(supportsListChanged, this.currentCategories);

            this.logger.info(`Serving tools for categories: ${effectiveCategories.join(', ')} (client=${clientName || 'unknown'}, supportsListChanged=${supportsListChanged})`);

            const status = dynamicToolManager.getStatus();
            const sanitized = buildSanitizedToolList(effectiveCategories);

            this.logger.debug(`Tool filtering: ${status.enabledTools}/${status.totalTools} enabled, ${sanitized.length} visible`);
            return { tools: sanitized };
        });

        this.server.setRequestHandler(CallToolRequestSchema, async (request) => {
            const { name } = request.params;
            let args: Record<string, unknown> = mergeActionParams(request.params.arguments || {});

            if (name === 'manage_tools') {
                const result = await handleManageToolsAction(args);
                const action = args.action as string;
                if (TOOL_LIST_CHANGED_ACTIONS.has(action)) {
                    this.server.notification({
                        method: 'notifications/tools/list_changed',
                        params: {}
                    }).catch((error: unknown) => {
                        this.logger.error('Failed to send list_changed notification', error instanceof Error ? error : String(error));
                    });
                }
                return { content: [{ type: 'text', text: JSON.stringify(result) }] };
            }

            if (!dynamicToolManager.getToolState(name)) {
                return {
                    content: [{ type: 'text', text: `Unknown tool: ${name}` }],
                    isError: true
                };
            }

            if (!dynamicToolManager.isToolEnabled(name)) {
                this.healthMonitor.trackPerformance(Date.now(), false);
                return {
                    content: [{ type: 'text', text: `Cannot execute tool '${name}': tool is disabled or not available.` }],
                    isError: true
                };
            }

            const startTime = Date.now();

            const canRunWithoutConnection = name.startsWith('debug_')
                || (name === 'system_control' && args.action === 'get_project_settings');
            const connected = canRunWithoutConnection || await this.ensureConnected();
            if (!connected && !canRunWithoutConnection) {
                this.healthMonitor.trackPerformance(startTime, false);
                return {
                    content: [{ type: 'text', text: `Cannot execute tool '${name}': Unreal Engine is not connected.` }],
                    isError: true
                };
            }

            const tools = {
                systemTools,
                elicit: elicitation.elicit,
                supportsElicitation: elicitation.supports,
                elicitationTimeoutMs: this.defaultElicitationTimeoutMs,
                assetResources: this.assetResources,
                actorResources: this.actorResources,
                levelResources: this.levelResources,
                bridge: this.bridge,
                automationBridge: this.automationBridge,
                debugService: this.debugService
            };

            try {
                this.logger.debug(`Executing tool: ${name}`);

                args = await maybeElicitMissingArgs(
                    name,
                    args,
                    tools.elicit,
                    this.defaultElicitationTimeoutMs,
                    this.logger
                );

                let result = await handleConsolidatedToolCall(name, args, tools);
                this.logger.debug(`Tool ${name} returned result`);
                result = cleanObject(result);

                const resultObj = result as Record<string, unknown> | null;
                const explicitSuccess = typeof resultObj?.success === 'boolean' ? Boolean(resultObj.success) : undefined;
                const wrappedResult = await responseValidator.wrapResponse(name, result);

                let wrappedSuccess: boolean | undefined = undefined;
                if (isRecord(wrappedResult.structuredContent)) {
                    const sc = wrappedResult.structuredContent;
                    if (sc && typeof sc.success === 'boolean') wrappedSuccess = Boolean(sc.success);
                }

                const isErrorResponse = Boolean(wrappedResult.isError === true);
                const tentative = explicitSuccess ?? wrappedSuccess;
                const finalSuccess = tentative === true && !isErrorResponse;

                this.healthMonitor.trackPerformance(startTime, finalSuccess);

                const durationMs = Date.now() - startTime;
                if (finalSuccess) {
                    this.logger.info(`Tool ${name} completed successfully in ${durationMs}ms`);
                } else {
                    this.logger.warn(`Tool ${name} completed with errors in ${durationMs}ms`);
                }

                if (this.logger.isEnabled('debug')) {
                    const responsePreview = JSON.stringify(redactImagePayloadForLog(wrappedResult)).substring(0, 100);
                    this.logger.debug(`Returning response to MCP client: ${responsePreview}...`);
                }

                return wrappedResult;
            } catch (error) {
                this.healthMonitor.trackPerformance(startTime, false);
                const normalizedError = error instanceof Error || isRecord(error) ? error : String(error);
                const errorResponse = ErrorHandler.createErrorResponse(normalizedError, name, { ...args, scope: `tool-call/${name}` });
                this.logger.error(`Tool execution failed: ${name}`, errorResponse);
                if (isRecord(errorResponse)) {
                    this.healthMonitor.recordError(errorResponse);
                }

                const sanitizedError = cleanObject(errorResponse);
                if (isRecord(sanitizedError)) {
                    sanitizedError.isError = true;
                    return responseValidator.wrapResponse(name, sanitizedError);
                }
                return responseValidator.wrapResponse(name, {
                    success: false,
                    isError: true,
                    error: 'UNKNOWN_ERROR',
                    message: `Failed to execute ${name}`
                });
            }
        });
    }
}
