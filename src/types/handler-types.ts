/**
 * Shared type definitions for handler arguments and responses.
 * Used across all *-handlers.ts files to replace 'any' types.
 */

// ============================================================================
// Common Geometry Types
// ============================================================================

/** 3D Vector - used for locations, forces, scales */
export interface Vector3 {
    x: number;
    y: number;
    z: number;
}

/** Rotation in Unreal format (Pitch, Yaw, Roll in degrees) */
export interface Rotator {
    pitch: number;
    yaw: number;
    roll: number;
}

/** Transform combining location, rotation, and scale */
export interface Transform {
    location?: Vector3;
    rotation?: Rotator;
    scale?: Vector3;
}

// ============================================================================
// Base Handler Types
// ============================================================================

/**
 * Base interface for handler arguments.
 * All handler args should extend this or use it directly for loose typing.
 */
export interface HandlerArgs {
    action?: string;
    subAction?: string;
    [key: string]: unknown;
}

/**
 * Standard response from automation bridge requests.
 */
export interface AutomationResponse {
    success: boolean;
    message?: string;
    error?: string;
    result?: unknown;
    [key: string]: unknown;
}

/**
 * Component information returned from getComponents.
 */
export interface ComponentInfo {
    name: string;
    class?: string;
    objectPath?: string;
    [key: string]: unknown;
}

// ============================================================================
// Actor Types
// ============================================================================

export interface ActorArgs extends HandlerArgs {
    actorName?: string;
    name?: string;
    classPath?: string;
    class?: string;
    type?: string;
    location?: Vector3;
    rotation?: Rotator;
    scale?: Vector3;
    meshPath?: string;
    timeoutMs?: number;
    force?: Vector3;
    parentActor?: string;
    childActor?: string;
    tag?: string;
    newName?: string;
    offset?: Vector3;
    visible?: boolean;
    componentName?: string;
    componentType?: string;
    properties?: Record<string, unknown>;
}

// ============================================================================
// Asset Types
// ============================================================================

export interface AssetArgs extends HandlerArgs {
    assetPath?: string;
    path?: string;
    directory?: string;
    directoryPath?: string;
    sourcePath?: string;
    destinationPath?: string;
    newName?: string;
    name?: string;
    filter?: string;
    recursive?: boolean;
    overwrite?: boolean;
    classNames?: string[];
    packagePaths?: string[];
    parentMaterial?: string;
    parameters?: Record<string, unknown>;
    assetPaths?: string[];
    meshPath?: string;
}

// ============================================================================
// Blueprint Types
// ============================================================================

export interface BlueprintArgs extends HandlerArgs {
    blueprintPath?: string;
    name?: string;
    savePath?: string;
    blueprintType?: string;
    componentType?: string;
    componentName?: string;
    attachTo?: string;
    variableName?: string;
    eventType?: string;
    customEventName?: string;
    nodeType?: string;
    graphName?: string;
    x?: number;
    y?: number;
    memberName?: string;
    nodeId?: string;
    pinName?: string;
    linkedTo?: string;
    fromNodeId?: string;
    fromPin?: string;
    fromPinName?: string;
    toNodeId?: string;
    toPin?: string;
    toPinName?: string;
    propertyName?: string;
    value?: unknown;
    properties?: Record<string, unknown>;
    compile?: boolean;
    save?: boolean;
    metadata?: Record<string, unknown>;
}

// ============================================================================
// Editor Types
// ============================================================================

export interface EditorArgs extends HandlerArgs {
    command?: string;
    filename?: string;
    resolution?: string;
    location?: Vector3;
    rotation?: Rotator;
    fov?: number;
    speed?: number;
    viewMode?: string;
    width?: number;
    height?: number;
    enabled?: boolean;
    realtime?: boolean;
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
    dataLayerLabel?: string;
    dataLayerName?: string;
    dataLayerState?: string;
    actorPath?: string;
    min?: number[];
    max?: number[];
    origin?: number[];
    extent?: number[];
    metadata?: Record<string, unknown>;
    timeoutMs?: number;
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

// ============================================================================
// Effect Types
// ============================================================================

export interface EffectArgs extends HandlerArgs {
    location?: Vector3;
    rotation?: Rotator;
    scale?: number;
    preset?: string;
    systemPath?: string;
    shape?: string;
    size?: number;
    color?: number[];
    name?: string;
    emitterName?: string;
    modulePath?: string;
    parameterName?: string;
    parameterType?: string;
    type?: string;
    filter?: string;
}

// ============================================================================
// Environment Types
// ============================================================================

export interface EnvironmentArgs extends HandlerArgs {
    name?: string;
    landscapeName?: string;
    location?: Vector3;
    scale?: Vector3;
    componentCount?: { x: number; y: number };
    sectionSize?: number;
    sectionsPerComponent?: number;
    materialPath?: string;
    foliageType?: string;
    foliageTypePath?: string;
    meshPath?: string;
    density?: number;
    radius?: number;
    minScale?: number;
    maxScale?: number;
    alignToNormal?: boolean;
    randomYaw?: boolean;
    cullDistance?: number;
    transforms?: Transform[];
    locations?: Vector3[];
    bounds?: { min: Vector3; max: Vector3 };
    seed?: number;
    heightData?: number[];
    layerName?: string;
}

// ============================================================================
// Lighting Types
// ============================================================================

export interface LightingArgs extends HandlerArgs {
    lightType?: string;
    name?: string;
    location?: Vector3;
    rotation?: Rotator;
    intensity?: number;
    color?: number[];
    temperature?: number;
    radius?: number;
    falloffExponent?: number;
    innerCone?: number;
    outerCone?: number;
    width?: number;
    height?: number;
    castShadows?: boolean;
    method?: string;
    bounces?: number;
    quality?: string;
    enabled?: boolean;
    density?: number;
    scatteringIntensity?: number;
    fogHeight?: number;
    cubemapPath?: string;
    sourceType?: string;
    recapture?: boolean;
    size?: number;
    levelName?: string;
    copyActors?: boolean;
    useTemplate?: boolean;
    pulse?: boolean;
    useAsAtmosphereSunLight?: boolean;
    shadowQuality?: string;
    cascadedShadows?: boolean;
    shadowDistance?: number;
    contactShadows?: boolean;
    rayTracedShadows?: boolean;
    compensationValue?: number;
    minBrightness?: number;
    maxBrightness?: number;
    indirectLightingIntensity?: number;
    buildOnlySelected?: boolean;
    buildReflectionCaptures?: boolean;
}

// ============================================================================
// Performance Types
// ============================================================================

export interface PerformanceArgs extends HandlerArgs {
    type?: 'CPU' | 'GPU' | 'Memory' | 'RenderThread' | 'GameThread' | 'All';
    category?: string;
    duration?: number;
    outputPath?: string;
    level?: number;
    scale?: number;
    enabled?: boolean;
    maxFPS?: number;
    verbose?: boolean;
    detailed?: boolean;
}

// ============================================================================
// Inspect Types
// ============================================================================

export interface InspectArgs extends HandlerArgs {
    objectPath?: string;
    name?: string;
    actorName?: string;
    componentName?: string;
    propertyName?: string;
    propertyPath?: string;
    value?: unknown;
    className?: string;
    classPath?: string;
    filter?: string;
    tag?: string;
    snapshotName?: string;
    destinationPath?: string;
    outputPath?: string;
    format?: string;
}

// ============================================================================
// Graph Types (Blueprint, Material, Niagara, BehaviorTree)
// ============================================================================

export interface GraphArgs extends HandlerArgs {
    assetPath?: string;
    blueprintPath?: string;
    systemPath?: string;
    graphName?: string;
    nodeType?: string;
    nodeId?: string;
    x?: number;
    y?: number;
    memberName?: string;
    variableName?: string;
    eventName?: string;
    functionName?: string;
    targetClass?: string;
    memberClass?: string;
    componentClass?: string;
    pinName?: string;
    linkedTo?: string;
    fromNodeId?: string;
    fromPinName?: string;
    toNodeId?: string;
    toPinName?: string;
    parentNodeId?: string;
    childNodeId?: string;
    properties?: Record<string, unknown>;
}

// ============================================================================
// System Types
// ============================================================================

export interface SystemArgs extends HandlerArgs {
    command?: string;
    category?: string;
    profileType?: string;
    level?: number;
    key?: string;
    value?: string;
    section?: string;
    configName?: string;
    resolution?: string;
    enabled?: boolean;
    widgetPath?: string;
    parentName?: string;
    childClass?: string;
    target?: string;
    platform?: string;
    configuration?: string;
    arguments?: string;
}

// ============================================================================
// Input Types
// ============================================================================

export interface InputArgs extends HandlerArgs {
    name?: string;
    path?: string;
    actionPath?: string;
    contextPath?: string;
    key?: string;
}

// ============================================================================
// Pipeline Types
// ============================================================================

export interface PipelineArgs extends HandlerArgs {
    target?: string;
    platform?: string;
    configuration?: string;
    arguments?: string;
    projectPath?: string;
}

// ============================================================================
// Animation & Physics Types
// ============================================================================

/** Axis definition for blend spaces */
export interface BlendSpaceAxis {
    minValue?: number;
    maxValue?: number;
    name?: string;
}

export interface AnimationArgs extends HandlerArgs {
    name?: string;
    blueprintName?: string;
    skeletonPath?: string;
    targetSkeleton?: string;
    savePath?: string;
    path?: string;
    actorName?: string;
    meshPath?: string;
    montagePath?: string;
    playRate?: number;
    
    // Blend space
    horizontalAxis?: BlendSpaceAxis;
    verticalAxis?: BlendSpaceAxis;
    minX?: number;
    maxX?: number;
    minY?: number;
    maxY?: number;
    
    // State machine
    machineName?: string;
    states?: unknown[];
    transitions?: unknown[];
    blueprintPath?: string;
    
    // IK
    ikBones?: unknown[];
    enableFootPlacement?: boolean;
    
    // Procedural anim
    systemName?: string;
    baseAnimation?: string;
    modifiers?: unknown[];
    
    // Blend tree
    treeName?: string;
    blendType?: string;
    basePose?: string;
    additiveAnimations?: unknown[];
    
    // Animation asset
    assetType?: string;
    
    // Notify
    animationPath?: string;
    assetPath?: string;
    notifyName?: string;
    time?: number;
    startTime?: number;
    
    // Vehicle
    vehicleName?: string;
    vehicleType?: string;
    wheels?: unknown[];
    engine?: unknown;
    transmission?: unknown;
    pluginDependencies?: string[];
    plugins?: string[];
    
    // Physics simulation
    physicsAssetName?: string;
    
    // Cleanup
    artifacts?: unknown[];
}

// ============================================================================
// Audio Types
// ============================================================================

export interface AudioArgs extends HandlerArgs {
    name?: string;
    soundPath?: string;
    wavePath?: string;
    savePath?: string;
    location?: Vector3;
    rotation?: Rotator;
    volume?: number;
    pitch?: number;
    startTime?: number;
    attenuationPath?: string;
    concurrencyPath?: string;
    actorName?: string;
    componentName?: string;
    autoPlay?: boolean;
    is3D?: boolean;
    innerRadius?: number;
    falloffDistance?: number;
    attenuationShape?: string;
    falloffMode?: string;
    parentClass?: string;
    properties?: Record<string, unknown>;
    classAdjusters?: unknown[];
    mixName?: string;
    size?: Vector3;
    reverbEffect?: string;
    fadeTime?: number;
    enabled?: boolean;
    fftSize?: number;
    outputType?: string;
    soundName?: string;
    targetVolume?: number;
    fadeType?: string;
    scale?: number;
    lowPassFilterFrequency?: number;
    volumeAttenuation?: number;
    settings?: Record<string, unknown>;
}
