/**
 * Editor, level, and sequence handler argument types.
 */
import type { HandlerArgs, Rotator, Vector3 } from './handler-common-types.js';

// ============================================================================
// Editor Types
// ============================================================================

export interface EditorArgs extends HandlerArgs {
    command?: string;
    filename?: string;
    resolution?: string;
    mode?: string;
    returnBase64?: boolean;
    includeMetadata?: boolean;
    metadata?: Record<string, unknown>;
    type?: string;
    inputType?: string;
    inputAction?: string;
    key?: string;
    x?: number;
    y?: number;
    button?: string;
    value?: number;
    holdSeconds?: number;
    widget?: string;
    location?: Vector3;
    rotation?: Rotator;
    fov?: number;
    speed?: number;
    viewMode?: string;
    width?: number;
    height?: number;
    enabled?: boolean;
    realtime?: boolean;
    actorName?: string;
    name?: string;
    objectPath?: string;
    blendTime?: number;
    bookmarkName?: string;
    assetPath?: string;
    path?: string;
    category?: string;
    preferences?: Record<string, unknown>;
    timeoutMs?: number;
}

// ============================================================================
// Level Types
// ============================================================================

export interface LevelArgs extends HandlerArgs {
    levelPath?: string;
    path?: string;
    levelName?: string;
    levelPaths?: string[];
    destinationPath?: string;
    savePath?: string;
    subLevelPath?: string;
    parentLevel?: string;
    parentPath?: string;
    streamingMethod?: 'Blueprint' | 'AlwaysLoaded';
    exportPath?: string;
    packagePath?: string;
    sourcePath?: string;
    newName?: string;
    template?: string;
    lightType?: 'Directional' | 'Point' | 'Spot' | 'Rect';
    name?: string;
    location?: Vector3;
    rotation?: Rotator;
    intensity?: number;
    color?: number[];
    quality?: string;
    streaming?: boolean;
    shouldBeLoaded?: boolean;
    shouldBeVisible?: boolean;
    saveDirtyPackages?: boolean;
    overwrite?: boolean;
    metadata?: Record<string, unknown>;
    timeoutMs?: number;
    useWorldPartition?: boolean;
}

// ============================================================================
// Sequence Types
// ============================================================================

export interface SequenceArgs extends HandlerArgs {
    path?: string;
    name?: string;
    actorName?: string;
    actorNames?: string[];
    spawnable?: boolean;
    trackName?: string;
    trackType?: string;
    property?: string;
    frame?: number;
    value?: unknown;
    speed?: number;
    lengthInFrames?: number;
    start?: number;
    end?: number;
    startFrame?: number;
    endFrame?: number;
    assetPath?: string;
    muted?: boolean;
    solo?: boolean;
    locked?: boolean;
}
