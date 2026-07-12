import type { Server } from '@modelcontextprotocol/sdk/server/index.js';
import { dynamicToolManager } from '../tools/dynamic/dynamic-tool-manager.js';
import { handleConsolidatedToolCall } from '../tools/orchestration/consolidated-tool-handlers.js';
import { mergeActionParams } from '../tools/orchestration/consolidated-call-utils.js';
import { responseValidator } from '../utils/responses/response-validator.js';
import { ErrorHandler } from '../utils/responses/error-handler.js';
import { cleanObject } from '../utils/serialization/safe-json.js';
import { isRecord } from '../utils/validation/type-guards.js';
import { redactImagePayloadForLog } from '../utils/logging/log-redaction.js';
import type { Logger } from '../utils/logging/logger.js';
import type { HealthMonitor } from '../services/health-monitor.js';
import type { ITools } from '../types/tools/tool-interfaces.js';
import type { ToolDefinition } from '../tools/catalog/consolidated-tool-definitions.js';
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

export type LegacyContext = {
    server: Server;
    tools: ITools;
    logger: Logger;
    healthMonitor: HealthMonitor;
    elicitationTimeoutMs: number;
    ensureConnected: () => Promise<boolean>;
};

export function buildLegacyToolList(server: Server, logger: Logger): ToolDefinition[] {
    const clientInfo = Reflect.get(server, '_clientVersion');
    const clientName = isRecord(clientInfo) && typeof clientInfo.name === 'string' ? clientInfo.name : undefined;
    const supportsListChanged = clientSupportsListChanged(clientName);
    logger.debug(`Client detection: name=${clientName}, supportsListChanged=${supportsListChanged}`);
    const effectiveCategories = getEffectiveCategories(supportsListChanged, parseDefaultCategories());
    logger.info(`Serving tools for categories: ${effectiveCategories.join(', ')} (client=${clientName || 'unknown'}, supportsListChanged=${supportsListChanged})`);
    const sanitized = buildSanitizedToolList(effectiveCategories);
    const status = dynamicToolManager.getStatus();
    logger.debug(`Tool filtering: ${status.enabledTools}/${status.totalTools} enabled, ${sanitized.length} visible`);
    return sanitized;
}

export async function handleLegacyToolCall(
    name: string,
    rawArgs: Record<string, unknown>,
    startTime: number,
    context: LegacyContext
): Promise<Record<string, unknown>> {
    const args = mergeActionParams(rawArgs);

    if (name === 'manage_tools') {
        const result = await handleManageToolsAction(args);
        const action = args.action as string;
        if (TOOL_LIST_CHANGED_ACTIONS.has(action)) {
            context.server.notification({
                method: 'notifications/tools/list_changed',
                params: {}
            }).catch((error: unknown) => {
                context.logger.error('Failed to send list_changed notification', error instanceof Error ? error : String(error));
            });
        }
        return { content: [{ type: 'text' as const, text: JSON.stringify(result) }] };
    }

    if (!dynamicToolManager.getToolState(name)) {
        context.healthMonitor.trackPerformance(startTime, false);
        return { content: [{ type: 'text' as const, text: `Unknown tool: ${name}` }], isError: true };
    }

    if (!dynamicToolManager.isToolEnabled(name)) {
        context.healthMonitor.trackPerformance(startTime, false);
        return { content: [{ type: 'text' as const, text: `Cannot execute tool '${name}': tool is disabled or not available.` }], isError: true };
    }

    const connected = await context.ensureConnected();
    const canRunWithoutConnection = name === 'system_control' && args.action === 'get_project_settings';
    if (!connected && !canRunWithoutConnection) {
        context.healthMonitor.trackPerformance(startTime, false);
        return { content: [{ type: 'text' as const, text: `Cannot execute tool '${name}': Unreal Engine is not connected.` }], isError: true };
    }

    try {
        const elicitedArgs = await maybeElicitMissingArgs(
            name, args, context.tools.elicit, context.elicitationTimeoutMs, context.logger
        );
        let result = await handleConsolidatedToolCall(name, elicitedArgs, context.tools);
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

        context.healthMonitor.trackPerformance(startTime, finalSuccess);

        const durationMs = Date.now() - startTime;
        if (finalSuccess) {
            context.logger.info(`Tool ${name} completed successfully in ${durationMs}ms`);
        } else {
            context.logger.warn(`Tool ${name} completed with errors in ${durationMs}ms`);
        }

        if (context.logger.isEnabled('debug')) {
            const responsePreview = JSON.stringify(redactImagePayloadForLog(wrappedResult)).substring(0, 100);
            context.logger.debug(`Returning response to MCP client: ${responsePreview}...`);
        }

        return wrappedResult;
    } catch (error) {
        context.healthMonitor.trackPerformance(startTime, false);
        const normalizedError = error instanceof Error || isRecord(error) ? error : String(error);
        const errorResponse = ErrorHandler.createErrorResponse(normalizedError, name, { ...args, scope: `tool-call/${name}` });
        context.logger.error(`Tool execution failed: ${name}`, errorResponse);
        if (isRecord(errorResponse)) {
            context.healthMonitor.recordError(errorResponse);
        }
        const sanitizedError = cleanObject(errorResponse);
        if (isRecord(sanitizedError)) {
            sanitizedError.isError = true;
            return responseValidator.wrapResponse(name, sanitizedError);
        }
        return responseValidator.wrapResponse(name, {
            success: false, isError: true, error: 'UNKNOWN_ERROR',
            message: `Failed to execute ${name}`
        });
    }
}
