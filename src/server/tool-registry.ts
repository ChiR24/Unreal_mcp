import { isInputRequiredResult, ProtocolError, type CallToolResult, type Server, type Tool } from '@modelcontextprotocol/server';
import { AutomationBridge } from '../automation/index.js';
import { Logger } from '../utils/logging/logger.js';
import { HealthMonitor } from '../services/health-monitor.js';
import { actionClassForGatewayArgs, failureClassForError } from '../services/telemetry-observation.js';
import { wrapGatewayResponse } from '../utils/responses/response-validator.js';
import { cleanObject } from '../utils/serialization/safe-json.js';
import { redactImagePayloadForLog } from '../utils/logging/log-redaction.js';
import type { ITools } from '../types/tools/tool-interfaces.js';
import {
    canonicalizeMcpRequestId,
    runWithMcpRequestContext
} from '../automation/request-context.js';
import { readProgressToken } from './mcp-primitives/progress/progress-token.js';
import { createProgressReporter } from './mcp-primitives/progress/progress-reporter.js';
import { ProgressSinkRegistry } from './mcp-primitives/progress/progress-sink-registry.js';
import { handleUnrealGatewayCall, type GatewayContext } from './tool-registry-gateway.js';
import { unrealGatewayToolDefinition } from '../tools/catalog/unreal-gateway-definition.js';
import { buildDirectCallMigration } from './gateway/direct-call-migration.js';
import { consentChannel } from './gateway/gateway-execute-consent.js';

export class ToolRegistry {
    private readonly progressSinks = new ProgressSinkRegistry();

    constructor(
        private server: Server,
        private automationBridge: AutomationBridge,
        private logger: Logger,
        private healthMonitor: HealthMonitor,
        private ensureConnected: () => Promise<boolean>
    ) { }

    register() {
        const tools: ITools = { automationBridge: this.automationBridge };

        this.server.setRequestHandler('tools/list', async () => {
            this.logger.debug('Serving gateway tool list (static single-tool mode)');
            return { tools: [unrealGatewayToolDefinition as Tool] };
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

        this.server.setRequestHandler('tools/call', async (request, ctx) => {
            const { name } = request.params;
            const args: Record<string, unknown> = (request.params.arguments || {}) as Record<string, unknown>;
            const startTime = Date.now();
            // Bounded telemetry dimension resolved once per call. It is derived
            // from the capability's declared scope, never from the raw args, so
            // no caller-supplied string can reach a metric label.
            const actionClass = actionClassForGatewayArgs(args);

            const mcpRequestId = ctx.mcpReq.id !== undefined
                ? canonicalizeMcpRequestId(ctx.mcpReq.id)
                : undefined;

            // The token is READ from the client's _meta, never allocated. A
            // client that sent none gets an inert reporter, so absent stays
            // absent instead of becoming a server-invented id.
            const progress = createProgressReporter({
                token: readProgressToken(ctx.mcpReq._meta),
                notify: (notification) => ctx.mcpReq.notify(notification),
                onError: (error) => this.logger.debug('Progress notification dropped', {
                    error: error instanceof Error ? error.message : String(error)
                })
            });
            if (mcpRequestId) this.progressSinks.register(mcpRequestId, progress);
            // Closing before the response leaves is what guarantees no progress
            // frame can trail the terminal result for this request.
            // Idempotent: the abort listener and the call's finally both call it.
            let progressEnded = false;
            const endProgress = (): void => {
                if (progressEnded) return;
                progressEnded = true;
                progress.close();
                if (mcpRequestId) this.progressSinks.unregister(mcpRequestId);
            };

            // Both SDK AbortSignal cancellation and explicit notifications/cancelled
            // converge on AutomationBridge.cancelMcpRequest via the canonical id.
            // Cancellation is ADVISORY — editor work already dispatched to Unreal
            // still runs to completion — but the client has said it no longer
            // wants to hear about it, so the progress stream ends here rather
            // than trickling on until the abandoned handler settles.
            if (mcpRequestId) {
                ctx.mcpReq.signal.addEventListener(
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
                    ? runWithMcpRequestContext({ requestId: mcpRequestId, signal: ctx.mcpReq.signal }, fn)
                    : fn();

            if (name !== 'unreal') {
                endProgress();
                this.healthMonitor.trackPerformance(startTime, false, { actionClass, failureClass: 'validation' });
                const migration = buildDirectCallMigration(name, args);
                // The receipt carries the gateway envelope fields the `unreal`
                // output schema requires (success:false + operation); wrapResponse
                // promotes success:false to top-level isError.
                return wrapGatewayResponse(migration) as CallToolResult;
            }

            try {
                const context: GatewayContext = {
                    tools, logger: this.logger, ensureConnected: this.ensureConnected, consent: consentChannel(ctx)
                };
                const gatewayResult = cleanObject(await withRequestContext(() => handleUnrealGatewayCall(args, context)));
                // A consent question for the client (MCP 2026-07-28) goes back as it is, not as a tool result.
                if (isInputRequiredResult(gatewayResult)) return gatewayResult;
                const wrapped = wrapGatewayResponse(gatewayResult);

                const finalSuccess = wrapped.isError !== true;
                this.healthMonitor.trackPerformance(startTime, finalSuccess, {
                    actionClass,
                    ...(finalSuccess ? {} : { failureClass: failureClassForError(gatewayResult) })
                });

                if (this.logger.isEnabled('debug')) {
                    const preview = JSON.stringify(redactImagePayloadForLog(wrapped)).substring(0, 100);
                    this.logger.debug(`Returning gateway response to MCP client: ${preview}...`);
                }
                return wrapped as CallToolResult;
            } catch (error) {
                // A protocol-level refusal (an unverifiable consent requestState) answers as a JSON-RPC error.
                if (error instanceof ProtocolError) throw error;
                this.healthMonitor.trackPerformance(startTime, false, { actionClass, failureClass: failureClassForError(error) });
                const detail = error instanceof Error ? error.message : String(error);
                const errorResponse = { success: false, isError: true, error: detail, message: `Failed to execute unreal: ${detail}`, scope: 'tool-call/unreal' };
                this.logger.error('Gateway tool execution failed', errorResponse);
                this.healthMonitor.recordError(errorResponse);
                return wrapGatewayResponse(errorResponse) as CallToolResult;
            } finally {
                endProgress();
            }
        });
    }
}
