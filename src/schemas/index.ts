/**
 * Barrel export for all schema modules.
 * Import from this file for convenient access to all schemas and utilities.
 */

// Primitive schemas and types
export {
    // Geometric primitives
    Vec3Schema,
    Vec3,
    OptionalVec3Schema,
    RotatorSchema,
    Rotator,
    OptionalRotatorSchema,
    TransformSchema,
    Transform,
    Vec3ArraySchema,
    Vec3Array,
    RotatorArraySchema,
    RotatorArray,
    ColorRGBASchema,
    ColorRGBA,
    FlexibleVec3Schema,
    FlexibleVec3,
    FlexibleRotatorSchema,
    FlexibleRotator,

    // Path schemas
    AssetPathSchema,
    AssetPath,
    ActorNameSchema,
    ActorName,
    ClassPathSchema,
    ClassPath,
    BlueprintPathSchema,
    BlueprintPath,
    LevelPathSchema,
    LevelPath,

    // Common parameter schemas
    TimeoutMsSchema,
    TimeoutMs,
    PropertiesSchema,
    Properties,
    TagSchema,
    Tag,
    ComponentNameSchema,
    ComponentName,
    ComponentTypeSchema,
    ComponentType,
    ActionSchema,
    Action,
    BaseHandlerArgsSchema,
    BaseHandlerArgs,
    TimeoutArgsSchema,
    TimeoutArgs,

    // Actor parameter schemas
    SpawnActorParamsSchema,
    SpawnActorParams,
    DeleteActorParamsSchema,
    DeleteActorParams,
    SetTransformParamsSchema,
    SetTransformParams,
    ApplyForceParamsSchema,
    ApplyForceParams,
    SetVisibilityParamsSchema,
    SetVisibilityParams,
    AddComponentParamsSchema,
    AddComponentParams,
    SetComponentPropertiesParamsSchema,
    SetComponentPropertiesParams,
    DuplicateActorParamsSchema,
    DuplicateActorParams,
    AttachActorParamsSchema,
    AttachActorParams,
    TagActorParamsSchema,
    TagActorParams,
    FindByTagParamsSchema,
    FindByTagParams,
    SpawnBlueprintParamsSchema,
    SpawnBlueprintParams,
    SnapshotActorParamsSchema,
    SnapshotActorParams,
    BlueprintVariablesParamsSchema,
    BlueprintVariablesParams,
} from './primitives.js';

// Parser utilities
export {
    parseOrThrow,
    safeParse,
    parseOrUndefined,
    parseOrDefault,
    formatZodError,
    isPlainObject,
    toPlainObject,
    parseWithPassthrough,
    requireField,
    getFieldOrDefault,
} from './parser.js';

// Response schemas and types
export {
    // Base response schemas
    ErrorSchema,
    ErrorType,
    BaseResponseSchema,
    BaseResponse,
    StandardActionResponseSchema,

    // Actor response schemas
    ActorInfoSchema,
    ActorInfo,
    ComponentInfoSchema,
    ComponentInfo,
    SpawnActorResponseSchema,
    SpawnActorResponse,
    DeleteActorResponseSchema,
    DeleteActorResponse,
    TransformResponseSchema,
    TransformResponse,
    ComponentsResponseSchema,
    ComponentsResponse,
    ListActorsResponseSchema,
    ListActorsResponse,
    FindActorsResponseSchema,
    FindActorsResponse,

    // Asset response schemas
    AssetInfoSchema,
    AssetInfo,
    ListAssetsResponseSchema,
    ListAssetsResponse,
    ImportAssetResponseSchema,
    ImportAssetResponse,
    CreateMaterialResponseSchema,
    CreateMaterialResponse,

    // Editor response schemas
    PIEStateResponseSchema,
    PIEStateResponse,
    ViewportResponseSchema,
    ViewportResponse,
    ScreenshotResponseSchema,
    ScreenshotResponse,

    // Level response schemas
    LevelInfoSchema,
    LevelInfo,
    LevelResponseSchema,
    LevelResponse,

    // Blueprint response schemas
    BlueprintInfoSchema,
    BlueprintInfo,
    BlueprintResponseSchema,
    BlueprintResponse,

    // Sequence response schemas
    SequenceResponseSchema,
    SequenceResponse,

    // Console command response
    ConsoleCommandResponseSchema,
    ConsoleCommandResponse,

    // Helper functions
    createResponseSchema,
    parseBaseResponse,
    isSuccessResponse,
    getErrorMessage,
} from './responses.js';
