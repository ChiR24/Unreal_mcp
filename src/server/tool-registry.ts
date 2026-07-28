import { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { CallToolRequestSchema, ListToolsRequestSchema } from '@modelcontextprotocol/sdk/types.js';
import { UnrealBridge } from '../unreal-bridge.js';
import { AutomationBridge } from '../automation/index.js';
import { Logger } from '../utils/logging/logger.js';
import { HealthMonitor } from '../services/health-monitor.js';
import { actionClassForGatewayArgs, failureClassForError } from '../services/telemetry-observation.js';
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
import type { ITools } from '../types/tools/tool-interfaces.js';
import {
    canonicalizeMcpRequestId,
    runWithMcpRequestContext
} from '../automation/request-context.js';
import { readProgressToken } from './mcp-primitives/progress/progress-token.js';
import { createProgressReporter } from './mcp-primitives/progress/progress-reporter.js';
import { ProgressSinkRegistry } from './mcp-primitives/progress/progress-sink-registry.js';
import { runTaskCheckpoint, taskCheckpointRefusal } from './mcp-primitives/task-checkpoint.js';
import { handleUnrealGatewayCall, type GatewayContext } from './tool-registry-gateway.js';
import { buildGatewayToolDefinition } from './tool-registry-listing.js';
import { buildDirectCallMigration } from './gateway/direct-call-migration.js';

export class ToolRegistry {
    private defaultElicitationTimeoutMs = 60000;
    private readonly progressSinks = new ProgressSinkRegistry();

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
            this.logger.debug('Serving gateway tool list (static single-tool mode)');
            return { tools: [buildGatewayToolDefinition()] };
        });

        // Unreal reports progress against an automation id; the bridge resolves
        // that to the owning MCP request and this sink turns it into a
        // notification stamped with THAT request's own client token.
        this.automationBridge.setRequestProgressListener(
            (mcpRequestId, update) => this.progressSinks.report(mcpRequestId, update)
        );
        this.automationBridge.setRequestCancelledListener(
            (mcpRequestId) => this.progressSinks.close(mcpRequestId)
        );

        // Shutdown drain, chained rather than assigned so an already-installed
        // close handler (primitive-wiring installs one the same way) still runs.
        const previousOnClose = this.server.onclose;
        this.server.onclose = (): void => {
            this.progressSinks.clear();
            previousOnClose?.();
        };

        this.server.setRequestHandler(CallToolRequestSchema, async (request, extra) => {
            const { name } = request.params;
            const args: Record<string, unknown> = (request.params.arguments || {}) as Record<string, unknown>;
            const startTime = Date.now();
            // Bounded telemetry dimension resolved once per call. It is derived
            // from the capability's declared scope, never from the raw args, so
            // no caller-supplied string can reach a metric label.
            const actionClass = actionClassForGatewayArgs(args);

            const mcpRequestId = extra.requestId !== undefined
                ? canonicalizeMcpRequestId(extra.requestId)
                : undefined;

            // The token is READ from the client's _meta, never allocated. A
            // client that sent none gets an inert reporter, so absent stays
            // absent instead of becoming a server-invented id.
            const progress = createProgressReporter({
                token: readProgressToken(extra._meta),
                notify: (notification) => extra.sendNotification(notification),
                onError: (error) => this.logger.debug('Progress notification dropped', {
                    error: error instanceof Error ? error.message : String(error)
                })
            });
            if (mcpRequestId) this.progressSinks.register(mcpRequestId, progress);
            // Closing before the response leaves is what guarantees no progress
            // frame can trail the terminal result for this request.
            const endProgress = (): void => {
                progress.close();
                if (mcpRequestId) this.progressSinks.unregister(mcpRequestId);
            };

            // Both SDK AbortSignal cancellation and explicit notifications/cancelled
            // converge on AutomationBridge.cancelMcpRequest via the canonical id.
            // Cancellation is ADVISORY — editor work already dispatched to Unreal
            // still runs to completion — but the client has said it no longer
            // wants to hear about it, so the progress stream ends here rather
            // than trickling on until the abandoned handler settles.
            if (mcpRequestId && extra.signal) {
                extra.signal.addEventListener(
                    'abort',
                    () => {
                        endProgress();
                        this.automationBridge.cancelMcpRequest(mcpRequestId, 'Client aborted request');
                    },
                    { once: true }
                );
            }

            const withRequestContext = <T>(fn: () => T): T =>
                mcpRequestId
                    ? runWithMcpRequestContext({ requestId: mcpRequestId, signal: extra.signal }, fn)
                    : fn();

            // Task 44: a task-augmented call is a request for a POLLABLE handle,
            // so it is validated before any work happens — answering it any other
            // way would strand the client polling an id that never existed.
            const taskCreation = request.params.task;
            if (taskCreation !== undefined) {
                const refusal = taskCheckpointRefusal(name, args, extra.taskStore);
                if (refusal) {
                    endProgress();
                    this.healthMonitor.trackPerformance(startTime, false, { actionClass, failureClass: 'validation' });
                    throw refusal;
                }
            }

            if (name !== 'unreal') {
                endProgress();
                this.healthMonitor.trackPerformance(startTime, false, { actionClass, failureClass: 'validation' });
                const migration = buildDirectCallMigration(name, args);
                // The receipt carries the gateway envelope fields the `unreal`
                // output schema requires (success:false + operation); wrapResponse
                // promotes success:false to top-level isError.
                return responseValidator.wrapResponse('unreal', migration);
            }

            const runGateway = async () => {
            try {
                const context: GatewayContext = { tools, logger: this.logger, elicitationTimeoutMs: this.defaultElicitationTimeoutMs, ensureConnected: this.ensureConnected };
                const gatewayResult = cleanObject(await withRequestContext(() => handleUnrealGatewayCall(args, context)));
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
                this.healthMonitor.trackPerformance(startTime, finalSuccess, {
                    actionClass,
                    ...(finalSuccess ? {} : { failureClass: failureClassForError(resultObj) })
                });

                if (this.logger.isEnabled('debug')) {
                    const preview = JSON.stringify(redactImagePayloadForLog(wrapped)).substring(0, 100);
                    this.logger.debug(`Returning gateway response to MCP client: ${preview}...`);
                }
                return wrapped;
            } catch (error) {
                this.healthMonitor.trackPerformance(startTime, false, { actionClass, failureClass: failureClassForError(error) });
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
            } finally {
                endProgress();
            }
            };

            // The checkpoint retains EXACTLY the payload the synchronous branch
            // would have returned, so tasks/result and a plain tools/call can
            // never disagree about what the operation did.
            if (taskCreation !== undefined && extra.taskStore !== undefined) {
                return await runTaskCheckpoint({
                    taskStore: extra.taskStore,
                    taskCreation,
                    run: runGateway
                });
            }
            return await runGateway();
        });
    }
}
