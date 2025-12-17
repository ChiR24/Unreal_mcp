/**
 * Common type definitions for automation bridge responses
 * Used to provide type safety for sendAutomationRequest calls
 */

/**
 * Base response structure from the Automation Bridge
 * Most responses follow this pattern with optional additional fields
 */
export interface AutomationResponse {
    success: boolean;
    message?: string;
    error?: string | { message: string; code?: string };
    result?: unknown;
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
    location?: { x: number; y: number; z: number };
    rotation?: { pitch: number; yaw: number; roll: number };
    scale?: { x: number; y: number; z: number };
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
    location?: { x: number; y: number; z: number };
    rotation?: { pitch: number; yaw: number; roll: number };
    cameraSettings?: unknown;
    camera?: { location?: unknown; rotation?: unknown };
    resolution?: { width: number; height: number };
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
