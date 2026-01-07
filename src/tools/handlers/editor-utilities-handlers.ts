import { cleanObject } from '../../utils/safe-json.js';
import { ITools } from '../../types/tool-interfaces.js';
import { executeAutomationRequest, requireNonEmptyString } from './common-handlers.js';

/**
 * Editor Utilities Handlers - Phase 34
 * 
 * Covers:
 * - Editor Modes (set_editor_mode, configure_editor_preferences, set_grid_settings, set_snap_settings)
 * - Content Browser (navigate_to_path, sync_to_asset, create_collection, add_to_collection, show_in_explorer)
 * - Selection (select_actor, select_actors_by_class, select_actors_by_tag, deselect_all, group_actors, ungroup_actors)
 * - Collision (create_collision_channel, create_collision_profile, configure_channel_responses, get_collision_info)
 * - Physics Materials (create_physical_material, set_friction, set_restitution, configure_surface_type, get_physical_material_info)
 * - Subsystems (create_game_instance_subsystem, create_world_subsystem, create_local_player_subsystem, get_subsystem_info)
 * - Timers (set_timer, clear_timer, clear_all_timers, get_active_timers)
 * - Delegates (create_event_dispatcher, bind_to_event, unbind_from_event, broadcast_event, create_blueprint_interface)
 * - Undo/Redo (begin_transaction, end_transaction, cancel_transaction, undo, redo, get_transaction_history)
 */

export interface EditorUtilitiesArgs {
  action?: string;
  
  // Editor Mode parameters
  modeName?: string;
  category?: string;
  preferences?: Record<string, unknown>;
  gridSize?: number;
  rotationSnap?: number;
  scaleSnap?: number;
  
  // Content Browser parameters
  path?: string;
  assetPath?: string;
  collectionName?: string;
  collectionType?: string;
  assetPaths?: string[];
  
  // Selection parameters
  actorName?: string;
  className?: string;
  tag?: string;
  addToSelection?: boolean;
  groupName?: string;
  
  // Collision parameters
  channelName?: string;
  channelType?: string;
  profileName?: string;
  collisionEnabled?: boolean;
  objectType?: string;
  responses?: Record<string, string>;
  
  // Physical Material parameters
  materialName?: string;
  friction?: number;
  staticFriction?: number;
  restitution?: number;
  density?: number;
  surfaceType?: string;
  
  // Subsystem parameters
  subsystemClass?: string;
  parentClass?: string;
  
  // Timer parameters
  timerHandle?: string;
  duration?: number;
  looping?: boolean;
  functionName?: string;
  targetActor?: string;
  
  // Delegate parameters
  dispatcherName?: string;
  eventName?: string;
  blueprintPath?: string;
  interfaceName?: string;
  functions?: Array<{ name: string; inputs?: Array<{ name: string; type: string }>; outputs?: Array<{ name: string; type: string }> }>;
  
  // Transaction parameters
  transactionName?: string;
  transactionId?: string;
  
  // Common
  save?: boolean;
}

export async function handleEditorUtilitiesTools(
  action: string,
  args: EditorUtilitiesArgs,
  tools: ITools
) {
  if (!action || typeof action !== 'string' || action.trim() === '') {
    throw new Error('manage_editor_utilities: Missing required parameter: action');
  }
  
  switch (action) {
    // ==================== EDITOR MODES ====================
    case 'set_editor_mode': {
      const modeName = requireNonEmptyString(args.modeName, 'modeName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'set_editor_mode',
        modeName
      }));
    }
    
    case 'configure_editor_preferences': {
      const category = requireNonEmptyString(args.category, 'category');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'configure_editor_preferences',
        category,
        preferences: args.preferences ?? {}
      }));
    }
    
    case 'set_grid_settings': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'set_grid_settings',
        gridSize: args.gridSize ?? 10,
        rotationSnap: args.rotationSnap ?? 15,
        scaleSnap: args.scaleSnap ?? 0.25
      }));
    }
    
    case 'set_snap_settings': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'set_snap_settings',
        gridSize: args.gridSize,
        rotationSnap: args.rotationSnap,
        scaleSnap: args.scaleSnap
      }));
    }
    
    // ==================== CONTENT BROWSER ====================
    case 'navigate_to_path': {
      const path = requireNonEmptyString(args.path || args.assetPath, 'path');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'navigate_to_path',
        path
      }));
    }
    
    case 'sync_to_asset': {
      const assetPath = requireNonEmptyString(args.assetPath, 'assetPath');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'sync_to_asset',
        assetPath
      }));
    }
    
    case 'create_collection': {
      const collectionName = requireNonEmptyString(args.collectionName, 'collectionName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'create_collection',
        collectionName,
        collectionType: args.collectionType ?? 'Local'
      }));
    }
    
    case 'add_to_collection': {
      const collectionName = requireNonEmptyString(args.collectionName, 'collectionName');
      const assetPaths = args.assetPaths ?? (args.assetPath ? [args.assetPath] : []);
      if (assetPaths.length === 0) {
        throw new Error('add_to_collection requires assetPaths or assetPath');
      }
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'add_to_collection',
        collectionName,
        assetPaths
      }));
    }
    
    case 'show_in_explorer': {
      const path = requireNonEmptyString(args.path || args.assetPath, 'path');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'show_in_explorer',
        path
      }));
    }
    
    // ==================== SELECTION ====================
    case 'select_actor': {
      const actorName = requireNonEmptyString(args.actorName, 'actorName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'select_actor',
        actorName,
        addToSelection: args.addToSelection ?? false
      }));
    }
    
    case 'select_actors_by_class': {
      const className = requireNonEmptyString(args.className, 'className');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'select_actors_by_class',
        className,
        addToSelection: args.addToSelection ?? false
      }));
    }
    
    case 'select_actors_by_tag': {
      const tag = requireNonEmptyString(args.tag, 'tag');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'select_actors_by_tag',
        tag,
        addToSelection: args.addToSelection ?? false
      }));
    }
    
    case 'deselect_all': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'deselect_all'
      }));
    }
    
    case 'group_actors': {
      const groupName = args.groupName ?? 'NewGroup';
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'group_actors',
        groupName
      }));
    }
    
    case 'ungroup_actors': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'ungroup_actors'
      }));
    }
    
    case 'get_selected_actors': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'get_selected_actors'
      }));
    }
    
    // ==================== COLLISION ====================
    case 'create_collision_channel': {
      const channelName = requireNonEmptyString(args.channelName, 'channelName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'create_collision_channel',
        channelName,
        channelType: args.channelType ?? 'Object'
      }));
    }
    
    case 'create_collision_profile': {
      const profileName = requireNonEmptyString(args.profileName, 'profileName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'create_collision_profile',
        profileName,
        collisionEnabled: args.collisionEnabled ?? true,
        objectType: args.objectType ?? 'WorldDynamic'
      }));
    }
    
    case 'configure_channel_responses': {
      const profileName = requireNonEmptyString(args.profileName, 'profileName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'configure_channel_responses',
        profileName,
        responses: args.responses ?? {}
      }));
    }
    
    case 'get_collision_info': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'get_collision_info'
      }));
    }
    
    // ==================== PHYSICAL MATERIALS ====================
    case 'create_physical_material': {
      const materialName = requireNonEmptyString(args.materialName || args.assetPath, 'materialName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'create_physical_material',
        materialName,
        friction: args.friction ?? 0.7,
        restitution: args.restitution ?? 0.3,
        density: args.density ?? 1.0,
        save: args.save ?? true
      }));
    }
    
    case 'set_friction': {
      const assetPath = requireNonEmptyString(args.assetPath || args.materialName, 'assetPath');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'set_friction',
        assetPath,
        friction: args.friction ?? 0.7,
        staticFriction: args.staticFriction,
        save: args.save ?? true
      }));
    }
    
    case 'set_restitution': {
      const assetPath = requireNonEmptyString(args.assetPath || args.materialName, 'assetPath');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'set_restitution',
        assetPath,
        restitution: args.restitution ?? 0.3,
        save: args.save ?? true
      }));
    }
    
    case 'configure_surface_type': {
      const assetPath = requireNonEmptyString(args.assetPath || args.materialName, 'assetPath');
      const surfaceType = requireNonEmptyString(args.surfaceType, 'surfaceType');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'configure_surface_type',
        assetPath,
        surfaceType,
        save: args.save ?? true
      }));
    }
    
    case 'get_physical_material_info': {
      const assetPath = requireNonEmptyString(args.assetPath || args.materialName, 'assetPath');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'get_physical_material_info',
        assetPath
      }));
    }
    
    // ==================== SUBSYSTEMS ====================
    case 'create_game_instance_subsystem': {
      const subsystemClass = requireNonEmptyString(args.subsystemClass || args.assetPath, 'subsystemClass');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'create_game_instance_subsystem',
        subsystemClass,
        save: args.save ?? true
      }));
    }
    
    case 'create_world_subsystem': {
      const subsystemClass = requireNonEmptyString(args.subsystemClass || args.assetPath, 'subsystemClass');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'create_world_subsystem',
        subsystemClass,
        save: args.save ?? true
      }));
    }
    
    case 'create_local_player_subsystem': {
      const subsystemClass = requireNonEmptyString(args.subsystemClass || args.assetPath, 'subsystemClass');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'create_local_player_subsystem',
        subsystemClass,
        save: args.save ?? true
      }));
    }
    
    case 'get_subsystem_info': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'get_subsystem_info'
      }));
    }
    
    // ==================== TIMERS ====================
    case 'set_timer': {
      const functionName = requireNonEmptyString(args.functionName, 'functionName');
      const targetActor = requireNonEmptyString(args.targetActor || args.actorName, 'targetActor');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'set_timer',
        functionName,
        targetActor,
        duration: args.duration ?? 1.0,
        looping: args.looping ?? false
      }));
    }
    
    case 'clear_timer': {
      const timerHandle = requireNonEmptyString(args.timerHandle, 'timerHandle');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'clear_timer',
        timerHandle
      }));
    }
    
    case 'clear_all_timers': {
      const targetActor = args.targetActor || args.actorName;
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'clear_all_timers',
        targetActor
      }));
    }
    
    case 'get_active_timers': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'get_active_timers',
        targetActor: args.targetActor || args.actorName
      }));
    }
    
    // ==================== DELEGATES ====================
    case 'create_event_dispatcher': {
      const blueprintPath = requireNonEmptyString(args.blueprintPath, 'blueprintPath');
      const dispatcherName = requireNonEmptyString(args.dispatcherName, 'dispatcherName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'create_event_dispatcher',
        blueprintPath,
        dispatcherName,
        save: args.save ?? true
      }));
    }
    
    case 'bind_to_event': {
      const blueprintPath = requireNonEmptyString(args.blueprintPath, 'blueprintPath');
      const eventName = requireNonEmptyString(args.eventName || args.dispatcherName, 'eventName');
      const functionName = requireNonEmptyString(args.functionName, 'functionName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'bind_to_event',
        blueprintPath,
        eventName,
        functionName,
        save: args.save ?? true
      }));
    }
    
    case 'unbind_from_event': {
      const blueprintPath = requireNonEmptyString(args.blueprintPath, 'blueprintPath');
      const eventName = requireNonEmptyString(args.eventName || args.dispatcherName, 'eventName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'unbind_from_event',
        blueprintPath,
        eventName,
        save: args.save ?? true
      }));
    }
    
    case 'broadcast_event': {
      const targetActor = requireNonEmptyString(args.targetActor || args.actorName, 'targetActor');
      const eventName = requireNonEmptyString(args.eventName || args.dispatcherName, 'eventName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'broadcast_event',
        targetActor,
        eventName
      }));
    }
    
    case 'create_blueprint_interface': {
      const interfaceName = requireNonEmptyString(args.interfaceName || args.assetPath, 'interfaceName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'create_blueprint_interface',
        interfaceName,
        functions: args.functions ?? [],
        save: args.save ?? true
      }));
    }
    
    // ==================== UNDO/REDO & TRANSACTIONS ====================
    case 'begin_transaction': {
      const transactionName = requireNonEmptyString(args.transactionName, 'transactionName');
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'begin_transaction',
        transactionName
      }));
    }
    
    case 'end_transaction': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'end_transaction'
      }));
    }
    
    case 'cancel_transaction': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'cancel_transaction'
      }));
    }
    
    case 'undo': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'undo'
      }));
    }
    
    case 'redo': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'redo'
      }));
    }
    
    case 'get_transaction_history': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'get_transaction_history'
      }));
    }
    
    // ==================== UTILITY ====================
    case 'get_editor_utilities_info': {
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', {
        action: 'get_editor_utilities_info'
      }));
    }
    
    default:
      // Pass through to C++ for any unhandled actions
      return cleanObject(await executeAutomationRequest(tools, 'manage_editor_utilities', { ...args, action }));
  }
}
