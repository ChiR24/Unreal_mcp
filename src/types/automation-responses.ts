/**
 * Common type definitions for automation bridge responses
 * Used to provide type safety for sendAutomationRequest calls
 */

export interface AutomationErrorDetail {
    message: string;
    code?: string;
    [key: string]: unknown;
}

export interface Vector3Response {
    x: number;
    y: number;
    z: number;
}

export interface RotatorResponse {
    pitch: number;
    yaw: number;
    roll: number;
}

export interface ResolutionResponse {
    width: number;
    height: number;
}

/**
 * Base response structure from the Automation Bridge
 * Most responses follow this pattern with optional additional fields
 */
export interface AutomationResponse {
    success: boolean;
    message?: string;
    error?: string | AutomationErrorDetail;
    /** Optional error code for business logic errors (e.g., 'SECURITY_VIOLATION', 'NOT_FOUND') */
    errorCode?: string;
    result?: unknown;
    assetPath?: string;
    // Common additional fields
    warnings?: string[];
    details?: unknown;
    data?: unknown;
    [key: string]: unknown;
}


/**
 * Level-related response fields
 */
export interface LevelResponse extends AutomationResponse {
    levelPath?: string;
    level?: string;
    path?: string;
    packagePath?: string;
    objectPath?: string;
    currentMap?: string;
    currentMapPath?: string;
    currentWorldLevels?: unknown[];
    allMaps?: unknown[];
    partitioned?: boolean;
    streaming?: boolean;
    loaded?: boolean;
    visible?: boolean;
    skipped?: boolean;
    reason?: string;
}

/**
 * Actor-related response fields
 */
export interface ActorResponse extends AutomationResponse {
    actorName?: string;
    actorLabel?: string;
    actorPath?: string;
    actors?: unknown[];
    components?: unknown[];
    location?: Vector3Response;
    rotation?: RotatorResponse;
    scale?: Vector3Response;
    transform?: unknown;
    tags?: string[];
    properties?: Record<string, unknown>;
    deleted?: boolean;
    deletedCount?: number;
}

/**
 * Asset-related response fields
 */
export interface AssetResponse extends AutomationResponse {
    assetPath?: string;
    asset?: string;
    assets?: unknown[];
    source?: string;
    saved?: boolean;
    metadata?: Record<string, unknown>;
    tags?: string[];
    graph?: Record<string, unknown[]>;
}

/**
 * Editor-related response fields
 */
export interface EditorResponse extends AutomationResponse {
    viewMode?: string;
    fov?: number;
    location?: Vector3Response;
    rotation?: RotatorResponse;
    cameraSettings?: unknown;
    camera?: { location?: unknown; rotation?: unknown };
    resolution?: ResolutionResponse;
    filename?: string;
    filePath?: string;
    isPlaying?: boolean;
    isPaused?: boolean;
    isInPIE?: boolean;
    playSessionId?: string;
    bookmarkName?: string;
    bookmarks?: unknown[];
    realtime?: boolean;
}

/**
 * Sequence/Animation-related response fields
 */
export interface SequenceResponse extends AutomationResponse {
    sequencePath?: string;
    sequence?: string;
    bindingId?: string;
    trackName?: string;
    keyframe?: unknown;
    frameNumber?: number;
    length?: number;
    playbackPosition?: number;
    requestId?: string;
}

/**
 * Console command response
 */
export interface ConsoleResponse extends AutomationResponse {
    output?: string;
    command?: string;
}
