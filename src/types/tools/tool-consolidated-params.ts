import type {
  ActorAction,
  AnimationAction,
  AssetAction,
  BlueprintAction,
  EditorAction,
  EffectAction,
  EnvironmentAction,
  LevelAction,
  SystemAction,
  VerificationAction,
} from './tool-action-types.js';
import type { Rotation3D, ScreenshotMode, Vector3D } from './tool-base-types.js';

export interface ConsolidatedToolParams {
  manage_asset: {
    action: AssetAction;
    directory?: string;
    recursive?: boolean;
    sourcePath?: string;
    destinationPath?: string;
    name?: string;
    path?: string;
    parentMaterial?: string;
    parameters?: Record<string, unknown>;
  };

  control_actor: {
    action: ActorAction;
    actorName?: string;
    classPath?: string;
    location?: Vector3D;
    rotation?: Rotation3D;
    scale?: Vector3D;
    force?: Vector3D;
    blueprintPath?: string;
    componentType?: string;
    componentName?: string;
    properties?: Record<string, unknown>;
    visible?: boolean;
    newName?: string;
    offset?: Vector3D;
    tag?: string;
    matchType?: string;
    variables?: Record<string, unknown>;
    snapshotName?: string;
    childActor?: string;
    parentActor?: string;
    actorNames?: string[];
  };

  control_editor: {
    action: EditorAction;
    location?: Vector3D;
    rotation?: Rotation3D;
    viewMode?: string;
    speed?: number;
    filename?: string;
    path?: string;
    resolution?: string;
    mode?: string;
    returnBase64?: boolean;
    includeMetadata?: boolean;
    metadata?: Record<string, unknown>;
    fov?: number;
    width?: number;
    height?: number;
    command?: string;
    steps?: number;
    frameRate?: number;
    durationSeconds?: number;
    bookmarkName?: string;
    category?: string;
    preferences?: Record<string, unknown>;
  };

  manage_level: {
    action: LevelAction;
    levelPath?: string;
    levelName?: string;
    streaming?: boolean;
    shouldBeLoaded?: boolean;
    shouldBeVisible?: boolean;
    saveDirtyPackages?: boolean;
    lightType?: 'Directional' | 'Point' | 'Spot' | 'Rect';
    name?: string;
    location?: Vector3D;
    intensity?: number;
    quality?: 'Preview' | 'Medium' | 'High' | 'Production';
  };

  animation_physics: {
    action: AnimationAction;
    name?: string;
    actorName?: string;
    savePath?: string;
    path?: string;
    skeletonPath?: string;
    blueprintName?: string;
    blueprintPath?: string;
    montagePath?: string;
    animationPath?: string;
    playRate?: number;
    physicsAssetName?: string;
    blendWeight?: number;
    meshPath?: string;
    assignToMesh?: boolean;
    previewSkeleton?: string;
    generateConstraints?: boolean;
    vehicleName?: string;
    vehicleType?: 'Car' | 'Bike' | 'Tank' | 'Aircraft' | string;
    wheels?: Array<{ name: string; radius: number; width: number; mass: number; isSteering?: boolean; isDriving?: boolean }>;
    engine?: { maxRPM: number; torqueCurve: Array<[number, number]> };
    transmission?: { gears: number[]; finalDriveRatio: number };
    pluginDependencies?: string[];
    dimensions?: number | [number, number];
    horizontalAxis?: { name?: string; minValue?: number; maxValue?: number };
    verticalAxis?: { name?: string; minValue?: number; maxValue?: number };
    samples?: Array<Record<string, unknown>>;
    states?: Array<Record<string, unknown>>;
    transitions?: Array<Record<string, unknown>>;
    chain?: Record<string, unknown>;
    effector?: Record<string, unknown>;
    sourceSkeleton?: string;
    targetSkeleton?: string;
    assets?: string[];
    retargetAssets?: string[];
    suffix?: string;
    overwrite?: boolean;
    artifacts?: string[];
    assetType?: string;
  };

  create_effect: {
    action: EffectAction;
    name?: string;
    location?: Vector3D;
    effectType?: string;
    systemPath?: string;
    scale?: number;
    shape?: string;
    size?: number;
    color?: [number, number, number, number];
    lightName?: string;
    lightType?: 'Point' | 'Spot' | 'Directional' | 'Rect' | string;
    intensity?: number;
    rotation?: Rotation3D;
    pulse?: { enabled?: boolean; frequency?: number };
    filter?: string;
    systemName?: string;
    parameterName?: string;
    parameterType?: string;
    value?: unknown;
    isUserParameter?: boolean;
    duration?: number;
  };

  manage_blueprint: {
    action: BlueprintAction;
    name: string;
    blueprintType?: string;
    componentType?: string;
    componentName?: string;
    savePath?: string;
    waitForCompletion?: boolean;
    applyAndSave?: boolean;
    waitForCompletionTimeoutMs?: number;
  };

  build_environment: {
    action: EnvironmentAction;
    name?: string;
    sizeX?: number;
    sizeY?: number;
    tool?: string;
    meshPath?: string;
    foliageType?: string;
    density?: number;
    position?: Vector3D;
    brushSize?: number;
    strength?: number;
  };

  system_control: {
    action: SystemAction;
    profileType?: string;
    category?: string;
    level?: number;
    enabled?: boolean;
    verbose?: boolean;
    soundPath?: string;
    location?: Vector3D;
    volume?: number;
    is3D?: boolean;
    widgetName?: string;
    widgetType?: string;
    visible?: boolean;
    filename?: string;
    resolution?: string;
    mode?: ScreenshotMode;
    returnBase64?: boolean;
    includeMetadata?: boolean;
    metadata?: Record<string, unknown>;
    width?: number;
    height?: number;
    windowed?: boolean;
  };

  console_command: {
    command: string;
  };

  verify_environment: {
    action: VerificationAction;
    name?: string;
    position?: Vector3D;
    radius?: number;
    category?: string;
  };
}
