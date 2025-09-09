// Tool handlers for all 16 MCP tools
import { UnrealBridge } from '../unreal-bridge.js';
import { ActorTools } from './actors.js';
import { AssetTools } from './assets.js';
import { MaterialTools } from './materials.js';
import { EditorTools } from './editor.js';
import { AnimationTools } from './animation.js';
import { PhysicsTools } from './physics.js';
import { NiagaraTools } from './niagara.js';
import { BlueprintTools } from './blueprint.js';
import { LevelTools } from './level.js';
import { LightingTools } from './lighting.js';
import { LandscapeTools } from './landscape.js';
import { FoliageTools } from './foliage.js';
import { DebugVisualizationTools } from './debug.js';
import { PerformanceTools } from './performance.js';
import { AudioTools } from './audio.js';
import { UITools } from './ui.js';

export async function handleToolCall(
  name: string, 
  args: any,
  tools: {
    actorTools: ActorTools,
    assetTools: AssetTools,
    materialTools: MaterialTools,
    editorTools: EditorTools,
    animationTools: AnimationTools,
    physicsTools: PhysicsTools,
    niagaraTools: NiagaraTools,
    blueprintTools: BlueprintTools,
    levelTools: LevelTools,
    lightingTools: LightingTools,
    landscapeTools: LandscapeTools,
    foliageTools: FoliageTools,
    debugTools: DebugVisualizationTools,
    performanceTools: PerformanceTools,
    audioTools: AudioTools,
    uiTools: UITools,
    bridge: UnrealBridge
  }
) {
  try {
    let result: any;
    let message: string;

    switch (name) {
      // Asset Tools
      case 'list_assets':
        // AssetTools doesn't have listAssets, use bridge directly
        result = { assets: [] }; // TODO: Implement asset listing
        message = `Asset listing not yet implemented`;
        break;
      
      case 'import_asset':
        result = await tools.assetTools.importAsset(args.sourcePath, args.destinationPath);
        message = result.error || `Asset imported to ${args.destinationPath}`;
        break;

      // Actor Tools
      case 'spawn_actor':
        result = await tools.actorTools.spawn(args);
        message = `Actor spawned: ${JSON.stringify(result)}`;
        break;
      
      case 'delete_actor':
        // ActorTools doesn't have deleteActor, use console command
        result = await tools.bridge.httpCall('/remote/object/call', 'PUT', {
          objectPath: '/Script/Engine.Default__KismetSystemLibrary',
          functionName: 'ExecuteConsoleCommand',
          parameters: {
            Command: `DestroyActor ${args.actorName}`,
            SpecificPlayer: null
          },
          generateTransaction: false
        });
        message = `Actor deleted: ${args.actorName}`;
        break;

      // Material Tools
      case 'create_material':
        result = await tools.materialTools.createMaterial(args.name, args.path);
        message = result.success ? `Material created: ${result.path}` : result.error;
        break;

      // Editor Tools
      case 'play_in_editor':
        result = await tools.editorTools.playInEditor();
        message = result.message || 'PIE started';
        break;
      
      case 'stop_play_in_editor':
        result = await tools.editorTools.stopPlayInEditor();
        message = result.message || 'PIE stopped';
        break;
      
      case 'set_camera':
        result = await tools.editorTools.setViewportCamera(args.location, args.rotation);
        message = result.message || 'Camera set';
        break;

      // Animation Tools
      case 'create_animation_blueprint':
        result = await tools.animationTools.createAnimationBlueprint(args);
        message = result.message || `Animation blueprint ${args.name} created`;
        break;
      
      case 'play_animation_montage':
        result = await tools.animationTools.playAnimation({
          actorName: args.actorName,
          animationType: 'Montage',
          animationPath: args.montagePath,
          playRate: args.playRate
        });
        message = result.message || `Playing montage ${args.montagePath}`;
        break;

      // Physics Tools
      case 'setup_ragdoll':
        result = await tools.physicsTools.setupRagdoll(args);
        message = result.message || 'Ragdoll physics configured';
        break;
      
      case 'apply_force':
        result = await tools.physicsTools.applyForce(args);
        message = result.message || `Force applied to ${args.actorName}`;
        break;

      // Niagara Tools
      case 'create_particle_effect':
        result = await tools.niagaraTools.createEffect(args);
        message = result.message || `${args.effectType} effect created`;
        break;
      
      case 'spawn_niagara_system':
        result = await tools.niagaraTools.spawnEffect({
          systemPath: args.systemPath,
          location: args.location,
          scale: args.scale ? [args.scale, args.scale, args.scale] : undefined
        });
        message = result.message || `Niagara system spawned`;
        break;

      // Blueprint Tools
      case 'create_blueprint':
        result = await tools.blueprintTools.createBlueprint(args);
        message = result.message || `Blueprint ${args.name} created`;
        break;
      
      case 'add_blueprint_component':
        result = await tools.blueprintTools.addComponent(args);
        message = result.message || `Component ${args.componentName} added`;
        break;

      // Level Tools
      case 'load_level':
        result = await tools.levelTools.loadLevel(args);
        message = result.message || `Level ${args.levelPath} loaded`;
        break;
      
      case 'save_level':
        result = await tools.levelTools.saveLevel(args);
        message = result.message || 'Level saved';
        break;
      
      case 'stream_level':
        result = await tools.levelTools.streamLevel(args);
        message = result.message || `Level streaming updated`;
        break;

      // Lighting Tools
      case 'create_light':
        switch (args.lightType?.toLowerCase()) {
          case 'directional':
            result = await tools.lightingTools.createDirectionalLight({
              name: args.name,
              intensity: args.intensity,
              rotation: args.rotation
            });
            break;
          case 'point':
            result = await tools.lightingTools.createPointLight({
              name: args.name,
              location: args.location,
              intensity: args.intensity
            });
            break;
          case 'spot':
            result = await tools.lightingTools.createSpotLight({
              name: args.name,
              location: args.location,
              rotation: args.rotation || [0, 0, 0],
              intensity: args.intensity
            });
            break;
          case 'rect':
            result = await tools.lightingTools.createRectLight({
              name: args.name,
              location: args.location,
              rotation: args.rotation || [0, 0, 0],
              intensity: args.intensity
            });
            break;
          default:
            throw new Error(`Unknown light type: ${args.lightType}`);
        }
        message = result.message || `${args.lightType} light created`;
        break;
      
      case 'build_lighting':
        result = await tools.lightingTools.buildLighting(args);
        message = result.message || 'Lighting built';
        break;

      // Landscape Tools
      case 'create_landscape':
        result = await tools.landscapeTools.createLandscape(args);
        message = result.message || `Landscape ${args.name} created`;
        break;
      
      case 'sculpt_landscape':
        result = await tools.landscapeTools.sculptLandscape(args);
        message = result.message || 'Landscape sculpted';
        break;

      // Foliage Tools
      case 'add_foliage_type':
        result = await tools.foliageTools.addFoliageType(args);
        message = result.message || `Foliage type ${args.name} added`;
        break;
      
      case 'paint_foliage':
        result = await tools.foliageTools.paintFoliage(args);
        message = result.message || 'Foliage painted';
        break;

      // Debug Visualization Tools
      case 'draw_debug_shape':
        switch (args.shape?.toLowerCase()) {
          case 'line':
            result = await tools.debugTools.drawDebugLine({
              start: args.position,
              end: args.end || [args.position[0] + 100, args.position[1], args.position[2]],
              color: args.color,
              duration: args.duration
            });
            break;
          case 'box':
            result = await tools.debugTools.drawDebugBox({
              center: args.position,
              extent: [args.size, args.size, args.size],
              color: args.color,
              duration: args.duration
            });
            break;
          case 'sphere':
            result = await tools.debugTools.drawDebugSphere({
              center: args.position,
              radius: args.size || 50,
              color: args.color,
              duration: args.duration
            });
            break;
          default:
            throw new Error(`Unknown debug shape: ${args.shape}`);
        }
        message = `Debug ${args.shape} drawn`;
        break;
      
      case 'set_view_mode':
        result = await tools.debugTools.setViewMode(args);
        message = `View mode set to ${args.mode}`;
        break;

      // Performance Tools
      case 'start_profiling':
        result = await tools.performanceTools.startProfiling(args);
        message = result.message || `${args.type} profiling started`;
        break;
      
      case 'show_fps':
        result = await tools.performanceTools.showFPS(args);
        message = `FPS display ${args.enabled ? 'enabled' : 'disabled'}`;
        break;
      
      case 'set_scalability':
        result = await tools.performanceTools.setScalability(args);
        message = `${args.category} quality set to level ${args.level}`;
        break;

      // Audio Tools
      case 'play_sound':
        if (args.is3D !== false && args.location) {
          result = await tools.audioTools.playSoundAtLocation({
            soundPath: args.soundPath,
            location: args.location,
            volume: args.volume,
            pitch: args.pitch
          });
        } else {
          result = await tools.audioTools.playSound2D({
            soundPath: args.soundPath,
            volume: args.volume,
            pitch: args.pitch
          });
        }
        message = `Playing sound: ${args.soundPath}`;
        break;
      
      case 'create_ambient_sound':
        result = await tools.audioTools.createAmbientSound(args);
        message = result.message || `Ambient sound ${args.name} created`;
        break;

      // UI Tools
      case 'create_widget':
        result = await tools.uiTools.createWidget(args);
        message = `Widget ${args.name} created`;
        break;
      
      case 'show_widget':
        result = await tools.uiTools.setWidgetVisibility(args);
        message = `Widget ${args.widgetName} ${args.visible ? 'shown' : 'hidden'}`;
        break;
      
      case 'create_hud':
        result = await tools.uiTools.createHUD(args);
        message = result.message || `HUD ${args.name} created`;
        break;

      // Console command (fallback)
      case 'console_command':
        result = await tools.bridge.httpCall('/remote/object/call', 'PUT', {
          objectPath: '/Script/Engine.Default__KismetSystemLibrary',
          functionName: 'ExecuteConsoleCommand',
          parameters: {
            Command: args.command,
            SpecificPlayer: null
          },
          generateTransaction: false
        });
        message = `Command executed: ${args.command}`;
        break;

      default:
        throw new Error(`Unknown tool: ${name}`);
    }

    return {
      content: [{
        type: 'text',
        text: message
      }],
      isError: false
    };

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
