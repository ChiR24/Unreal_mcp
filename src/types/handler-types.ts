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
    fromPin?: string;
    toNodeId?: string;
    toPinName?: string;
    toPin?: string;
    sourceNodeId?: string;
    targetNodeId?: string;
    inputName?: string;
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

// ============================================================================
// Game Framework Types (Phase 21)
// ============================================================================

/**
 * Match state definition for game mode configuration
 */
export interface MatchStateDefinition {
    name: 'waiting' | 'warmup' | 'in_progress' | 'post_match' | 'custom';
    duration?: number;
    customName?: string;
}

/**
 * Arguments for manage_game_framework tool (Phase 21)
 * 
 * Covers:
 * - Core Classes: GameMode, GameState, PlayerController, PlayerState, GameInstance, HUD
 * - Game Mode Configuration: class assignments, game rules
 * - Match Flow: match states, rounds, teams, scoring, spawning
 * - Player Management: spawn points, respawning, spectating
 */
export interface GameFrameworkArgs extends HandlerArgs {
    // Asset identification
    name?: string;
    path?: string;
    gameModeBlueprint?: string;
    blueprintPath?: string;
    levelPath?: string;
    
    // Class assignments
    parentClass?: string;
    pawnClass?: string;
    defaultPawnClass?: string;
    playerControllerClass?: string;
    gameStateClass?: string;
    playerStateClass?: string;
    spectatorClass?: string;
    hudClass?: string;
    
    // Game rules
    timeLimit?: number;
    scoreLimit?: number;
    bDelayedStart?: boolean;
    startPlayersNeeded?: number;
    
    // Match states
    states?: MatchStateDefinition[];
    
    // Round system
    numRounds?: number;
    roundTime?: number;
    intermissionTime?: number;
    
    // Team system
    numTeams?: number;
    teamSize?: number;
    autoBalance?: boolean;
    friendlyFire?: boolean;
    teamIndex?: number;
    
    // Scoring
    scorePerKill?: number;
    scorePerObjective?: number;
    scorePerAssist?: number;
    
    // Spawn system
    spawnSelectionMethod?: 'Random' | 'RoundRobin' | 'FarthestFromEnemies';
    respawnDelay?: number;
    respawnLocation?: 'PlayerStart' | 'LastDeath' | 'TeamBase';
    respawnConditions?: string[];
    usePlayerStarts?: boolean;
    
    // PlayerStart configuration
    location?: Vector3;
    rotation?: Rotator;
    bPlayerOnly?: boolean;
    
    // Spectating
    allowSpectating?: boolean;
    spectatorViewMode?: 'FreeCam' | 'ThirdPerson' | 'FirstPerson' | 'DeathCam';
    
    // Save option
    save?: boolean;
}

// ============================================================================
// Navigation System Types (Phase 25)
// ============================================================================

/**
 * Arguments for manage_navigation tool (Phase 25)
 * 
 * Covers:
 * - NavMesh: settings configuration, agent properties, rebuild
 * - Nav Modifiers: component creation, area class, cost configuration
 * - Nav Links: proxy creation, link configuration, smart links
 */
export interface NavigationArgs extends HandlerArgs {
    // NavMesh identification
    navMeshPath?: string;
    actorName?: string;
    actorPath?: string;
    blueprintPath?: string;
    
    // Nav agent properties (ARecastNavMesh)
    agentRadius?: number;
    agentHeight?: number;
    agentStepHeight?: number;
    agentMaxSlope?: number;
    
    // NavMesh generation settings (FNavMeshResolutionParam)
    cellSize?: number;
    cellHeight?: number;
    tileSizeUU?: number;
    minRegionArea?: number;
    mergeRegionSize?: number;
    maxSimplificationError?: number;
    
    // Nav modifier component (UNavModifierComponent)
    componentName?: string;
    areaClass?: string;
    areaClassToReplace?: string;
    failsafeExtent?: Vector3;
    bIncludeAgentHeight?: boolean;
    
    // Nav area cost configuration
    areaCost?: number;
    fixedAreaEnteringCost?: number;
    
    // Nav link configuration (ANavLinkProxy, FNavigationLink)
    linkName?: string;
    startPoint?: Vector3;
    endPoint?: Vector3;
    direction?: 'BothWays' | 'LeftToRight' | 'RightToLeft';
    snapRadius?: number;
    linkEnabled?: boolean;
    
    // Smart link configuration (UNavLinkCustomComponent)
    linkType?: 'simple' | 'smart';
    bSmartLinkIsRelevant?: boolean;
    enabledAreaClass?: string;
    disabledAreaClass?: string;
    broadcastRadius?: number;
    broadcastInterval?: number;
    
    // Obstacle configuration
    bCreateBoxObstacle?: boolean;
    obstacleOffset?: Vector3;
    obstacleExtent?: Vector3;
    obstacleAreaClass?: string;
    
    // Location and transform
    location?: Vector3;
    rotation?: Rotator;
    
    // Query parameters
    filter?: string;
    
    // Save option
    save?: boolean;
}


// ============================================================================
// Sessions & Local Multiplayer Types (Phase 22)
// ============================================================================

/**
 * Voice chat settings for session configuration
 */
export interface VoiceSettings {
    /** Volume level (0.0 - 1.0) */
    volume?: number;
    /** Noise gate threshold */
    noiseGateThreshold?: number;
    /** Enable noise suppression */
    noiseSuppression?: boolean;
    /** Enable echo cancellation */
    echoCancellation?: boolean;
    /** Sample rate in Hz */
    sampleRate?: number;
}

/**
 * Arguments for manage_sessions tool (Phase 22)
 * 
 * Covers:
 * - Session Management: local session settings, session interface
 * - Local Multiplayer: split-screen configuration, local players
 * - LAN: LAN play configuration, hosting/joining servers
 * - Voice Chat: voice settings, channels, muting, attenuation
 */
export interface SessionsArgs extends HandlerArgs {
    // Session identification
    sessionName?: string;
    sessionId?: string;
    
    // Local session settings
    maxPlayers?: number;
    bIsLANMatch?: boolean;
    bAllowJoinInProgress?: boolean;
    bAllowInvites?: boolean;
    bUsesPresence?: boolean;
    bUseLobbiesIfAvailable?: boolean;
    bShouldAdvertise?: boolean;
    
    // Session interface
    interfaceType?: 'Default' | 'LAN' | 'Null';
    
    // Split-screen configuration
    enabled?: boolean;
    splitScreenType?: 'None' | 'TwoPlayer_Horizontal' | 'TwoPlayer_Vertical' | 'ThreePlayer_FavorTop' | 'ThreePlayer_FavorBottom' | 'FourPlayer_Grid';
    
    // Local player management
    playerIndex?: number;
    controllerId?: number;
    
    // LAN settings
    serverAddress?: string;
    serverPort?: number;
    serverPassword?: string;
    serverName?: string;
    mapName?: string;
    travelOptions?: string;
    
    // Voice chat
    voiceEnabled?: boolean;
    voiceSettings?: VoiceSettings;
    channelName?: string;
    channelType?: 'Team' | 'Global' | 'Proximity' | 'Party';
    
    // Player targeting for voice operations
    playerName?: string;
    targetPlayerId?: string;
    muted?: boolean;
    
    // Voice attenuation
    attenuationRadius?: number;
    attenuationFalloff?: number;
    
    // Push-to-talk
    pushToTalkEnabled?: boolean;
    pushToTalkKey?: string;
}

// ============================================================================
// Level Structure Types (Phase 23)
// ============================================================================

/**
 * Arguments for manage_level_structure tool (Phase 23)
 * 
 * Covers:
 * - Levels: create levels, sublevels, streaming, bounds
 * - World Partition: grid configuration, data layers, HLOD
 * - Level Blueprint: open, add nodes, connect nodes
 * - Level Instances: packed level actors, level instances
 */
export interface LevelStructureArgs extends HandlerArgs {
    // Level identification
    levelName?: string;
    levelPath?: string;
    parentLevel?: string;
    
    // Level creation
    templateLevel?: string;
    bCreateWorldPartition?: boolean;
    
    // Sublevel configuration
    sublevelName?: string;
    sublevelPath?: string;
    
    // Level streaming
    streamingMethod?: 'Blueprint' | 'AlwaysLoaded' | 'Disabled';
    bShouldBeVisible?: boolean;
    bShouldBlockOnLoad?: boolean;
    bDisableDistanceStreaming?: boolean;
    
    // Streaming distance
    streamingDistance?: number;
    minStreamingDistance?: number;
    
    // Level bounds
    boundsOrigin?: Vector3;
    boundsExtent?: Vector3;
    bAutoCalculateBounds?: boolean;
    
    // World Partition
    bEnableWorldPartition?: boolean;
    gridCellSize?: number;
    loadingRange?: number;
    
    // Data layers
    dataLayerName?: string;
    dataLayerLabel?: string;
    bIsInitiallyVisible?: boolean;
    bIsInitiallyLoaded?: boolean;
    dataLayerType?: 'Runtime' | 'Editor';
    
    // Actor assignment to data layer
    actorName?: string;
    actorPath?: string;
    
    // HLOD configuration
    hlodLayerName?: string;
    hlodLayerPath?: string;
    bIsSpatiallyLoaded?: boolean;
    cellSize?: number;
    loadingDistance?: number;
    
    // Minimap volume
    volumeName?: string;
    volumeLocation?: Vector3;
    volumeExtent?: Vector3;
    
    // Level Blueprint
    nodeClass?: string;
    nodePosition?: { x: number; y: number };
    nodeName?: string;
    
    // Node connections
    sourceNodeName?: string;
    sourcePinName?: string;
    targetNodeName?: string;
    targetPinName?: string;
    
    // Level instances
    levelInstanceName?: string;
    levelAssetPath?: string;
    instanceLocation?: Vector3;
    instanceRotation?: Rotator;
    instanceScale?: Vector3;
    
    // Packed level actor
    packedLevelName?: string;
    bPackBlueprints?: boolean;
    bPackStaticMeshes?: boolean;
    
    // Save option
    save?: boolean;
}

// ============================================================================
// Volumes & Zones Types (Phase 24)
// ============================================================================

/**
 * Volume-specific properties for different volume types
 */
export interface VolumeProperties {
    // Physics Volume
    bWaterVolume?: boolean;
    fluidFriction?: number;
    terminalVelocity?: number;
    priority?: number;
    
    // Pain Causing Volume
    bPainCausing?: boolean;
    damagePerSec?: number;
    damageType?: string;
    bEntryPain?: boolean;
    painInterval?: number;
    
    // Audio Volume
    bEnabled?: boolean;
    
    // Reverb Volume
    reverbSettings?: {
        bApplyReverb?: boolean;
        volume?: number;
        fadeTime?: number;
        reverbEffect?: string;
    };
    
    // Cull Distance Volume
    cullDistances?: Array<{
        size: number;
        cullDistance: number;
    }>;
    
    // Nav Modifier Volume
    areaClass?: string;
    bDynamicModifier?: boolean;
    
    // Post Process Volume (Note: Full PP config is Phase 29.5)
    bUnbound?: boolean;
    blendRadius?: number;
    blendWeight?: number;
}

/**
 * Arguments for manage_volumes tool (Phase 24)
 * 
 * Covers:
 * - Trigger Volumes: trigger_volume, trigger_box, trigger_sphere, trigger_capsule
 * - Gameplay Volumes: blocking, kill_z, pain_causing, physics, audio, reverb
 * - Rendering Volumes: cull_distance, precomputed_visibility, lightmass_importance
 * - Navigation Volumes: nav_mesh_bounds, nav_modifier, camera_blocking
 * - Volume Configuration: extent, properties
 */
export interface VolumesArgs extends HandlerArgs {
    // Volume identification
    volumeName?: string;
    volumePath?: string;
    volumeClass?: string;
    
    // Location and transform
    location?: Vector3;
    rotation?: Rotator;
    
    // Volume extent/size
    extent?: Vector3;
    brushType?: 'Additive' | 'Subtractive';
    
    // Trigger shape parameters
    sphereRadius?: number;
    capsuleRadius?: number;
    capsuleHalfHeight?: number;
    boxExtent?: Vector3;
    
    // Volume-specific properties
    properties?: VolumeProperties;
    
    // Pain Causing Volume specific
    bPainCausing?: boolean;
    damagePerSec?: number;
    damageType?: string;
    
    // Physics Volume specific
    bWaterVolume?: boolean;
    fluidFriction?: number;
    terminalVelocity?: number;
    priority?: number;
    
    // Audio Volume specific
    bEnabled?: boolean;
    
    // Reverb Volume specific
    reverbEffect?: string;
    reverbVolume?: number;
    fadeTime?: number;
    
    // Cull Distance Volume specific
    cullDistances?: Array<{
        size: number;
        cullDistance: number;
    }>;
    
    // Nav Modifier Volume specific
    areaClass?: string;
    bDynamicModifier?: boolean;
    
    // Post Process Volume (basic - full config in Phase 29.5)
    bUnbound?: boolean;
    blendRadius?: number;
    blendWeight?: number;
    
    // Lightmass Importance Volume specific
    bLightmassReplacementPrimitive?: boolean;
    
    // Query parameters
    filter?: string;
    volumeType?: string;
    
    // Save option
    save?: boolean;
}

