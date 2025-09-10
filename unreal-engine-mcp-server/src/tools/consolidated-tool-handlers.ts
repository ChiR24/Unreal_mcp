// Consolidated tool handlers - maps 10 tools to all 36 operations
import { handleToolCall } from './tool-handlers.js';

export async function handleConsolidatedToolCall(
  name: string,
  args: any,
  tools: any
) {
  try {
    let mappedName: string;
    let mappedArgs: any = { ...args };

    switch (name) {
      // 1. ASSET MANAGER
      case 'manage_asset':
        switch (args.action) {
          case 'list':
            mappedName = 'list_assets';
            mappedArgs = {
              directory: args.directory,
              recursive: args.recursive
            };
            break;
          case 'import':
            mappedName = 'import_asset';
            mappedArgs = {
              sourcePath: args.sourcePath,
              destinationPath: args.destinationPath
            };
            break;
          case 'create_material':
            mappedName = 'create_material';
            mappedArgs = {
              name: args.name,
              path: args.path
            };
            break;
          default:
            throw new Error(`Unknown asset action: ${args.action}`);
        }
        break;

      // 2. ACTOR CONTROL
      case 'control_actor':
        switch (args.action) {
          case 'spawn':
            mappedName = 'spawn_actor';
            mappedArgs = {
              classPath: args.classPath,
              location: args.location,
              rotation: args.rotation
            };
            break;
          case 'delete':
            mappedName = 'delete_actor';
            mappedArgs = {
              actorName: args.actorName
            };
            break;
          case 'apply_force':
            mappedName = 'apply_force';
            mappedArgs = {
              actorName: args.actorName,
              force: args.force
            };
            break;
          default:
            throw new Error(`Unknown actor action: ${args.action}`);
        }
        break;

      // 3. EDITOR CONTROL
      case 'control_editor':
        switch (args.action) {
          case 'play':
            mappedName = 'play_in_editor';
            mappedArgs = {};
            break;
          case 'stop':
            mappedName = 'stop_play_in_editor';
            mappedArgs = {};
            break;
          case 'set_camera':
            mappedName = 'set_camera';
            mappedArgs = {
              location: args.location,
              rotation: args.rotation
            };
            break;
          case 'set_view_mode':
            mappedName = 'set_view_mode';
            mappedArgs = {
              mode: args.viewMode
            };
            break;
          default:
            throw new Error(`Unknown editor action: ${args.action}`);
        }
        break;

      // 4. LEVEL MANAGER
      case 'manage_level':
        switch (args.action) {
          case 'load':
            mappedName = 'load_level';
            mappedArgs = {
              levelPath: args.levelPath,
              streaming: args.streaming
            };
            break;
          case 'save':
            mappedName = 'save_level';
            mappedArgs = {
              levelName: args.levelName,
              savePath: args.savePath
            };
            break;
          case 'stream':
            mappedName = 'stream_level';
            mappedArgs = {
              levelName: args.levelName,
              shouldBeLoaded: args.shouldBeLoaded,
              shouldBeVisible: args.shouldBeVisible
            };
            break;
          case 'create_light':
            mappedName = 'create_light';
            mappedArgs = {
              lightType: args.lightType,
              name: args.name,
              location: args.location,
              intensity: args.intensity
            };
            break;
          case 'build_lighting':
            mappedName = 'build_lighting';
            mappedArgs = {
              quality: args.quality
            };
            break;
          default:
            throw new Error(`Unknown level action: ${args.action}`);
        }
        break;

      // 5. ANIMATION & PHYSICS
      case 'animation_physics':
        switch (args.action) {
          case 'create_animation_bp':
            mappedName = 'create_animation_blueprint';
            mappedArgs = {
              name: args.name,
              skeletonPath: args.skeletonPath,
              savePath: args.savePath
            };
            break;
          case 'play_montage':
            mappedName = 'play_animation_montage';
            mappedArgs = {
              actorName: args.actorName,
              montagePath: args.montagePath || args.animationPath,
              playRate: args.playRate
            };
            break;
          case 'setup_ragdoll':
            mappedName = 'setup_ragdoll';
            mappedArgs = {
              skeletonPath: args.skeletonPath,
              physicsAssetName: args.physicsAssetName,
              blendWeight: args.blendWeight
            };
            break;
          default:
            throw new Error(`Unknown animation/physics action: ${args.action}`);
        }
        break;

      // 6. EFFECTS SYSTEM
      case 'create_effect':
        switch (args.action) {
          case 'particle':
            mappedName = 'create_particle_effect';
            mappedArgs = {
              effectType: args.effectType,
              name: args.name,
              location: args.location
            };
            break;
          case 'niagara':
            mappedName = 'spawn_niagara_system';
            mappedArgs = {
              systemPath: args.systemPath,
              location: args.location,
              scale: args.scale
            };
            break;
          case 'debug_shape':
            mappedName = 'draw_debug_shape';
            // Convert location object to array for position
            const pos = args.location || args.position;
            mappedArgs = {
              shape: args.shape,
              position: pos ? [pos.x || 0, pos.y || 0, pos.z || 0] : [0, 0, 0],
              size: args.size,
              color: args.color,
              duration: args.duration
            };
            break;
          default:
            throw new Error(`Unknown effect action: ${args.action}`);
        }
        break;

      // 7. BLUEPRINT MANAGER
      case 'manage_blueprint':
        switch (args.action) {
          case 'create':
            mappedName = 'create_blueprint';
            mappedArgs = {
              name: args.name,
              blueprintType: args.blueprintType,
              savePath: args.savePath
            };
            break;
          case 'add_component':
            mappedName = 'add_blueprint_component';
            mappedArgs = {
              blueprintName: args.name,
              componentType: args.componentType,
              componentName: args.componentName
            };
            break;
          default:
            throw new Error(`Unknown blueprint action: ${args.action}`);
        }
        break;

      // 8. ENVIRONMENT BUILDER
      case 'build_environment':
        switch (args.action) {
          case 'create_landscape':
            mappedName = 'create_landscape';
            mappedArgs = {
              name: args.name,
              sizeX: args.sizeX,
              sizeY: args.sizeY,
              materialPath: args.materialPath
            };
            break;
          case 'sculpt':
            mappedName = 'sculpt_landscape';
            mappedArgs = {
              landscapeName: args.name,
              tool: args.tool,
              brushSize: args.brushSize,
              strength: args.strength
            };
            break;
          case 'add_foliage':
            mappedName = 'add_foliage_type';
            mappedArgs = {
              name: args.name,
              meshPath: args.meshPath,
              density: args.density
            };
            break;
          case 'paint_foliage':
            mappedName = 'paint_foliage';
            mappedArgs = {
              foliageType: args.foliageType,
              position: args.position,
              brushSize: args.brushSize
            };
            break;
          default:
            throw new Error(`Unknown environment action: ${args.action}`);
        }
        break;

      // 9. SYSTEM CONTROL
      case 'system_control':
        switch (args.action) {
          case 'profile':
            mappedName = 'start_profiling';
            mappedArgs = {
              type: args.profileType,
              duration: args.duration
            };
            break;
          case 'show_fps':
            mappedName = 'show_fps';
            mappedArgs = {
              enabled: args.enabled,
              verbose: args.verbose
            };
            break;
          case 'set_quality':
            mappedName = 'set_scalability';
            mappedArgs = {
              category: args.category,
              level: args.level
            };
            break;
          case 'play_sound':
            mappedName = 'play_sound';
            mappedArgs = {
              soundPath: args.soundPath,
              location: args.location,
              volume: args.volume,
              is3D: args.is3D
            };
            break;
          case 'create_widget':
            mappedName = 'create_widget';
            mappedArgs = {
              name: args.widgetName,
              type: args.widgetType,
              savePath: args.savePath
            };
            break;
          case 'show_widget':
            mappedName = 'show_widget';
            mappedArgs = {
              widgetName: args.widgetName,
              visible: args.visible
            };
            break;
          default:
            throw new Error(`Unknown system action: ${args.action}`);
        }
        break;

      // 10. CONSOLE COMMAND - pass through
      case 'console_command':
        mappedName = 'console_command';
        mappedArgs = args;
        break;

      default:
        throw new Error(`Unknown consolidated tool: ${name}`);
    }

    // Call the original handler with mapped name and args
    return await handleToolCall(mappedName, mappedArgs, tools);

  } catch (err) {
    return {
      content: [{
        type: 'text',
        text: `Failed to execute ${name}: ${err}`
      }],
      isError: true
    };
  }
}
