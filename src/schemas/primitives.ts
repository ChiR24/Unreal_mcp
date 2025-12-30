/**
 * Shared Zod schemas for primitive types used throughout the codebase.
 * These provide runtime validation and eliminate `any` types.
 */
import { z } from 'zod';

// ============================================================================
// Geometric Primitives
// ============================================================================

/** 3D Vector schema (location, offset, force, etc.) */
export const Vec3Schema = z.object({
    x: z.number(),
    y: z.number(),
    z: z.number(),
});
export type Vec3 = z.infer<typeof Vec3Schema>;

/** Optional 3D Vector - allows undefined or null */
export const OptionalVec3Schema = Vec3Schema.optional().nullable();

/** Rotator schema (pitch, yaw, roll in degrees) */
export const RotatorSchema = z.object({
    pitch: z.number(),
    yaw: z.number(),
    roll: z.number(),
});
export type Rotator = z.infer<typeof RotatorSchema>;

/** Optional Rotator */
export const OptionalRotatorSchema = RotatorSchema.optional().nullable();

/** Transform schema (location + rotation + scale) */
export const TransformSchema = z.object({
    location: Vec3Schema.optional(),
    rotation: RotatorSchema.optional(),
    scale: Vec3Schema.optional(),
});
export type Transform = z.infer<typeof TransformSchema>;

/** Array-based vector [x, y, z] */
export const Vec3ArraySchema = z.tuple([z.number(), z.number(), z.number()]);
export type Vec3Array = z.infer<typeof Vec3ArraySchema>;

/** Array-based rotator [pitch, yaw, roll] */
export const RotatorArraySchema = z.tuple([z.number(), z.number(), z.number()]);
export type RotatorArray = z.infer<typeof RotatorArraySchema>;

/** Color with alpha channel [R, G, B, A] - values 0-255 or 0-1 depending on context */
export const ColorRGBASchema = z.tuple([z.number(), z.number(), z.number(), z.number()]);
export type ColorRGBA = z.infer<typeof ColorRGBASchema>;

// ============================================================================
// Unreal Engine Path Schemas
// ============================================================================

/** 
 * Asset path - must start with /Game/ or /Engine/ or be a plugin path
 * Examples: /Game/MyFolder/MyAsset, /Engine/BasicShapes/Cube
 */
export const AssetPathSchema = z.string().refine(
    (val) => val.startsWith('/') || val.includes('.'),
    { message: 'Asset path must start with / or contain a dot' }
);
export type AssetPath = z.infer<typeof AssetPathSchema>;

/** Actor name - non-empty string identifier */
export const ActorNameSchema = z.string().min(1, 'Actor name cannot be empty');
export type ActorName = z.infer<typeof ActorNameSchema>;

/** Class path for spawning - e.g., /Script/Engine.StaticMeshActor */
export const ClassPathSchema = z.string().min(1, 'Class path cannot be empty');
export type ClassPath = z.infer<typeof ClassPathSchema>;

/** Blueprint path - asset path ending in _C for blueprint class */
export const BlueprintPathSchema = z.string().min(1, 'Blueprint path cannot be empty');
export type BlueprintPath = z.infer<typeof BlueprintPathSchema>;

/** Level path - e.g., /Game/Maps/MyLevel */
export const LevelPathSchema = z.string().min(1, 'Level path cannot be empty');
export type LevelPath = z.infer<typeof LevelPathSchema>;

// ============================================================================
// Common Parameter Schemas
// ============================================================================

/** Timeout in milliseconds */
export const TimeoutMsSchema = z.number().int().positive().optional();
export type TimeoutMs = z.infer<typeof TimeoutMsSchema>;

/** Generic properties object - Record<string, unknown> */
export const PropertiesSchema = z.record(z.string(), z.unknown());
export type Properties = z.infer<typeof PropertiesSchema>;

/** Tag string for actor tagging */
export const TagSchema = z.string().min(1, 'Tag cannot be empty');
export type Tag = z.infer<typeof TagSchema>;

/** Component name */
export const ComponentNameSchema = z.string().min(1, 'Component name cannot be empty');
export type ComponentName = z.infer<typeof ComponentNameSchema>;

/** Component type/class */
export const ComponentTypeSchema = z.string().min(1, 'Component type cannot be empty');
export type ComponentType = z.infer<typeof ComponentTypeSchema>;

// ============================================================================
// Action Parameter Schemas (for handlers)
// ============================================================================

/** Base action parameter - all consolidated tools have an action field */
export const ActionSchema = z.string().min(1, 'Action is required');
export type Action = z.infer<typeof ActionSchema>;

/** Spawn actor parameters */
export const SpawnActorParamsSchema = z.object({
    classPath: ClassPathSchema,
    location: Vec3Schema.optional(),
    rotation: RotatorSchema.optional(),
    actorName: ActorNameSchema.optional(),
    meshPath: AssetPathSchema.optional(),
    timeoutMs: TimeoutMsSchema,
});
export type SpawnActorParams = z.infer<typeof SpawnActorParamsSchema>;

/** Delete actor parameters */
export const DeleteActorParamsSchema = z.object({
    actorName: ActorNameSchema.optional(),
    actorNames: z.array(ActorNameSchema).optional(),
}).refine(
    (data) => data.actorName !== undefined || (data.actorNames !== undefined && data.actorNames.length > 0),
    { message: 'Either actorName or actorNames must be provided' }
);
export type DeleteActorParams = z.infer<typeof DeleteActorParamsSchema>;

/** Set transform parameters */
export const SetTransformParamsSchema = z.object({
    actorName: ActorNameSchema,
    location: Vec3Schema.optional(),
    rotation: RotatorSchema.optional(),
    scale: Vec3Schema.optional(),
});
export type SetTransformParams = z.infer<typeof SetTransformParamsSchema>;

/** Apply force parameters */
export const ApplyForceParamsSchema = z.object({
    actorName: ActorNameSchema,
    force: Vec3Schema,
});
export type ApplyForceParams = z.infer<typeof ApplyForceParamsSchema>;

/** Set visibility parameters */
export const SetVisibilityParamsSchema = z.object({
    actorName: ActorNameSchema,
    visible: z.boolean(),
});
export type SetVisibilityParams = z.infer<typeof SetVisibilityParamsSchema>;

/** Add component parameters */
export const AddComponentParamsSchema = z.object({
    actorName: ActorNameSchema,
    componentType: ComponentTypeSchema,
    componentName: ComponentNameSchema.optional(),
    properties: PropertiesSchema.optional(),
});
export type AddComponentParams = z.infer<typeof AddComponentParamsSchema>;

/** Set component properties parameters */
export const SetComponentPropertiesParamsSchema = z.object({
    actorName: ActorNameSchema,
    componentName: ComponentNameSchema,
    properties: PropertiesSchema,
});
export type SetComponentPropertiesParams = z.infer<typeof SetComponentPropertiesParamsSchema>;

/** Duplicate actor parameters */
export const DuplicateActorParamsSchema = z.object({
    actorName: ActorNameSchema,
    newName: ActorNameSchema.optional(),
    offset: Vec3Schema.optional(),
});
export type DuplicateActorParams = z.infer<typeof DuplicateActorParamsSchema>;

/** Attach actor parameters */
export const AttachActorParamsSchema = z.object({
    childActor: ActorNameSchema,
    parentActor: ActorNameSchema,
});
export type AttachActorParams = z.infer<typeof AttachActorParamsSchema>;

/** Tag actor parameters */
export const TagActorParamsSchema = z.object({
    actorName: ActorNameSchema,
    tag: TagSchema,
});
export type TagActorParams = z.infer<typeof TagActorParamsSchema>;

/** Find by tag parameters */
export const FindByTagParamsSchema = z.object({
    tag: TagSchema,
    matchType: z.enum(['exact', 'contains', 'startsWith', 'endsWith']).optional(),
});
export type FindByTagParams = z.infer<typeof FindByTagParamsSchema>;

/** Spawn blueprint parameters */
export const SpawnBlueprintParamsSchema = z.object({
    blueprintPath: BlueprintPathSchema,
    actorName: ActorNameSchema.optional(),
    location: Vec3Schema.optional(),
    rotation: RotatorSchema.optional(),
});
export type SpawnBlueprintParams = z.infer<typeof SpawnBlueprintParamsSchema>;

/** Snapshot actor parameters */
export const SnapshotActorParamsSchema = z.object({
    actorName: ActorNameSchema,
    snapshotName: z.string().min(1, 'Snapshot name cannot be empty'),
});
export type SnapshotActorParams = z.infer<typeof SnapshotActorParamsSchema>;

/** Blueprint variables parameters */
export const BlueprintVariablesParamsSchema = z.object({
    actorName: ActorNameSchema,
    variables: PropertiesSchema,
});
export type BlueprintVariablesParams = z.infer<typeof BlueprintVariablesParamsSchema>;

// ============================================================================
// Helper: Flexible Vec3 that accepts both object and array formats
// ============================================================================

/** Vec3 that accepts either {x,y,z} or [x,y,z] format */
export const FlexibleVec3Schema = z.union([
    Vec3Schema,
    Vec3ArraySchema.transform((arr) => ({ x: arr[0], y: arr[1], z: arr[2] })),
]);
export type FlexibleVec3 = z.infer<typeof FlexibleVec3Schema>;

/** Rotator that accepts either {pitch,yaw,roll} or [pitch,yaw,roll] format */
export const FlexibleRotatorSchema = z.union([
    RotatorSchema,
    RotatorArraySchema.transform((arr) => ({ pitch: arr[0], yaw: arr[1], roll: arr[2] })),
]);
export type FlexibleRotator = z.infer<typeof FlexibleRotatorSchema>;

// ============================================================================
// Utility Types for Handler Arguments
// ============================================================================

/** Generic handler arguments - base for all tool handlers */
export const BaseHandlerArgsSchema = z.object({
    action: ActionSchema,
}).passthrough(); // Allow additional properties

export type BaseHandlerArgs = z.infer<typeof BaseHandlerArgsSchema>;

/** Arguments with optional timeout */
export const TimeoutArgsSchema = z.object({
    timeoutMs: TimeoutMsSchema,
});
export type TimeoutArgs = z.infer<typeof TimeoutArgsSchema>;
