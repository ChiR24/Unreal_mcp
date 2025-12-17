/**
 * Base Response Types for Unreal Engine MCP Server
 * 
 * These interfaces provide type safety for tool responses.
 * Use with BaseTool.sendRequest<T>() for typed responses.
 */

// ============================================================================
// Base Types
// ============================================================================

export interface BaseResponse {
    success: boolean;
    message?: string;
    error?: string;
    /** Optional error code for programmatic handling */
    errorCode?: string;
}

export interface Vector3 {
    x: number;
    y: number;
    z: number;
}

export interface Rotator {
    pitch: number;
    yaw: number;
    roll: number;
}

export interface Transform {
    location?: Vector3;
    rotation?: Rotator;
    scale?: Vector3;
}

// ============================================================================
// Actor Responses
// ============================================================================

export interface SpawnActorResponse extends BaseResponse {
    actorName?: string;
    actorPath?: string;
    /** Class path of the spawned actor */
    classPath?: string;
}

export interface DeleteActorResponse extends BaseResponse {
    deletedCount?: number;
    deletedActors?: string[];
}

export interface GetTransformResponse extends BaseResponse {
    location?: Vector3;
    rotation?: Rotator;
    scale?: Vector3;
}

export interface SetTransformResponse extends BaseResponse {
    actorName?: string;
}

export interface FindActorsResponse extends BaseResponse {
    actors?: Array<{
        name: string;
        label?: string;
        class?: string;
        path?: string;
    }>;
    count?: number;
}

export interface GetComponentsResponse extends BaseResponse {
    components?: Array<{
        name: string;
        type: string;
        properties?: Record<string, unknown>;
    }>;
}

export interface ApplyForceResponse extends BaseResponse {
    actorName?: string;
    forceApplied?: Vector3;
}

// ============================================================================
// Asset Responses
// ============================================================================

export interface AssetInfo {
    name: string;
    path: string;
    class?: string;
    packagePath?: string;
    size?: number;
}

export interface ListAssetsResponse extends BaseResponse {
    assets?: AssetInfo[];
    count?: number;
    directory?: string;
}

export interface CreateAssetResponse extends BaseResponse {
    assetPath?: string;
    assetName?: string;
}

export interface DeleteAssetResponse extends BaseResponse {
    deletedPath?: string;
    deletedCount?: number;
}

export interface GetDependenciesResponse extends BaseResponse {
    dependencies?: string[];
    referencers?: string[];
    count?: number;
}

export interface AssetExistsResponse extends BaseResponse {
    exists?: boolean;
    assetPath?: string;
}

// ============================================================================
// Blueprint Responses
// ============================================================================

export interface CreateBlueprintResponse extends BaseResponse {
    blueprintPath?: string;
    blueprintName?: string;
    parentClass?: string;
}

export interface GetBlueprintResponse extends BaseResponse {
    path?: string;
    name?: string;
    parentClass?: string;
    variables?: Array<{
        name: string;
        type: string;
        defaultValue?: unknown;
    }>;
    functions?: Array<{
        name: string;
        parameters?: Array<{ name: string; type: string }>;
        returnType?: string;
    }>;
    components?: Array<{
        name: string;
        type: string;
        properties?: Record<string, unknown>;
    }>;
}

export interface AddVariableResponse extends BaseResponse {
    variableName?: string;
    variableType?: string;
}

export interface CreateNodeResponse extends BaseResponse {
    nodeId?: string;
    nodeName?: string;
}

export interface ConnectPinsResponse extends BaseResponse {
    connected?: boolean;
    fromNode?: string;
    toNode?: string;
}

// ============================================================================
// Level Responses
// ============================================================================

export interface LoadLevelResponse extends BaseResponse {
    levelPath?: string;
    levelName?: string;
}

export interface SaveLevelResponse extends BaseResponse {
    savedPath?: string;
}

export interface ListLevelsResponse extends BaseResponse {
    levels?: Array<{
        name: string;
        path: string;
        isLoaded?: boolean;
        isVisible?: boolean;
    }>;
}

export interface BuildLightingResponse extends BaseResponse {
    quality?: string;
    buildTime?: number;
}

// ============================================================================
// Editor Responses
// ============================================================================

export interface PlayInEditorResponse extends BaseResponse {
    isPlaying?: boolean;
    isPaused?: boolean;
}

export interface SetCameraResponse extends BaseResponse {
    location?: Vector3;
    rotation?: Rotator;
    fov?: number;
}

export interface ScreenshotResponse extends BaseResponse {
    filePath?: string;
    width?: number;
    height?: number;
}

export interface ViewModeResponse extends BaseResponse {
    viewMode?: string;
}

// ============================================================================
// Sequence Responses
// ============================================================================

export interface CreateSequenceResponse extends BaseResponse {
    sequencePath?: string;
    sequenceName?: string;
}

export interface AddActorToSequenceResponse extends BaseResponse {
    bindingId?: string;
    actorName?: string;
}

export interface AddKeyframeResponse extends BaseResponse {
    frameNumber?: number;
    property?: string;
    value?: unknown;
}

// ============================================================================
// Audio Responses
// ============================================================================

export interface PlaySoundResponse extends BaseResponse {
    soundPath?: string;
    audioComponentId?: string;
}

export interface CreateSoundCueResponse extends BaseResponse {
    cuePath?: string;
    cueName?: string;
}

// ============================================================================
// Effect Responses
// ============================================================================

export interface SpawnEffectResponse extends BaseResponse {
    effectName?: string;
    effectPath?: string;
    location?: Vector3;
}

export interface CreateNiagaraSystemResponse extends BaseResponse {
    systemPath?: string;
    systemName?: string;
}

// ============================================================================
// Lighting Responses
// ============================================================================

export interface SpawnLightResponse extends BaseResponse {
    lightName?: string;
    lightType?: string;
    location?: Vector3;
}

export interface SetupGIResponse extends BaseResponse {
    method?: string;
    bounces?: number;
}

// ============================================================================
// Animation Responses
// ============================================================================

export interface CreateAnimBlueprintResponse extends BaseResponse {
    blueprintPath?: string;
    skeletonPath?: string;
}

export interface PlayMontageResponse extends BaseResponse {
    montagePath?: string;
    playRate?: number;
}

// ============================================================================
// System Responses
// ============================================================================

export interface ConsoleCommandResponse extends BaseResponse {
    command?: string;
    output?: string;
}

export interface ProfileResponse extends BaseResponse {
    profileType?: string;
    enabled?: boolean;
}

export interface SetCVarResponse extends BaseResponse {
    cvarName?: string;
    value?: string;
}

// ============================================================================
// Inspect Responses
// ============================================================================

export interface InspectObjectResponse extends BaseResponse {
    objectPath?: string;
    className?: string;
    properties?: Record<string, unknown>;
}

export interface GetPropertyResponse extends BaseResponse {
    propertyName?: string;
    value?: unknown;
    propertyType?: string;
}

export interface SetPropertyResponse extends BaseResponse {
    propertyName?: string;
    newValue?: unknown;
}

// ============================================================================
// Environment Responses
// ============================================================================

export interface CreateLandscapeResponse extends BaseResponse {
    landscapeName?: string;
    componentCount?: { x: number; y: number };
}

export interface AddFoliageResponse extends BaseResponse {
    foliageType?: string;
    instanceCount?: number;
}
