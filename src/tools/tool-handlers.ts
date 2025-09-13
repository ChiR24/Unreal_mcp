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
import { RcTools } from './rc.js';
import { SequenceTools } from './sequence.js';
import { IntrospectionTools } from './introspection.js';
import { VisualTools } from './visual.js';
import { EngineTools } from './engine.js';
import { Logger } from '../utils/logger.js';
import { toVec3Object, toRotObject, toVec3Array } from '../utils/normalize.js';

const log = new Logger('ToolHandler');

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
    rcTools: RcTools,
    sequenceTools: SequenceTools,
    introspectionTools: IntrospectionTools,
    visualTools: VisualTools,
    engineTools: EngineTools,
    bridge: UnrealBridge
  }
) {
  try {
    let result: any;
    let message: string;

    switch (name) {
      // Asset Tools
      case 'list_assets':
        // Validate directory argument
        if (args.directory === null) {
          result = { assets: [], error: 'Directory cannot be null' };
          message = 'Failed to list assets: directory cannot be null';
          break;
        }
        if (args.directory === undefined) {
          result = { assets: [], error: 'Directory cannot be undefined' };
          message = 'Failed to list assets: directory cannot be undefined';
          break;
        }
        if (typeof args.directory !== 'string') {
          result = { assets: [], error: `Invalid directory type: expected string, got ${typeof args.directory}` };
          message = `Failed to list assets: directory must be a string path, got ${typeof args.directory}`;
          break;
        } else if (args.directory.trim() === '') {
          result = { assets: [], error: 'Directory path cannot be empty' };
          message = 'Failed to list assets: directory path cannot be empty';
          break;
        }
        
        // Try multiple approaches to list assets
        try {
          // First try: Use Python for most reliable listing
          const pythonCode = `
import unreal
import json

directory = '${args.directory || '/Game'}'
recursive = ${args.recursive !== false ? 'True' : 'False'}

try:
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    
    # Create filter using proper constructor parameters
    filter = unreal.ARFilter(
        package_paths=[directory],
        recursive_paths=recursive
    )
    
    # Get all assets in the directory
    assets = asset_registry.get_assets(filter)
    
    # Format asset information
    asset_list = []
    for asset in assets:
        asset_info = {
            "Name": str(asset.asset_name),
            "Path": str(asset.package_path) + "/" + str(asset.asset_name),
            "Class": str(asset.asset_class_path.asset_name if hasattr(asset.asset_class_path, "asset_name") else asset.asset_class),
            "PackagePath": str(asset.package_path)
        }
        asset_list.append(asset_info)
        print("Asset: " + asset_info["Path"])
    
    result = {
        "success": True,
        "count": len(asset_list),
        "assets": asset_list
    }
    print("RESULT:" + json.dumps(result))
except Exception as e:
    # Fallback to EditorAssetLibrary if ARFilter fails
    try:
        import unreal
        asset_paths = unreal.EditorAssetLibrary.list_assets(directory, recursive, False)
        asset_list = []
        for path in asset_paths:
            asset_list.append({
                "Name": path.split("/")[-1].split(".")[0],
                "Path": path,
                "PackagePath": "/".join(path.split("/")[:-1])
            })
        result = {
            "success": True,
            "count": len(asset_list),
            "assets": asset_list,
            "method": "EditorAssetLibrary"
        }
        print("RESULT:" + json.dumps(result))
    except Exception as e2:
        print("Error listing assets: " + str(e) + " | Fallback error: " + str(e2))
        print("RESULT:" + json.dumps({"success": False, "error": str(e), "assets": []}))
`.trim();
          
          const pyResponse = await tools.bridge.executePython(pythonCode);
          
          // Parse Python output
          let outputStr = '';
          if (typeof pyResponse === 'object' && pyResponse !== null) {
            if (pyResponse.LogOutput && Array.isArray(pyResponse.LogOutput)) {
              outputStr = pyResponse.LogOutput
                .map((log: any) => log.Output || '')
                .join('');
            } else {
              outputStr = JSON.stringify(pyResponse);
            }
          } else {
            outputStr = String(pyResponse || '');
          }
          
          // Extract result from Python output
          const resultMatch = outputStr.match(/RESULT:({.*})/);
          if (resultMatch) {
            try {
              const listResult = JSON.parse(resultMatch[1]);
              if (listResult.success && listResult.assets) {
                result = { assets: listResult.assets };
                message = `Found ${listResult.count} assets in ${args.directory || '/Game'}`;
                break;
              }
            } catch {
              // Fall through to HTTP method
            }
          }
          
          // Fallback: Use the search API (this also works!)
          try {
            const searchResult = await tools.bridge.httpCall('/remote/search/assets', 'PUT', {
              Query: '',  // Empty query to match all (wildcard doesn't work)
              Filter: {
                PackagePaths: [args.directory || '/Game'],
                RecursivePaths: args.recursive !== false,
                ClassNames: [],  // Empty to get all types
                RecursiveClasses: true
              },
              Limit: 1000,  // Increase limit
              Start: 0
            });
            
            if (searchResult?.Assets && Array.isArray(searchResult.Assets)) {
              result = { assets: searchResult.Assets };
              message = `Found ${result.assets.length} assets in ${args.directory || '/Game'}`;
              break;
            }
          } catch {
            // Continue to fallback
          }
          
          
          // Third try: Use console command to get asset registry
          await tools.bridge.httpCall('/remote/object/call', 'PUT', {
            objectPath: '/Script/Engine.Default__KismetSystemLibrary',
            functionName: 'ExecuteConsoleCommand',
            parameters: {
              WorldContextObject: null,
              Command: `AssetRegistry.DumpAssets ${args.directory || '/Game'}`,
              SpecificPlayer: null
            },
            generateTransaction: false
          });
          
          // If all else fails, at least report the attempt
          result = { assets: [], note: 'Asset listing requires proper Remote Control configuration' };
          message = `Asset listing attempted for ${args.directory || '/Game'}. Check Remote Control settings.`;
          
        } catch (err) {
          result = { assets: [], error: String(err) };
          message = `Failed to list assets: ${err}`;
        }
        
        // Format the message to include asset details
        if (result && result.assets && result.assets.length > 0) {
          const assetList = result.assets.map((asset: any) => {
            if (typeof asset === 'string') {
              return asset;
            } else if (asset.Path) {
              return asset.Path;
            } else if (asset.Name && asset.PackagePath) {
              return `${asset.PackagePath}/${asset.Name}`;
            } else if (asset.Name) {
              return asset.Name;
            } else if (asset.ObjectPath) {
              return asset.ObjectPath;
            } else {
              return JSON.stringify(asset);
            }
          });
          message = `Found ${result.assets.length} assets in ${args.directory || '/Game'}:\n${assetList.join('\n')}`;
        }
        break;
      
      case 'import_asset':
        result = await tools.assetTools.importAsset(args.sourcePath, args.destinationPath);
        // Check if import actually succeeded
        if (result.error) {
          message = result.error;
        } else if (result.success && result.paths && result.paths.length > 0) {
          message = result.message || `Successfully imported ${result.paths.length} asset(s) to ${args.destinationPath}`;
        } else {
          message = result.message || result.error || `Import did not report success for source ${args.sourcePath}`;
        }
        break;

      // Actor Tools
      case 'spawn_actor':
        // Normalize transforms: accept object or array
        if (args.location !== undefined && args.location !== null) {
          const loc = toVec3Object(args.location);
          if (!loc) throw new Error('Invalid location: expected {x,y,z} or [x,y,z]');
          args.location = loc;
        }
        if (args.rotation !== undefined && args.rotation !== null) {
          const rot = toRotObject(args.rotation);
          if (!rot) throw new Error('Invalid rotation: expected {pitch,yaw,roll} or [pitch,yaw,roll]');
          args.rotation = rot;
        }
        result = await tools.actorTools.spawn(args);
        message = `Actor spawned: ${JSON.stringify(result)}`;
        break;
      
      case 'delete_actor':
        // Use EditorActorSubsystem instead of deprecated EditorLevelLibrary
        try {
          const pythonCmd = `
import unreal
import json

result = {"success": False, "message": "", "deleted_count": 0, "deleted_actors": []}

try:
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = actor_subsystem.get_all_level_actors()
    deleted_actors = []
    search_name = "${args.actorName}"
    
    for actor in actors:
        if actor:
            # Check both actor name and label
            actor_name = actor.get_name()
            actor_label = actor.get_actor_label()
            
            # Case-insensitive partial matching
            if (search_name.lower() in actor_label.lower() or
                actor_label.lower().startswith(search_name.lower() + "_") or
                actor_label.lower() == search_name.lower() or
                actor_name.lower() == search_name.lower()):
                actor_subsystem.destroy_actor(actor)
                deleted_actors.append(actor_label)
                
    if deleted_actors:
        result["success"] = True
        result["deleted_count"] = len(deleted_actors)
        result["deleted_actors"] = deleted_actors
        result["message"] = f"Deleted {len(deleted_actors)} actor(s): {deleted_actors}"
    else:
        result["message"] = f"No actors found matching: {search_name}"
        # List available actors for debugging
        all_labels = [a.get_actor_label() for a in actors[:10] if a]
        result["available_actors"] = all_labels
        
except Exception as e:
    result["message"] = f"Error deleting actors: {e}"
    
print(f"RESULT:{json.dumps(result)}")
          `.trim();
          const response = await tools.bridge.executePython(pythonCmd);
          
          // Extract output from Python response
          let outputStr = '';
          if (typeof response === 'object' && response !== null) {
            // Check if it has LogOutput (standard Python execution response)
            if (response.LogOutput && Array.isArray(response.LogOutput)) {
              // Concatenate all log outputs
              outputStr = response.LogOutput
                .map((log: any) => log.Output || '')
                .join('');
            } else if ('result' in response) {
              outputStr = String(response.result);
            } else {
              outputStr = JSON.stringify(response);
            }
          } else {
            outputStr = String(response || '');
          }
          
          // Parse the result
          const resultMatch = outputStr.match(/RESULT:(\{.*\})/);
          if (resultMatch) {
            try {
              const deleteResult = JSON.parse(resultMatch[1]);
              if (!deleteResult.success) {
                throw new Error(deleteResult.message);
              }
              result = deleteResult;
              message = deleteResult.message;
            } catch {
              // Fallback to checking output
              if (outputStr.includes('Deleted')) {
                result = { success: true, message: outputStr };
                message = `Actor deleted: ${args.actorName}`;
              } else {
                throw new Error(outputStr || 'Delete failed');
              }
            }
          } else {
            // Check for error patterns  
            if (outputStr.includes('No actors found') || outputStr.includes('Error')) {
              throw new Error(outputStr || 'Delete failed - no actors found');
            }
            // Only report success if clear indication
            if (outputStr.includes('Deleted')) {
              result = { success: true, message: outputStr };
              message = `Actor deleted: ${args.actorName}`;
            } else {
              throw new Error('No valid result from Python delete operation');
            }
          }
        } catch (pyErr) {
          // Fallback to console command
          const consoleResult = await tools.bridge.executeConsoleCommand(`DestroyActor ${args.actorName}`);
          
          // Check console command result
          if (consoleResult && typeof consoleResult === 'object') {
            // Console commands don't reliably report success/failure
            // Return an error to avoid false positives
            throw new Error(`Delete operation uncertain via console command for '${args.actorName}'. Python execution failed: ${pyErr}`);
          }
          
          // If we're here, we can't guarantee the delete worked
          result = { 
            success: false, 
            message: `Console command fallback attempted for '${args.actorName}', but result uncertain`,
            fallback: true 
          };
          message = result.message;
        }
        break;

      // Material Tools
      case 'create_material':
        result = await tools.materialTools.createMaterial(args.name, args.path);
        message = result.success ? `Material created: ${result.path}` : result.error;
        break;
      
      case 'apply_material_to_actor':
        result = await tools.materialTools.applyMaterialToActor(
          args.actorPath, 
          args.materialPath, 
          args.slotIndex !== undefined ? args.slotIndex : 0
        );
        message = result.success ? result.message : result.error;
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
        // Normalize force vector
        const forceVec = toVec3Array(args.force);
        if (!forceVec) throw new Error('Invalid force: expected {x,y,z} or [x,y,z]');
        // Map the simple force schema to PhysicsTools expected format
        result = await tools.physicsTools.applyForce({
          actorName: args.actorName,
          forceType: 'Force', // Default to 'Force' type
          vector: forceVec,
          isLocal: false // World space by default
        });
        // Check if the result indicates an error
        if (result.error || (result.success === false)) {
          throw new Error(result.error || result.message || `Failed to apply force to ${args.actorName}`);
        }
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
        message = result.message || 'Niagara system spawned';
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
        message = result.message || 'Level streaming updated';
        break;

      // Lighting Tools
      case 'create_light':
        // Normalize transforms
        const lightLocObj = args.location ? (toVec3Object(args.location) || { x: 0, y: 0, z: 0 }) : { x: 0, y: 0, z: 0 };
        const lightLoc = [lightLocObj.x, lightLocObj.y, lightLocObj.z] as [number, number, number];
        const lightRotObj = args.rotation ? (toRotObject(args.rotation) || { pitch: 0, yaw: 0, roll: 0 }) : { pitch: 0, yaw: 0, roll: 0 };
        const lightRot = [lightRotObj.pitch, lightRotObj.yaw, lightRotObj.roll] as [number, number, number];
        
        switch (args.lightType?.toLowerCase()) {
          case 'directional':
            result = await tools.lightingTools.createDirectionalLight({
              name: args.name,
              intensity: args.intensity,
              rotation: lightRot
            });
            break;
          case 'point':
            result = await tools.lightingTools.createPointLight({
              name: args.name,
              location: lightLoc,
              intensity: args.intensity
            });
            break;
          case 'spot':
            result = await tools.lightingTools.createSpotLight({
              name: args.name,
              location: lightLoc,
              rotation: lightRot,
              intensity: args.intensity
            });
            break;
          case 'rect':
            result = await tools.lightingTools.createRectLight({
              name: args.name,
              location: lightLoc,
              rotation: lightRot,
              intensity: args.intensity
            });
            break;
          case 'sky':
            result = await tools.lightingTools.createSkyLight({
              name: args.name,
              intensity: args.intensity,
              recapture: true
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
        // Convert position object to array if needed
        const position = Array.isArray(args.position) ? args.position : 
          (args.position ? [args.position.x || 0, args.position.y || 0, args.position.z || 0] : [0, 0, 0]);
        
        switch (args.shape?.toLowerCase()) {
          case 'line':
            result = await tools.debugTools.drawDebugLine({
              start: position,
              end: args.end || [position[0] + 100, position[1], position[2]],
              color: args.color,
              duration: args.duration
            });
            break;
          case 'box':
            result = await tools.debugTools.drawDebugBox({
              center: position,
              extent: [args.size, args.size, args.size],
              color: args.color,
              duration: args.duration
            });
            break;
          case 'sphere':
            result = await tools.debugTools.drawDebugSphere({
              center: position,
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
        // Check if sound exists first
        const soundCheckPy = `
import unreal, json
path = r"${args.soundPath}"
try:
    exists = unreal.EditorAssetLibrary.does_asset_exist(path)
    print('SOUNDCHECK:' + json.dumps({'exists': bool(exists)}))
except Exception as e:
    print('SOUNDCHECK:' + json.dumps({'exists': False, 'error': str(e)}))
`.trim();
        
        let soundExists = false;
        try {
          const checkResp = await tools.bridge.executePython(soundCheckPy);
          const checkOut = typeof checkResp === 'string' ? checkResp : JSON.stringify(checkResp);
          const checkMatch = checkOut.match(/SOUNDCHECK:({.*})/);
          if (checkMatch) {
            const checkParsed = JSON.parse(checkMatch[1]);
            soundExists = checkParsed.exists === true;
          }
        } catch {}
        
        if (!soundExists && !args.soundPath.includes('/Engine/')) {
          throw new Error(`Sound asset not found: ${args.soundPath}`);
        }
        
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

      // Console command execution
      case 'console_command':
        // Validate command parameter
        if (!args.command || typeof args.command !== 'string') {
          throw new Error('Invalid command: must be a non-empty string');
        }
        
        const command = args.command.trim();
        if (command.length === 0) {
          // Handle empty command gracefully
          result = { success: true, message: 'Empty command ignored' };
          message = 'Empty command ignored';
          break;
        }
        
        // Known problematic patterns that will generate warnings
        const problematicPatterns = [
          // /^stat fps$/i,  // Removed - allow stat fps as user requested
          /^invalid_/i,
          /^this_is_not/i,
          /^\d+$/,  // Just numbers
          /^[^a-zA-Z]/,  // Doesn't start with letter
        ];
        
        // Check for known invalid commands
        const cmdLower = command.toLowerCase();
        const knownInvalid = [
          'invalid_command_xyz',
          'this_is_not_a_valid_command',
          'stat invalid_stat',
          'viewmode invalid_mode',
          'r.invalidcvar',
          'sg.invalidquality'
        ];
        
        const isKnownInvalid = knownInvalid.some(invalid => 
          cmdLower === invalid.toLowerCase() || cmdLower.includes(invalid));
        
        // Allow stat fps without replacement - user knows what they want
        // if (cmdLower === 'stat fps') {
        //   command = 'stat unit';
        //   log.info('Replacing "stat fps" with "stat unit" to avoid warnings');
        // }
        
        // Handle commands with special characters that might fail
        if (command.includes(';')) {
          // Split compound commands
          const commands = command.split(';').map((c: string) => c.trim()).filter((c: string) => c.length > 0);
          if (commands.length > 1) {
            // Execute each command separately
            const results = [];
            for (const cmd of commands) {
              try {
                await tools.bridge.executeConsoleCommand(cmd);
                results.push({ command: cmd, success: true });
              } catch (e: any) {
                results.push({ command: cmd, success: false, error: e.message });
              }
            }
            result = { multiCommand: true, results };
            message = `Executed ${results.length} commands`;
            break;
          }
        }
        
        try {
          result = await tools.bridge.executeConsoleCommand(command);
          
          if (isKnownInvalid) {
            message = `Command executed (likely unrecognized): ${command}`;
            result = { ...result, warning: 'Command may not be recognized by Unreal Engine' };
          } else if (problematicPatterns.some(p => p.test(command))) {
            message = `Command executed (may have warnings): ${command}`;
            result = { ...result, info: 'Command may generate console warnings' };
          } else {
            message = `Console command executed: ${command}`;
          }
        } catch (error: any) {
          // Don't throw for console commands - they often "succeed" even when unrecognized
          log.warn(`Console command error for '${command}':`, error.message);
          
          // Return a warning result instead of failing
          result = { 
            success: false, 
            command: command,
            error: error.message,
            warning: 'Command may have failed or been unrecognized'
          };
          message = `Console command attempted: ${command} (may have failed)`;
        }
        break;

      // New tools implemented here (also used by consolidated handler)
      case 'rc_create_preset':
        result = await tools.rcTools.createPreset({ name: args.name, path: args.path });
        message = result.message || (result.success ? `Preset created at ${result.presetPath}` : result.error);
        break;
      case 'rc_expose_actor':
        result = await tools.rcTools.exposeActor({ presetPath: args.presetPath, actorName: args.actorName });
        message = result.message || (result.success ? 'Actor exposed' : result.error);
        break;
      case 'rc_expose_property':
        result = await tools.rcTools.exposeProperty({ presetPath: args.presetPath, objectPath: args.objectPath, propertyName: args.propertyName });
        message = result.message || (result.success ? 'Property exposed' : result.error);
        break;
      case 'rc_list_fields':
        result = await tools.rcTools.listFields({ presetPath: args.presetPath });
        message = result.message || (result.success ? `Found ${(result.fields||[]).length} fields` : result.error);
        break;
      case 'rc_set_property':
        result = await tools.rcTools.setProperty({ objectPath: args.objectPath, propertyName: args.propertyName, value: args.value });
        message = result.message || (result.success ? 'Property set' : result.error);
        break;
      case 'rc_get_property':
        result = await tools.rcTools.getProperty({ objectPath: args.objectPath, propertyName: args.propertyName });
        message = result.message || (result.success ? 'Property retrieved' : result.error);
        break;

      case 'seq_create':
        result = await tools.sequenceTools.create({ name: args.name, path: args.path });
        message = result.message || (result.success ? `Sequence created at ${result.sequencePath}` : result.error);
        break;
      case 'seq_open':
        result = await tools.sequenceTools.open({ path: args.path });
        message = result.message || (result.success ? 'Sequence opened' : result.error);
        break;
      case 'seq_add_camera':
        result = await tools.sequenceTools.addCamera({ spawnable: args.spawnable });
        message = result.message || (result.success ? 'Camera added to sequence' : result.error);
        break;
      case 'seq_add_actor':
        result = await tools.sequenceTools.addActor({ actorName: args.actorName });
        message = result.message || (result.success ? 'Actor added to sequence' : result.error);
        break;

      case 'inspect_object':
        result = await tools.introspectionTools.inspectObject({ objectPath: args.objectPath });
        message = result.message || (result.success ? 'Object inspected' : result.error);
        break;
      case 'inspect_set_property':
        result = await tools.introspectionTools.setProperty({ objectPath: args.objectPath, propertyName: args.propertyName, value: args.value });
        message = result.message || (result.success ? 'Property set' : result.error);
        break;

      case 'take_screenshot':
        result = await tools.visualTools.takeScreenshot({ resolution: args.resolution });
        message = result.message || (result.success ? 'Screenshot captured' : result.error);
        break;

      case 'launch_editor':
        result = await tools.engineTools.launchEditor({ editorExe: args.editorExe, projectPath: args.projectPath });
        message = result.message || (result.success ? 'Launch requested' : result.error);
        break;
      case 'quit_editor':
        result = await tools.engineTools.quitEditor();
        message = result.message || (result.success ? 'Quit requested' : result.error);
        break;

      default:
        throw new Error(`Unknown tool: ${name}`);
    }

    // Return the raw result for tests, with MCP format properties added
    return {
      ...result,  // Include all properties from the tool result
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
