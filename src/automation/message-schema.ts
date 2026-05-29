import { z } from 'zod';

const stringArray = z.array(z.string());
const nonNegativeInteger = z.number().int().min(0);

export const automationResponseSchema = z.looseObject({
    type: z.literal('automation_response'),
    requestId: z.string().min(1),
    success: z.boolean().optional(),
    message: z.string().optional(),
    error: z.string().optional(),
    result: z.unknown().optional(),
    action: z.string().optional()
});

export const automationEventSchema = z.looseObject({
    type: z.literal('automation_event'),
    requestId: z.string().optional(),
    event: z.string().optional(),
    payload: z.unknown().optional(),
    result: z.unknown().optional(),
    message: z.string().optional()
});

export const bridgeAckSchema = z.looseObject({
    type: z.literal('bridge_ack'),
    message: z.string().optional(),
    serverName: z.string().optional(),
    serverVersion: z.string().optional(),
    sessionId: z.string().optional(),
    protocolVersion: nonNegativeInteger.optional(),
    supportedOpcodes: stringArray.optional(),
    expectedResponseOpcodes: stringArray.optional(),
    capabilities: stringArray.optional(),
    heartbeatIntervalMs: nonNegativeInteger.optional()
});

export const bridgeErrorSchema = z.looseObject({
    type: z.literal('bridge_error'),
    error: z.string().optional(),
    message: z.string().optional()
});

export const bridgePingSchema = z.looseObject({
    type: z.literal('bridge_ping'),
    timestamp: z.string().optional()
});

export const bridgePongSchema = z.looseObject({
    type: z.literal('bridge_pong'),
    timestamp: z.string().optional()
});

export const bridgeGoodbyeSchema = z.looseObject({
    type: z.literal('bridge_goodbye'),
    reason: z.string().optional(),
    timestamp: z.string().optional()
});

// Progress update message - sent by UE during long operations to keep request alive
export const progressUpdateSchema = z.looseObject({
    type: z.literal('progress_update'),
    requestId: z.string().min(1),
    percent: z.number().min(0).max(100).optional(),
    message: z.string().optional(),
    timestamp: z.string().optional(),
    stillWorking: z.boolean().optional()  // True if operation is still in progress
});

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
