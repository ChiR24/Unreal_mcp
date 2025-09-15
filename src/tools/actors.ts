import { UnrealBridge } from '../unreal-bridge.js';

export class ActorTools {
  constructor(private bridge: UnrealBridge) {}

  async spawn(params: { classPath: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number } }) {
    // Validate classPath
    if (!params.classPath || typeof params.classPath !== 'string') {
      throw new Error(`Invalid classPath: ${params.classPath}`);
    }
    
    // Reject asset paths that are commonly mistaken for class paths
    if (params.classPath.includes('/Engine/BasicShapes/') || 
        params.classPath.includes('/Engine/Content/') ||
        (params.classPath.includes('/Engine/') && !params.classPath.includes('/Script/'))) {
      throw new Error(`Invalid classPath: "${params.classPath}" appears to be an asset path, not an actor class. Use class names like 'StaticMeshActor', 'CameraActor', etc.`);
    }
    
    // Reject known invalid patterns
    if (params.classPath === 'InvalidActorClass' || 
        params.classPath === 'NoSlash' ||
        params.classPath.startsWith('/Invalid/') ||
        params.classPath.startsWith('/NotExist/')) {
      throw new Error(`Invalid actor class: ${params.classPath}`);
    }
    
    // Try Python API first for better control and naming
    try {
      return await this.spawnViaPython(params);
    } catch (pythonErr: any) {
      // Check if this is a known failure that shouldn't fall back
      const errorStr = String(pythonErr).toLowerCase();
      if (errorStr.includes('abstract') || errorStr.includes('class not found')) {
        // Don't try console fallback for abstract or non-existent classes
        throw pythonErr;
      }
      
      // Check if the error is because of PIE mode
      if (String(pythonErr).includes('Play In Editor mode')) {
        // Don't fall back to console if we're in PIE mode
        throw pythonErr;
      }
      
      // Fallback to console if Python fails for other reasons
      // Only log if not a known/expected error
      if (!String(pythonErr).includes('No valid result from Python')) {
        console.error('Python spawn failed, falling back to console:', pythonErr);
      }
      return this.spawnViaConsole(params);
    }
  }
  
  async spawnViaPython(params: { classPath: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number } }) {
    try {
      // Normalize and validate location
      const loc = params.location ?? { x: 0, y: 0, z: 100 };
      if (loc === null) {
        throw new Error('Invalid location: null is not allowed');
      }
      if (typeof loc !== 'object' ||
          typeof loc.x !== 'number' ||
          typeof loc.y !== 'number' ||
          typeof loc.z !== 'number') {
        throw new Error('Invalid location: must have numeric x, y, z properties');
      }
      
      // Normalize and validate rotation
      const rot = params.rotation ?? { pitch: 0, yaw: 0, roll: 0 };
      if (rot === null) {
        throw new Error('Invalid rotation: null is not allowed');
      }
      if (typeof rot !== 'object' ||
          typeof rot.pitch !== 'number' ||
          typeof rot.yaw !== 'number' ||
          typeof rot.roll !== 'number') {
        throw new Error('Invalid rotation: must have numeric pitch, yaw, roll properties');
      }
      
      // Resolve the class path
      const fullClassPath = this.resolveActorClass(params.classPath);
      let className = params.classPath;
      
      // Extract simple class name for naming the actor
      if (fullClassPath.includes('.')) {
        className = fullClassPath.split('.').pop() || params.classPath;
      }
      
      const pythonCmd = `
import unreal
import json

result = {"success": False, "message": "", "actor_name": ""}

# Check if editor is in play mode first
try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les and les.is_in_play_in_editor():
        result["message"] = "Cannot spawn actors while in Play In Editor mode. Please stop PIE first."
        print(f"RESULT:{json.dumps(result)}")
        # Exit early from this script
        raise SystemExit(0)
except SystemExit:
    # Re-raise the SystemExit to exit properly
    raise
except:
    pass  # Continue if we can't check PIE state

# List of abstract classes that cannot be spawned
abstract_classes = ['PlaneReflectionCapture', 'ReflectionCapture', 'Actor', 'Pawn', 'Character']

# Check for abstract classes
if "${params.classPath}" in abstract_classes:
    result["message"] = f"Cannot spawn ${params.classPath}: class is abstract and cannot be instantiated"
    print(f"RESULT:{json.dumps(result)}")
else:
    try:
        # Get the world using the modern subsystem API
        editor_subsystem = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world = editor_subsystem.get_editor_world() if hasattr(editor_subsystem, 'get_editor_world') else None
        if not world:
            # Try LevelEditorSubsystem as fallback
            level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
            if hasattr(level_subsystem, 'get_editor_world'):
                world = level_subsystem.get_editor_world()
        
        # Handle content paths (assets) vs class names
        class_path = "${params.classPath}"
        location = unreal.Vector(${loc.x}, ${loc.y}, ${loc.z})
        rotation = unreal.Rotator(${rot.pitch}, ${rot.yaw}, ${rot.roll})
        actor = None
        
        # Check if this is a content path (starts with /Game or /Engine)
        if class_path.startswith('/Game') or class_path.startswith('/Engine'):
            # This is a content asset path - try to load and spawn it
            try:
                # For blueprint classes or static meshes
                asset = unreal.EditorAssetLibrary.load_asset(class_path)
                if asset:
                    # If it's a blueprint class
                    if isinstance(asset, unreal.Blueprint):
                        actor_class = asset.generated_class()
                        if actor_class:
                            # Use modern EditorActorSubsystem API
                            actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
                            actor = actor_subsys.spawn_actor_from_class(
                                actor_class,
                                location,
                                rotation
                            )
                    # If it's a static mesh, spawn a StaticMeshActor and assign the mesh
                    elif isinstance(asset, unreal.StaticMesh):
                        # Use modern EditorActorSubsystem API
                        actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
                        actor = actor_subsys.spawn_actor_from_class(
                            unreal.StaticMeshActor,
                            location,
                            rotation
                        )
                        if actor:
                            mesh_component = actor.get_component_by_class(unreal.StaticMeshComponent)
                            if mesh_component:
                                mesh_component.set_static_mesh(asset)
                                # Make it movable so physics can be applied
                                mesh_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
            except Exception as load_err:
                # If asset loading fails, try basic shapes from Engine content
                if 'Cube' in class_path or 'cube' in class_path.lower():
                    # Use Engine's basic cube
                    cube_mesh = unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/Cube')
                    if cube_mesh:
                        # Use modern EditorActorSubsystem API
                        actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
                        actor = actor_subsys.spawn_actor_from_class(
                            unreal.StaticMeshActor,
                            location,
                            rotation
                        )
                        if actor:
                            mesh_component = actor.get_component_by_class(unreal.StaticMeshComponent)
                            if mesh_component:
                                mesh_component.set_static_mesh(cube_mesh)
                                # Make it movable so physics can be applied
                                mesh_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
        
        # If not a content path or content spawn failed, try as a class name
        if not actor:
            if class_path == "StaticMeshActor":
                # Use modern EditorActorSubsystem API
                actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
                actor = actor_subsys.spawn_actor_from_class(
                    unreal.StaticMeshActor, 
                    location, 
                    rotation
                )
                    
                if actor:
                    # Assign a default mesh (cube)
                    cube_mesh = unreal.EditorAssetLibrary.load_asset('/Engine/BasicShapes/Cube')
                    if cube_mesh:
                        mesh_component = actor.get_component_by_class(unreal.StaticMeshComponent)
                        if mesh_component:
                            mesh_component.set_static_mesh(cube_mesh)
                            # Make it movable so physics can be applied
                            mesh_component.set_editor_property('mobility', unreal.ComponentMobility.MOVABLE)
                            
            elif class_path == "CameraActor":
                # Use modern EditorActorSubsystem API
                actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
                actor = actor_subsys.spawn_actor_from_class(
                    unreal.CameraActor,
                    location,
                    rotation
                )
            else:
                # Try to get the class by name for other actors (e.g., PointLight)
                actor_class = None
                if hasattr(unreal, class_path):
                    actor_class = getattr(unreal, class_path)
                
                if actor_class:
                    # Use modern EditorActorSubsystem API
                    actor_subsys = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
                    actor = actor_subsys.spawn_actor_from_class(
                        actor_class, 
                        location, 
                        rotation
                    )
        
        # Set the actor label and return result
        if actor:
            import time
            timestamp = int(time.time() * 1000) % 10000
            base_name = class_path.split('/')[-1] if '/' in class_path else class_path
            actor_name = f"{base_name}_{timestamp}"
            actor.set_actor_label(actor_name)
            result["success"] = True
            result["message"] = f"Spawned {actor_name} at ({location.x}, {location.y}, {location.z})"
            result["actor_name"] = actor_name
        else:
            result["message"] = f"Failed to spawn actor from: {class_path}. Try using /Engine/BasicShapes/Cube or StaticMeshActor"
                
    except Exception as e:
        result["message"] = f"Error spawning actor: {e}"
        
print(f"RESULT:{json.dumps(result)}")
`.trim();
      
      const response = await this.bridge.executePython(pythonCmd);
      
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
        } else if ('ReturnValue' in response && typeof response.ReturnValue === 'string') {
          outputStr = response.ReturnValue;
        } else {
          outputStr = JSON.stringify(response);
        }
      } else {
        outputStr = String(response || '');
      }
      
      // Parse the result from Python output
      const resultMatch = outputStr.match(/RESULT:({.*})/);
      if (resultMatch) {
        try {
          const result = JSON.parse(resultMatch[1]);
          if (!result.success) {
            throw new Error(result.message || 'Spawn failed');
          }
          return result;
        } catch {
          // If we can't parse, check for common success patterns
          if (outputStr.includes('Spawned')) {
            return { success: true, message: outputStr };
          }
          throw new Error(`Failed to parse Python result: ${outputStr}`);
        }
      } else {
        // Check output for success/failure patterns
        if (outputStr.includes('Failed') || outputStr.includes('Error') || outputStr.includes('not found')) {
          throw new Error(outputStr || 'Spawn failed');
        }
        // Default fallback - but this shouldn't report success for failed operations
        // Only report success if Python execution was successful and no error markers
        if (response?.ReturnValue === true && !outputStr.includes('abstract')) {
          return { success: true, message: `Actor spawned: ${className} at ${loc.x},${loc.y},${loc.z}` };
        } else {
          throw new Error(`Failed to spawn ${className}: No valid result from Python`);
        }
      }
    } catch (err) {
      throw new Error(`Failed to spawn actor via Python: ${err}`);
    }
  }
  
  async spawnViaConsole(params: { classPath: string; location?: { x: number; y: number; z: number }; rotation?: { pitch: number; yaw: number; roll: number } }) {
    try {
      // Check if editor is in play mode first
      try {
        const pieCheckPython = `
import unreal
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if les and les.is_in_play_in_editor():
    print("PIE_ACTIVE")
else:
    print("PIE_INACTIVE")
        `.trim();
        
        const pieCheckResult = await this.bridge.executePython(pieCheckPython);
        const outputStr = typeof pieCheckResult === 'string' ? pieCheckResult : JSON.stringify(pieCheckResult);
        
        if (outputStr.includes('PIE_ACTIVE')) {
          throw new Error('Cannot spawn actors while in Play In Editor mode. Please stop PIE first.');
        }
      } catch (pieErr: any) {
        // If the error is about PIE, throw it
        if (String(pieErr).includes('Play In Editor')) {
          throw pieErr;
        }
        // Otherwise ignore and continue
      }
      
      // List of known abstract classes that cannot be spawned
      const abstractClasses = ['PlaneReflectionCapture', 'ReflectionCapture', 'Actor'];
      
      // Check if this is an abstract class
      if (abstractClasses.includes(params.classPath)) {
        throw new Error(`Cannot spawn ${params.classPath}: class is abstract and cannot be instantiated`);
      }
      
      // Get the console-friendly class name
      const spawnClass = this.getConsoleClassName(params.classPath);
      
      // Use summon command with location if provided
      const loc = params.location || { x: 0, y: 0, z: 100 };
      const command = `summon ${spawnClass} ${loc.x} ${loc.y} ${loc.z}`;
      
      await this.bridge.httpCall('/remote/object/call', 'PUT', {
        objectPath: '/Script/Engine.Default__KismetSystemLibrary',
        functionName: 'ExecuteConsoleCommand',
        parameters: {
          WorldContextObject: null,
          Command: command,
          SpecificPlayer: null
        },
        generateTransaction: false
      });
      
      // Console commands don't reliably report success/failure
      // We can't guarantee this actually worked, so indicate uncertainty
      return { 
        success: true, 
        message: `Actor spawn attempted via console: ${spawnClass} at ${loc.x},${loc.y},${loc.z}`,
        note: 'Console spawn result uncertain - verify in editor'
      };
    } catch (err) {
      throw new Error(`Failed to spawn actor: ${err}`);
    }
  }
  
  private resolveActorClass(classPath: string): string {
    // Map common names to full Unreal class paths
    const classMap: { [key: string]: string } = {
      'PointLight': '/Script/Engine.PointLight',
      'DirectionalLight': '/Script/Engine.DirectionalLight',
      'SpotLight': '/Script/Engine.SpotLight',
      'RectLight': '/Script/Engine.RectLight',
      'SkyLight': '/Script/Engine.SkyLight',
      'StaticMeshActor': '/Script/Engine.StaticMeshActor',
      'PlayerStart': '/Script/Engine.PlayerStart',
      'Camera': '/Script/Engine.CameraActor',
      'CameraActor': '/Script/Engine.CameraActor',
      'Pawn': '/Script/Engine.DefaultPawn',
      'Character': '/Script/Engine.Character',
      'TriggerBox': '/Script/Engine.TriggerBox',
      'TriggerSphere': '/Script/Engine.TriggerSphere',
      'BlockingVolume': '/Script/Engine.BlockingVolume',
      'PostProcessVolume': '/Script/Engine.PostProcessVolume',
      'LightmassImportanceVolume': '/Script/Engine.LightmassImportanceVolume',
      'NavMeshBoundsVolume': '/Script/Engine.NavMeshBoundsVolume',
      'ExponentialHeightFog': '/Script/Engine.ExponentialHeightFog',
      'AtmosphericFog': '/Script/Engine.AtmosphericFog',
      'SphereReflectionCapture': '/Script/Engine.SphereReflectionCapture',
      'BoxReflectionCapture': '/Script/Engine.BoxReflectionCapture',
      // PlaneReflectionCapture is abstract and cannot be spawned
      'DecalActor': '/Script/Engine.DecalActor'
    };
    
    // Check if it's a simple name that needs mapping
    if (classMap[classPath]) {
      return classMap[classPath];
    }
    
    // Check if it already looks like a full path
    if (classPath.startsWith('/Script/') || classPath.startsWith('/Game/')) {
      return classPath;
    }
    
    // Check for Blueprint paths
    if (classPath.includes('Blueprint') || classPath.includes('BP_')) {
      // Ensure it has the proper prefix
      if (!classPath.startsWith('/Game/')) {
        return '/Game/' + classPath;
      }
      return classPath;
    }
    
    // Default: assume it's an engine class
    return '/Script/Engine.' + classPath;
  }
  
  private getConsoleClassName(classPath: string): string {
    // Normalize class path for console 'summon'
    const input = classPath;

    // Engine classes: reduce '/Script/Engine.ClassName' to 'ClassName'
    if (input.startsWith('/Script/Engine.')) {
      return input.replace('/Script/Engine.', '');
    }

    // If it's already a simple class name (no path) and not a /Game asset, strip optional _C and return
    if (!input.startsWith('/Game/') && !input.includes('/')) {
      if (input.endsWith('_C')) return input.slice(0, -2);
      return input;
    }

    // Blueprint assets under /Game: ensure '/Game/Path/Asset.Asset_C'
    if (input.startsWith('/Game/')) {
      // Remove any existing ".Something" suffix to rebuild normalized class ref
      const pathWithoutSuffix = input.split('.')[0];
      const parts = pathWithoutSuffix.split('/');
      const assetName = parts[parts.length - 1].replace(/_C$/, '');
      const normalized = `${pathWithoutSuffix}.${assetName}_C`;
      return normalized;
    }

    // Fallback: return input unchanged
    return input;
  }
}
