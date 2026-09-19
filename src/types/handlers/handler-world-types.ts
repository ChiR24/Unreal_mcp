/**
 * World, effects, environment, and lighting handler argument types.
 */
import type { HandlerArgs, Rotator, Transform, Vector3 } from './handler-common-types.js';

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
    // Debug shapes (C++ TryGetStringField)
    shapeType?: string;
    boxSize?: number[];
    endLocation?: Vector3;
    direction?: Vector3;
    duration?: number;
    thickness?: number;
    length?: number;
    angle?: number;
    halfHeight?: number;
    // Dynamic lights (C++ TryGetStringField/NumberField)
    lightName?: string;
    lightType?: string;
    intensity?: number;
    pulse?: { enabled?: boolean; frequency?: number };
    attachToActor?: string;
    // Niagara system control (C++ TryGetStringField)
    systemName?: string;
    actorName?: string;
    autoDestroy?: boolean;
    reset?: boolean;
    deltaTime?: number;
    steps?: number;
    // Niagara authoring (C++ TryGetStringField)
    savePath?: string;
    subAction?: string;
    assetPath?: string;
    emitterPath?: string;
    spawnRate?: number;
    burstCount?: number;
    burstTime?: number;
    spawnPerUnit?: number;
    lifetime?: number;
    mass?: number;
    // Force/velocity modules
    forceType?: string;
    forceStrength?: number;
    forceVector?: Vector3;
    velocity?: Vector3;
    acceleration?: Vector3;
    velocityMode?: string;
    // Size/color modules
    sizeMode?: string;
    uniformSize?: number;
    colorMode?: string;
    // Renderer configuration
    alignment?: string;
    facingMode?: string;
    sortMode?: string;
    meshScale?: number;
    ribbonWidth?: number;
    // Light renderer
    lightRadius?: number;
    lightIntensity?: number;
    lightColor?: number[];
    volumetricScattering?: number;
    lightExponent?: number;
    affectsTranslucency?: boolean;
    // Collision module
    collisionMode?: string;
    restitution?: number;
    friction?: number;
    dieOnCollision?: boolean;
    // Kill module
    killCondition?: string;
    killBox?: number[];
    invertKillZone?: boolean;
    // Camera offset
    cameraOffset?: number;
    cameraOffsetMode?: string;
    // Parameter binding
    parameterValue?: unknown;
    sourceBinding?: string;
    // Skeletal mesh data interface
    skeletalMeshPath?: string;
    useWholeSkeletonOrBones?: string;
    specificBones?: string[];
    samplingMode?: string;
    // Event handling
    eventName?: string;
    eventPayload?: Array<Record<string, unknown>>;
    spawnOnEvent?: boolean;
    eventSpawnCount?: number;
    // GPU simulation
    gpuEnabled?: boolean;
    fixedBoundsEnabled?: boolean;
    deterministicEnabled?: boolean;
    fixedBounds?: number[];
    stageName?: string;
    stageIterationSource?: string;
    // Graph operations (C++ TryGetStringField)
    scriptType?: string;
    fromNode?: string;
    fromPin?: string;
    toNode?: string;
    toPin?: string;
    nodeId?: string;
    // Emitter properties
    emitterProperties?: { enabled?: boolean };
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
    // normalizeFiniteLocation/normalizeFiniteRotation accept either form, and
    // the [x, y, z] array is what the record examples and callers use.
    location?: Vector3 | readonly number[];
    rotation?: Rotator | readonly number[];
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
    // The only volumetric-fog knob the native handler applies. density,
    // scatteringIntensity and fogHeight used to live here and were forwarded
    // to a handler that reads none of them.
    viewDistance?: number;
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
    // Virtual shadow maps are a DIFFERENT feature from ray-traced shadows; the
    // native handler applies only this one and explicitly refuses to treat
    // rayTracedShadows as a substitute.
    virtualShadowMaps?: boolean;
    compensationValue?: number;
    minBrightness?: number;
    maxBrightness?: number;
    indirectLightingIntensity?: number;
    buildOnlySelected?: boolean;
    buildReflectionCaptures?: boolean;
}
