import { z } from 'zod';

const stringArray = z.array(z.string());

export const automationResponseSchema = z.object({
    type: z.literal('automation_response'),
    requestId: z.string().min(1),
    success: z.boolean().optional(),
    message: z.string().optional(),
    error: z.string().optional(),
    result: z.unknown().optional(),
    action: z.string().optional()
}).passthrough();

export const automationEventSchema = z.object({
    type: z.literal('automation_event'),
    requestId: z.string().optional(),
    event: z.string().optional(),
    payload: z.unknown().optional(),
    result: z.unknown().optional(),
    message: z.string().optional()
}).passthrough();

export const bridgeAckSchema = z.object({
    type: z.literal('bridge_ack'),
    message: z.string().optional(),
    serverName: z.string().optional(),
    serverVersion: z.string().optional(),
    sessionId: z.string().optional(),
    protocolVersion: z.number().optional(),
    supportedOpcodes: stringArray.optional(),
    expectedResponseOpcodes: stringArray.optional(),
    capabilities: stringArray.optional(),
    heartbeatIntervalMs: z.number().optional()
}).passthrough();

export const bridgeErrorSchema = z.object({
    type: z.literal('bridge_error'),
    error: z.string().optional(),
    message: z.string().optional()
}).passthrough();

export const bridgePingSchema = z.object({
    type: z.literal('bridge_ping'),
    timestamp: z.string().optional()
}).passthrough();

export const bridgePongSchema = z.object({
    type: z.literal('bridge_pong'),
    timestamp: z.string().optional()
}).passthrough();

export const bridgeGoodbyeSchema = z.object({
    type: z.literal('bridge_goodbye'),
    reason: z.string().optional(),
    timestamp: z.string().optional()
}).passthrough();

// Progress update message - sent by UE during long operations to keep request alive
export const progressUpdateSchema = z.object({
    type: z.literal('progress_update'),
    requestId: z.string().min(1),
    percent: z.number().min(0).max(100).optional(),
    message: z.string().optional(),
    timestamp: z.string().optional(),
    stillWorking: z.boolean().optional()  // True if operation is still in progress
}).passthrough();

export const automationMessageSchema = z.discriminatedUnion('type', [
    automationResponseSchema,
    automationEventSchema,
    bridgeAckSchema,
    bridgeErrorSchema,
    bridgePingSchema,
    bridgePongSchema,
    bridgeGoodbyeSchema,
    progressUpdateSchema
]);

export type AutomationMessageSchema = z.infer<typeof automationMessageSchema>;
