import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { CallToolRequestSchema, ListToolsRequestSchema } from '@modelcontextprotocol/sdk/types.js';
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { Logger } from '../utils/logging/logger.js';
import { HealthMonitor } from '../services/health-monitor.js';
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
import { config } from '../config.js';
import type { ITools } from '../types/tools/tool-interfaces.js';
import { handleUnrealGatewayCall, type GatewayContext } from './tool-registry-gateway.js';
import { buildGatewayToolDefinition } from './tool-registry-listing.js';
import { buildLegacyToolList, handleLegacyToolCall, type LegacyContext } from './tool-registry-legacy.js';

function isGatewayMode(): boolean {
    return config.MCP_GATEWAY_MODE;
}

export class ToolRegistry {
    private defaultElicitationTimeoutMs = 60000;

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

    private buildSystemTools() {
        return {
            executeConsoleCommand: (command: string) => this.bridge.executeConsoleCommand(command),
            getProjectSettings: async (section?: string) => {
                const category = typeof section === 'string' && section.trim().length > 0 ? section.trim() : 'Project';
                if (!this.automationBridge || !this.automationBridge.isConnected()) {
                    const diskSettings = await this.readProjectSettingsFromDisk(category);
                    if (diskSettings) return diskSettings;
                    const error = process.env.UE_PROJECT_PATH
                        ? 'Automation bridge not connected and disk read failed'
                        : 'Automation bridge not connected';
                    return { success: false as const, error, section: category };
                }
                try {
                    const resp = await this.automationBridge.sendAutomationRequest('system_control', {
                        action: 'get_project_settings', category
                    }, { timeoutMs: 30000 }) as Record<string, unknown>;
                    const rawError = (resp?.error || '').toString();
                    const msgLower = (resp?.message || '').toString().toLowerCase();
                    const isNotImplemented = rawError.toUpperCase() === 'NOT_IMPLEMENTED' || msgLower.includes('not implemented');
                    if (!resp || resp.success === false) {
                        if (isNotImplemented) {
                            const diskSettings = await this.readProjectSettingsFromDisk(category);
                            if (diskSettings) return diskSettings;
                            return { success: true as const, section: category, settings: { category, available: false, note: 'Project settings are not exposed by the current runtime but validation can proceed.' } };
                        }
                        return { success: false as const, error: rawError || resp?.message || 'Failed to get project settings', section: category, settings: resp?.result };
                    }
                    const result = resp.result && typeof resp.result === 'object' ? (resp.result as Record<string, unknown>) : {};
                    const settings = (result.settings && typeof result.settings === 'object') ? (result.settings as Record<string, unknown>) : result;
                    return { success: true as const, section: category, settings };
                } catch (e) {
                    const diskSettings = await this.readProjectSettingsFromDisk(category);
                    if (diskSettings) return diskSettings;
                    return { success: false as const, error: `Failed to get project settings: ${e instanceof Error ? e.message : String(e)}`, section: category };
                }
            }
        };
    }

    register() {
        const systemTools = this.buildSystemTools();
        const elicitation = createElicitationHelper(this.server, this.logger);
        const gateway = isGatewayMode();

        const tools: ITools = {
            systemTools,
            elicit: elicitation.elicit,
            supportsElicitation: elicitation.supports,
            elicitationTimeoutMs: this.defaultElicitationTimeoutMs,
            assetResources: this.assetResources,
            actorResources: this.actorResources,
            levelResources: this.levelResources,
            bridge: this.bridge,
            automationBridge: this.automationBridge
        };

        this.server.setRequestHandler(ListToolsRequestSchema, async () => {
            if (gateway) {
                this.logger.debug('Serving gateway tool list (static single-tool mode)');
                return { tools: [buildGatewayToolDefinition()] };
            }
            return { tools: buildLegacyToolList(this.server, this.logger) };
        });

        this.server.setRequestHandler(CallToolRequestSchema, async (request) => {
            const { name } = request.params;
            const args: Record<string, unknown> = (request.params.arguments || {}) as Record<string, unknown>;
            const startTime = Date.now();

            if (gateway && name !== 'unreal') {
                this.healthMonitor.trackPerformance(startTime, false);
                const op = typeof args.operation === 'string' ? args.operation : 'unknown';
                return responseValidator.wrapResponse('unreal', {
                    success: false, isError: true, operation: op, error: 'UNKNOWN_TOOL',
                    message: `Unknown tool: ${name}. Capabilities are exposed only through the 'unreal' gateway. Call 'unreal' with operation 'search' to discover available tools.`
                });
            }

            if (gateway) {
                try {
                    const context: GatewayContext = { tools, logger: this.logger, elicitationTimeoutMs: this.defaultElicitationTimeoutMs, ensureConnected: this.ensureConnected };
                    const gatewayResult = cleanObject(await handleUnrealGatewayCall(args, context));
                    const wrapped = await responseValidator.wrapResponse('unreal', gatewayResult);

                    const resultObj = gatewayResult as Record<string, unknown> | null;
                    const explicitSuccess = typeof resultObj?.success === 'boolean' ? Boolean(resultObj.success) : undefined;
                    let wrappedSuccess: boolean | undefined = undefined;
                    if (isRecord(wrapped.structuredContent)) {
                        const sc = wrapped.structuredContent;
                        if (sc && typeof sc.success === 'boolean') wrappedSuccess = Boolean(sc.success);
                    }
                    const isErrorResponse = Boolean(wrapped.isError === true);
                    const finalSuccess = (explicitSuccess ?? wrappedSuccess) === true && !isErrorResponse;
                    this.healthMonitor.trackPerformance(startTime, finalSuccess);

                    if (this.logger.isEnabled('debug')) {
                        const preview = JSON.stringify(redactImagePayloadForLog(wrapped)).substring(0, 100);
                        this.logger.debug(`Returning gateway response to MCP client: ${preview}...`);
                    }
                    return wrapped;
                } catch (error) {
                    this.healthMonitor.trackPerformance(startTime, false);
                    const normalizedError = error instanceof Error || isRecord(error) ? error : String(error);
                    const errorResponse = ErrorHandler.createErrorResponse(normalizedError, 'unreal', { ...args, scope: 'tool-call/unreal' });
                    this.logger.error('Gateway tool execution failed', errorResponse);
                    if (isRecord(errorResponse)) this.healthMonitor.recordError(errorResponse);
                    const sanitizedError = cleanObject(errorResponse);
                    if (isRecord(sanitizedError)) {
                        sanitizedError.isError = true;
                        return responseValidator.wrapResponse('unreal', sanitizedError);
                    }
                    return responseValidator.wrapResponse('unreal', { success: false, isError: true, operation: 'execute', error: 'UNKNOWN_ERROR', message: 'Failed to execute unreal gateway' });
                }
            }

            const legacyContext: LegacyContext = {
                server: this.server, tools, logger: this.logger, healthMonitor: this.healthMonitor,
                elicitationTimeoutMs: this.defaultElicitationTimeoutMs, ensureConnected: this.ensureConnected
            };
            return handleLegacyToolCall(name, args, startTime, legacyContext);
        });
    }
}
