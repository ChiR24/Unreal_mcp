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
  deleted?: string;
  physicsEnabled?: boolean;
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
export type AssetAction = 'list' | 'import' | 'create_material';
export type ActorAction = 'spawn' | 'delete' | 'apply_force';
export type EditorAction = 'play' | 'stop' | 'set_camera' | 'set_view_mode';
export type LevelAction = 'load' | 'save' | 'stream' | 'create_light' | 'build_lighting';
export type AnimationAction = 'create_animation_bp' | 'play_montage' | 'setup_ragdoll';
export type EffectAction = 'particle' | 'niagara' | 'debug_shape';
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
  };

  control_actor: {
    action: ActorAction;
    actorName?: string;
    classPath?: string;
    location?: Vector3D;
    rotation?: Rotation3D;
    force?: Vector3D;
  };

  control_editor: {
    action: EditorAction;
    location?: Vector3D;
    rotation?: Rotation3D;
    viewMode?: string;
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
    name?: string;
    actorName?: string;
    skeletonPath?: string;
    montagePath?: string;
    animationPath?: string;
    playRate?: number;
    physicsAssetName?: string;
    blendWeight?: number;
    savePath?: string;
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
    duration?: number;
  };

  manage_blueprint: {
    action: BlueprintAction;
    name: string;
    blueprintType?: string;
    componentType?: string;
    componentName?: string;
    savePath?: string;
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
    resolution?: string;
    projectPath?: string;
    editorExe?: string;
    filter_category?: string | string[];
    filter_level?: 'Error' | 'Warning' | 'Log' | 'Verbose' | 'VeryVerbose' | 'All';
    lines?: number;
    log_path?: string;
    include_prefixes?: string[];
    exclude_categories?: string[];
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

