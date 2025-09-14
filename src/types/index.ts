/**
 * Comprehensive type definitions for Unreal Engine MCP Server
 */

// Re-export existing types
export * from './env.js';
export * from './tool-types.js';

// Common Unreal Engine types
export interface Vector3 {
  x: number;
  y: number;
  z: number;
}

export interface Rotation3 {
  pitch: number;
  yaw: number;
  roll: number;
}

export interface Transform {
  location: Vector3;
  rotation: Rotation3;
  scale?: Vector3;
}

export interface Color {
  r: number;
  g: number;
  b: number;
  a?: number;
}

// Asset types
export interface Asset {
  Name: string;
  Path: string;
  Class: string;
  PackagePath: string;
  Type?: string;
  Size?: number;
  LastModified?: Date;
}

export interface Material extends Asset {
  BaseColor?: Color;
  Metallic?: number;
  Roughness?: number;
  Emissive?: Color;
}

export interface Texture extends Asset {
  Width?: number;
  Height?: number;
  Format?: string;
  MipLevels?: number;
}

// Actor types
export interface Actor {
  Name: string;
  Class: string;
  Path: string;
  Transform?: Transform;
  Components?: Component[];
  Tags?: string[];
  IsSelected?: boolean;
  IsHidden?: boolean;
}

export interface Component {
  Name: string;
  Class: string;
  Properties?: Record<string, any>;
}

// Level types
export interface Level {
  Name: string;
  Path: string;
  IsLoaded: boolean;
  IsVisible: boolean;
  Actors?: Actor[];
  StreamingLevels?: StreamingLevel[];
}

export interface StreamingLevel {
  Name: string;
  Path: string;
  LoadingState: 'Unloaded' | 'Loading' | 'Loaded';
  ShouldBeLoaded: boolean;
  ShouldBeVisible: boolean;
}

// Blueprint types
export interface Blueprint {
  Name: string;
  Path: string;
  ParentClass: string;
  Components?: BlueprintComponent[];
  Variables?: BlueprintVariable[];
  Functions?: BlueprintFunction[];
}

export interface BlueprintComponent {
  Name: string;
  Type: string;
  DefaultProperties?: Record<string, any>;
}

export interface BlueprintVariable {
  Name: string;
  Type: string;
  DefaultValue?: any;
  IsPublic?: boolean;
  Category?: string;
}

export interface BlueprintFunction {
  Name: string;
  ReturnType?: string;
  Parameters?: FunctionParameter[];
  IsPublic?: boolean;
}

export interface FunctionParameter {
  Name: string;
  Type: string;
  DefaultValue?: any;
  IsOptional?: boolean;
}

// Animation types
export interface AnimationSequence {
  Name: string;
  Path: string;
  Duration: number;
  FrameRate: number;
  Skeleton?: string;
}

export interface AnimationMontage {
  Name: string;
  Path: string;
  Sections?: MontageSection[];
  BlendIn?: number;
  BlendOut?: number;
}

export interface MontageSection {
  Name: string;
  StartTime: number;
  EndTime: number;
  NextSection?: string;
}

// Physics types
export interface PhysicsBody {
  Mass: number;
  LinearDamping: number;
  AngularDamping: number;
  EnableGravity: boolean;
  IsKinematic: boolean;
  CollisionEnabled: boolean;
}

export interface PhysicsConstraint {
  Name: string;
  Actor1: string;
  Actor2: string;
  LinearLimits?: Vector3;
  AngularLimits?: Vector3;
}

// Niagara/VFX types
export interface NiagaraSystem {
  Name: string;
  Path: string;
  Emitters?: NiagaraEmitter[];
  Parameters?: NiagaraParameter[];
}

export interface NiagaraEmitter {
  Name: string;
  SpawnRate: number;
  Lifetime: number;
  VelocityModule?: Vector3;
  ColorModule?: Color;
}

export interface NiagaraParameter {
  Name: string;
  Type: string;
  Value: any;
}

// Landscape types
export interface Landscape {
  Name: string;
  ComponentCount: number;
  Resolution: { x: number; y: number };
  Scale: Vector3;
  Materials?: string[];
  Layers?: LandscapeLayer[];
}

export interface LandscapeLayer {
  Name: string;
  BlendMode: string;
  Weight: number;
}

// Remote Control types
export interface RemoteControlPreset {
  Name: string;
  Path: string;
  ExposedProperties?: ExposedProperty[];
  ExposedFunctions?: ExposedFunction[];
}

export interface ExposedProperty {
  Name: string;
  DisplayName?: string;
  ObjectPath: string;
  PropertyPath: string;
  Type: string;
  Value?: any;
  Metadata?: Record<string, any>;
}

export interface ExposedFunction {
  Name: string;
  DisplayName?: string;
  ObjectPath: string;
  FunctionName: string;
  Parameters?: FunctionParameter[];
}

// Sequencer types
export interface LevelSequence {
  Name: string;
  Path: string;
  Duration: number;
  FrameRate: number;
  Tracks?: SequencerTrack[];
}

export interface SequencerTrack {
  Name: string;
  Type: string;
  BoundObject?: string;
  Sections?: SequencerSection[];
}

export interface SequencerSection {
  StartFrame: number;
  EndFrame: number;
  Properties?: Record<string, any>;
}

// Performance types
export interface PerformanceMetrics {
  FPS: number;
  FrameTime: number;
  GameThreadTime: number;
  RenderThreadTime: number;
  GPUTime: number;
  DrawCalls: number;
  Triangles: number;
  Memory: MemoryMetrics;
}

export interface MemoryMetrics {
  Physical: number;
  Virtual: number;
  GPU: number;
  TextureMemory: number;
  MeshMemory: number;
}

// Engine info types
export interface EngineVersion {
  Major: number;
  Minor: number;
  Patch: number;
  Build?: number;
  Branch?: string;
  Compatible?: boolean;
}

export interface ProjectInfo {
  Name: string;
  Path: string;
  EngineVersion: string;
  Plugins?: PluginInfo[];
  Settings?: ProjectSettings;
}

export interface PluginInfo {
  Name: string;
  Version: string;
  Enabled: boolean;
  Category?: string;
}

export interface ProjectSettings {
  DefaultMap?: string;
  DefaultGameMode?: string;
  TargetFrameRate?: number;
  EnableRayTracing?: boolean;
  EnableNanite?: boolean;
}

// Response types
export interface SuccessResponse<T = any> {
  success: true;
  data?: T;
  message?: string;
  metadata?: Record<string, any>;
}

export interface ErrorResponse {
  success: false;
  error: string;
  code?: string;
  statusCode?: number;
  details?: Record<string, any>;
}

export type ApiResponse<T = any> = SuccessResponse<T> | ErrorResponse;

// Tool execution types
export interface ToolContext {
  bridge: any; // UnrealBridge instance
  tools: Record<string, any>;
  cache?: any;
  metrics?: any;
}

export interface ToolResult<T = any> {
  content: Array<{
    type: 'text' | 'json' | 'error';
    text?: string;
    data?: T;
  }>;
  isError?: boolean;
  metadata?: Record<string, any>;
}

// Event types
export interface UnrealEvent {
  type: string;
  timestamp: Date;
  data: any;
  source?: string;
}

export interface PropertyChangeEvent extends UnrealEvent {
  type: 'property_change';
  objectPath: string;
  propertyName: string;
  oldValue: any;
  newValue: any;
}

export interface ActorSpawnEvent extends UnrealEvent {
  type: 'actor_spawn';
  actorName: string;
  actorClass: string;
  location: Vector3;
}

// Utility types
export type DeepPartial<T> = {
  [P in keyof T]?: T[P] extends object ? DeepPartial<T[P]> : T[P];
};

export type Nullable<T> = T | null;

export type Optional<T> = T | undefined;

export type AsyncResult<T> = Promise<ApiResponse<T>>;

export type Callback<T> = (error: Error | null, result?: T) => void;

// Type guards
export function isVector3(value: any): value is Vector3 {
  return (
    typeof value === 'object' &&
    value !== null &&
    typeof value.x === 'number' &&
    typeof value.y === 'number' &&
    typeof value.z === 'number'
  );
}

export function isRotation3(value: any): value is Rotation3 {
  return (
    typeof value === 'object' &&
    value !== null &&
    typeof value.pitch === 'number' &&
    typeof value.yaw === 'number' &&
    typeof value.roll === 'number'
  );
}

export function isSuccessResponse<T>(response: ApiResponse<T>): response is SuccessResponse<T> {
  return response.success === true;
}

export function isErrorResponse(response: ApiResponse): response is ErrorResponse {
  return response.success === false;
}