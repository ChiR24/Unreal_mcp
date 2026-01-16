/**
 * Phase 35: Additional Gameplay Systems Handlers
 * 
 * Implements common gameplay patterns:
 * - Targeting (lock-on, aim assist)
 * - Checkpoints (save/load)
 * - Objectives (quest objectives)
 * - World Markers (ping systems)
 * - Photo Mode
 * - Quest/Dialogue systems
 * - Instancing (ISMC, HISMC)
 * - HLOD management
 * - Localization (string tables)
 * - Scalability (device profiles)
 */

import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';

/** Quest reward structure */
export interface QuestReward {
  type: 'item' | 'currency' | 'experience' | 'reputation' | 'custom';
  id?: string;
  amount?: number;
  data?: Record<string, unknown>;
}

export interface GameplaySystemsArgs {
  action?: string;
  
  // Targeting parameters
  actorName?: string;
  componentName?: string;
  maxTargetingRange?: number;
  targetingConeAngle?: number;
  autoTargetNearest?: boolean;
  lockOnRange?: number;
  lockOnAngle?: number;
  breakLockOnDistance?: number;
  stickyLockOn?: boolean;
  lockOnSpeed?: number;
  aimAssistStrength?: number;
  aimAssistRadius?: number;
  magnetismStrength?: number;
  bulletMagnetism?: boolean;
  frictionZoneScale?: number;
  
  // Checkpoint parameters
  location?: { x: number; y: number; z: number };
  rotation?: { pitch: number; yaw: number; roll: number };
  checkpointId?: string;
  autoActivate?: boolean;
  triggerRadius?: number;
  slotName?: string;
  playerIndex?: number;
  saveWorldState?: boolean;
  
  // Objective parameters
  objectiveId?: string;
  objectiveName?: string;
  description?: string;
  objectiveType?: string;
  initialState?: string;
  parentObjectiveId?: string;
  trackingType?: string;
  state?: string;
  progress?: number;
  notify?: boolean;
  showOnCompass?: boolean;
  showOnMap?: boolean;
  showInWorld?: boolean;
  markerIcon?: string;
  markerColor?: { r: number; g: number; b: number; a: number };
  distanceDisplay?: boolean;
  
  // World Marker parameters
  markerId?: string;
  markerType?: string;
  iconPath?: string;
  label?: string;
  color?: { r: number; g: number; b: number; a: number };
  lifetime?: number;
  visibleRange?: number;
  maxPingsPerPlayer?: number;
  pingLifetime?: number;
  pingCooldown?: number;
  replicatedPings?: boolean;
  contextualPings?: boolean;
  widgetClass?: string;
  clampToScreen?: boolean;
  fadeWithDistance?: boolean;
  fadeStartDistance?: number;
  fadeEndDistance?: number;
  scaleWithDistance?: boolean;
  minScale?: number;
  maxScale?: number;
  
  // Photo Mode parameters
  enabled?: boolean;
  pauseGame?: boolean;
  hideUI?: boolean;
  hidePlayer?: boolean;
  allowCameraMovement?: boolean;
  maxCameraDistance?: number;
  fov?: number;
  aperture?: number;
  focalDistance?: number;
  depthOfField?: boolean;
  exposure?: number;
  contrast?: number;
  saturation?: number;
  vignetteIntensity?: number;
  filmGrain?: number;
  filename?: string;
  resolution?: string;
  format?: string;
  superSampling?: number;
  includeUI?: boolean;
  
  // Quest/Dialogue parameters
  assetPath?: string;
  questId?: string;
  questName?: string;
  questType?: string;
  prerequisites?: string[];
  rewards?: QuestReward[];
  dialogueName?: string;
  startNodeId?: string;
  nodeId?: string;
  speakerId?: string;
  text?: string;
  audioAsset?: string;
  duration?: number;
  choices?: Array<{ text: string; nextNodeId: string; condition?: string }>;
  nextNodeId?: string;
  events?: string[];
  targetActor?: string;
  skipable?: boolean;
  
  // Instancing parameters
  meshPath?: string;
  materialPath?: string;
  cullDistance?: number;
  castShadow?: boolean;
  minLOD?: number;
  useGpuLodSelection?: boolean;
  transform?: {
    location: { x: number; y: number; z: number };
    rotation: { pitch: number; yaw: number; roll: number };
    scale: { x: number; y: number; z: number };
  };
  instances?: Array<{
    location: { x: number; y: number; z: number };
    rotation: { pitch: number; yaw: number; roll: number };
    scale: { x: number; y: number; z: number };
  }>;
  instanceIndex?: number;
  instanceIndices?: number[];
  
  // HLOD parameters
  layerName?: string;
  cellSize?: number;
  loadingRange?: number;
  parentLayer?: string;
  hlodBuildMethod?: string;
  minDrawDistance?: number;
  spatiallyLoaded?: boolean;
  alwaysLoaded?: boolean;
  buildAll?: boolean;
  forceRebuild?: boolean;
  
  // Localization parameters
  tableName?: string;
  namespace?: string;
  key?: string;
  sourceString?: string;
  comment?: string;
  sourcePath?: string;
  targetPath?: string;
  outputPath?: string;
  culture?: string;
  saveToConfig?: boolean;
  
  // Scalability parameters
  profileName?: string;
  baseProfile?: string;
  deviceType?: string;
  cvars?: Record<string, string | number | boolean>;
  groupName?: string;
  qualityLevel?: number;
  overallQuality?: number;
  applyImmediately?: boolean;
  scale?: number;
  
  // Wave 3.41-3.50: Additional parameters
  // Objective chain
  objectiveIds?: string[];
  chainType?: string;
  failOnAnyFail?: boolean;
  
  // Checkpoint data
  savePlayerState?: boolean;
  saveActorStates?: boolean;
  actorFilter?: string[];
  customData?: Record<string, unknown>;
  
  // Dialogue node
  nodeType?: string;
  conditions?: string[];
  
  // Targeting priority
  targetPriorities?: Array<{ tag: string; priority: number }>;
  ignoreTags?: string[];
  preferredTargetType?: string;
  
  // Minimap icon
  iconTexture?: string;
  iconSize?: number;
  rotateWithActor?: boolean;
  visibleOnMinimap?: boolean;
  minimapLayer?: number;
  
  // Quest stage
  stageId?: string;
  stageName?: string;
  stageType?: string;
  nextStageIds?: string[];
  stageObjectives?: string[];
  
  // Game state
  stateKey?: string;
  stateValue?: unknown;
  persistent?: boolean;
  replicated?: boolean;
  
  // Save system
  saveSystemType?: string;
  maxSaveSlots?: number;
  autoSaveInterval?: number;
  compressSaves?: boolean;
  encryptSaves?: boolean;
  
  // Common
  save?: boolean;
}

export async function handleGameplaySystemsTools(
  action: string,
  args: GameplaySystemsArgs,
  tools: ITools
) {
  if (!action || typeof action !== 'string' || action.trim() === '') {
    return {
      success: false,
      error: 'manage_gameplay_systems: Missing required parameter: action',
      hint: 'Check available actions in the tool schema'
    };
  }
  
  switch (action) {
    // ==================== TARGETING ====================
    case 'create_targeting_component': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'create_targeting_component',
        actorName,
        componentName: args.componentName ?? 'TargetingComponent',
        maxTargetingRange: args.maxTargetingRange ?? 2000.0,
        targetingConeAngle: args.targetingConeAngle ?? 45.0,
        autoTargetNearest: args.autoTargetNearest ?? true,
        save: args.save ?? false
      }));
    }
    
    case 'configure_lock_on_target': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'configure_lock_on_target',
        actorName,
        componentName: args.componentName,
        lockOnRange: args.lockOnRange ?? 1500.0,
        lockOnAngle: args.lockOnAngle ?? 30.0,
        breakLockOnDistance: args.breakLockOnDistance ?? 2000.0,
        stickyLockOn: args.stickyLockOn ?? true,
        lockOnSpeed: args.lockOnSpeed ?? 10.0
      }));
    }
    
    case 'configure_aim_assist': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'configure_aim_assist',
        actorName,
        componentName: args.componentName,
        aimAssistStrength: args.aimAssistStrength ?? 0.5,
        aimAssistRadius: args.aimAssistRadius ?? 100.0,
        magnetismStrength: args.magnetismStrength ?? 0.3,
        bulletMagnetism: args.bulletMagnetism ?? false,
        frictionZoneScale: args.frictionZoneScale ?? 1.0
      }));
    }
    
    // ==================== CHECKPOINTS ====================
    case 'create_checkpoint_actor': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'create_checkpoint_actor',
        actorName: args.actorName ?? 'Checkpoint_1',
        location: args.location ?? { x: 0, y: 0, z: 0 },
        rotation: args.rotation ?? { pitch: 0, yaw: 0, roll: 0 },
        checkpointId: args.checkpointId,
        autoActivate: args.autoActivate ?? false,
        triggerRadius: args.triggerRadius ?? 200.0,
        save: args.save ?? false
      }));
    }
    
    case 'save_checkpoint': {
      const checkpointId = requireNonEmptyString(args.checkpointId, 'checkpointId');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'save_checkpoint',
        checkpointId,
        slotName: args.slotName ?? 'Checkpoint',
        playerIndex: args.playerIndex ?? 0,
        saveWorldState: args.saveWorldState ?? true
      }));
    }
    
    case 'load_checkpoint': {
      const checkpointId = requireNonEmptyString(args.checkpointId, 'checkpointId');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'load_checkpoint',
        checkpointId,
        slotName: args.slotName ?? 'Checkpoint',
        playerIndex: args.playerIndex ?? 0
      }));
    }
    
    // ==================== OBJECTIVES ====================
    case 'create_objective': {
      const objectiveId = requireNonEmptyString(args.objectiveId, 'objectiveId');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'create_objective',
        objectiveId,
        objectiveName: args.objectiveName,
        description: args.description,
        objectiveType: args.objectiveType ?? 'Primary',
        initialState: args.initialState ?? 'Inactive',
        parentObjectiveId: args.parentObjectiveId,
        trackingType: args.trackingType ?? 'None'
      }));
    }
    
    case 'set_objective_state': {
      const objectiveId = requireNonEmptyString(args.objectiveId, 'objectiveId');
      const state = requireNonEmptyString(args.state, 'state');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'set_objective_state',
        objectiveId,
        state,
        progress: args.progress,
        notify: args.notify ?? true
      }));
    }
    
    case 'configure_objective_markers': {
      const objectiveId = requireNonEmptyString(args.objectiveId, 'objectiveId');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'configure_objective_markers',
        objectiveId,
        showOnCompass: args.showOnCompass ?? true,
        showOnMap: args.showOnMap ?? true,
        showInWorld: args.showInWorld ?? true,
        markerIcon: args.markerIcon,
        markerColor: args.markerColor ?? { r: 1.0, g: 0.8, b: 0.0, a: 1.0 },
        distanceDisplay: args.distanceDisplay ?? true
      }));
    }
    
    // ==================== WORLD MARKERS ====================
    case 'create_world_marker': {
      const markerId = requireNonEmptyString(args.markerId, 'markerId');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'create_world_marker',
        markerId,
        location: args.location ?? { x: 0, y: 0, z: 0 },
        markerType: args.markerType ?? 'Generic',
        iconPath: args.iconPath,
        label: args.label,
        color: args.color ?? { r: 1.0, g: 1.0, b: 1.0, a: 1.0 },
        lifetime: args.lifetime ?? 0.0,
        visibleRange: args.visibleRange ?? 0.0
      }));
    }
    
    case 'create_ping_system': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'create_ping_system',
        actorName: args.actorName ?? 'PingSystem',
        maxPingsPerPlayer: args.maxPingsPerPlayer ?? 3,
        pingLifetime: args.pingLifetime ?? 10.0,
        pingCooldown: args.pingCooldown ?? 1.0,
        replicatedPings: args.replicatedPings ?? true,
        contextualPings: args.contextualPings ?? true
      }));
    }
    
    case 'configure_marker_widget': {
      const widgetClass = requireNonEmptyString(args.widgetClass, 'widgetClass');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'configure_marker_widget',
        widgetClass,
        clampToScreen: args.clampToScreen ?? true,
        fadeWithDistance: args.fadeWithDistance ?? true,
        fadeStartDistance: args.fadeStartDistance ?? 1000.0,
        fadeEndDistance: args.fadeEndDistance ?? 5000.0,
        scaleWithDistance: args.scaleWithDistance ?? false,
        minScale: args.minScale ?? 0.5,
        maxScale: args.maxScale ?? 1.0
      }));
    }
    
    // ==================== PHOTO MODE ====================
    case 'enable_photo_mode': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'enable_photo_mode',
        enabled: args.enabled ?? true,
        pauseGame: args.pauseGame ?? true,
        hideUI: args.hideUI ?? true,
        hidePlayer: args.hidePlayer ?? false,
        allowCameraMovement: args.allowCameraMovement ?? true,
        maxCameraDistance: args.maxCameraDistance ?? 500.0
      }));
    }
    
    case 'configure_photo_mode_camera': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'configure_photo_mode_camera',
        fov: args.fov ?? 90.0,
        aperture: args.aperture ?? 2.8,
        focalDistance: args.focalDistance ?? 1000.0,
        depthOfField: args.depthOfField ?? true,
        exposure: args.exposure ?? 0.0,
        contrast: args.contrast ?? 1.0,
        saturation: args.saturation ?? 1.0,
        vignetteIntensity: args.vignetteIntensity ?? 0.0,
        filmGrain: args.filmGrain ?? 0.0
      }));
    }
    
    case 'take_photo_mode_screenshot': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'take_photo_mode_screenshot',
        filename: args.filename,
        resolution: args.resolution ?? '1920x1080',
        format: args.format ?? 'PNG',
        superSampling: args.superSampling ?? 1,
        includeUI: args.includeUI ?? false
      }));
    }
    
    // ==================== QUEST/DIALOGUE ====================
    case 'create_quest_data_asset': {
      const assetPath = requireNonEmptyString(args.assetPath, 'assetPath');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'create_quest_data_asset',
        assetPath,
        questId: args.questId,
        questName: args.questName,
        description: args.description,
        questType: args.questType ?? 'MainQuest',
        prerequisites: args.prerequisites ?? [],
        rewards: args.rewards ?? [],
        save: args.save ?? true
      }));
    }
    
    case 'create_dialogue_tree': {
      const assetPath = requireNonEmptyString(args.assetPath, 'assetPath');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'create_dialogue_tree',
        assetPath,
        dialogueName: args.dialogueName,
        startNodeId: args.startNodeId ?? 'Start',
        save: args.save ?? true
      }));
    }
    
    case 'add_dialogue_node': {
      const assetPath = requireNonEmptyString(args.assetPath, 'assetPath');
      const nodeId = requireNonEmptyString(args.nodeId, 'nodeId');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'add_dialogue_node',
        assetPath,
        nodeId,
        speakerId: args.speakerId,
        text: args.text,
        audioAsset: args.audioAsset,
        duration: args.duration ?? 0.0,
        choices: args.choices ?? [],
        nextNodeId: args.nextNodeId,
        events: args.events ?? []
      }));
    }
    
    case 'play_dialogue': {
      const assetPath = requireNonEmptyString(args.assetPath, 'assetPath');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'play_dialogue',
        assetPath,
        startNodeId: args.startNodeId ?? 'Start',
        targetActor: args.targetActor,
        skipable: args.skipable ?? true
      }));
    }
    
    // ==================== INSTANCING ====================
    case 'create_instanced_static_mesh_component': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      const meshPath = requireNonEmptyString(args.meshPath, 'meshPath');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'create_instanced_static_mesh_component',
        actorName,
        componentName: args.componentName ?? 'InstancedStaticMesh',
        meshPath,
        materialPath: args.materialPath,
        cullDistance: args.cullDistance ?? 0,
        castShadow: args.castShadow ?? true,
        save: args.save ?? false
      }));
    }
    
    case 'create_hierarchical_instanced_static_mesh': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      const meshPath = requireNonEmptyString(args.meshPath, 'meshPath');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'create_hierarchical_instanced_static_mesh',
        actorName,
        componentName: args.componentName ?? 'HierarchicalISM',
        meshPath,
        materialPath: args.materialPath,
        cullDistance: args.cullDistance ?? 0,
        minLOD: args.minLOD ?? 0,
        castShadow: args.castShadow ?? true,
        useGpuLodSelection: args.useGpuLodSelection ?? true,
        save: args.save ?? false
      }));
    }
    
    case 'add_instance': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      const componentName = requireNonEmptyString(args.componentName, 'componentName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'add_instance',
        actorName,
        componentName,
        transform: args.transform ?? {
          location: { x: 0, y: 0, z: 0 },
          rotation: { pitch: 0, yaw: 0, roll: 0 },
          scale: { x: 1, y: 1, z: 1 }
        },
        instances: args.instances
      }));
    }
    
    case 'remove_instance': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      const componentName = requireNonEmptyString(args.componentName, 'componentName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'remove_instance',
        actorName,
        componentName,
        instanceIndex: args.instanceIndex,
        instanceIndices: args.instanceIndices
      }));
    }
    
    case 'get_instance_count': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      const componentName = requireNonEmptyString(args.componentName, 'componentName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'get_instance_count',
        actorName,
        componentName
      }));
    }
    
    // ==================== HLOD ====================
    case 'create_hlod_layer': {
      const layerName = requireNonEmptyString(args.layerName, 'layerName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'create_hlod_layer',
        layerName,
        cellSize: args.cellSize ?? 25600,
        loadingRange: args.loadingRange ?? 51200,
        parentLayer: args.parentLayer
      }));
    }
    
    case 'configure_hlod_settings': {
      const layerName = requireNonEmptyString(args.layerName, 'layerName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'configure_hlod_settings',
        layerName,
        hlodBuildMethod: args.hlodBuildMethod ?? 'MeshMerge',
        minDrawDistance: args.minDrawDistance ?? 0,
        spatiallyLoaded: args.spatiallyLoaded ?? true,
        alwaysLoaded: args.alwaysLoaded ?? false
      }));
    }
    
    case 'build_hlod': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'build_hlod',
        layerName: args.layerName,
        buildAll: args.buildAll ?? false,
        forceRebuild: args.forceRebuild ?? false
      }));
    }
    
    case 'assign_actor_to_hlod': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      const layerName = requireNonEmptyString(args.layerName, 'layerName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'assign_actor_to_hlod',
        actorName,
        layerName
      }));
    }
    
    // ==================== LOCALIZATION ====================
    case 'create_string_table': {
      const assetPath = requireNonEmptyString(args.assetPath, 'assetPath');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'create_string_table',
        assetPath,
        tableName: args.tableName,
        namespace: args.namespace ?? 'Game',
        save: args.save ?? true
      }));
    }
    
    case 'add_string_entry': {
      const assetPath = requireNonEmptyString(args.assetPath, 'assetPath');
      const key = requireNonEmptyString(args.key, 'key');
      const sourceString = requireNonEmptyString(args.sourceString, 'sourceString');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'add_string_entry',
        assetPath,
        key,
        sourceString,
        comment: args.comment,
        save: args.save ?? true
      }));
    }
    
    case 'get_string_entry': {
      const assetPath = requireNonEmptyString(args.assetPath, 'assetPath');
      const key = requireNonEmptyString(args.key, 'key');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'get_string_entry',
        assetPath,
        key
      }));
    }
    
    case 'import_localization': {
      const sourcePath = requireNonEmptyString(args.sourcePath, 'sourcePath');
      const targetPath = requireNonEmptyString(args.targetPath, 'targetPath');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'import_localization',
        sourcePath,
        targetPath,
        culture: args.culture ?? 'en',
        format: args.format ?? 'CSV'
      }));
    }
    
    case 'export_localization': {
      const assetPath = requireNonEmptyString(args.assetPath, 'assetPath');
      const outputPath = requireNonEmptyString(args.outputPath, 'outputPath');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'export_localization',
        assetPath,
        outputPath,
        culture: args.culture ?? 'en',
        format: args.format ?? 'CSV'
      }));
    }
    
    case 'set_culture': {
      const culture = requireNonEmptyString(args.culture, 'culture');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'set_culture',
        culture,
        saveToConfig: args.saveToConfig ?? true
      }));
    }
    
    case 'get_available_cultures': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'get_available_cultures'
      }));
    }
    
    // ==================== SCALABILITY ====================
    case 'create_device_profile': {
      const profileName = requireNonEmptyString(args.profileName, 'profileName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'create_device_profile',
        profileName,
        baseProfile: args.baseProfile,
        deviceType: args.deviceType ?? 'Desktop',
        cvars: args.cvars ?? {}
      }));
    }
    
    case 'configure_scalability_group': {
      const groupName = requireNonEmptyString(args.groupName, 'groupName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'configure_scalability_group',
        groupName,
        qualityLevel: args.qualityLevel ?? 3,
        cvars: args.cvars ?? {}
      }));
    }
    
    case 'set_quality_level': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'set_quality_level',
        overallQuality: args.overallQuality ?? 3,
        applyImmediately: args.applyImmediately ?? true
      }));
    }
    
    case 'get_scalability_settings': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'get_scalability_settings'
      }));
    }
    
    case 'set_resolution_scale': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'set_resolution_scale',
        scale: args.scale ?? 100.0,
        minScale: args.minScale ?? 50.0,
        maxScale: args.maxScale ?? 100.0
      }));
    }
    
    // ==================== UTILITY ====================
    case 'get_gameplay_systems_info': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'get_gameplay_systems_info'
      }));
    }
    
    // ==================== WAVE 3.41-3.50: ADDITIONAL GAMEPLAY ACTIONS ====================
    
    // 3.41: Create linked objectives
    case 'create_objective_chain': {
      const objectiveIds = args.objectiveIds;
      if (!objectiveIds || !Array.isArray(objectiveIds) || objectiveIds.length === 0) {
        return {
          success: false,
          error: 'manage_gameplay_systems: create_objective_chain requires objectiveIds array'
        };
      }
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'create_objective_chain',
        objectiveIds,
        chainType: args.chainType ?? 'Sequential',
        failOnAnyFail: args.failOnAnyFail ?? false,
        description: args.description
      }));
    }
    
    // 3.42: Configure checkpoint save data
    case 'configure_checkpoint_data': {
      const checkpointId = requireNonEmptyString(args.checkpointId, 'checkpointId');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'configure_checkpoint_data',
        checkpointId,
        savePlayerState: args.savePlayerState ?? true,
        saveActorStates: args.saveActorStates ?? true,
        saveWorldState: args.saveWorldState ?? true,
        actorFilter: args.actorFilter ?? [],
        customData: args.customData ?? {}
      }));
    }
    
    // 3.43: Create dialogue tree node
    case 'create_dialogue_node': {
      const assetPath = requireNonEmptyString(args.assetPath, 'assetPath');
      const nodeId = requireNonEmptyString(args.nodeId, 'nodeId');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'create_dialogue_node',
        assetPath,
        nodeId,
        nodeType: args.nodeType ?? 'Speech',
        speakerId: args.speakerId,
        text: args.text,
        audioAsset: args.audioAsset,
        duration: args.duration ?? 0.0,
        choices: args.choices ?? [],
        nextNodeId: args.nextNodeId,
        conditions: args.conditions ?? [],
        events: args.events ?? []
      }));
    }
    
    // 3.44: Configure targeting priorities
    case 'configure_targeting_priority': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'configure_targeting_priority',
        actorName,
        componentName: args.componentName,
        targetPriorities: args.targetPriorities ?? [],
        ignoreTags: args.ignoreTags ?? [],
        preferredTargetType: args.preferredTargetType
      }));
    }
    
    // 3.45: Enable/configure photo mode (already exists as enable_photo_mode, this is an alias)
    // Note: enable_photo_mode already implemented above
    
    // 3.46: Add/modify localization entry
    case 'configure_localization_entry': {
      const assetPath = requireNonEmptyString(args.assetPath, 'assetPath');
      const key = requireNonEmptyString(args.key, 'key');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'configure_localization_entry',
        assetPath,
        key,
        sourceString: args.sourceString,
        namespace: args.namespace ?? 'Game',
        culture: args.culture ?? 'en',
        comment: args.comment,
        save: args.save ?? true
      }));
    }
    
    // 3.47: Create quest stage
    case 'create_quest_stage': {
      const assetPath = requireNonEmptyString(args.assetPath, 'assetPath');
      const stageId = requireNonEmptyString(args.stageId, 'stageId');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'create_quest_stage',
        assetPath,
        stageId,
        stageName: args.stageName,
        description: args.description,
        stageType: args.stageType ?? 'Progress',
        nextStageIds: args.nextStageIds ?? [],
        stageObjectives: args.stageObjectives ?? [],
        save: args.save ?? true
      }));
    }
    
    // 3.48: Configure minimap display
    case 'configure_minimap_icon': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'configure_minimap_icon',
        actorName,
        iconTexture: args.iconTexture ?? args.iconPath,
        iconSize: args.iconSize ?? 32.0,
        color: args.color ?? { r: 1.0, g: 1.0, b: 1.0, a: 1.0 },
        rotateWithActor: args.rotateWithActor ?? true,
        visibleOnMinimap: args.visibleOnMinimap ?? true,
        minimapLayer: args.minimapLayer ?? 0
      }));
    }
    
    // 3.49: Set global game state value
    case 'set_game_state': {
      const stateKey = requireNonEmptyString(args.stateKey, 'stateKey');
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'set_game_state',
        stateKey,
        stateValue: args.stateValue,
        persistent: args.persistent ?? false,
        replicated: args.replicated ?? false
      }));
    }
    
    // 3.50: Configure save system settings
    case 'configure_save_system': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_gameplay_systems', {
        action: 'configure_save_system',
        saveSystemType: args.saveSystemType ?? 'Slot',
        maxSaveSlots: args.maxSaveSlots ?? 10,
        autoSaveInterval: args.autoSaveInterval ?? 0,
        compressSaves: args.compressSaves ?? true,
        encryptSaves: args.encryptSaves ?? false
      }));
    }
    
    default:
      return {
        success: false,
        error: `manage_gameplay_systems: Unknown action '${action}'`,
        hint: 'Available actions: create_targeting_component, configure_lock_on_target, configure_aim_assist, ' +
          'create_checkpoint_actor, save_checkpoint, load_checkpoint, ' +
          'create_objective, set_objective_state, configure_objective_markers, ' +
          'create_world_marker, create_ping_system, configure_marker_widget, ' +
          'enable_photo_mode, configure_photo_mode_camera, take_photo_mode_screenshot, ' +
          'create_quest_data_asset, create_dialogue_tree, add_dialogue_node, play_dialogue, ' +
          'create_instanced_static_mesh_component, create_hierarchical_instanced_static_mesh, ' +
          'add_instance, remove_instance, get_instance_count, ' +
          'create_hlod_layer, configure_hlod_settings, build_hlod, assign_actor_to_hlod, ' +
          'create_string_table, add_string_entry, get_string_entry, import_localization, ' +
          'export_localization, set_culture, get_available_cultures, ' +
          'create_device_profile, configure_scalability_group, set_quality_level, ' +
          'get_scalability_settings, set_resolution_scale, get_gameplay_systems_info, ' +
          'create_objective_chain, configure_checkpoint_data, create_dialogue_node, ' +
          'configure_targeting_priority, configure_localization_entry, create_quest_stage, ' +
          'configure_minimap_icon, set_game_state, configure_save_system'
      };
  }
}
