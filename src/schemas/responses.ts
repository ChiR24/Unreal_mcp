/**
 * Zod schemas for Automation Bridge responses.
 * Provides runtime validation for data coming from the Unreal Engine plugin.
 */
import { z } from 'zod';
import { Vec3Schema, RotatorSchema } from './primitives.js';

// ============================================================================
// Base Response Schemas
// ============================================================================

/** Error object that can be either a string or structured error */
export const ErrorSchema = z.union([
    z.string(),
    z.object({
        message: z.string(),
        code: z.string().optional(),
    }).passthrough(),
]);
export type ErrorType = z.infer<typeof ErrorSchema>;

/** Base automation response - all responses have these fields */
export const BaseResponseSchema = z.object({
    success: z.boolean(),
    message: z.string().optional(),
    error: ErrorSchema.optional().nullable(),
    warnings: z.array(z.string()).optional(),
    result: z.unknown().optional(),
    data: z.unknown().optional(),
}).passthrough();
export type BaseResponse = z.infer<typeof BaseResponseSchema>;

/** Standard action response used by most tools */
export const StandardActionResponseSchema = <T extends z.ZodTypeAny>(dataSchema?: T) => {
    const base = z.object({
        success: z.boolean(),
        warnings: z.array(z.string()).optional(),
        error: ErrorSchema.optional().nullable(),
    });
    
    if (dataSchema) {
        return base.extend({ data: dataSchema.optional() }).passthrough();
    }
    return base.passthrough();
};

// ============================================================================
// Actor Response Schemas
// ============================================================================

/** Actor info returned in listings */
export const ActorInfoSchema = z.object({
    name: z.string().optional(),
    label: z.string().optional(),
    path: z.string().optional(),
    objectPath: z.string().optional(),
    class: z.string().optional(),
    className: z.string().optional(),
    tags: z.array(z.string()).optional(),
}).passthrough();
export type ActorInfo = z.infer<typeof ActorInfoSchema>;

/** Component info */
export const ComponentInfoSchema = z.object({
    name: z.string(),
    class: z.string().optional(),
    className: z.string().optional(),
    path: z.string().optional(),
}).passthrough();
export type ComponentInfo = z.infer<typeof ComponentInfoSchema>;

/** Actor spawn response */
export const SpawnActorResponseSchema = z.object({
    success: z.boolean(),
    actorName: z.string().optional(),
    actorLabel: z.string().optional(),
    actorPath: z.string().optional(),
    message: z.string().optional(),
    error: ErrorSchema.optional().nullable(),
}).passthrough();
export type SpawnActorResponse = z.infer<typeof SpawnActorResponseSchema>;

/** Actor delete response */
export const DeleteActorResponseSchema = z.object({
    success: z.boolean(),
    deleted: z.union([z.string(), z.array(z.string())]).optional(),
    deletedCount: z.number().optional(),
    missing: z.array(z.string()).optional(),
    message: z.string().optional(),
    error: ErrorSchema.optional().nullable(),
}).passthrough();
export type DeleteActorResponse = z.infer<typeof DeleteActorResponseSchema>;

/** Transform response */
export const TransformResponseSchema = z.object({
    success: z.boolean(),
    location: Vec3Schema.optional(),
    rotation: RotatorSchema.optional(),
    scale: Vec3Schema.optional(),
    transform: z.unknown().optional(),
    message: z.string().optional(),
    error: ErrorSchema.optional().nullable(),
}).passthrough();
export type TransformResponse = z.infer<typeof TransformResponseSchema>;

/** Components response */
export const ComponentsResponseSchema = z.object({
    success: z.boolean(),
    components: z.array(ComponentInfoSchema).optional(),
    message: z.string().optional(),
    error: ErrorSchema.optional().nullable(),
}).passthrough();
export type ComponentsResponse = z.infer<typeof ComponentsResponseSchema>;

/** List actors response */
export const ListActorsResponseSchema = z.object({
    success: z.boolean(),
    actors: z.array(ActorInfoSchema).optional(),
    count: z.number().optional(),
    message: z.string().optional(),
    error: ErrorSchema.optional().nullable(),
}).passthrough();
export type ListActorsResponse = z.infer<typeof ListActorsResponseSchema>;

/** Find actors response */
export const FindActorsResponseSchema = z.object({
    success: z.boolean(),
    actors: z.array(ActorInfoSchema).optional(),
    count: z.number().optional(),
    message: z.string().optional(),
    error: ErrorSchema.optional().nullable(),
}).passthrough();
export type FindActorsResponse = z.infer<typeof FindActorsResponseSchema>;

// ============================================================================
// Asset Response Schemas
// ============================================================================

/** Asset info returned in listings */
export const AssetInfoSchema = z.object({
    Name: z.string().optional(),
    name: z.string().optional(),
    Path: z.string().optional(),
    path: z.string().optional(),
    Class: z.string().optional(),
    class: z.string().optional(),
    PackagePath: z.string().optional(),
    packagePath: z.string().optional(),
}).passthrough();
export type AssetInfo = z.infer<typeof AssetInfoSchema>;

/** List assets response */
export const ListAssetsResponseSchema = z.object({
    success: z.boolean(),
    assets: z.array(AssetInfoSchema).optional(),
    paths: z.array(z.string()).optional(),
    count: z.number().optional(),
    message: z.string().optional(),
    error: ErrorSchema.optional().nullable(),
}).passthrough();
export type ListAssetsResponse = z.infer<typeof ListAssetsResponseSchema>;

/** Import asset response */
export const ImportAssetResponseSchema = z.object({
    success: z.boolean(),
    assetPath: z.string().optional(),
    asset: z.string().optional(),
    source: z.string().optional(),
    message: z.string().optional(),
    error: ErrorSchema.optional().nullable(),
}).passthrough();
export type ImportAssetResponse = z.infer<typeof ImportAssetResponseSchema>;

/** Create material response */
export const CreateMaterialResponseSchema = z.object({
    success: z.boolean(),
    materialPath: z.string().optional(),
    materialInstancePath: z.string().optional(),
    message: z.string().optional(),
    error: ErrorSchema.optional().nullable(),
}).passthrough();
export type CreateMaterialResponse = z.infer<typeof CreateMaterialResponseSchema>;

// ============================================================================
// Editor Response Schemas
// ============================================================================

/** PIE (Play In Editor) state response */
export const PIEStateResponseSchema = z.object({
    success: z.boolean(),
    isPlaying: z.boolean().optional(),
    isPaused: z.boolean().optional(),
    isInPIE: z.boolean().optional(),
    playSessionId: z.string().optional(),
    message: z.string().optional(),
    error: ErrorSchema.optional().nullable(),
}).passthrough();
export type PIEStateResponse = z.infer<typeof PIEStateResponseSchema>;

/** Camera/viewport response */
export const ViewportResponseSchema = z.object({
    success: z.boolean(),
    location: Vec3Schema.optional(),
    rotation: RotatorSchema.optional(),
    fov: z.number().optional(),
    viewMode: z.string().optional(),
    cameraSettings: z.unknown().optional(),
    message: z.string().optional(),
    error: ErrorSchema.optional().nullable(),
}).passthrough();
export type ViewportResponse = z.infer<typeof ViewportResponseSchema>;

/** Screenshot response */
export const ScreenshotResponseSchema = z.object({
    success: z.boolean(),
    filename: z.string().optional(),
    filePath: z.string().optional(),
    resolution: z.object({
        width: z.number(),
        height: z.number(),
    }).optional(),
    message: z.string().optional(),
    error: ErrorSchema.optional().nullable(),
}).passthrough();
export type ScreenshotResponse = z.infer<typeof ScreenshotResponseSchema>;

// ============================================================================
// Level Response Schemas
// ============================================================================

/** Level info */
export const LevelInfoSchema = z.object({
    name: z.string().optional(),
    path: z.string().optional(),
    packagePath: z.string().optional(),
    loaded: z.boolean().optional(),
    visible: z.boolean().optional(),
    current: z.boolean().optional(),
}).passthrough();
export type LevelInfo = z.infer<typeof LevelInfoSchema>;

/** Level response */
export const LevelResponseSchema = z.object({
    success: z.boolean(),
    levelPath: z.string().optional(),
    level: z.string().optional(),
    path: z.string().optional(),
    currentMap: z.string().optional(),
    currentMapPath: z.string().optional(),
    levels: z.array(LevelInfoSchema).optional(),
    loaded: z.boolean().optional(),
    visible: z.boolean().optional(),
    message: z.string().optional(),
    error: ErrorSchema.optional().nullable(),
}).passthrough();
export type LevelResponse = z.infer<typeof LevelResponseSchema>;

// ============================================================================
// Blueprint Response Schemas
// ============================================================================

/** Blueprint info */
export const BlueprintInfoSchema = z.object({
    name: z.string().optional(),
    path: z.string().optional(),
    parentClass: z.string().optional(),
    generatedClass: z.string().optional(),
    components: z.array(z.unknown()).optional(),
    variables: z.array(z.unknown()).optional(),
    functions: z.array(z.unknown()).optional(),
    events: z.array(z.unknown()).optional(),
}).passthrough();
export type BlueprintInfo = z.infer<typeof BlueprintInfoSchema>;

/** Blueprint response */
export const BlueprintResponseSchema = z.object({
    success: z.boolean(),
    blueprintPath: z.string().optional(),
    blueprint: z.string().optional(),
    componentAdded: z.string().optional(),
    compiled: z.boolean().optional(),
    saved: z.boolean().optional(),
    info: BlueprintInfoSchema.optional(),
    message: z.string().optional(),
    error: ErrorSchema.optional().nullable(),
}).passthrough();
export type BlueprintResponse = z.infer<typeof BlueprintResponseSchema>;

// ============================================================================
// Sequence Response Schemas
// ============================================================================

/** Sequence response */
export const SequenceResponseSchema = z.object({
    success: z.boolean(),
    sequencePath: z.string().optional(),
    sequence: z.string().optional(),
    bindingId: z.string().optional(),
    trackName: z.string().optional(),
    frameNumber: z.number().optional(),
    length: z.number().optional(),
    playbackPosition: z.number().optional(),
    playing: z.boolean().optional(),
    message: z.string().optional(),
    error: ErrorSchema.optional().nullable(),
}).passthrough();
export type SequenceResponse = z.infer<typeof SequenceResponseSchema>;

// ============================================================================
// Console Command Response
// ============================================================================

/** Console command response */
export const ConsoleCommandResponseSchema = z.object({
    success: z.boolean(),
    command: z.string().optional(),
    output: z.string().optional(),
    result: z.unknown().optional(),
    message: z.string().optional(),
    error: ErrorSchema.optional().nullable(),
}).passthrough();
export type ConsoleCommandResponse = z.infer<typeof ConsoleCommandResponseSchema>;

// ============================================================================
// Generic Response Helpers
// ============================================================================

/**
 * Create a typed response schema with custom data fields
 */
export function createResponseSchema<T extends z.ZodRawShape>(dataFields: T) {
    return z.object({
        success: z.boolean(),
        message: z.string().optional(),
        error: ErrorSchema.optional().nullable(),
        warnings: z.array(z.string()).optional(),
        ...dataFields,
    }).passthrough();
}

/**
 * Parse an automation response with the base schema.
 * Use this when you just need to check success/error without specific data fields.
 */
export function parseBaseResponse(response: unknown): BaseResponse {
    return BaseResponseSchema.parse(response);
}

/**
 * Check if a response indicates success
 */
export function isSuccessResponse(response: unknown): response is { success: true } & Record<string, unknown> {
    if (typeof response !== 'object' || response === null) {
        return false;
    }
    return (response as Record<string, unknown>).success === true;
}

/**
 * Extract error message from a response
 */
export function getErrorMessage(response: unknown): string | undefined {
    if (typeof response !== 'object' || response === null) {
        return undefined;
    }
    const resp = response as Record<string, unknown>;
    if (typeof resp.error === 'string') {
        return resp.error;
    }
    if (typeof resp.error === 'object' && resp.error !== null) {
        const err = resp.error as Record<string, unknown>;
        if (typeof err.message === 'string') {
            return err.message;
        }
    }
    if (typeof resp.message === 'string' && resp.success === false) {
        return resp.message;
    }
    return undefined;
}
