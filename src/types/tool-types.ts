/**
 * Auto-generated TypeScript types from tool schemas
 * This provides type safety and IntelliSense support
 */

// Base response types
export interface BaseToolResponse {
  success: boolean;
  message?: string;
  error?: string;
  warning?: string;
}

// Asset Management Types
export interface AssetInfo {
  Name: string;
  Path: string;
  Class?: string;
  PackagePath?: string;
}

export interface ManageAssetResponse extends BaseToolResponse {
  assets?: AssetInfo[];
  paths?: string[];
  materialPath?: string;
  materialInstancePath?: string;
}

// Actor Control Types
export interface Vector3D {
  x: number;
  y: number;
  z: number;
}

export interface Rotation3D {
  pitch: number;
  yaw: number;
  roll: number;
}

export interface ControlActorResponse extends BaseToolResponse {
  actor?: string;
  actorName?: string;
  actorPath?: string;
  componentName?: string;
  componentPath?: string;
  componentClass?: string;
  componentPaths?: Array<{ name: string; path: string; class?: string }>;
  components?: Array<Record<string, unknown>>;
  actors?: Array<Record<string, unknown>>;
  deleted?: string | string[];
  deletedCount?: number;
  missing?: string[];
  physicsEnabled?: boolean;
  visible?: boolean;
  tag?: string;
  snapshotName?: string;
}

// Editor Control Types
export interface ControlEditorResponse extends BaseToolResponse {
  playing?: boolean;
  location?: [number, number, number];
  rotation?: [number, number, number];
  viewMode?: string;
}

// Level Management Types
export interface ManageLevelResponse extends BaseToolResponse {
  levelName?: string;
  loaded?: boolean;
  visible?: boolean;
  lightName?: string;
  buildQuality?: string;
}

// Animation & Physics Types
export interface AnimationPhysicsResponse extends BaseToolResponse {
  blueprintPath?: string;
  playing?: boolean;
  playRate?: number;
  ragdollActive?: boolean;
  path?: string;
  blendSpacePath?: string;
  skeletonPath?: string;
  controlRigPath?: string;
  twoDimensional?: boolean;
  warnings?: string[];
  details?: unknown;
}

// Effects System Types
export interface CreateEffectResponse extends BaseToolResponse {
  effectName?: string;
  effectPath?: string;
  spawned?: boolean;
  location?: [number, number, number];
}

// Blueprint Manager Types
export interface ManageBlueprintResponse extends BaseToolResponse {
  blueprintPath?: string;
  componentAdded?: string;
}

// Environment Builder Types
export interface BuildEnvironmentResponse extends BaseToolResponse {
  landscapeName?: string;
  foliageTypeName?: string;
  instancesPlaced?: number;
}

// System Control Types
export interface SystemControlResponse extends BaseToolResponse {
  profiling?: boolean;
  fpsVisible?: boolean;
  qualityLevel?: number;
  soundPlaying?: boolean;
  widgetPath?: string;
  widgetVisible?: boolean;
  imagePath?: string;
  imageBase64?: string;
  pid?: number;
  logPath?: string;
  entries?: Array<{ timestamp?: string; category?: string; level?: string; message: string }>;
  filteredCount?: number;
}

// Console Command Types
export interface ConsoleCommandResponse extends BaseToolResponse {
  command?: string;
  result?: any;
  info?: string;
}

// Verification Types
export interface VerifyEnvironmentResponse extends BaseToolResponse {
  exists?: boolean;
  count?: number;
  actual?: number;
  method?: string;
}

// Tool parameter types
export interface ToolParameters {
  // Asset Management
  ListAssetsParams: {
    directory: string;
    recursive?: boolean;
  };

  ImportAssetParams: {
    sourcePath: string;
    destinationPath: string;
  };

  CreateMaterialParams: {
    name: string;
    path: string;
  };

  // Actor Control
  SpawnActorParams: {
    classPath: string;
    location?: Vector3D;
    rotation?: Rotation3D;
  };

  DeleteActorParams: {
    actorName: string;
  };

  ApplyForceParams: {
    actorName: string;
    force: Vector3D;
  };

  SpawnBlueprintParams: {
    blueprintPath: string;
    actorName?: string;
    location?: Vector3D;
    rotation?: Rotation3D;
  };

  SetTransformParams: {
    actorName: string;
    location?: Vector3D;
    rotation?: Rotation3D;
    scale?: Vector3D;
  };

  SetVisibilityParams: {
    actorName: string;
    visible: boolean;
  };

  ComponentParams: {
    actorName: string;
    componentType?: string;
    componentName?: string;
    properties?: Record<string, unknown>;
  };

  DuplicateActorParams: {
    actorName: string;
    newName?: string;
    offset?: Vector3D;
  };

  AttachActorParams: {
    childActor: string;
    parentActor: string;
  };

  DetachActorParams: {
    actorName: string;
  };

  TagActorParams: {
    actorName: string;
    tag: string;
  };

  FindByTagParams: {
    tag: string;
    matchType?: string;
  };

  FindByNameParams: {
    name: string;
  };

  BlueprintVariablesParams: {
    actorName: string;
    variables: Record<string, unknown>;
  };

  SnapshotActorParams: {
    actorName: string;
    snapshotName: string;
  };

  // Editor Control
  SetCameraParams: {
    location?: Vector3D;
    rotation?: Rotation3D;
  };

  SetViewModeParams: {
    mode: string;
  };

  // Console Command
  ConsoleCommandParams: {
    command: string;
  };
}

// Consolidated tool action types
export type AssetAction = 'list' | 'import' | 'create_material' | 'create_material_instance';
export type ActorAction =
  | 'spawn'
  | 'spawn_blueprint'
  | 'delete'
  | 'delete_by_tag'
  | 'duplicate'
  | 'apply_force'
  | 'set_transform'
  | 'get_transform'
  | 'set_visibility'
  | 'add_component'
  | 'set_component_properties'
  | 'get_components'
  | 'add_tag'
  | 'find_by_tag'
  | 'find_by_name'
  | 'set_blueprint_variables'
  | 'create_snapshot'
  | 'attach'
  | 'detach';
export type EditorAction = 'play' | 'stop' | 'set_camera' | 'set_view_mode';
export type LevelAction = 'load' | 'save' | 'stream' | 'create_light' | 'build_lighting';
export type AnimationAction =
  | 'create_animation_bp'
  | 'create_anim_blueprint'
  | 'create_animation_blueprint'
  | 'play_montage'
  | 'play_anim_montage'
  | 'setup_ragdoll'
  | 'activate_ragdoll'
  | 'configure_vehicle'
  | 'create_blend_space'
  | 'create_state_machine'
  | 'setup_ik'
  | 'create_procedural_anim'
  | 'create_blend_tree'
  | 'setup_retargeting'
  | 'setup_physics_simulation'
  | 'create_animation_asset'
  | 'cleanup';
export type EffectAction =
  | 'particle'
  | 'niagara'
  | 'debug_shape'
  | 'spawn_niagara'
  | 'set_niagara_parameter'
  | 'clear_debug_shapes'
  | 'create_dynamic_light'
  | 'cleanup';
export type BlueprintAction = 'create' | 'add_component';
export type EnvironmentAction = 'create_landscape' | 'sculpt' | 'add_foliage' | 'paint_foliage';
export type SystemAction = 'profile' | 'show_fps' | 'set_quality' | 'play_sound' | 'create_widget' | 'show_widget' | 'screenshot' | 'engine_start' | 'engine_quit' | 'read_log';
export type VerificationAction = 'foliage_type_exists' | 'foliage_instances_near' | 'landscape_exists' | 'quality_level';

// Consolidated tool parameter types
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
    parameters?: Record<string, any>;
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
    lightType?: 'Directional' | 'Point' | 'Spot' | 'Rect';
    name?: string;
    location?: Vector3D;
    intensity?: number;
    quality?: 'Preview' | 'Medium' | 'High' | 'Production';
  };

  animation_physics: {
    action: AnimationAction;
    // Common
    name?: string;
    actorName?: string;
    savePath?: string;
    path?: string;

    // Animation blueprint
    skeletonPath?: string;
    blueprintName?: string;
    blueprintPath?: string;

    // Playback
    montagePath?: string;
    animationPath?: string;
    playRate?: number;

    // Ragdoll/physics
    physicsAssetName?: string;
    blendWeight?: number;

    meshPath?: string;
    assignToMesh?: boolean;
    previewSkeleton?: string;
    generateConstraints?: boolean;

    // Vehicle config
    vehicleName?: string;
    vehicleType?: 'Car' | 'Bike' | 'Tank' | 'Aircraft' | string;
    wheels?: Array<{ name: string; radius: number; width: number; mass: number; isSteering?: boolean; isDriving?: boolean }>;
    engine?: { maxRPM: number; torqueCurve: Array<[number, number]> };
    transmission?: { gears: number[]; finalDriveRatio: number };
    pluginDependencies?: string[];

    // Blend space / tree
    dimensions?: number | [number, number];
    horizontalAxis?: { name?: string; minValue?: number; maxValue?: number };
    verticalAxis?: { name?: string; minValue?: number; maxValue?: number };
    samples?: any[];

    // State machine
    states?: any[];
    transitions?: any[];

    // IK / Retargeting / Procedural
    chain?: any;
    effector?: any;
    // Retargeting
    sourceSkeleton?: string;
    targetSkeleton?: string;
    assets?: string[];
    retargetAssets?: string[];
    suffix?: string;
    overwrite?: boolean;

    // Cleanup
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
    // Dynamic light support
    lightName?: string;
    lightType?: 'Point' | 'Spot' | 'Directional' | 'Rect' | string;
    intensity?: number;
    rotation?: Rotation3D;
    pulse?: { enabled?: boolean; frequency?: number };
    // Cleanup filter for actor cleanup
    filter?: string;
    // Niagara parameter helpers
    systemName?: string;
    parameterName?: string;
    parameterType?: string;
    value?: any;
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
    // Optional: wait until the plugin's background completion event before returning
    waitForCompletion?: boolean;
    // Convenience: apply changes and force save, then wait for completion
    applyAndSave?: boolean;
    // Optional override for event wait timeout in milliseconds
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
    // Resolution / fullscreen helpers
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

// Type-safe tool response map
export interface ToolResponseMap {
  // Consolidated tools
  manage_asset: ManageAssetResponse;
  control_actor: ControlActorResponse;
  control_editor: ControlEditorResponse;
  manage_level: ManageLevelResponse;
  animation_physics: AnimationPhysicsResponse;
  create_effect: CreateEffectResponse;
  manage_blueprint: ManageBlueprintResponse;
  build_environment: BuildEnvironmentResponse;
  system_control: SystemControlResponse;
  console_command: ConsoleCommandResponse;
  verify_environment: VerifyEnvironmentResponse;

  // Individual tools (subset for brevity)
  list_assets: ManageAssetResponse;
  import_asset: ManageAssetResponse;
  spawn_actor: ControlActorResponse;
  delete_actor: ControlActorResponse;
  create_material: ManageAssetResponse;
  play_in_editor: ControlEditorResponse;
  stop_play_in_editor: ControlEditorResponse;
  set_camera: ControlEditorResponse;
}

// Helper type for tool names
export type ToolName = keyof ToolResponseMap;

// Helper type for getting response type by tool name
export type GetToolResponse<T extends ToolName> = ToolResponseMap[T];

// Helper type for getting parameters by tool name
export type GetToolParams<T extends keyof ConsolidatedToolParams> = ConsolidatedToolParams[T];

