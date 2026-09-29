import { z } from 'zod';
import { LiveStateRevisionsSchema } from '../tools/catalog/capabilities/semantic/live-state-revisions.js';

const stringArray = z.array(z.string());
const nonNegativeInteger = z.number().int().min(0);

export const automationResponseSchema = z.looseObject({
    type: z.literal('automation_response'),
    requestId: z.string().min(1),
    success: z.boolean().optional(),
    message: z.string().optional(),
    error: z.string().optional(),
    result: z.unknown().optional(),
    action: z.string().optional(),
    liveRevisions: LiveStateRevisionsSchema.optional()
});

export const automationEventSchema = z.looseObject({
    type: z.literal('automation_event'),
    requestId: z.string().optional(),
    event: z.string().optional(),
    payload: z.unknown().optional(),
    result: z.unknown().optional(),
    message: z.string().optional()
});

// Authority descriptor (additive). z.object STRIPS unknown keys, so a
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
    sessionId: z.string().optional(),
    protocolVersion: nonNegativeInteger.optional(),
    authority: bridgeAuthoritySchema.optional(),
    // The editor's mounted content roots (`/Game`, `/ShooterCore`, ...), which
    // feed the path allowlist. Absent from plugins that predate it.
    contentRoots: stringArray.optional()
});

/**
 * The automation_event the plugin sends when a content mount comes or goes. Its
 * payload carries the new `contentRoots` snapshot. It is an internal bridge
 * message: it updates the path allowlist and is not forwarded to MCP clients.
 */
export const CONTENT_ROOTS_CHANGED_EVENT = 'content_roots_changed';

/**
 * The raw `contentRoots` of a bridge_ack's metadata or a content_roots_changed
 * payload, or undefined when `source` is not a plain object. The value is not
 * validated here; setEditorContentRoots (path-security.ts) validates it.
 */
export function readContentRoots(source: unknown): unknown {
    if (source === null || typeof source !== 'object' || Array.isArray(source)) return undefined;
    return (source as Record<string, unknown>).contentRoots;
}

export const bridgeErrorSchema = z.looseObject({
    type: z.literal('bridge_error'),
    error: z.string().optional(),
    message: z.string().optional()
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
    progressUpdateSchema,
    cancelRequestSchema
]);

