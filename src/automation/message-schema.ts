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

// Task 40 authority descriptor (additive). z.object STRIPS unknown keys, so a
// stray token, path prefix or limit a plugin might place here can never survive
// into the cached descriptor: only these six non-secret fields are retained.
export const bridgeAuthoritySchema = z.object({
    profile: z.string().optional(),
    scopes: stringArray.optional(),
    deprecated: z.boolean().optional(),
    tokenRequired: z.boolean().optional(),
    pathRestricted: z.boolean().optional(),
    projectRestricted: z.boolean().optional()
});

export type BridgeAuthority = z.infer<typeof bridgeAuthoritySchema>;

export function readBridgeAuthority(
    metadata: Record<string, unknown> | undefined
): BridgeAuthority | undefined {
    const raw = metadata?.authority;
    if (raw === undefined) return undefined;
    const parsed = bridgeAuthoritySchema.safeParse(raw);
    return parsed.success ? parsed.data : undefined;
}

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
    heartbeatIntervalMs: nonNegativeInteger.optional(),
    authority: bridgeAuthoritySchema.optional()
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

// Targeted cancellation frame sent by the TS bridge to Unreal when an MCP
// request is cancelled. Carries the previously-allocated automation request id.
export const cancelRequestSchema = z.looseObject({
    type: z.literal('cancel_request'),
    requestId: z.string().min(1),
    reason: z.string().optional()
});

export const automationMessageSchema = z.discriminatedUnion('type', [
    automationResponseSchema,
    automationEventSchema,
    bridgeAckSchema,
    bridgeErrorSchema,
    bridgePingSchema,
    bridgePongSchema,
    bridgeGoodbyeSchema,
    progressUpdateSchema,
    cancelRequestSchema
]);

export type AutomationMessageSchema = z.infer<typeof automationMessageSchema>;
